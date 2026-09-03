#pragma once
#include <string>
#include <chrono>
#include <filesystem>
#include <optional>

namespace cpprepl {
namespace utils {

/**
 * @file pch_cache.h
 * @brief Precompiled header cache for bits/stdc++.h (0.5s cold → ~20ms warm).
 * @details Caches the PCH for bits/stdc++.h (and other heavy headers) on first
 *          std:: use. Subsequent inits reuse the cached PCH via clang::CompilerInstance
 *          PCH flags. Invalidated when LLVM version or header mtime changes.
 */
struct PCHKey {
  std::string header = "bits/stdc++.h";
  std::string llvmVersion;
  std::filesystem::file_time_type headerMtime;
  bool operator==(const PCHKey &o) const { return header == o.header && llvmVersion == o.llvmVersion; }
};

struct PCHEntry {
  std::string pchPath;
  std::chrono::milliseconds buildTime{0};
  bool valid = false;
};

class PCHCache {
public:
  explicit PCHCache(std::string cacheDir = "/tmp/cpp-repl-pch");
  void setCacheDir(std::string dir) { cacheDir_ = std::move(dir); }
  const std::string &cacheDir() const { return cacheDir_; }

  // Returns cached PCH path if valid, otherwise std::nullopt (caller should build)
  std::optional<PCHEntry> lookup(const PCHKey &key) const;
  // Store a newly built PCH (called after successful clang PCH generation)
  void store(const PCHKey &key, const PCHEntry &entry);
  // Build PCH for header via clang -x c++ -std=c++23 -O0 -emit-pch (best-effort)
  PCHEntry build(const PCHKey &key, const std::string &resourceDir, std::string &err);
  // Ensure cache is valid for current LLVM/header, build if missing
  PCHEntry ensure(const std::string &header, const std::string &llvmVersion, const std::string &resourceDir);
  void clear();

private:
  std::string cacheDir_;
  std::string keyToFilename(const PCHKey &key) const;
};

} // namespace utils
} // namespace cpprepl
