// Part of the launcheruwp integration (uaibrain-lab).
// Released under the BSD license - see LICENSE in the root for more details.

#include <memory>
#include <string>

#include "xenia/launcher/runtime_backend.h"

#include "third_party/catch/include/catch.hpp"

namespace xe {
namespace launcher {
namespace test {

class FakeEmulatedBackend final : public RuntimeBackend {
 public:
  explicit FakeEmulatedBackend(bool available) : available_(available) {}
  ExecutionMode mode() const override { return ExecutionMode::kEmulated; }
  bool IsAvailable(std::string* reason) const override {
    if (!available_ && reason) {
      *reason = "fake indisponivel";
    }
    return available_;
  }
  LaunchResult Launch(const LaunchRequest&) override {
    return {true, "fake iniciado"};
  }

 private:
  bool available_;
};

TEST_CASE("Recompiled backend is unavailable until a game registers",
          "[launcher]") {
  RecompiledBackend backend;
  std::string reason;
  REQUIRE_FALSE(backend.IsAvailable(&reason));
  REQUIRE_FALSE(reason.empty());

  LaunchRequest request;
  request.mode = ExecutionMode::kRecompiled;
  request.target_id = "gta-iv";
  REQUIRE_FALSE(backend.Launch(request).started);
}

TEST_CASE("Recompiled backend launches only registered games", "[launcher]") {
  RecompiledBackend backend;
  backend.RegisterGame("gta-iv", [](const LaunchRequest&) {
    return LaunchResult{true, "ok"};
  });
  REQUIRE(backend.IsAvailable(nullptr));

  LaunchRequest registered;
  registered.mode = ExecutionMode::kRecompiled;
  registered.target_id = "gta-iv";
  REQUIRE(backend.Launch(registered).started);

  LaunchRequest other = registered;
  other.target_id = "web-of-shadows";
  REQUIRE_FALSE(backend.Launch(other).started);

  LaunchRequest wrong_mode = registered;
  wrong_mode.mode = ExecutionMode::kEmulated;
  REQUIRE_FALSE(backend.Launch(wrong_mode).started);
}

TEST_CASE("Router dispatches by execution mode", "[launcher]") {
  LaunchRouter router;
  router.Register(std::make_unique<FakeEmulatedBackend>(true));
  router.Register(std::make_unique<RecompiledBackend>());

  LaunchRequest emulated;
  emulated.mode = ExecutionMode::kEmulated;
  emulated.target_id = "xenia-canary";
  REQUIRE(router.Launch(emulated).started);

  // Recompiled backend has no registered game: blocked, not silently emulated.
  LaunchRequest recompiled;
  recompiled.mode = ExecutionMode::kRecompiled;
  recompiled.target_id = "gta-iv";
  LaunchResult result = router.Launch(recompiled);
  REQUIRE_FALSE(result.started);
  REQUIRE_FALSE(result.message.empty());
}

TEST_CASE("Router reports unavailable backend and missing backend",
          "[launcher]") {
  LaunchRouter only_unavailable;
  only_unavailable.Register(std::make_unique<FakeEmulatedBackend>(false));
  LaunchRequest emulated;
  emulated.mode = ExecutionMode::kEmulated;
  LaunchResult unavailable = only_unavailable.Launch(emulated);
  REQUIRE_FALSE(unavailable.started);
  REQUIRE(unavailable.message == "fake indisponivel");

  LaunchRouter empty;
  REQUIRE_FALSE(empty.Launch(emulated).started);
}

}  // namespace test
}  // namespace launcher
}  // namespace xe
