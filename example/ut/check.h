#ifndef UT_H
#define UT_H

#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif
#include <unistd.h>  // for isatty()
#include <cstdio>    // for fileno()

namespace ut {
struct Check {
  static constexpr const char* RESET = "\033[0m";
  static constexpr const char* RED = "\033[31m";
  static constexpr const char* GREEN = "\033[32m";
  static constexpr const char* YELLOW = "\033[33m";
  static constexpr const char* BLUE = "\033[34m";
  static constexpr const char* MAGENTA = "\033[35m";
  static constexpr const char* CYAN = "\033[36m";
  static constexpr const char* WHITE = "\033[37m";

  static constexpr const char* BOLD = "\033[1m";
  static constexpr const char* UNDERLINE = "\033[4m";

  bool all_tests_passed;

  Check()
      : all_tests_passed(true),
        _supportsColor(supportsColor())
  {}


  void test(const char *name, bool ok) {
    const char* passColor  = _supportsColor ? "\033[32m\033[1m" : "";
    const char* failColor  = _supportsColor ? "\033[31m\033[1m" : "";
    const char* resetColor = _supportsColor ? "\033[0m" : "";

    if (ok) {
      std::cout << passColor << "[PASS ] " << resetColor << name << std::endl;
    }else{
      std::cerr << failColor << "[FALSE] " << resetColor << name << std::endl;
      all_tests_passed = false;
    }
  }

  static std::string getPath(const std::string &fileName)
  {
    size_t lastSlash = fileName.find_last_of("/\\");
    if (lastSlash == std::string::npos) {
      return "";
    }else{
      return fileName.substr(0, lastSlash);
    }
  }

  static bool supportsColor() {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    return GetConsoleMode(hConsole, &mode) && (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    return isatty(fileno(stdout));
#endif
  }
protected:
  bool _supportsColor;
};
}

#endif
