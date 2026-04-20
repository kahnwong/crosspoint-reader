#include "TodoActivity.h"

#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HTTPClient.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>

#include <ctime>
#include <memory>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/UrlUtils.h"

void TodoActivity::onEnter() {
  Activity::onEnter();
  state = State::BROWSING;
  selectorIndex = 0;
  errorMessage.clear();
  consumeConfirm = false;
  downLongHandled = false;
  loadStruckState();
  loadCache();
  requestUpdate();
}

void TodoActivity::onExit() {
  Activity::onExit();
  WiFi.mode(WIFI_OFF);
}

// --- Cache ---

void TodoActivity::loadCache() {
  if (!Storage.exists(CACHE_PATH)) {
    return;
  }
  const String json = Storage.readFile(CACHE_PATH);
  if (json.isEmpty()) {
    return;
  }
  parseJson(json);
}

void TodoActivity::saveCache(const String& json) {
  Storage.mkdir("/.crosspoint");
  Storage.writeFile(CACHE_PATH, json);
}

bool TodoActivity::parseJson(const String& json) {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, json);
  if (err) {
    LOG_ERR("TODO", "JSON parse error: %s", err.c_str());
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  displayItems.clear();
  displayItems.reserve(arr.size());

  for (JsonObject item : arr) {
    const std::string context = item["context"] | std::string("");
    const std::string project = item["project"] | std::string("");
    const std::string todo = item["todo"] | std::string("");

    std::string line;
    if (!context.empty()) {
      line += context;
      line += " ";
    }
    if (!project.empty()) {
      line += project;
      line += " ";
    }
    line += todo;
    displayItems.push_back(std::move(line));
  }
  return true;
}

// --- Struck state ---

void TodoActivity::loadStruckState() {
  struckItems.clear();
  if (!Storage.exists(STRUCK_PATH)) {
    return;
  }
  const String json = Storage.readFile(STRUCK_PATH);
  if (json.isEmpty()) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return;
  }
  JsonObject obj = doc.as<JsonObject>();
  for (JsonPair kv : obj) {
    struckItems[kv.key().c_str()] = kv.value().as<uint32_t>();
  }
  pruneExpiredStruck();
}

void TodoActivity::saveStruckState() {
  pruneExpiredStruck();
  JsonDocument doc;
  JsonObject obj = doc.to<JsonObject>();
  for (const auto& kv : struckItems) {
    obj[kv.first] = kv.second;
  }
  String out;
  serializeJson(doc, out);
  Storage.mkdir("/.crosspoint");
  Storage.writeFile(STRUCK_PATH, out);
}

void TodoActivity::pruneExpiredStruck() {
  const uint32_t now = static_cast<uint32_t>(time(nullptr));
  // Only prune if clock is synced (epoch > year 2020)
  if (now < 1577836800UL) {
    return;
  }
  for (auto it = struckItems.begin(); it != struckItems.end();) {
    if (now - it->second > STRUCK_TTL_S) {
      it = struckItems.erase(it);
    } else {
      ++it;
    }
  }
}

bool TodoActivity::isStruck(int index) const {
  if (index < 0 || index >= static_cast<int>(displayItems.size())) return false;
  return struckItems.count(displayItems[index]) > 0;
}

void TodoActivity::toggleStruck(int index) {
  if (index < 0 || index >= static_cast<int>(displayItems.size())) return;
  const std::string& key = displayItems[index];
  if (struckItems.count(key)) {
    struckItems.erase(key);
  } else {
    const uint32_t now = static_cast<uint32_t>(time(nullptr));
    // If clock not synced, store a sentinel so it persists until next boot prune
    struckItems[key] = (now < 1577836800UL) ? 1577836800UL : now;
  }
  saveStruckState();
}

// --- Network ---

void TodoActivity::startRefresh() {
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    onWifiConnected();
    return;
  }
  launchWifiSelection();
}

void TodoActivity::launchWifiSelection() {
  consumeConfirm = true;
  state = State::WIFI_SELECTION;
  requestUpdate();
  startActivityForResult(
      std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          onWifiConnected();
        } else {
          WiFi.disconnect();
          WiFi.mode(WIFI_OFF);
          state = State::BROWSING;
          requestUpdate();
        }
      });
}

