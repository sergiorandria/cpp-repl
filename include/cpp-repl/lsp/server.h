#pragma once
#include <string>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include "llvm/Support/ThreadSafeAllocator.h"

namespace cpprepl {
namespace lsp {

/**
 * @file server.h
 * @brief LSP/notebook concurrency via ThreadSafeContext + thread pool.
 * @details Each cell/hover/completion runs in a thread pool with
 *          llvm::orc::ThreadSafeContext. History replay is O(1) via
 *          shared IRTransformLayer cache (future).
 */

struct Task {
  std::string id;
  std::string code;
  std::function<void(std::string result, std::string err)> onDone;
};

class ThreadPool {
public:
  explicit ThreadPool(size_t threads = std::thread::hardware_concurrency());
  ~ThreadPool();
  void enqueue(Task t);
  void shutdown();
  size_t size() const { return workers_.size(); }
private:
  std::vector<std::thread> workers_;
  std::queue<Task> queue_;
  std::mutex m_;
  std::condition_variable cv_;
  std::atomic<bool> stop_{false};
};

class LspServer {
public:
  explicit LspServer(size_t threads = 4);
  ~LspServer();

  // Enqueue code for async eval (non-blocking, for notebook cells)
  void evalAsync(const std::string &code, std::function<void(std::string, std::string)> cb);
  // Hover/completion are synchronous wrappers for now
  std::string hover(const std::string &code, size_t pos);
  std::vector<std::string> complete(const std::string &prefix);

  void shutdown();

private:
  ThreadPool pool_;
  // ThreadSafeContext would be held here if using LLVM 22 ThreadSafeContext
  // llvm::orc::ThreadSafeContext tsc_;
};

} // namespace lsp
} // namespace cpprepl
