#include "cpp-repl/cli/cli.h"
#include <iostream>

namespace cpprepl {
namespace cli {

Options parse(int argc, char **argv, std::string &err) {
  Options opts;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help")
      opts.showHelp = true;
    else if (arg == "-v" || arg == "--version")
      opts.showVersion = true;
    else if (arg == "--scaffold")
      opts.showScaffold = true;
    else if (arg == "--no-scaffold")
      opts.showScaffold = false;
    else if (arg == "--no-interactive")
      opts.noInteractive = true;
    else if (arg == "-e") {
      if (i + 1 >= argc) {
        err = "-e requires argument";
        return opts;
      }
      opts.execCodes.push_back(argv[++i]);
    } else if (!arg.empty() && arg[0] == '-') {
      err = "unknown option: " + arg;
      return opts;
    } else {
      opts.files.push_back(arg);
    }
  }
  return opts;
}
void printHelp(const char *prog) {
  std::cout
      << "cpp-repl – low-level C++ REPL (LLVM 22, O0)\n"
         "  Like Python interpreter, but for C++ – no int main() needed.\n"
         "Usage:\n  "
      << prog
      << " [options] [file ...]\n"
         "Options:\n"
         "  -h, --help           show help\n"
         "  -v, --version        show version\n"
         "  --scaffold           show low-level VM scaffold\n"
         "  --no-interactive     exit after file/-e execution\n"
         "  -e <code>            execute raw C++ code\n"
         "  <file>               execute raw C++ file as script\n"
         "REPL: :help :dump :reset :load :lib :undo :version :quit\n"
         "C++ versions: auto-detects C++20/23 keywords "
         "(concept/requires/import)\n"
         "BigInt: boost::multiprecision::cpp_int / bigint\n";
}
void printVersion() {
  std::cout
      << "cpp-repl 0.2.0 (LLVM 22.1.3 / clang 22.1.8)\n"
         "VM: llvm::orc::LLJIT (O0) | Frontend: clang::Interpreter\n"
         "Supports: C++17/20/23 auto-detect, BigInt (boost::multiprecision)\n";
}

} // namespace cli
} // namespace cpprepl
