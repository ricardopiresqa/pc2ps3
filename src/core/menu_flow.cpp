#include "pc2ps3/menu_flow.hpp"

namespace pc2ps3 {

MenuFlow::MenuFlow() : page_(MenuPage::kClosed), return_page_(MenuPage::kMain) {}

MenuCommand MenuFlow::apply(MenuAction action, bool has_save) {
  switch (action) {
    case MenuAction::kOpenMain:
      if (page_ == MenuPage::kClosed) page_ = MenuPage::kMain;
      break;
    case MenuAction::kPause:
      if (page_ == MenuPage::kClosed) page_ = MenuPage::kPause;
      break;
    case MenuAction::kStartNewGame:
      if (page_ == MenuPage::kMain) {
        page_ = MenuPage::kClosed;
        return MenuCommand::kStartNewGame;
      }
      break;
    case MenuAction::kContinueGame:
      if (page_ == MenuPage::kMain && has_save) {
        page_ = MenuPage::kClosed;
        return MenuCommand::kContinueGame;
      }
      break;
    case MenuAction::kOpenSettings:
      if (page_ == MenuPage::kMain || page_ == MenuPage::kPause) {
        return_page_ = page_;
        page_ = MenuPage::kSettings;
      }
      break;
    case MenuAction::kResume:
      if (page_ == MenuPage::kPause) page_ = MenuPage::kClosed;
      break;
    case MenuAction::kBack:
      if (page_ == MenuPage::kMain) {
        return_page_ = MenuPage::kMain;
        page_ = MenuPage::kConfirmExit;
      } else if (page_ == MenuPage::kPause) {
        page_ = MenuPage::kClosed;
      } else if (page_ == MenuPage::kSettings ||
                 page_ == MenuPage::kConfirmExit) {
        page_ = return_page_;
      }
      break;
    case MenuAction::kConfirm:
      if (page_ == MenuPage::kConfirmExit) {
        page_ = MenuPage::kClosed;
        return MenuCommand::kQuitRequested;
      }
      break;
    case MenuAction::kCancel:
      if (page_ == MenuPage::kSettings ||
          page_ == MenuPage::kConfirmExit) {
        page_ = return_page_;
      } else if (page_ == MenuPage::kPause) {
        page_ = MenuPage::kClosed;
      }
      break;
  }
  return MenuCommand::kNone;
}

MenuPage MenuFlow::page() const { return page_; }

void MenuFlow::reset() {
  page_ = MenuPage::kClosed;
  return_page_ = MenuPage::kMain;
}

}  // namespace pc2ps3
