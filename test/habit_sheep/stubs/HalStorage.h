#pragma once
#include <fcntl.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>
struct HalFile {
  std::string* data = nullptr;
  size_t pos = 0;
  explicit operator bool() const { return data != nullptr; }
  bool available() const { return data && pos < data->size(); }
  int read() { return available() ? static_cast<unsigned char>((*data)[pos++]) : -1; }
  int read(void* buffer, size_t size) {
    if (!data) return -1;
    size_t count = std::min(size, data->size() - pos);
    data->copy(static_cast<char*>(buffer), count, pos);
    pos += count;
    return count;
  }
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
  using Files = std::map<std::string, std::string>;
  Files files;
  int operations = 0, failAt = 0;
  bool capture = false;
  std::vector<Files> checkpoints;
  bool mutate() {
    ++operations;
    return operations != failAt;
  }
  void checkpoint() {
    if (capture) checkpoints.push_back(files);
  }
  bool exists(const char* path) const { return files.count(path) != 0; }
  bool ensureDirectoryExists(const char* path) {
    if (exists(path)) return true;
    if (!mutate()) return false;
    files[path] = "__DIR__";
    checkpoint();
    return true;
  }
  bool openFileForRead(const char*, const std::string& path, HalFile& file) {
    const auto it = files.find(path);
    if (it == files.end()) return false;
    file = HalFile{&it->second, 0};
    return true;
  }
  HalFile open(const char* path, int flags) {
    if ((flags & O_ACCMODE) == O_RDONLY) {
      const auto it = files.find(path);
      return it == files.end() ? HalFile{} : HalFile{&it->second, 0};
    }
    if (!mutate()) return {};
    if (flags & O_TRUNC) files[path].clear();
    return HalFile{&files[path], static_cast<size_t>(flags & O_APPEND ? files[path].size() : 0)};
  }
  bool writeFile(const char* path, const std::string& text) {
    if (!mutate()) return false;
    files[path] = text;
    checkpoint();
    return true;
  }
  bool rename(const char* oldPath, const char* newPath) {
    if (!exists(oldPath) || exists(newPath) || !mutate()) return false;
    const std::string old = oldPath, newName = newPath;
    std::vector<std::pair<std::string, std::string>> moving;
    for (const auto& item : files)
      if (item.first == old || item.first.rfind(old + "/", 0) == 0) moving.push_back(item);
    for (const auto& item : moving) {
      files[newName + item.first.substr(old.size())] = item.second;
      files.erase(item.first);
    }
    checkpoint();
    return true;
  }
  bool remove(const char* path) {
    if (!exists(path) || !mutate()) return false;
    files.erase(path);
    checkpoint();
    return true;
  }
  bool removeDir(const char* path) {
    if (!exists(path) || !mutate()) return false;
    const std::string prefix = std::string(path) + "/";
    for (auto it = files.begin(); it != files.end();)
      if (it->first == path || it->first.rfind(prefix, 0) == 0)
        it = files.erase(it);
      else
        ++it;
    checkpoint();
    return true;
  }
};
extern TestStorage Storage;
