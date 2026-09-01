#include "cpp-repl/utils/i_variable_tracker.h"
#include <memory>
#include <unordered_map>

namespace cpprepl {
namespace utils {

class MapVariableTracker : public IVariableTracker {
public:
  std::optional<VarInfo> find(const std::string &name) const override {
    auto it = vars_.find(name);
    if (it == vars_.end()) return std::nullopt;
    return it->second;
  }
  void track(const std::string &name, VarInfo info) override {
    vars_[name] = std::move(info);
  }
  void forget(const std::string &name) override {
    vars_.erase(name);
  }
  void clear() override { vars_.clear(); }
  size_t size() const override { return vars_.size(); }
  bool isSameRedefinition(const std::string &name, const VarInfo &info) const override {
    auto it = vars_.find(name);
    if (it == vars_.end()) return false;
    return it->second.type == info.type && it->second.value == info.value;
  }
private:
  std::unordered_map<std::string, VarInfo> vars_;
};

std::unique_ptr<IVariableTracker> VariableTrackerFactory::create() {
  return std::make_unique<MapVariableTracker>();
}

} // namespace utils
} // namespace cpprepl