void TodoActivity::onWifiConnected() {
  state = State::LOADING;
  requestUpdate();
  fetchTodos();
}

void TodoActivity::fetchTodos() {
  if (SETTINGS.todoApiEndpoint[0] == '\0') {
    errorMessage = tr(STR_NO_SERVER_URL);
    state = State::ERROR;
    requestUpdate();
    return;
  }

  const std::string url(SETTINGS.todoApiEndpoint);

  std::unique_ptr<NetworkClient> client;
  if (UrlUtils::isHttpsUrl(url)) {
    auto* secureClient = new NetworkClientSecure();
    secureClient->setInsecure();
    client.reset(secureClient);
  } else {
    client.reset(new NetworkClient());
  }

  HTTPClient http;
  http.begin(*client, url.c_str());
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);
  if (SETTINGS.todoApiKey[0] != '\0') {
    http.addHeader("X-API-Key", SETTINGS.todoApiKey);
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    LOG_ERR("TODO", "Fetch failed: %d", httpCode);
    char errBuf[32];
    snprintf(errBuf, sizeof(errBuf), "%s (%d)", tr(STR_FETCH_FEED_FAILED), httpCode);
    errorMessage = errBuf;
    state = State::ERROR;
    http.end();
    requestUpdate();
    return;
  }

  const String body = http.getString();
  http.end();

  if (!parseJson(body)) {
    errorMessage = tr(STR_FETCH_FEED_FAILED);
    state = State::ERROR;
    requestUpdate();
    return;
  }

  saveCache(body);
  selectorIndex = 0;
  state = State::BROWSING;
  requestUpdate();
}

// --- Loop ---

void TodoActivity::loop() {
  if (state == State::WIFI_SELECTION) {
    return;
  }

  if (consumeConfirm && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    consumeConfirm = false;
    return;
  }

  if (state == State::LOADING) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      finish();
    }
    return;
  }

  if (state == State::ERROR) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      finish();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      errorMessage.clear();
      state = State::BROWSING;
      startRefresh();
    }
    return;
  }

  // BROWSING
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    startRefresh();
    return;
  }

  if (!displayItems.empty()) {
    // Long press Down or Right = toggle strikethrough (one-shot)
    const bool downHeld = mappedInput.isPressed(MappedInputManager::Button::Down);
    const bool rightHeld = mappedInput.isPressed(MappedInputManager::Button::Right);
    if ((downHeld || rightHeld) && mappedInput.getHeldTime() >= LONG_PRESS_MS && !downLongHandled) {
      downLongHandled = true;
      toggleStruck(selectorIndex);
      requestUpdate();
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
        mappedInput.wasReleased(MappedInputManager::Button::Right)) {
      if (downLongHandled) {
        // Swallow the release — don't navigate
        downLongHandled = false;
      } else {
        selectorIndex = ButtonNavigator::nextIndex(selectorIndex, displayItems.size());
        requestUpdate();
      }
    }

    buttonNavigator.onPreviousRelease([this] {
      selectorIndex = ButtonNavigator::previousIndex(selectorIndex, displayItems.size());
      requestUpdate();
    });
  }
}

// --- Render ---

void TodoActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_TODO));

  if (state == State::WIFI_SELECTION || state == State::LOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_LOADING));
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, "", "", "");
    renderer.displayBuffer();
    return;
  }

  if (state == State::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, errorMessage.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, "", "");
    renderer.displayBuffer();
    return;
  }

  // BROWSING
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_UPDATE), tr(STR_DIR_UP), tr(STR_DIR_DOWN));

  if (displayItems.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, tr(STR_NO_ENTRIES));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, "", "");
    renderer.displayBuffer();
    return;
  }

  const int listTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int listHeight = pageHeight - listTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  GUI.drawList(
      renderer, Rect{0, listTop, pageWidth, listHeight},
      static_cast<int>(displayItems.size()), selectorIndex,
      [this](int i) { return displayItems[i]; },
      nullptr, nullptr,
      [this](int i) -> std::string {
        return isStruck(i) ? std::string(tr(STR_TODO_DONE)) : std::string();
      },
      true);

  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
