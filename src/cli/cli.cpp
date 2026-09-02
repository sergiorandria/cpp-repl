/**
 * @file cli.cpp
 * @brief CLI parsing implementation for cpp-repl.
 */
#include "cpp-repl/cli/cli.h"

#include <iostream>

namespace cpprepl {
namespace cli {

static std::string extractValue(const std::string &arg, const std::string &prefix, int argc,
                                char **argv, int &i, std::string &err) {
  if (arg == prefix) {
    if (i + 1 >= argc) {
      err = prefix + " requires argument";
      return "";
    }
    return argv[++i];
  }
  if (arg.rfind(prefix, 0) == 0) {
    return arg.substr(prefix.size());
  }
  return "";
}

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
    else if (arg == "--verbose" || arg == "-V" || arg == "--stats" || arg == "--stat")
      opts.verbose = opts.showStats = opts.showMem = opts.showTime = true;
    else if (arg == "--mem" || arg == "--memory" || arg == "--meminfo" || arg == "--memory-info" || arg == "--heap")
      opts.showMem = true;
    else if (arg == "--time" || arg == "--timing" || arg == "--profile" || arg == "--trace")
      opts.showTime = true;
    else if (arg == "--stack" || arg == "--stack-info")
      opts.showStats = true;
    else if (arg == "--no-color" || arg == "--nocolor" || arg == "--color=never" ||
             arg == "--color=none")
      opts.noColor = true;
    else if (arg == "--color" || arg == "--color=always" || arg == "--color=auto")
      opts.noColor = false;
    else if (arg == "-e") {
      if (i + 1 >= argc) {
        err = "-e requires argument";
        return opts;
      }
      opts.execCodes.push_back(argv[++i]);
    } else if (arg == "-I" || arg.rfind("-I", 0) == 0) {
      std::string v = extractValue(arg, "-I", argc, argv, i, err);
      if (!err.empty())
        return opts;
      // Handle -I=path variant
      if (!v.empty() && v[0] == '=')
        v = v.substr(1);
      // Resolve relative vs absolute is handled by clang; keep as-is
      opts.includePaths.push_back(v);
    } else if (arg == "-L" || arg.rfind("-L", 0) == 0) {
      std::string v = extractValue(arg, "-L", argc, argv, i, err);
      if (!err.empty())
        return opts;
      if (!v.empty() && v[0] == '=')
        v = v.substr(1);
      opts.libraryPaths.push_back(v);
    } else if (arg == "-l" || arg.rfind("-l", 0) == 0) {
      std::string v = extractValue(arg, "-l", argc, argv, i, err);
      if (!err.empty())
        return opts;
      if (!v.empty() && v[0] == '=')
        v = v.substr(1);
      opts.libraries.push_back(v);
    } else if (arg == "-D" || arg.rfind("-D", 0) == 0) {
      std::string v = extractValue(arg, "-D", argc, argv, i, err);
      if (!err.empty())
        return opts;
      if (!v.empty() && v[0] == '=')
        v = v.substr(1);
      opts.defines.push_back(v);
    } else if (arg.rfind("--include", 0) == 0) {
      std::string v;
      if (arg == "--include" || arg == "--include-path") {
        if (i + 1 >= argc) {
          err = arg + " requires argument";
          return opts;
        }
        v = argv[++i];
      } else if (arg.rfind("--include=", 0) == 0) {
        v = arg.substr(std::string("--include=").size());
      } else {
        err = "unknown option: " + arg;
        return opts;
      }
      opts.includePaths.push_back(v);
    } else if (arg.rfind("--library", 0) == 0) {
      std::string v;
      if (arg == "--library" || arg == "--lib") {
        if (i + 1 >= argc) {
          err = arg + " requires argument";
          return opts;
        }
        v = argv[++i];
      } else if (arg.rfind("--library=", 0) == 0) {
        v = arg.substr(std::string("--library=").size());
      } else if (arg.rfind("--lib=", 0) == 0) {
        v = arg.substr(std::string("--lib=").size());
      } else {
        err = "unknown option: " + arg;
        return opts;
      }
      opts.libraries.push_back(v);
    } else if (arg == "--") {
      // Remaining args are files
      for (++i; i < argc; ++i)
        opts.files.push_back(argv[i]);
      break;
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
  std::cout << "cpp-repl – low-level C++ REPL (LLVM 22, O0)\n"
               "  Like Python interpreter, but for C++ – no int main() needed.\n"
               "Usage:\n  "
             << prog
             << " [options] [file ...]\n"
                "Options:\n"
                "  -h, --help           show help\n"
                "  -v, --version        show version\n"
                "  --scaffold           show low-level VM scaffold\n"
                "  --no-interactive     exit after file/-e execution\n"
                "  --no-color           disable colored prompt & timing\n"
                "  --color[=when]       force color (always/auto/never)\n"
                "  --stats              show stats after file/-e (time, memory, stack/heap)\n"
                "  --mem, --memory      show memory (heap/RSS/mallinfo) after file\n"
                "  --time, --profile    show timing per file\n"
                "  --verbose, -V        verbose (implies --stats --mem --time)\n"
                "  -e <code>            execute raw C++ code\n"
                "  -I <path>, -I<path>   add include search path (absolute or relative)\n"
                "  -L <path>, -L<path>   add library search path\n"
                "  -l <lib>, -l<lib>     link library (e.g. -l m -l gmp)\n"
                "  -D <macro>, -D<macro> define macro (e.g. -DDEBUG=1)\n"
                "  --include <path>      same as -I\n"
                "  --library <lib>       same as -l\n"
                "  <file>               execute raw C++ file as script\n"
                "  When <file> given with --stats/--mem/--verbose: shows after each file:\n"
                "    • execution time, heap/RSS/mallinfo, stack/heap layout, variables\n"
                "REPL: :help :dump :reset :load :lib :I :L :version :quit\n"
                "  :I <path>  add include path (interactive)\n"
                "  :L <path>  add library path\n"
                "  :stack, :heap, :trace, :security  (see :help inside REPL)\n"
                "Prompt: colored cpp:C++17/20/23 [n] (time ✓/✗)> with execution time\n"
                "        honor NO_COLOR / CPP_REPL_NO_COLOR=1 and --no-color, TERM=dumb\n"
                "C++ versions: auto-detects C++20/23 keywords "
                "(concept/requires/import)\n"
                "BigInt: boost::multiprecision::cpp_int / bigint\n"
                "Examples:\n"
                "  cpp-repl -I ./include -I /abs/path/header.h\n"
                "  cpp-repl -L ./lib -l mylib -I ./include\n"
                "  cpp-repl --include ./include --library m\n"
                "  cpp-repl --stats examples/hello.cpp --no-interactive  # time+mem+stack\n"
                "  cpp-repl --mem --verbose examples/bigint.cpp\n";
}
void printVersion() {
  std::cout << "cpp-repl 0.3.0 (LLVM 22.1.3 / clang 22.1.8)\n"
               "VM: llvm::orc::LLJIT (O0) | Frontend: clang::Interpreter\n"
               "Supports: C++17/20/23 auto-detect, BigInt (boost::multiprecision)\n";
}

} // namespace cli
} // namespace cpprepl
