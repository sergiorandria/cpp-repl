#pragma once
#include <memory>
#include <string>
#include <optional>

namespace cpprepl {
namespace utils {

// Interface for variable tracking — Strategy pattern.
// Current impl is MapVariableTracker (unordered_map), future could be AST-based.
struct VarInfo {
  std::string type;
  std::string value; // empty if no initializer (e.g. FILE *file;)
};

class IVariableTracker {
public:
  virtual ~IVariableTracker() = default;
  virtual std::optional<VarInfo> find(const std::string &name) const = 0;
  virtual void track(const std::string &name, VarInfo info) = 0;
  virtual void forget(const std::string &name) = 0;
  virtual void clear() = 0;
  virtual size_t size() const = 0;
  // Returns true if redefinition with same type+value should be ignored (not an error)
  virtual bool isSameRedefinition(const std::string &name, const VarInfo &info) const = 0;
};

// Factory
class VariableTrackerFactory {
public:
  static std::unique_ptr<IVariableTracker> create();
};

} // namespace utils
} // namespace cpprepl
