// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "cache.h"
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  modcache::arguments actual;
  for (int i=1; i<argc; ++i) {
#ifdef _WIN32
    actual.push_back(modcache::utf8(argv[i]));
#else
    actual.push_back(argv[i]);
#endif
  }
  return actual == modcache::arguments({"", "two words", "a\"b", "tail\\", "@opaque.rsp", "\xc3\xa9"}) ? 37 : 1;
}
