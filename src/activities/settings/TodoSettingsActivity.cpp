#include "TodoSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int MENU_ITEMS = 2;
const StrId menuNames[MENU_ITEMS] = {StrId::STR_TODO_API_ENDPOINT, StrId::STR_TODO_API_KEY};
}  // namespace

void TodoSettingsActivity::onEnter() {
  Activity::onEnter();
  selectedIndex = 0;
  requestUpdate();
}

void TodoSettingsActivity::onExit() { Activity::onExit(); }

void TodoSettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }

  buttonNavigator.onNext([this] {
    selectedIndex = (selectedIndex + 1) % MENU_ITEMS;
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectedIndex = (selectedIndex + MENU_ITEMS - 1) % MENU_ITEMS;
    requestUpdate();
  });
}

void TodoSettingsActivity::handleSelection() {
  if (selectedIndex == 0) {
    startActivityForResult(
        std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_TODO_API_ENDPOINT),
                                               SETTINGS.todoApiEndpoint, sizeof(SETTINGS.todoApiEndpoint) - 1, InputType::Text),
        [](const ActivityResult& result) {
          if (!result.isCancelled) {
            const auto& kb = std::get<KeyboardResult>(result.data);
            strncpy(SETTINGS.todoApiEndpoint, kb.text.c_str(), sizeof(SETTINGS.todoApiEndpoint) - 1);
            SETTINGS.todoApiEndpoint[sizeof(SETTINGS.todoApiEndpoint) - 1] = '\0';
            SETTINGS.saveToFile();
          }
        });
  } else if (selectedIndex == 1) {
    startActivityForResult(
        std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_TODO_API_KEY), SETTINGS.todoApiKey,
                                               sizeof(SETTINGS.todoApiKey) - 1, InputType::Password),
        [](const ActivityResult& result) {
          if (!result.isCancelled) {
            const auto& kb = std::get<KeyboardResult>(result.data);
            strncpy(SETTINGS.todoApiKey, kb.text.c_str(), sizeof(SETTINGS.todoApiKey) - 1);
            SETTINGS.todoApiKey[sizeof(SETTINGS.todoApiKey) - 1] = '\0';
            SETTINGS.saveToFile();
          }
        });
  }
}

void TodoSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TODO));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, MENU_ITEMS, static_cast<int>(selectedIndex),
      [](int index) { return std::string(I18N.get(menuNames[index])); }, nullptr, nullptr,
      [](int index) -> std::string {
        if (index == 0) {
          return (strlen(SETTINGS.todoApiEndpoint) > 0) ? std::string(SETTINGS.todoApiEndpoint)
                                                        : std::string(tr(STR_NOT_SET));
        } else {
          return (strlen(SETTINGS.todoApiKey) > 0) ? std::string("******") : std::string(tr(STR_NOT_SET));
        }
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
