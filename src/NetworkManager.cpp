#include "NetworkManager.h"
#include <ArduinoJson.h>

using namespace VinylChroma;

void NetworkManager::begin() {
  WiFi.setAutoReconnect(false);

  // Start with Wi-Fi completely disabled.
  // Hostname should be configured before starting STA mode.
  WiFi.mode(WIFI_MODE_NULL);
  delay(50);

  WiFi.setHostname(config_.wifi.hostname.c_str());

  resetDisconnectTimer();

  manualDisconnect_ = false;
  stationWasConnected_ = false;
  mdnsActive_ = false;
  mdnsAttempted_ = false;

  apActive_ = false;
  activeApSsid_ = "";
  activeApPassword_ = "";
  apAttempted_ = false;

  if (!config_.wifi.ssid.isEmpty()) {
    // Normal boot:
    // Start in STA-only mode.
    // Do NOT enable AP mode here.
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    WiFi.begin(
      config_.wifi.ssid.c_str(),
      config_.wifi.password.c_str()
    );

    logger_.log(
      LogLevel::Info,
      "Connecting to " + config_.wifi.ssid
    );
  } else {
    WiFi.setAutoReconnect(false);

    if (config_.wifi.fallbackEnabled) {
      startAp(true);
    } else {
      logger_.log(
        LogLevel::Error,
        "No Wi-Fi SSID configured and fallback AP is disabled"
      );
    }
  }
}

void NetworkManager::resetDisconnectTimer() {
  disconnectedSince_ = millis();
  disconnectTimerActive_ = true;
}

bool NetworkManager::apConfigMatches() const {
  const auto mode = WiFi.getMode();

  const bool apModeEnabled =
    mode == WIFI_AP ||
    mode == WIFI_AP_STA;

  return
    apActive_ &&
    apModeEnabled &&
    WiFi.softAPSSID() == config_.wifi.fallbackSsid &&
    activeApSsid_ == config_.wifi.fallbackSsid &&
    activeApPassword_ == config_.wifi.fallbackPassword;
}

void NetworkManager::stopAp(const char* reason) {
  const auto mode = WiFi.getMode();

  const bool apModeEnabled =
    mode == WIFI_AP ||
    mode == WIFI_AP_STA;

  // Do not rely only on apActive_.
  // This also catches an unintended/default ESP_xxxxxx AP.
  if (!apActive_ && !apModeEnabled) {
    return;
  }

  const bool stopped = WiFi.softAPdisconnect(true);

  apActive_ = false;
  activeApSsid_ = "";
  activeApPassword_ = "";
  apAttempted_ = false;

  // Always return to Station-only mode.
  WiFi.mode(WIFI_STA);

  if (!stopped) {
    logger_.log(
      LogLevel::Warning,
      "SoftAP shutdown returned false"
    );
  }

  if (reason && *reason) {
    logger_.log(
      LogLevel::Info,
      reason
    );
  }
}

void NetworkManager::startAp(bool force) {
  if (!config_.wifi.fallbackEnabled) {
    return;
  }

  if (apConfigMatches()) {
    return;
  }

  if (apActive_) {
    stopAp(
      "Restarting fallback AP with updated settings"
    );
  }

  const uint32_t now = millis();

  if (
    !force &&
    apAttempted_ &&
    now - lastApAttempt_ < ApRetryIntervalMs
  ) {
    return;
  }

  apAttempted_ = true;
  lastApAttempt_ = now;

  // AP+STA is enabled only when the fallback AP
  // is actually required.
  WiFi.mode(WIFI_AP_STA);

  apActive_ = WiFi.softAP(
    config_.wifi.fallbackSsid.c_str(),
    config_.wifi.fallbackPassword.c_str()
  );

  if (apActive_) {
    activeApSsid_ = config_.wifi.fallbackSsid;
    activeApPassword_ = config_.wifi.fallbackPassword;

    logger_.log(
      LogLevel::Warning,
      "Fallback AP started: " +
        activeApSsid_ +
        " at " +
        WiFi.softAPIP().toString()
    );
  } else {
    logger_.log(
      LogLevel::Error,
      "Fallback AP start failed for SSID " +
        config_.wifi.fallbackSsid
    );
  }
}

