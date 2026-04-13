#include "HelloWorldActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void HelloWorldActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void HelloWorldActivity::onExit() { Activity::onExit(); }

void HelloWorldActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
  }
}

void HelloWorldActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_HELLO_WORLD));

  // Display the configured text (or default "Hello World") centered on screen
  const char* displayText =
      (SETTINGS.helloWorldText[0] != '\0') ? SETTINGS.helloWorldText : tr(STR_HELLO_WORLD);
  renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2, displayText, true, EpdFontFamily::BOLD);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
