# modcache

[![build](https://img.shields.io/github/actions/workflow/status/ekmett/modcache/build.yml?branch=main&label=build&logo=githubactions&logoColor=white)](https://github.com/ekmett/modcache/actions/workflows/build.yml)
[![C++11](https://img.shields.io/badge/C%2B%2B-11-00599C?logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/compiler_support/11)
[![License](https://img.shields.io/badge/license-BSD--2--Clause%20OR%20Apache--2.0-blue)](LICENSE.md)

`modcache` makes sccache usable with Clang's C++ modules. It is a small C++11
compiler launcher, with no Python or library dependencies beyond the C++ runtime.

A compiled module remembers where its source came from. Two headers can expand
to exactly the same tokens while putting those tokens on different lines.
sccache can treat those compilations as interchangeable. Clang cannot always do
so: restoring the wrong BMI can crash the next compilation that imports it.

`modcache` hashes the raw inputs as well. It reads CMake's dependency scan,
includes the imported BMIs, and supplies a digest manifest to sccache. It also
handles the quoted module arguments in CMake's response files. CMake's files
are left alone. If the inputs cannot be accounted for, the compiler runs without
caching.

## Build

You need CMake 3.28 or later and a C++11 compiler. The compiler used to build
`modcache` need not be the one compiling your modules.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix /your/prefix
```

## Use

Put `modcache` and `sccache` on PATH, then configure your project:

```sh
cmake -S . -B build -DCMAKE_CXX_COMPILER_LAUNCHER=modcache
```

The installed CMake package also exports an executable target:

```cmake
find_package(modcache CONFIG REQUIRED)
set(CMAKE_CXX_COMPILER_LAUNCHER "$<TARGET_FILE:modcache::modcache>")
```

Set the launcher before creating the targets that should use it. For an existing
target, set its `CXX_COMPILER_LAUNCHER` property instead.

`FetchContent` works too:

```cmake
include(FetchContent)
FetchContent_Declare(modcache
  GIT_REPOSITORY https://github.com/ekmett/modcache.git
  GIT_TAG main)
FetchContent_MakeAvailable(modcache)

add_executable(app main.cc)
add_dependencies(app modcache)
set_property(TARGET app PROPERTY CXX_COMPILER_LAUNCHER
  "$<TARGET_FILE:modcache::modcache>")
```

The explicit dependency builds the launcher before compiling `app`. In a cross
build, use an installed launcher built for the host instead.

## Boundaries

This handles CMake/Ninja's explicit Clang module builds. It is not a general
response-file parser or a replacement for sccache. Unknown response syntax,
implicit module configurations and missing dependencies bypass the cache.

On Windows, recognized `clang-cl` module importers can be cached. Module
producers and PCH invocations still go directly to the compiler. On Linux and
macOS, explicit `-include-pch` inputs are included in the digest.

The `.modcache` sidecar beside each object records input paths and hashes. It
belongs to the build directory. Existing `SCCACHE_EXTRAFILES` entries are
preserved. Source files must remain unchanged during a compilation, as with the
compiler itself.

To exercise actual module cache hits and invalidation, enable the small
integration test with a module-capable Clang, Ninja and sccache available:

```sh
cmake -S . -B build -DMODCACHE_TEST_MODULES=ON \
  -DMODCACHE_TEST_COMPILER=clang++
cmake --build build
ctest --test-dir build --output-on-failure
```

Use `clang-cl` for that option on Windows, from a configured developer shell.

## License

[BSD-2-Clause OR Apache-2.0](LICENSE.md).
