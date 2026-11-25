/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_SURFACE_WIN_H_
#define XENIA_UI_SURFACE_WIN_H_

#include <cstdint>
#include <memory>

#include "xenia/ui/surface.h"

// Must be included before Windows headers for things like NOMINMAX.
#include "xenia/base/platform_win.h"
#include <wrl/client.h>

namespace xe {
namespace ui {

// The host owns the CoreWindow and provides current physical pixel dimensions.
// A COM reference keeps the borrowed identity alive for the surface lifetime.
class WinCoreWindowSurface final : public Surface {
 public:
  WinCoreWindowSurface(IUnknown* window, const uint32_t& width,
                       const uint32_t& height)
      : window_(window), width_(width), height_(height) {}
  TypeIndex GetType() const override { return kTypeIndex_WinCoreWindow; }
  IUnknown* core_window() const { return window_.Get(); }
 protected:
  bool GetSizeImpl(uint32_t& width_out, uint32_t& height_out) const override {
    width_out = width_;
    height_out = height_;
    return window_.Get() != nullptr;
  }
 private:
  Microsoft::WRL::ComPtr<IUnknown> window_;
  const uint32_t& width_;
  const uint32_t& height_;
};

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_GAMES)
class Win32HwndSurface final : public Surface {
 public:
  explicit Win32HwndSurface(HINSTANCE hinstance, HWND hwnd)
      : hinstance_(hinstance), hwnd_(hwnd) {}
  TypeIndex GetType() const override { return kTypeIndex_Win32Hwnd; }
  HINSTANCE hinstance() const { return hinstance_; }
  HWND hwnd() const { return hwnd_; }

 protected:
  bool GetSizeImpl(uint32_t& width_out, uint32_t& height_out) const override;

 private:
  HINSTANCE hinstance_;
  HWND hwnd_;
};
#endif

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_SURFACE_WIN_H_