void NetworkManager::loop() {
  const bool connected =
    WiFi.status() == WL_CONNECTED;

  const auto wifiMode =
    WiFi.getMode();

  const bool apModeEnabled =
    wifiMode == WIFI_AP ||
    wifiMode == WIFI_AP_STA;

  // If the fallback AP is disabled in the configuration,
  // make sure no AP interface remains active.
  if (
    !config_.wifi.fallbackEnabled &&
    apModeEnabled
  ) {
    stopAp(
      "Fallback AP disabled"
    );
  }

  if (connected && manualDisconnect_) {
    return;
  }

  if (connected) {
    if (!stationWasConnected_) {
      logger_.log(
        LogLevel::Info,
        "Wi-Fi connected: " +
          WiFi.localIP().toString()
      );
    }

    stationWasConnected_ = true;
    WiFi.setAutoReconnect(true);
    disconnectTimerActive_ = false;

    // Make absolutely sure that no AP remains active.
    //
    // Important:
    // We intentionally check the real Wi-Fi mode here,
    // not only apActive_. This also removes a generic
    // ESP_xxxxxx AP that was not started via startAp().
    const auto connectedMode =
      WiFi.getMode();

    if (
      apActive_ ||
      connectedMode == WIFI_AP ||
      connectedMode == WIFI_AP_STA
    ) {
      stopAp(
        "Access Point stopped after Wi-Fi connection"
      );
    } else if (connectedMode != WIFI_STA) {
      WiFi.mode(WIFI_STA);
    }

    const uint32_t now = millis();

    if (
      !mdnsActive_ &&
      (
        !mdnsAttempted_ ||
        now - lastMdnsAttempt_ >=
          MdnsRetryIntervalMs
      )
    ) {
      mdnsAttempted_ = true;
      lastMdnsAttempt_ = now;

      if (
        MDNS.begin(
          config_.wifi.hostname.c_str()
        )
      ) {
        MDNS.addService(
          "http",
          "tcp",
          80
        );

        mdnsActive_ = true;

        logger_.log(
          LogLevel::Info,
          "mDNS started: " +
            config_.wifi.hostname +
            ".local"
        );
      }
    }

    return;
  }

  if (stationWasConnected_) {
    stationWasConnected_ = false;

    if (mdnsActive_) {
      MDNS.end();
    }

    mdnsActive_ = false;
    mdnsAttempted_ = false;

    resetDisconnectTimer();

    logger_.log(
      LogLevel::Warning,
      "Wi-Fi connection lost; automatic reconnect active"
    );
  } else if (!disconnectTimerActive_) {
    resetDisconnectTimer();
  }

  if (
    apActive_ &&
    !apConfigMatches()
  ) {
    startAp(true);
  }

  const bool noStation =
    config_.wifi.ssid.isEmpty() ||
    manualDisconnect_;

  if (
    config_.wifi.fallbackEnabled &&
    (
      noStation ||
      millis() - disconnectedSince_ >=
        config_.wifi.fallbackDelaySeconds *
          1000UL
    )
  ) {
    startAp();
  }
}

