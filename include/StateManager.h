#pragma once
#include <Arduino.h>
#include "AppTypes.h"
#include "ConfigManager.h"

class StateManager {
  ConfigManager* _cfgMgr = nullptr;
public:
  void init(ConfigManager& cfgMgr) { _cfgMgr = &cfgMgr; }

  String toggle(UserProfile& user) {
    String eventType;
    if (user.atHome) {
      user.atHome = false;
      eventType   = "出門了";
    } else {
      user.atHome = true;
      eventType   = "回家了";
    }
    _cfgMgr->saveUserState(user);
    return eventType;
  }
};
