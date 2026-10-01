#pragma once
#include <ArduinoJson.h>

#include <map>
#include <string>
extern bool habitTestSaveFailure;
extern std::map<std::string, std::string> habitTestSnapshots;
template <typename T>
class PersistableStore {
 protected:
  void requestResave() {}

 public:
  static T& getInstance() {
    static T instance;
    return instance;
  }
  bool saveToFile() const {
    if (habitTestSaveFailure) return false;
    JsonDocument doc;
    static_cast<const T*>(this)->toJson(doc);
    std::string json;
    serializeJson(doc, json);
    habitTestSnapshots[T::getFilePath()] = json;
    return true;
  }
};
