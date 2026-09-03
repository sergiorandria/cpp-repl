#include "cpp-repl/lsp/server.h"
#include <chrono>

namespace cpprepl {
namespace lsp {

ThreadPool::ThreadPool(size_t threads) {
  if (threads == 0) threads = 2;
  for (size_t i = 0; i < threads; ++i) {
    workers_.emplace_back([this]{
      while (true) {
        Task t;
        {
          std::unique_lock<std::mutex> lk(m_);
          cv_.wait(lk, [this]{ return stop_ || !queue_.empty(); });
          if (stop_ && queue_.empty()) return;
          t = std::move(queue_.front());
          queue_.pop();
        }
        // Simulate eval latency (real impl would call Interpreter::eval with ThreadSafeContext)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (t.onDone) t.onDone("ok: " + t.code.substr(0, 20), "");
      }
    });
  }
}

ThreadPool::~ThreadPool() { shutdown(); }

void ThreadPool::enqueue(Task t) {
  {
    std::lock_guard<std::mutex> lk(m_);
    queue_.push(std::move(t));
  }
  cv_.notify_one();
}

void ThreadPool::shutdown() {
  stop_ = true;
  cv_.notify_all();
  for (auto &w : workers_) if (w.joinable()) w.join();
  workers_.clear();
}

LspServer::LspServer(size_t threads) : pool_(threads) {}
LspServer::~LspServer() { shutdown(); }

void LspServer::evalAsync(const std::string &code, std::function<void(std::string, std::string)> cb) {
  pool_.enqueue(Task{"cell-" + std::to_string(rand()), code, std::move(cb)});
}

std::string LspServer::hover(const std::string &code, size_t pos) {
  (void)code; (void)pos;
  return "hover: type info (via clang AST, ThreadSafeContext)";
}

std::vector<std::string> LspServer::complete(const std::string &prefix) {
  // Reuse Highlighter/VersionDetector for prefix
  std::vector<std::string> out;
  if (prefix.rfind(":") == 0) {
    // Delegate to CommandRegistry in real impl
    out = {":help", ":stack", ":heap", ":trace"};
  } else {
    out = {"std::vector", "std::string", "auto", "int", "for", "while"};
  }
  std::vector<std::string> filtered;
  for (auto &s : out) if (s.rfind(prefix, 0) == 0) filtered.push_back(s);
  return filtered;
}

void LspServer::shutdown() { pool_.shutdown(); }

} // namespace lsp
} // namespace cpprepl
