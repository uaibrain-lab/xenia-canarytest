// Part of the launcheruwp integration (uaibrain-lab).
// Released under the BSD license - see LICENSE in the root for more details.
//
// Portal upload contract for games. Storage only: validating an upload never
// extracts, executes or activates anything. The SAME checks must run in the
// portal server (never only in the browser form).

#ifndef XENIA_LAUNCHER_GAME_UPLOAD_CONTRACT_H_
#define XENIA_LAUNCHER_GAME_UPLOAD_CONTRACT_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>

#include "xenia/launcher/execution_mode.h"

namespace xe {
namespace launcher {

inline constexpr uint64_t kMaxGameUploadBytes = 16ull * 1024 * 1024 * 1024;

inline constexpr std::string_view kXeniaEmulatorId = "xenia-canary";

// An emulator engine the portal may route uploads to. To add another engine
// later (e.g. a PS2 emulator) append one entry here AND register a matching
// RuntimeBackend; nothing else in the contract changes.
struct EmulatorInfo {
  std::string_view id;
  std::string_view display_name;
  std::string_view guest_platform;
  const std::wstring_view* extensions;
  size_t extension_count;
};

// First-pass filter only. The authority on what is really launchable is
// xe::Emulator::GetFileSignature at launch time.
inline constexpr std::wstring_view kXeniaExtensions[] = {
    L".iso", L".xex", L".zar", L".con", L".live", L".pirs"};

inline constexpr EmulatorInfo kEmulators[] = {
    {kXeniaEmulatorId, "Xenia Canary", "Xbox 360", kXeniaExtensions,
     std::size(kXeniaExtensions)},
};

// Fixed IDs shared with launcheruwp's library model. Recompiled runtimes are
// not integrated yet; this list only says which game folders may exist.
inline constexpr std::string_view kRecompiledGames[] = {"web-of-shadows",
                                                        "gta-iv"};

inline const EmulatorInfo* FindEmulator(std::string_view id) {
  for (const EmulatorInfo& emulator : kEmulators) {
    if (emulator.id == id) {
      return &emulator;
    }
  }
  return nullptr;
}

inline bool IsKnownRecompiledGame(std::string_view id) {
  for (std::string_view game : kRecompiledGames) {
    if (game == id) {
      return true;
    }
  }
  return false;
}

inline wchar_t UpperAscii(wchar_t c) {
  return (c >= L'a' && c <= L'z') ? static_cast<wchar_t>(c - (L'a' - L'A')) : c;
}

inline std::wstring LowerAscii(std::wstring_view text) {
  std::wstring out(text);
  for (wchar_t& c : out) {
    if (c >= L'A' && c <= L'Z') {
      c = static_cast<wchar_t>(c + (L'a' - L'A'));
    }
  }
  return out;
}

inline bool IsReservedWindowsFilename(std::wstring_view name) {
  size_t dot = name.find(L'.');
  std::wstring_view stem = name.substr(0, dot);
  if (stem == L"." || stem == L"..") {
    return true;
  }
  if (stem.size() == 3) {
    wchar_t a = UpperAscii(stem[0]);
    wchar_t b = UpperAscii(stem[1]);
    wchar_t c = UpperAscii(stem[2]);
    if ((a == L'C' && b == L'O' && c == L'N') ||
        (a == L'P' && b == L'R' && c == L'N') ||
        (a == L'A' && b == L'U' && c == L'X') ||
        (a == L'N' && b == L'U' && c == L'L')) {
      return true;
    }
  }
  if (stem.size() == 4) {
    wchar_t a = UpperAscii(stem[0]);
    wchar_t b = UpperAscii(stem[1]);
    wchar_t c = UpperAscii(stem[2]);
    wchar_t d = stem[3];
    if (((a == L'C' && b == L'O' && c == L'M') ||
         (a == L'L' && b == L'P' && c == L'T')) &&
        d >= L'1' && d <= L'9') {
      return true;
    }
  }
  return false;
}

// A single path component: no separators, traversal, reserved device names,
// control characters or bidi-override characters.
inline bool IsSafeGameFilename(std::wstring_view name) {
  if (name.empty() || name.size() > 180 || name.front() == L'.' ||
      name.front() == L' ' || name.back() == L'.' || name.back() == L' ' ||
      name.find(L"..") != std::wstring_view::npos ||
      IsReservedWindowsFilename(name)) {
    return false;
  }
  for (wchar_t c : name) {
    if (c < 32 || c == 127 || c == L'/' || c == L'\\' || c == L':' ||
        c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' ||
        c == L'|' || c == 0x202E || c == 0x202D) {
      return false;
    }
  }
  return true;
}

inline bool ExtensionAccepted(const EmulatorInfo& emulator,
                              std::wstring_view filename) {
  size_t dot = filename.rfind(L'.');
  if (dot == std::wstring_view::npos) {
    return false;
  }
  std::wstring extension = LowerAscii(filename.substr(dot));
  for (size_t i = 0; i < emulator.extension_count; ++i) {
    if (emulator.extensions[i] == extension) {
      return true;
    }
  }
  return false;
}

// What the portal form sends. `mode` is the raw form field and is REQUIRED.
// `target_id` is the emulator id (emulated) or the game id (recompiled).
struct GameUploadRequest {
  std::string_view mode;
  std::string_view target_id;
  std::wstring_view filename;
  uint64_t bytes;
};

enum class UploadVerdict {
  kAccepted,
  kModeMissingOrUnknown,
  kUnknownTarget,
  kInvalidFilename,
  kUnsupportedExtension,
  kInvalidSize,
};

inline UploadVerdict ValidateGameUpload(const GameUploadRequest& request) {
  std::optional<ExecutionMode> mode = ParseExecutionMode(request.mode);
  if (!mode.has_value()) {
    return UploadVerdict::kModeMissingOrUnknown;
  }
  if (request.bytes == 0 || request.bytes > kMaxGameUploadBytes) {
    return UploadVerdict::kInvalidSize;
  }
  if (!IsSafeGameFilename(request.filename)) {
    return UploadVerdict::kInvalidFilename;
  }
  if (*mode == ExecutionMode::kEmulated) {
    const EmulatorInfo* emulator = FindEmulator(request.target_id);
    if (!emulator) {
      return UploadVerdict::kUnknownTarget;
    }
    if (!ExtensionAccepted(*emulator, request.filename)) {
      return UploadVerdict::kUnsupportedExtension;
    }
  } else if (!IsKnownRecompiledGame(request.target_id)) {
    return UploadVerdict::kUnknownTarget;
  }
  return UploadVerdict::kAccepted;
}

// Relative path (forward slashes) under the app-local storage root where the
// file must be staged. Empty if the request is not accepted. Emulated and
// recompiled files never share a folder.
inline std::wstring GameUploadDestination(const GameUploadRequest& request) {
  if (ValidateGameUpload(request) != UploadVerdict::kAccepted) {
    return std::wstring();
  }
  ExecutionMode mode = *ParseExecutionMode(request.mode);
  std::wstring out = mode == ExecutionMode::kEmulated ? L"Games/emulated/"
                                                      : L"Games/recompiled/";
  // target_id was matched against the fixed ASCII lists above.
  for (char c : request.target_id) {
    out.push_back(static_cast<wchar_t>(c));
  }
  out.push_back(L'/');
  out.append(request.filename);
  return out;
}

// Short message for the portal response (no accents: safe in any encoding).
inline const char* UploadVerdictMessage(UploadVerdict verdict) {
  switch (verdict) {
    case UploadVerdict::kAccepted:
      return "Upload aceito.";
    case UploadVerdict::kModeMissingOrUnknown:
      return "Marque se o jogo e recompilado ou emulado.";
    case UploadVerdict::kUnknownTarget:
      return "Destino desconhecido para este modo.";
    case UploadVerdict::kInvalidFilename:
      return "Nome de arquivo invalido.";
    case UploadVerdict::kUnsupportedExtension:
      return "Extensao nao aceita por este emulador.";
    case UploadVerdict::kInvalidSize:
      return "Tamanho de arquivo invalido.";
  }
  return "Upload rejeitado.";
}

}  // namespace launcher
}  // namespace xe

#endif  // XENIA_LAUNCHER_GAME_UPLOAD_CONTRACT_H_
