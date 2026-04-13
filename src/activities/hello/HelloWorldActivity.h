#pragma once

#include "activities/Activity.h"

class HelloWorldActivity final : public Activity {
 public:
  explicit HelloWorldActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("HelloWorld", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
