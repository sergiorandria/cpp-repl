#include "cpp-repl/security/sandbox.h"
#include <algorithm>
#include <regex>

namespace cpprepl {
namespace security {

static bool containsWord(const std::string &code, const std::string &word) {
  size_t pos = 0;
  while ((pos = code.find(word, pos)) != std::string::npos) {
    bool leftOk = pos == 0 || (!std::isalnum((unsigned char)code[pos-1]) && code[pos-1] != '_');
    bool rightOk = pos + word.size() == code.size() || (!std::isalnum((unsigned char)code[pos+word.size()]) && code[pos+word.size()] != '_');
    if (leftOk && rightOk) return true;
    pos += word.size();
  }
  return false;
}

bool Sandbox::containsDangerous(const std::string &code, std::string &reason) const {
  // Strip comments/strings for accurate check (reuse simple logic)
  std::string stripped;
  stripped.reserve(code.size());
  bool inLine=false, inBlock=false, inStr=false, inChar=false, esc=false;
  for (size_t i=0;i<code.size();++i) {
    char c=code[i], n=(i+1<code.size()?code[i+1]:0);
    if (inLine) { if (c=='\n') { inLine=false; stripped.push_back(c); } continue; }
    if (inBlock) { if (c=='*' && n=='/') { inBlock=false; ++i; } continue; }
    if (inStr) { if (!esc && c=='"') inStr=false; esc = !esc && c=='\\'; stripped.push_back(' '); continue; }
    if (inChar) { if (!esc && c=='\'') inChar=false; esc = !esc && c=='\\'; stripped.push_back(' '); continue; }
    if (c=='/' && n=='/') { inLine=true; ++i; continue; }
    if (c=='/' && n=='*') { inBlock=true; ++i; continue; }
    if (c=='"') { inStr=true; stripped.push_back(' '); continue; }
    if (c=='\'') { inChar=true; stripped.push_back(' '); continue; }
    stripped.push_back(c);
  }

  if (!cfg_.allowSystemCalls) {
    const std::vector<std::pair<std::string,std::string>> denylist = {
      {"system", "system()"},
      {"popen", "popen()"},
      {"fork", "fork()"},
      {"exec", "exec*()"},
      {"execl", "execl()"},
      {"execv", "execv()"},
      {"kill", "kill()"},
    };
    for (auto &kv : denylist) {
      if (containsWord(stripped, kv.first)) {
        reason = "use of '" + kv.second + "' is blocked by sandbox (allowSystemCalls=false)";
        return true;
      }
    }
    // Also block std::system via regex
    if (stripped.find("std::system") != std::string::npos) {
      reason = "std::system is blocked by sandbox";
      return true;
    }
  }
  if (!cfg_.allowFileWrite) {
    const std::vector<std::string> denylist = {"unlink", "remove", "rename", "fopen", "freopen", "truncate"};
    for (auto &w : denylist) {
      // Only block if used as function call (word + '(')
      std::regex re("\\b" + w + "\\s*\\(");
      if (std::regex_search(stripped, re)) {
        // Allow fopen with read-only modes "r", "rb" – check if second arg contains "w" or "a" or "+"
        if (w == "fopen") {
          // Very simple: if code contains fopen.*\"w\" or \"a\" then block
          if (stripped.find("\"w") != std::string::npos || stripped.find("\"a") != std::string::npos) {
            reason = "fopen with write/append mode is blocked (allowFileWrite=false)";
            return true;
          } else {
            continue; // allow read-only fopen
          }
        }
        reason = "use of '" + w + "()' is blocked (allowFileWrite=false)";
        return true;
      }
    }
    if (stripped.find("std::filesystem::remove") != std::string::npos ||
        stripped.find("filesystem::remove") != std::string::npos) {
      reason = "filesystem::remove is blocked (allowFileWrite=false)";
      return true;
    }
  }
  if (!cfg_.allowNetwork) {
    if (containsWord(stripped, "socket") || containsWord(stripped, "connect") || containsWord(stripped, "bind") || containsWord(stripped, "listen")) {
      // Only if used as function
      std::regex re("\\b(socket|connect|bind|listen)\\s*\\(");
      if (std::regex_search(stripped, re)) {
        reason = "network socket API is blocked (allowNetwork=false)";
        return true;
      }
    }
  }
  if (stripped.size() > cfg_.maxCodeSize) {
    reason = "code size " + std::to_string(stripped.size()) + " exceeds limit " + std::to_string(cfg_.maxCodeSize);
    return true;
  }
  return false;
}

std::string Sandbox::check(const std::string &code) const {
  std::string reason;
  if (containsDangerous(code, reason)) {
    return "[security] " + reason + " [hint] run with --allow-system or :config allowSystemCalls=true to permit";
  }
  return "";
}

std::string Sandbox::checkIncludePath(const std::string &path) const {
  if (cfg_.sandboxRoot.empty()) return "";
  // Simple sandbox: include path must be within sandboxRoot or allowedIncludes
  if (!cfg_.allowedIncludes.empty()) {
    for (auto &a : cfg_.allowedIncludes) if (path.rfind(a,0)==0) return "";
    return "[security] include path '" + path + "' not in allowedIncludes";
  }
  // Check for traversal
  if (path.find("..") != std::string::npos) {
    return "[security] include path traversal '..' blocked";
  }
  return "";
}

std::string Sandbox::checkLibraryPath(const std::string &path) const {
  if (path.find("..") != std::string::npos) return "[security] library path traversal blocked";
  return "";
}

std::string Sandbox::checkLibrary(const std::string &lib) const {
  if (!cfg_.allowedLibraries.empty()) {
    for (auto &a : cfg_.allowedLibraries) if (lib == a) return "";
    return "[security] library '" + lib + "' not in allowedLibraries";
  }
  // Block loading of sensitive libs
  if (lib.find("libcrypto") != std::string::npos || lib.find("libssl") != std::string::npos) {
    // Allow unless explicitly blocked? For now allow
  }
  return "";
}

} // namespace security
} // namespace cpprepl