void NetworkManager::reconnect() {
  if (mdnsActive_) {
    MDNS.end();
  }

  mdnsActive_ = false;
  mdnsAttempted_ = false;

  // If the VinylChroma fallback AP is currently active,
  // keep it available while testing new Wi-Fi credentials.
  const bool keepFallbackAp =
    apConfigMatches();

  const auto currentMode =
    WiFi.getMode();

  const bool apModeEnabled =
    currentMode == WIFI_AP ||
    currentMode == WIFI_AP_STA;

  // Remove any AP which is not our intentionally
  // configured VinylChroma fallback AP.
  if (
    apModeEnabled &&
    !keepFallbackAp
  ) {
    stopAp(
      "Removing unintended Access Point before reconnect"
    );
  }

  WiFi.setAutoReconnect(false);

  WiFi.disconnect(
    false,
    false
  );

  delay(100);

  if (keepFallbackAp) {
    // Keep the setup interface reachable while
    // connecting to the newly configured Wi-Fi.
    WiFi.mode(WIFI_AP_STA);
  } else {
    // Clean STA-only reconnect.
    WiFi.mode(WIFI_MODE_NULL);
    delay(50);

    WiFi.setHostname(
      config_.wifi.hostname.c_str()
    );

    WiFi.mode(WIFI_STA);
  }

  stationWasConnected_ = false;
  manualDisconnect_ = false;

  resetDisconnectTimer();

  if (!config_.wifi.ssid.isEmpty()) {
    WiFi.setAutoReconnect(true);

    WiFi.begin(
      config_.wifi.ssid.c_str(),
      config_.wifi.password.c_str()
    );

    logger_.log(
      LogLevel::Info,
      "Connecting to " +
        config_.wifi.ssid
    );
  } else {
    WiFi.setAutoReconnect(false);

    if (config_.wifi.fallbackEnabled) {
      startAp(true);
    } else {
      logger_.log(
        LogLevel::Error,
        "No Wi-Fi SSID configured and fallback AP is disabled"
      );
    }
  }
}

void NetworkManager::disconnect(bool erase) {
  if (mdnsActive_) {
    MDNS.end();
  }

  mdnsActive_ = false;
  mdnsAttempted_ = false;

  WiFi.setAutoReconnect(false);

  const bool accepted =
    WiFi.disconnect(
      false,
      erase
    );

  if (!accepted) {
    logger_.log(
      LogLevel::Warning,
      "Wi-Fi disconnect request failed"
    );
  }

  if (erase) {
    config_.wifi.ssid = "";
    config_.wifi.password = "";
  }

  manualDisconnect_ =
    accepted ||
    WiFi.status() != WL_CONNECTED;

  stationWasConnected_ =
    WiFi.status() == WL_CONNECTED &&
    !manualDisconnect_;

  resetDisconnectTimer();

  if (manualDisconnect_) {
    startAp(true);
  }
}

String NetworkManager::ip() const {
  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {
    return WiFi.localIP().toString();
  }

  const auto mode =
    WiFi.getMode();

  if (
    mode == WIFI_AP ||
    mode == WIFI_AP_STA
  ) {
    return WiFi.softAPIP().toString();
  }

  return "0.0.0.0";
}

String NetworkManager::scanJson() {
  JsonDocument d;

  const int16_t result =
    WiFi.scanComplete();

  if (
    result ==
    WIFI_SCAN_RUNNING
  ) {
    d["running"] = true;
  } else if (result < 0) {
    WiFi.scanDelete();

    const int16_t started =
      WiFi.scanNetworks(
        true,
        true
      );

    d["running"] =
      started ==
      WIFI_SCAN_RUNNING;

    if (
      started !=
      WIFI_SCAN_RUNNING
    ) {
      d["error"] =
        "scan could not be started";
    }
  } else {
    d["running"] = false;

    auto networks =
      d["networks"]
        .to<JsonArray>();

    for (
      int i = 0;
      i < result;
      ++i
    ) {
      auto item =
        networks
          .add<JsonObject>();

      item["ssid"] =
        WiFi.SSID(i);

      item["rssi"] =
        WiFi.RSSI(i);

      item["secure"] =
        WiFi.encryptionType(i) !=
        WIFI_AUTH_OPEN;
    }

    WiFi.scanDelete();
  }

  String json;
  serializeJson(d, json);

  return json;
}
