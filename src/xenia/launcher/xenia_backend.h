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
//  - Launch() is serialized against polling and shutdown. The caller may
//    use a worker if the owning UI thread services the app context until
//    Launch() returns (CompleteLaunch dispatches synchronously to the UI).
// The Emulator is not owned and must outlive this backend.
class XeniaBackend final : public RuntimeBackend {
 public:
  explicit XeniaBackend(xe::Emulator* emulator) : emulator_(emulator) {}

  ExecutionMode mode() const override { return ExecutionMode::kEmulated; }
  bool IsAvailable(std::string* reason) const override;
  LaunchResult Launch(const LaunchRequest& request) override;

  // Single session per configured emulator: the host must rebuild the
  // emulator before another launch (guest mounts are not yet reusable).
  // These methods must run on the owning UI thread. kStopped means the
  // title has stopped, NOT that GPU/audio or the emulator is released.
  enum class SessionState { kIdle, kRunning, kStopped, kFailed };
  // Embedded hosts may defer kernel cleanup to their release path.
  SessionState PollSession(bool terminate_on_exit = true);
  bool StopSession();

 private:
  xe::Emulator* emulator_;
  SessionState session_ = SessionState::kIdle;
};

}  // namespace launcher
}  // namespace xe

#endif  // XENIA_LAUNCHER_XENIA_BACKEND_H_
