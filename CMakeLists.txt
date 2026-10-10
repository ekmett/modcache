# SPDX-License-Identifier: BSD-2-Clause OR Apache-2.0
cmake_minimum_required(VERSION 3.28)
project(modcache VERSION 0.1.0 LANGUAGES CXX)

add_library(modcache_core STATIC src/cache.cc)
target_include_directories(modcache_core PUBLIC src)
set_target_properties(modcache_core PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
add_executable(modcache src/main.cc)
target_link_libraries(modcache PRIVATE modcache_core)
set_target_properties(modcache PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
add_executable(modcache::modcache ALIAS modcache)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS modcache EXPORT modcache-targets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(EXPORT modcache-targets FILE modcache-config.cmake NAMESPACE modcache:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/modcache)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/modcache-config-version.cmake" COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/modcache-config-version.cmake" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/modcache)

option(MODCACHE_BUILD_TESTS "Build modcache tests" ${PROJECT_IS_TOP_LEVEL})
option(MODCACHE_TEST_MODULES "Test with a real Clang module build and sccache" OFF)
set(MODCACHE_TEST_COMPILER "clang++" CACHE STRING "Compiler for the module cache test")
if(MODCACHE_BUILD_TESTS)
  enable_testing()
  add_executable(modcache_test tests/check.cc)
  target_link_libraries(modcache_test PRIVATE modcache_core)
  set_target_properties(modcache_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
  add_test(NAME modcache.core COMMAND modcache_test)
  add_test(NAME modcache.help COMMAND modcache --help)
  add_executable(modcache_process tests/process.cc)
  target_link_libraries(modcache_process PRIVATE modcache_core)
  set_target_properties(modcache_process PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
  add_test(NAME modcache.process COMMAND ${CMAKE_COMMAND}
    "-DLAUNCHER=$<TARGET_FILE:modcache>" "-DPROBE=$<TARGET_FILE:modcache_process>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/process.cmake")
  if(MODCACHE_TEST_MODULES)
    find_program(SCCACHE_EXECUTABLE sccache REQUIRED)
    find_program(NINJA_EXECUTABLE ninja REQUIRED)
    add_test(NAME modcache.modules COMMAND ${CMAKE_COMMAND}
      "-DLAUNCHER=$<TARGET_FILE:modcache>"
      "-DCOMPILER=${MODCACHE_TEST_COMPILER}"
      "-DSCCACHE=${SCCACHE_EXECUTABLE}"
      "-DNINJA=${NINJA_EXECUTABLE}"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/module-test"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/modules.cmake")
    set_tests_properties(modcache.modules PROPERTIES TIMEOUT 180)
  endif()
endif()
