#include "StateManager.h"
#include <LittleFS.h>
bool StateManager::load(JsonDocument& doc){File f=LittleFS.open("/state.json","r");if(!f)return false;bool ok=!deserializeJson(doc,f);f.close();return ok;}
bool StateManager::save(const JsonDocument& doc){File f=LittleFS.open("/state.json","w");if(!f)return false;bool ok=serializeJson(doc,f)>0;f.close();_dirty=false;return ok;}
void StateManager::markDirty(){_dirty=true;} bool StateManager::dirty()const{return _dirty;}
