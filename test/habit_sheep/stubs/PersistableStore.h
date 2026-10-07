#pragma once
#include <ArduinoJson.h>
#include <HalStorage.h>

#include <map>
#include <string>
extern bool habitTestSaveFailure;
extern std::map<std::string, std::string> habitTestSnapshots;
class PersistableStoreBase {
 public:
  static bool writeDocToFile(const char* path, const JsonDocument& doc) {
    if (habitTestSaveFailure) return false;
    std::string json;
    serializeJson(doc, json);
    return Storage.writeFile(path, json);
  }
  static bool readDocFromFile(const char* path, JsonDocument& doc) {
    const auto it = Storage.files.find(path);
    return it != Storage.files.end() && !deserializeJson(doc, it->second);
  }
};
template <typename T>
class PersistableStore : public PersistableStoreBase {
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
    return Storage.writeFile(T::getFilePath(), json);
  }
  bool loadFromFile() {
    JsonDocument doc;
    return readDocFromFile(T::getFilePath(), doc) && static_cast<T*>(this)->fromJson(doc.as<JsonVariantConst>());
  }
};
