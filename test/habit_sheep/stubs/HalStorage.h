#pragma once
#include <fcntl.h>

#include <cstdint>
#include <map>
#include <string>
struct HalFile {
  std::string* data = nullptr;
  size_t pos = 0;
  explicit operator bool() const { return data != nullptr; }
  bool available() const { return data && pos < data->size(); }
  int read() { return available() ? static_cast<unsigned char>((*data)[pos++]) : -1; }
  size_t write(uint8_t value) {
    if (!data) return 0;
    data->push_back(static_cast<char>(value));
    return 1;
  }
  size_t write(const uint8_t* buffer, size_t size) {
    if (!data) return 0;
    data->append(reinterpret_cast<const char*>(buffer), size);
    return size;
  }
  void flush() {}
  void close() { data = nullptr; }
};
struct TestStorage {
  std::map<std::string, std::string> files;
  bool exists(const char* path) const { return files.count(path) != 0; }
  bool ensureDirectoryExists(const char*) const { return true; }
  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    const auto it = files.find(path);
    if (it == files.end()) return false;
    file = HalFile{&it->second, 0};
    return true;
  }
  HalFile open(const char* path, int) { return HalFile{&files[path], 0}; }
};
extern TestStorage Storage;
