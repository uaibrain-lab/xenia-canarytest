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
  if (session_ != SessionState::kIdle) {
    if (reason) {
      *reason = "Recrie o runtime Xenia antes de iniciar outro titulo.";
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
  // Fail closed even if LaunchPath throws after mounting a device.
  session_ = SessionState::kFailed;
  X_STATUS status = emulator_->LaunchPath(request.path);
  if (status != X_STATUS_SUCCESS) {
    return {false, "Falha ao iniciar o jogo no Xenia (X_STATUS " +
                       std::to_string(static_cast<uint32_t>(status)) + ")."};
  }
  session_ = SessionState::kRunning;
  return {true, "Jogo iniciado no Xenia."};
}

XeniaBackend::SessionState XeniaBackend::PollSession(bool terminate_on_exit) {
  if (session_ != SessionState::kRunning) {
    return session_;
  }
  switch (emulator_->PollTitleThread()) {
    case Emulator::TitleThreadState::kRunning:
      break;
    case Emulator::TitleThreadState::kExited:
      if (terminate_on_exit) {
        StopSession();
      } else {
        session_ = SessionState::kStopped;
      }
      break;
    default:
      session_ = SessionState::kFailed;
      break;
  }
  return session_;
}

bool XeniaBackend::StopSession() {
  if (!emulator_ || session_ == SessionState::kIdle) {
    return false;
  }
  if (session_ == SessionState::kStopped) {
    return true;
  }
  // TerminateTitle calls the real guest kernel, releases title objects and
  // invokes on_terminate. Do not run the standalone WaitUntilExit loop here.
  session_ = SessionState::kFailed;
  if (emulator_->is_title_open() &&
      emulator_->TerminateTitle() != X_STATUS_SUCCESS) {
    return false;
  }
  if (emulator_->PollTitleThread() != Emulator::TitleThreadState::kExited) {
    return false;
  }
  session_ = SessionState::kStopped;
  return true;
}

}  // namespace launcher
}  // namespace xe
