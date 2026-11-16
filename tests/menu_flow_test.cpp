#include "pc2ps3/menu_flow.hpp"

#include <iostream>

namespace {
int failures = 0;

void expect(bool condition, const char* label) {
  if (!condition) {
    std::cerr << "FAIL: " << label << '\n';
    ++failures;
  }
}

void test_main_pause_settings_and_commands() {
  pc2ps3::MenuFlow menu;
  expect(menu.page() == pc2ps3::MenuPage::kClosed, "starts closed");
  expect(menu.apply(pc2ps3::MenuAction::kOpenMain, false) ==
             pc2ps3::MenuCommand::kNone &&
             menu.page() == pc2ps3::MenuPage::kMain,
         "opens main page");
  expect(menu.apply(pc2ps3::MenuAction::kContinueGame, false) ==
             pc2ps3::MenuCommand::kNone &&
             menu.page() == pc2ps3::MenuPage::kMain,
         "continue without save is ignored");
  expect(menu.apply(pc2ps3::MenuAction::kOpenSettings, false) ==
             pc2ps3::MenuCommand::kNone &&
             menu.page() == pc2ps3::MenuPage::kSettings,
         "opens settings from main");
  menu.apply(pc2ps3::MenuAction::kBack, false);
  expect(menu.page() == pc2ps3::MenuPage::kMain,
         "settings back returns to main");
  expect(menu.apply(pc2ps3::MenuAction::kContinueGame, true) ==
             pc2ps3::MenuCommand::kContinueGame &&
             menu.page() == pc2ps3::MenuPage::kClosed,
         "continue with save emits one command");
  menu.apply(pc2ps3::MenuAction::kPause, false);
  expect(menu.page() == pc2ps3::MenuPage::kPause, "pause opens");
  menu.apply(pc2ps3::MenuAction::kOpenSettings, false);
  menu.apply(pc2ps3::MenuAction::kCancel, false);
  expect(menu.page() == pc2ps3::MenuPage::kPause,
         "settings cancel returns to pause");
  menu.apply(pc2ps3::MenuAction::kResume, false);
  expect(menu.page() == pc2ps3::MenuPage::kClosed,
         "resume closes pause page");
}

void test_exit_confirmation_and_reset() {
  pc2ps3::MenuFlow menu;
  menu.apply(pc2ps3::MenuAction::kOpenMain, false);
  menu.apply(pc2ps3::MenuAction::kBack, false);
  expect(menu.page() == pc2ps3::MenuPage::kConfirmExit,
         "back from main asks for confirmation");
  expect(menu.apply(pc2ps3::MenuAction::kCancel, false) ==
             pc2ps3::MenuCommand::kNone &&
             menu.page() == pc2ps3::MenuPage::kMain,
         "cancel returns to main");
  menu.apply(pc2ps3::MenuAction::kBack, false);
  expect(menu.apply(pc2ps3::MenuAction::kConfirm, false) ==
             pc2ps3::MenuCommand::kQuitRequested &&
             menu.page() == pc2ps3::MenuPage::kClosed,
         "confirm emits quit request without process side effect");
  menu.reset();
  expect(menu.page() == pc2ps3::MenuPage::kClosed, "reset closes menu");
  expect(menu.apply(pc2ps3::MenuAction::kConfirm, false) ==
             pc2ps3::MenuCommand::kNone,
         "invalid action has no command");

  pc2ps3::MenuFlow new_game;
  new_game.apply(pc2ps3::MenuAction::kOpenMain, false);
  expect(new_game.apply(pc2ps3::MenuAction::kStartNewGame, false) ==
             pc2ps3::MenuCommand::kStartNewGame &&
             new_game.page() == pc2ps3::MenuPage::kClosed,
         "new game emits command to caller");
}
}  // namespace

int main() {
  test_main_pause_settings_and_commands();
  test_exit_confirmation_and_reset();
  if (failures != 0) return 1;
  std::cout << "menu flow tests passed\n";
  return 0;
}
