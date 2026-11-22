// Part of the launcheruwp integration (uaibrain-lab).
// Released under the BSD license - see LICENSE in the root for more details.

#ifndef XENIA_LAUNCHER_XENIA_BACKEND_H_
#define XENIA_LAUNCHER_XENIA_BACKEND_H_

#include <string>

#include "xenia/launcher/runtime_backend.h"

namespace xe {
class Emulator;
}  // namespace xe

namespace xe {
namespace launcher {

// Emulated mode: runs Xbox 360 files through the Xenia engine.
//
// Preconditions (owned by the host app, not by this class):
//  - `emulator` was constructed AND Emulator::Setup() already succeeded, with
//    the window/graphics/audio/input factories of the target platform. On
//    Xbox UWP those platform pieces do not exist yet in this tree.
//  - Launch() is called on the thread that owns the emulator (UI thread).
// The Emulator is not owned and must outlive this backend.
class XeniaBackend final : public RuntimeBackend {
 public:
  explicit XeniaBackend(xe::Emulator* emulator) : emulator_(emulator) {}

  ExecutionMode mode() const override { return ExecutionMode::kEmulated; }
  bool IsAvailable(std::string* reason) const override;
  LaunchResult Launch(const LaunchRequest& request) override;

 private:
  xe::Emulator* emulator_;
};

}  // namespace launcher
}  // namespace xe

#endif  // XENIA_LAUNCHER_XENIA_BACKEND_H_
