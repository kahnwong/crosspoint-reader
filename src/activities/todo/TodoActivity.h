#pragma once

#include <string>
#include <unordered_map>
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
  bool downLongHandled = false;  // one-shot flag for long-press Down

  // key = item text, value = unix timestamp when struck
  std::unordered_map<std::string, uint32_t> struckItems;

  static constexpr const char* CACHE_PATH = "/.crosspoint/todo_cache.json";
  static constexpr const char* STRUCK_PATH = "/.crosspoint/todo_struck.json";
  static constexpr uint32_t STRUCK_TTL_S = 86400;  // 24 hours
  static constexpr unsigned long LONG_PRESS_MS = 500;

  void loadCache();
  void saveCache(const String& json);
  bool parseJson(const String& json);
  void startRefresh();
  void launchWifiSelection();
  void onWifiConnected();
  void fetchTodos();

  void loadStruckState();
  void saveStruckState();
  void pruneExpiredStruck();
  bool isStruck(int index) const;
  void toggleStruck(int index);
};
