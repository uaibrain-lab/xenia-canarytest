// Part of the launcheruwp integration (uaibrain-lab).
// Released under the BSD license - see LICENSE in the root for more details.

#ifndef XENIA_LAUNCHER_EXECUTION_MODE_H_
#define XENIA_LAUNCHER_EXECUTION_MODE_H_

#include <optional>
#include <string_view>

namespace xe {
namespace launcher {

// How a game in the launcher library is executed.
//  - kEmulated: the file is run by an emulator engine (Xenia for Xbox 360).
//  - kRecompiled: the game is a separately built recompiled runtime; the
//    launcher only stores files and launches through a registered adapter.
enum class ExecutionMode { kEmulated, kRecompiled };

constexpr std::string_view ToString(ExecutionMode mode) {
  return mode == ExecutionMode::kEmulated ? std::string_view("emulated")
                                          : std::string_view("recompiled");
}

// Strict parser for the portal's "mode" field. There is deliberately no
// default: a missing or unknown value must be rejected, never guessed.
constexpr std::optional<ExecutionMode> ParseExecutionMode(
    std::string_view text) {
  if (text == "emulated") {
    return ExecutionMode::kEmulated;
  }
  if (text == "recompiled") {
    return ExecutionMode::kRecompiled;
  }
  return std::nullopt;
}

}  // namespace launcher
}  // namespace xe

#endif  // XENIA_LAUNCHER_EXECUTION_MODE_H_
