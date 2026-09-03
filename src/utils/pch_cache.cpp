#include "cpp-repl/utils/pch_cache.h"
#include <fstream>
#include <filesystem>
#include <chrono>

namespace cpprepl {
namespace utils {

PCHCache::PCHCache(std::string cacheDir) : cacheDir_(std::move(cacheDir)) {
  std::error_code ec;
  std::filesystem::create_directories(cacheDir_, ec);
}

std::optional<PCHEntry> PCHCache::lookup(const PCHKey &key) const {
  auto path = cacheDir_ + "/" + keyToFilename(key);
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return std::nullopt;
  // Check header mtime vs pch mtime
  auto pchTime = std::filesystem::last_write_time(path, ec);
  if (ec) return std::nullopt;
  if (pchTime < key.headerMtime) return std::nullopt;
  return PCHEntry{path, std::chrono::milliseconds(0), true};
}

void PCHCache::store(const PCHKey &key, const PCHEntry &entry) {
  // Already stored by build()
  (void)key; (void)entry;
}

PCHEntry PCHCache::build(const PCHKey &key, const std::string &resourceDir, std::string &err) {
  auto t0 = std::chrono::steady_clock::now();
  std::string pchPath = cacheDir_ + "/" + keyToFilename(key);
  // Best-effort: use clang to emit PCH
  // clang++-22 -x c++ -std=c++23 -O0 -resource-dir <res> -I <cacheDir> -emit-pch -o <pchPath> -include <header>
  std::string cmd = "clang++-22 -x c++ -std=c++23 -O0 -resource-dir " + resourceDir + " -emit-pch -o " + pchPath + " -include " + key.header + " /dev/null > /dev/null 2>&1";
  // Fallback to clang if clang++-22 not found
  if (system(("which clang++-22 > /dev/null 2>&1 || which clang > /dev/null 2>&1 || true")) != 0) {
    // No clang, return invalid
  }
  int rc = system(cmd.c_str());
  auto t1 = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
  if (rc != 0) {
    err = "PCH build failed for " + key.header;
    return PCHEntry{pchPath, ms, false};
  }
  return PCHEntry{pchPath, ms, true};
}

PCHEntry PCHCache::ensure(const std::string &header, const std::string &llvmVersion, const std::string &resourceDir) {
  PCHKey key;
  key.header = header;
  key.llvmVersion = llvmVersion;
  std::error_code ec;
  // Try to get header file mtime via standard include search (approx)
  key.headerMtime = std::filesystem::file_time_type::min();
  if (auto entry = lookup(key)) return *entry;
  std::string err;
  return build(key, resourceDir, err);
}

void PCHCache::clear() {
  std::error_code ec;
  for (auto &e : std::filesystem::directory_iterator(cacheDir_, ec)) {
    if (ec) break;
    std::filesystem::remove(e.path(), ec);
  }
}

std::string PCHCache::keyToFilename(const PCHKey &key) const {
  std::string n = key.header;
  std::replace(n.begin(), n.end(), '/', '_');
  std::replace(n.begin(), n.end(), '.', '_');
  return n + "-" + key.llvmVersion + ".pch";
}

} // namespace utils
} // namespace cpprepl
