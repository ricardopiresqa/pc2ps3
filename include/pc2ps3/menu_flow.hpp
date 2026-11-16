#pragma once

#include <cstdint>

namespace pc2ps3 {

enum class MenuPage : std::uint8_t {
  kClosed,
  kMain,
  kPause,
  kSettings,
  kConfirmExit
};

enum class MenuAction : std::uint8_t {
  kOpenMain,
  kPause,
  kStartNewGame,
  kContinueGame,
  kOpenSettings,
  kResume,
  kBack,
  kConfirm,
  kCancel
};

enum class MenuCommand : std::uint8_t {
  kNone,
  kStartNewGame,
  kContinueGame,
  kQuitRequested
};

// Backend-independent navigation. Rendering and side effects stay caller-owned.
class MenuFlow {
 public:
  MenuFlow();

  MenuCommand apply(MenuAction action, bool has_save);
  MenuPage page() const;
  void reset();

 private:
  MenuPage page_;
  MenuPage return_page_;
};

}  // namespace pc2ps3
