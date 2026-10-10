// SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
#include "cache.h"
#include <cstdio>
#include <cstdlib>

void check(bool value, char const *what) {
  if (!value) { std::fprintf(stderr,"failed: %s\n",what); std::exit(1); }
}
int main() {
  using namespace modcache;
  check(sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256 empty");
  check(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 abc");
  check(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "SHA-256 padding across blocks");
  arguments args;
  check(modmap("-x c++-module\n-fmodule-output=\"out/base.pcm\"\n-fmodule-file=\"a=with space/a.pcm\"\n",args,false), "POSIX module map");
  check(args==arguments({"-x","c++-module","-fmodule-output=out/base.pcm","-fmodule-file=a=with space/a.pcm"}), "module argument values");
  args.clear();
  check(modmap("-fmodule-file=\"a=C:\\module files\\a.pcm\"\r\n",args,true), "Windows module map");
  check(args==arguments({"-fmodule-file=a=C:\\module files\\a.pcm"}), "Windows separators");
  auto original=args;
  check(!modmap("-fmodule-file=\"a=x\"\n@other.rsp\n",args,false) && args==original, "no partial response expansion");
  check(!modmap("-x c++-module\n",args,true), "Windows producer bypass");
  check(!modmap("-fmodule-file=\"a=x\\\"\n",args,true), "trailing escape bypass");
  args.clear();
  check(dependencies("obj.o.ddi: src.ccm \\\n with\\ space.h\n","obj.o",args,false), "POSIX depfile");
  check(args==arguments({"src.ccm","with space.h"}), "depfile inputs");
  args.clear();
  check(dependencies("CMakeFiles\\a.obj.ddi: \\\r\n C:\\source\\a.cc C:\\with\\ space\\a.h\r\n", "CMakeFiles\\a.obj", args,true), "Windows depfile");
  check(args==arguments({"C:\\source\\a.cc","C:\\with space\\a.h"}), "Windows depfile inputs");
  check(!dependencies("wrong.o.ddi: a.cc\n","obj.o",args,false), "wrong scan owner");
  check(!dependencies("obj.o.ddi: a.cc\nsecond: b.cc\n","obj.o",args,false), "multiple rules bypass");
  check(quote_windows("")=="\"\"", "empty Windows argument");
  check(quote_windows("x y\\")=="\"x y\\\\\"", "Windows trailing slash");
  check(quote_windows("a\"b")=="\"a\\\"b\"", "Windows embedded quote");
  std::puts("digest, dependency and response argument checks passed");
}
