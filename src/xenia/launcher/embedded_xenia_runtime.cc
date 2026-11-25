// Released under the BSD license - see LICENSE in the root.
#include "xenia/launcher/embedded_xenia_runtime.h"

#include <exception>
#include <utility>
#include "xenia/apu/xaudio2/xaudio2_audio_system.h"
#include "xenia/emulator.h"
#include "xenia/gpu/d3d12/d3d12_graphics_system.h"
#include "xenia/hid/input_driver.h"
#include "xenia/hid/xinput/xinput_hid.h"
#include "xenia/launcher/game_upload_contract.h"

namespace xe::launcher {

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
  else if (used_) error = "Create a new Xenia session for another title.";
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
    const X_STATUS status = emulator_->Setup(
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
    if (status != X_STATUS_SUCCESS || !emulator_->graphics_system() ||
        !emulator_->graphics_system()->presenter()) {
      Shutdown();
      return {false, "Xenia setup/presenter failed: " +
                     std::to_string(static_cast<uint32_t>(status))};
    }
    window_.SetPresenter(emulator_->graphics_system()->presenter());
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
  return backend_->PollSession();
}

bool EmbeddedXeniaRuntime::Shutdown() {
  if (!context_.IsInUIThread()) return false;
  if (emulator_) {
    if (emulator_->is_title_open() &&
        emulator_->TerminateTitle() != X_STATUS_SUCCESS)
      return false;
    const auto state = emulator_->PollTitleThread();
    if (state == Emulator::TitleThreadState::kRunning ||
        state == Emulator::TitleThreadState::kWaitFailed)
      return false;
  }
  ReleaseRuntime();
  return true;
}

void EmbeddedXeniaRuntime::ReleaseRuntime() {
  context_.ExecutePendingFunctionsFromUIThread();
  window_.SetPresenter(nullptr);
  backend_.reset();
  emulator_.reset();
  context_.ExecutePendingFunctionsFromUIThread();
  window_.RequestClose();
}

}  // namespace xe::launcher
