// Released under the BSD license - see LICENSE in the root.
#include "xenia/launcher/embedded_xenia_runtime.h"

#include <chrono>
#include <exception>
#include <future>
#include <utility>
#include "xenia/apu/xaudio2/xaudio2_audio_system.h"
#include "xenia/emulator.h"
#include "xenia/gpu/d3d12/d3d12_graphics_system.h"
#include "xenia/hid/input_driver.h"
#include "xenia/hid/xinput/xinput_hid.h"
#include "xenia/launcher/game_upload_contract.h"

namespace xe::launcher {
namespace {

// Match the standalone setup thread while keeping the borrowed window's UI
// context serviced. Always join before accessing or destroying guest state.
template <typename Work>
auto RunWithUIPump(EmbeddedAppContext& context, Work&& work) {
  auto task = std::async(std::launch::async, std::forward<Work>(work));
  std::exception_ptr callback_failure;
  while (task.wait_for(std::chrono::milliseconds(2)) !=
         std::future_status::ready) {
    try {
      // Drain unconditionally: a throwing deferred callback can leave more
      // entries queued without a new pending notification.
      context.ExecutePendingFunctionsFromUIThread();
    } catch (...) {
      if (!callback_failure) callback_failure = std::current_exception();
    }
  }
  auto result = task.get();
  if (callback_failure) std::rethrow_exception(callback_failure);
  return result;
}

}  // namespace

EmbeddedXeniaRuntime::EmbeddedXeniaRuntime(
    IUnknown* core_window, uint32_t width, uint32_t height,
    std::filesystem::path storage_root)
    : window_(context_, core_window, width, height),
      storage_root_(std::move(storage_root)) {}

EmbeddedXeniaRuntime::~EmbeddedXeniaRuntime() {
  // Destroying a live guest is not a supported recovery path.
  if (!Shutdown()) std::terminate();
}

bool EmbeddedXeniaRuntime::IsAvailable(std::string* reason) const {
  const char* error = nullptr;
  if (!context_.IsInUIThread()) error = "Xenia requires the host UI thread.";
  else if (used_ || released_) error = "Create a new Xenia session for another title.";
  else if (!storage_root_.is_absolute()) error = "Invalid Xenia storage root.";
  else if (!gpu::d3d12::D3D12GraphicsSystem::IsAvailable())
    error = "D3D12 unavailable.";
  if (error && reason) *reason = error;
  return !error;
}

LaunchResult EmbeddedXeniaRuntime::Launch(const LaunchRequest& request) {
  if (request.mode != ExecutionMode::kEmulated ||
      request.target_id != kXeniaEmulatorId || !request.path.is_absolute())
    return {false, "Invalid Xenia launch request."};
  std::string reason;
  if (!IsAvailable(&reason)) return {false, reason};
  used_ = true;
  try {
    const auto content = storage_root_ / "content";
    const auto cache = storage_root_ / "cache";
    std::filesystem::create_directories(content);
    std::filesystem::create_directories(cache);
    if (!window_.Open()) return {false, "Cannot open borrowed CoreWindow."};
    // InputSystem::Setup does not initialize individual drivers. Prepare the
    // real driver before creating the guest and fail instead of using nop HID.
    auto driver = hid::xinput::Create(&window_, 0);
    if (!driver || driver->Setup() != X_STATUS_SUCCESS) {
      Shutdown();
      return {false, "XInput initialization failed."};
    }
    emulator_ = std::make_unique<Emulator>(
        std::filesystem::path{}, storage_root_, content, cache, true);
    const X_STATUS status = RunWithUIPump(context_, [&]() {
      return emulator_->Setup(
        &window_, nullptr, true, true,
        [](cpu::Processor* processor) {
          return apu::xaudio2::XAudio2AudioSystem::Create(processor);
        },
        [] { return std::make_unique<gpu::d3d12::D3D12GraphicsSystem>(); },
        [&driver](ui::Window*) {
          std::vector<std::unique_ptr<hid::InputDriver>> drivers;
          drivers.emplace_back(std::move(driver));
          return drivers;
        });
    });
    if (status != X_STATUS_SUCCESS || !emulator_->graphics_system() ||
        !emulator_->graphics_system()->presenter()) {
      Shutdown();
      return {false, "Xenia setup/presenter failed: " +
                     std::to_string(static_cast<uint32_t>(status))};
    }
    if (!window_.AttachPresenter(emulator_->graphics_system()->presenter())) {
      Shutdown();
      return {false, "Xenia presenter cannot attach to the host CoreWindow."};
    }
    backend_ = std::make_unique<XeniaBackend>(emulator_.get());
    auto result = backend_->Launch(request);
    if (!result.started) Shutdown();
    return result;
  } catch (const std::exception& error) {
    Shutdown();
    return {false, error.what()};
  }
}

XeniaBackend::SessionState EmbeddedXeniaRuntime::Tick(
    uint32_t width, uint32_t height, bool focused) {
  if (!context_.IsInUIThread()) return XeniaBackend::SessionState::kFailed;
  context_.Pump();
  if (!backend_) return XeniaBackend::SessionState::kIdle;
  window_.Tick(width, height, focused);
  // Report natural exit without terminating remaining guest threads on the UI.
  // The ABI release call owns that cleanup and must precede host restoration.
  return backend_->PollSession(false);
}

bool EmbeddedXeniaRuntime::Shutdown() {
  if (!context_.IsInUIThread()) return false;
  // The C ABI calls Shutdown before delete; the destructor must not drain
  // callbacks a second time after the guest and its window were released.
  if (released_) return true;
  if (emulator_) {
    if (emulator_->is_title_open()) {
      // Kernel termination may wait for guest threads that have queued
      // synchronous UI work. Keep the UI thread pumping those callbacks
      // while a joined worker performs termination. No other emulator
      // operation may run until the worker completes.
      if (RunWithUIPump(context_, [this]() {
            return emulator_->TerminateTitle();
          }) != X_STATUS_SUCCESS) {
        return false;
      }
    }
    const auto state = emulator_->PollTitleThread();
    if (state == Emulator::TitleThreadState::kRunning ||
        state == Emulator::TitleThreadState::kWaitFailed)
      return false;
  }
  ReleaseRuntime();
  released_ = true;
  return true;
}

void EmbeddedXeniaRuntime::ReleaseRuntime() {
  context_.ExecutePendingFunctionsFromUIThread();
  window_.SetPresenter(nullptr);
  // Close the borrowed wrapper while its listeners are still alive. This
  // only closes the Xenia wrapper, not the host CoreWindow.
  window_.RequestClose();
  backend_.reset();
  emulator_.reset();
  context_.ExecutePendingFunctionsFromUIThread();
}

}  // namespace xe::launcher

