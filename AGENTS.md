# modcache

Keep this a small C++11 executable. No Python or runtime dependencies beyond the
compiler, sccache and C++ runtime. Use CMake and CTest. Use snake_case, two-space
indentation and east const. Preserve BSD-2-Clause OR Apache-2.0 notices.

The launcher must preserve compiler argument values and exit status. Unsupported
syntax or missing dependency information bypasses caching. Never rewrite
CMake-owned response files or retry a failed compilation to hide a cache error.

Test actual cache hits and invalidation. Tests own their temporary files and
cache server; never stop or clear a user's cache. Keep tests small and run them
repeatedly without cleanup as a prerequisite. No agent-prefixed branch names.
