// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "cache.h"
#include <cstdio>
#include <exception>

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  try {
    modcache::arguments args;
    for (int i = 1; i < argc; ++i) {
#ifdef _WIN32
      args.push_back(modcache::utf8(argv[i]));
#else
      args.push_back(argv[i]);
#endif
    }
    if (args.empty() || args[0] == "--help") {
      std::puts("usage: modcache COMPILER [ARGUMENT ...]\nSafe Clang module caching with sccache.");
      return args.empty() ? 2 : 0;
    }
    return modcache::launch(args);
  } catch (std::exception const &error) {
    std::fprintf(stderr, "modcache: %s\n", error.what());
    return 1;
  }
}