// C ABI for a single-package UWP host. No STL objects or allocations cross
// the module boundary. The host must invoke every operation on its UI thread,
// keep the module loaded and relinquish its swap chain before launching.
// A failed release leaves the handle alive; never resume host presentation.
extern "C" {
__declspec(dllexport) uint32_t uaibrain_xenia_core_abi_v1() noexcept {
  return 1;
}

__declspec(dllexport) void* uaibrain_xenia_core_create_v1(
    IUnknown* core_window, uint32_t width, uint32_t height,
    const wchar_t* storage_root) noexcept {
  if (!core_window || !width || !height || !storage_root || !*storage_root)
    return nullptr;
  try {
    return new xe::launcher::EmbeddedXeniaRuntime(
        core_window, width, height, std::filesystem::path(storage_root));
  } catch (...) {
    return nullptr;
  }
}

__declspec(dllexport) int uaibrain_xenia_core_boot_v1(
    void* handle, const wchar_t* game_path) noexcept {
  if (!handle || !game_path || !*game_path) return 0;
  try {
    auto* runtime = static_cast<xe::launcher::EmbeddedXeniaRuntime*>(handle);
    xe::launcher::LaunchRequest request;
    request.mode = xe::launcher::ExecutionMode::kEmulated;
    request.target_id = xe::launcher::kXeniaEmulatorId;
    request.path = std::filesystem::path(game_path);
    return runtime->Launch(request).started ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

// Stable C ABI values: 0 idle, 1 running, 2 stopped, -1 failed.
// Stopped is NOT permission to recreate the host presenter: release first.
__declspec(dllexport) int uaibrain_xenia_core_tick_v1(
    void* handle, uint32_t width, uint32_t height, int focused) noexcept {
  if (!handle) return -1;
  try {
    auto* runtime = static_cast<xe::launcher::EmbeddedXeniaRuntime*>(handle);
    using State = xe::launcher::XeniaBackend::SessionState;
    switch (runtime->Tick(width, height, focused != 0)) {
      case State::kIdle: return 0;
      case State::kRunning: return 1;
      case State::kStopped: return 2;
      case State::kFailed: return -1;
    }
  } catch (...) {
    return -1;
  }
  return -1;
}

__declspec(dllexport) int uaibrain_xenia_core_release_v1(
    void* handle) noexcept {
  if (!handle) return 1;
  try {
    auto* runtime = static_cast<xe::launcher::EmbeddedXeniaRuntime*>(handle);
    if (!runtime->Shutdown()) return 0;
    delete runtime;
    return 1;
  } catch (...) {
    return 0;
  }
}
}  // extern "C"
