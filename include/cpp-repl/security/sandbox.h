#pragma once
#include <string>
#include <vector>

namespace cpprepl {
namespace security {

// Security hardening for the REPL interpreter.
// Checks code for dangerous patterns and enforces limits on includes/libraries.
struct SecurityConfig {
  bool allowSystemCalls = false; // system(), popen(), exec*, fork()
  bool allowFileWrite = false;   // unlink, remove, fopen(w), std::filesystem::remove
  bool allowNetwork = false;     // socket, connect
  bool allowRawPointers = true;  // FILE*, raw new/delete
  size_t maxHistory = 1000;
  size_t maxCodeSize = 100000;               // 100KB per input
  std::vector<std::string> allowedIncludes;  // empty = allow all (within sandbox root)
  std::vector<std::string> allowedLibraries; // empty = allow all
  std::string sandboxRoot = "";              // e.g. "/tmp/cpp-repl-sandbox", empty = no chroot
};

class Sandbox {
public:
  explicit Sandbox(SecurityConfig cfg = SecurityConfig{}) : cfg_(std::move(cfg)) {}
  void setConfig(SecurityConfig cfg) {
    cfg_ = std::move(cfg);
  }
  const SecurityConfig &config() const {
    return cfg_;
  }

  // Returns empty string if allowed, otherwise error message with [security] label
  std::string check(const std::string &code) const;
  std::string checkIncludePath(const std::string &path) const;
  std::string checkLibraryPath(const std::string &path) const;
  std::string checkLibrary(const std::string &lib) const;

private:
  SecurityConfig cfg_;
  bool containsDangerous(const std::string &code, std::string &reason) const;
};

} // namespace security
} // namespace cpprepl
