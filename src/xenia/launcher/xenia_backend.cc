// Part of the launcheruwp integration (uaibrain-lab).
// Released under the BSD license - see LICENSE in the root for more details.

#include "xenia/launcher/xenia_backend.h"

#include <string>

#include "xenia/emulator.h"
#include "xenia/launcher/game_upload_contract.h"
#include "xenia/xbox.h"

namespace xe {
namespace launcher {

bool XeniaBackend::IsAvailable(std::string* reason) const {
  if (!emulator_) {
    if (reason) {
      *reason = "Motor Xenia nao inicializado.";
    }
    return false;
  }
  if (emulator_->is_title_open()) {
    if (reason) {
      *reason = "Ja existe um titulo em execucao no Xenia.";
    }
    return false;
  }
  return true;
}

LaunchResult XeniaBackend::Launch(const LaunchRequest& request) {
  if (request.mode != ExecutionMode::kEmulated ||
      request.target_id != kXeniaEmulatorId) {
    return {false, "Pedido nao e para o Xenia."};
  }
  std::string reason;
  if (!IsAvailable(&reason)) {
    return {false, reason};
  }
  X_STATUS status = emulator_->LaunchPath(request.path);
  if (status != X_STATUS_SUCCESS) {
    return {false, "Falha ao iniciar o jogo no Xenia (X_STATUS " +
                       std::to_string(static_cast<uint32_t>(status)) + ")."};
  }
  return {true, "Jogo iniciado no Xenia."};
}

}  // namespace launcher
}  // namespace xe
