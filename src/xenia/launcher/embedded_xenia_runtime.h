// Released under the BSD license - see LICENSE in the root.
#pragma once

#include <filesystem>
#include <memory>
#include "xenia/launcher/embedded_core_window.h"
#include "xenia/launcher/xenia_backend.h"

namespace xe::launcher {

// UI-thread owned, one guest session. The host must relinquish its own
// swap chain before Launch and resume presentation after Shutdown succeeds.
// A new instance is required for every title.
class EmbeddedXeniaRuntime final : public RuntimeBackend {
 public:
  EmbeddedXeniaRuntime(IUnknown* core_window, uint32_t width, uint32_t height,
                       std::filesystem::path storage_root);
  ~EmbeddedXeniaRuntime() override;
  ExecutionMode mode() const override { return ExecutionMode::kEmulated; }
  bool IsAvailable(std::string* reason) const override;
  LaunchResult Launch(const LaunchRequest& request) override;
  XeniaBackend::SessionState Tick(uint32_t width, uint32_t height, bool focused);
  // False retains the running runtime; do not restore the host swap chain.
  bool Shutdown();
  // Pre-boot only: 0 = engine default, 30 or 60 = guest frame pacing.
  bool SetFrameLimit(uint32_t fps);

 private:
  void ReleaseRuntime();
  EmbeddedAppContext context_;
  EmbeddedCoreWindow window_;
  std::filesystem::path storage_root_;
  std::unique_ptr<Emulator> emulator_;
  std::unique_ptr<XeniaBackend> backend_;
  bool frame_limit_overridden_ = false;
  bool used_ = false;
  bool released_ = false;
};

}  // namespace xe::launcher
