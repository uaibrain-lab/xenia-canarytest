// Part of the launcheruwp integration (uaibrain-lab).
// Released under the BSD license - see LICENSE in the root for more details.
//
// Backend boundary between the launcher UI and whatever actually runs a game.
// The UI never talks to Xenia (or a recompiled runtime) directly: it builds a
// LaunchRequest and hands it to LaunchRouter.

#ifndef XENIA_LAUNCHER_RUNTIME_BACKEND_H_
#define XENIA_LAUNCHER_RUNTIME_BACKEND_H_

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "xenia/launcher/execution_mode.h"

namespace xe {
namespace launcher {

struct LaunchRequest {
  ExecutionMode mode = ExecutionMode::kEmulated;
  // Emulator id (emulated) or game id (recompiled).
  std::string target_id;
  // Absolute path of the stored game file inside the app sandbox.
  std::filesystem::path path;
};

struct LaunchResult {
  bool started = false;
  std::string message;
};

class RuntimeBackend {
 public:
  virtual ~RuntimeBackend() = default;
  virtual ExecutionMode mode() const = 0;
  // False means Launch() must not be called; `reason` explains why.
  virtual bool IsAvailable(std::string* reason) const = 0;
  virtual LaunchResult Launch(const LaunchRequest& request) = 0;
};

// Recompiled games. Unavailable until a game adapter registers a launcher.
// An adapter must only register after its runtime is built, assets verified
// and validated on Xbox (same gate as launcheruwp's can_start_runtime).
class RecompiledBackend final : public RuntimeBackend {
 public:
  using GameLauncher = std::function<LaunchResult(const LaunchRequest&)>;

  ExecutionMode mode() const override { return ExecutionMode::kRecompiled; }

  void RegisterGame(std::string game_id, GameLauncher launcher) {
    launchers_[std::move(game_id)] = std::move(launcher);
  }

  bool IsAvailable(std::string* reason) const override {
    if (launchers_.empty()) {
      if (reason) {
        *reason = "Nenhum runtime recompilado integrado ainda.";
      }
      return false;
    }
    return true;
  }

  LaunchResult Launch(const LaunchRequest& request) override {
    if (request.mode != ExecutionMode::kRecompiled) {
      return {false, "Modo incompativel com o backend recompilado."};
    }
    auto it = launchers_.find(request.target_id);
    if (it == launchers_.end() || !it->second) {
      return {false, "Runtime recompilado ainda nao integrado para este jogo."};
    }
    return it->second(request);
  }

 private:
  std::map<std::string, GameLauncher> launchers_;
};

// Picks the backend that matches the request's execution mode.
class LaunchRouter {
 public:
  void Register(std::unique_ptr<RuntimeBackend> backend) {
    backends_.push_back(std::move(backend));
  }

  LaunchResult Launch(const LaunchRequest& request) {
    for (const std::unique_ptr<RuntimeBackend>& backend : backends_) {
      if (backend->mode() != request.mode) {
        continue;
      }
      std::string reason;
      if (!backend->IsAvailable(&reason)) {
        return {false, reason};
      }
      return backend->Launch(request);
    }
    return {false, "Nenhum backend registrado para este modo."};
  }

 private:
  std::vector<std::unique_ptr<RuntimeBackend>> backends_;
};

}  // namespace launcher
}  // namespace xe

#endif  // XENIA_LAUNCHER_RUNTIME_BACKEND_H_
