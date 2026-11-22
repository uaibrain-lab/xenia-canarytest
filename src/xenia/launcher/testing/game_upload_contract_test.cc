// Part of the launcheruwp integration (uaibrain-lab).
// Released under the BSD license - see LICENSE in the root for more details.

#include <string_view>

#include "xenia/launcher/game_upload_contract.h"

#include "third_party/catch/include/catch.hpp"

namespace xe {
namespace launcher {
namespace test {

static GameUploadRequest Make(std::string_view mode, std::string_view target,
                              std::wstring_view name, uint64_t bytes = 1024) {
  return GameUploadRequest{mode, target, name, bytes};
}

TEST_CASE("Execution mode parsing is strict", "[launcher]") {
  auto emulated = ParseExecutionMode("emulated");
  REQUIRE(emulated.has_value());
  REQUIRE(*emulated == ExecutionMode::kEmulated);
  auto recompiled = ParseExecutionMode("recompiled");
  REQUIRE(recompiled.has_value());
  REQUIRE(*recompiled == ExecutionMode::kRecompiled);
  REQUIRE_FALSE(ParseExecutionMode("").has_value());
  REQUIRE_FALSE(ParseExecutionMode("EMULATED").has_value());
  REQUIRE_FALSE(ParseExecutionMode("both").has_value());
  REQUIRE_FALSE(ParseExecutionMode("emulated ").has_value());
}

TEST_CASE("Upload without a valid mode is rejected", "[launcher]") {
  REQUIRE(ValidateGameUpload(Make("", "xenia-canary", L"game.iso")) ==
          UploadVerdict::kModeMissingOrUnknown);
  REQUIRE(ValidateGameUpload(Make("both", "gta-iv", L"data.bin")) ==
          UploadVerdict::kModeMissingOrUnknown);
}

TEST_CASE("Emulated uploads target a known emulator and extension",
          "[launcher]") {
  REQUIRE(ValidateGameUpload(Make("emulated", "xenia-canary", L"Game.iso")) ==
          UploadVerdict::kAccepted);
  REQUIRE(ValidateGameUpload(Make("emulated", "xenia-canary", L"default.XEX")) ==
          UploadVerdict::kAccepted);
  REQUIRE(ValidateGameUpload(Make("emulated", "other-emu", L"Game.iso")) ==
          UploadVerdict::kUnknownTarget);
  REQUIRE(ValidateGameUpload(Make("emulated", "xenia-canary", L"setup.exe")) ==
          UploadVerdict::kUnsupportedExtension);
  REQUIRE(ValidateGameUpload(Make("emulated", "xenia-canary", L"noextension")) ==
          UploadVerdict::kUnsupportedExtension);
  // A recompiled game id is not an emulator id.
  REQUIRE(ValidateGameUpload(Make("emulated", "gta-iv", L"Game.iso")) ==
          UploadVerdict::kUnknownTarget);
}

TEST_CASE("Recompiled uploads target a known game id", "[launcher]") {
  REQUIRE(ValidateGameUpload(Make("recompiled", "gta-iv", L"data.bin")) ==
          UploadVerdict::kAccepted);
  REQUIRE(ValidateGameUpload(Make("recompiled", "web-of-shadows", L"a.dat")) ==
          UploadVerdict::kAccepted);
  REQUIRE(ValidateGameUpload(Make("recompiled", "unknown-game", L"a.dat")) ==
          UploadVerdict::kUnknownTarget);
  // An emulator id is not a game id.
  REQUIRE(ValidateGameUpload(Make("recompiled", "xenia-canary", L"a.dat")) ==
          UploadVerdict::kUnknownTarget);
}

TEST_CASE("Unsafe filenames are rejected", "[launcher]") {
  const wchar_t* bad[] = {L"..\\x.iso", L"a/b.iso",   L"a\\b.iso", L"CON.iso",
                          L"nul",       L"COM1.iso",  L"x.iso.",   L" x.iso",
                          L".hidden.iso", L"a:b.iso", L"a*b.iso",  L""};
  for (const wchar_t* name : bad) {
    REQUIRE(ValidateGameUpload(Make("emulated", "xenia-canary", name)) ==
            UploadVerdict::kInvalidFilename);
  }
}

TEST_CASE("Upload size limits", "[launcher]") {
  REQUIRE(ValidateGameUpload(Make("emulated", "xenia-canary", L"g.iso", 0)) ==
          UploadVerdict::kInvalidSize);
  REQUIRE(ValidateGameUpload(Make("emulated", "xenia-canary", L"g.iso",
                                  kMaxGameUploadBytes)) ==
          UploadVerdict::kAccepted);
  REQUIRE(ValidateGameUpload(Make("emulated", "xenia-canary", L"g.iso",
                                  kMaxGameUploadBytes + 1)) ==
          UploadVerdict::kInvalidSize);
}

TEST_CASE("Destinations keep emulated and recompiled files apart",
          "[launcher]") {
  REQUIRE((GameUploadDestination(
               Make("emulated", "xenia-canary", L"Game.iso")) ==
           L"Games/emulated/xenia-canary/Game.iso"));
  REQUIRE((GameUploadDestination(Make("recompiled", "gta-iv", L"data.bin")) ==
           L"Games/recompiled/gta-iv/data.bin"));
  REQUIRE(GameUploadDestination(Make("", "gta-iv", L"data.bin")).empty());
  REQUIRE(GameUploadDestination(Make("emulated", "xenia-canary", L"..\\x.iso"))
              .empty());
}

}  // namespace test
}  // namespace launcher
}  // namespace xe
