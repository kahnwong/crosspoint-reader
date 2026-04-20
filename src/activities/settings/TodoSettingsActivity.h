#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class TodoSettingsActivity final : public Activity {
 public:
  explicit TodoSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("TodoSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  size_t selectedIndex = 0;
  void handleSelection();
};
