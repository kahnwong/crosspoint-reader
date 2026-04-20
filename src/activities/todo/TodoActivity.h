#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class TodoActivity final : public Activity {
 public:
  enum class State { BROWSING, WIFI_SELECTION, LOADING, ERROR };

  explicit TodoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Todo", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  ButtonNavigator buttonNavigator;
  State state = State::BROWSING;
  std::vector<std::string> displayItems;
  int selectorIndex = 0;
  std::string errorMessage;
  bool consumeConfirm = false;

  static constexpr const char* CACHE_PATH = "/.crosspoint/todo_cache.json";

  void loadCache();
  void saveCache(const String& json);
  bool parseJson(const String& json);
  void startRefresh();
  void launchWifiSelection();
  void onWifiConnected();
  void fetchTodos();
};
