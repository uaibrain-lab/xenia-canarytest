// Released under the BSD license - see LICENSE in the root.
#pragma once

#include <atomic>
#include <memory>
#include "xenia/ui/surface_win.h"
#include "xenia/ui/window.h"

namespace xe::launcher {

// Jadyx owns the UI loop. Pump from that thread while the session exists.
// No CoreApplication::Run, desktop window, or queued OS callback is created.
class EmbeddedAppContext final : public ui::WindowedAppContext {
 public:
  void Pump() {
    if (pending_.exchange(false)) ExecutePendingFunctionsFromUIThread();
  }
 protected:
  void NotifyUILoopOfPendingFunctions() override { pending_.store(true); }
  void PlatformQuitFromUIThread() override {}
 private:
  std::atomic<bool> pending_{false};
};

class EmbeddedCoreWindow final : public ui::Window {
 public:
  EmbeddedCoreWindow(EmbeddedAppContext& context, IUnknown* window,
                     uint32_t width, uint32_t height)
      : Window(context, "Xenia", width, height), window_(window),
        width_(width), height_(height) {}
  ~EmbeddedCoreWindow() override { EnterDestructor(); }

  // Called by the host on the owning thread with its current viewport/focus.
  // Surface dimensions are updated before notifying the presenter.
  void Tick(uint32_t width, uint32_t height, bool focused) {
    if (phase() != Phase::kOpen) return;
    width_ = width;
    height_ = height;
    WindowDestructionReceiver receiver(this);
    OnActualSizeUpdate(width_, height_, WindowResizeAction::kAutoMaximize,
                       receiver);
    if (receiver.IsWindowDestroyedOrClosed()) return;
    OnFocusUpdate(focused, receiver);
    if (receiver.IsWindowDestroyedOrClosed()) return;
    if (paint_pending_.exchange(false)) OnPaint();
  }
 protected:
  bool OpenImpl() override {
    if (!window_.Get() || !width_ || !height_) return false;
    WindowDestructionReceiver receiver(this);
    OnDesiredFullscreenUpdate(true);
    OnActualSizeUpdate(width_, height_, WindowResizeAction::kAutoMaximize,
                       receiver);
    return true;
  }
  void RequestCloseImpl() override {
    WindowDestructionReceiver receiver(this);
    OnBeforeClose(receiver);
    if (!receiver.IsWindowDestroyed()) OnAfterClose();
  }
  std::unique_ptr<ui::Surface> CreateSurfaceImpl(
      ui::Surface::TypeFlags allowed_types) override {
    if (!(allowed_types & ui::Surface::kTypeFlag_WinCoreWindow)) return {};
    return std::make_unique<ui::WinCoreWindowSurface>(window_.Get(), width_, height_);
  }
  void RequestPaintImpl() override { paint_pending_.store(true); }
 private:
  Microsoft::WRL::ComPtr<IUnknown> window_;
  uint32_t width_, height_;
  std::atomic<bool> paint_pending_{true};
};

}  // namespace xe::launcher
