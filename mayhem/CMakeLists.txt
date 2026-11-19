# mcrouter fuzz/test build, driven by mayhem/build.sh. Configured twice:
#   MCR_MODE=fuzz  upstream's own CMake build (add_subdirectory of the repo root -> mcrouter_lib,
#                  incl. ragel + thrift codegen) with $SANITIZER_FLAGS -gdwarf-3, and the harnesses
#                  linked against it (libFuzzer + standalone reproducers)
#   MCR_MODE=test  normal flags: the parser sources upstream's unit tests below cover, + those tests
# Dependencies (folly, fizz, wangle, mvfst, fbthrift) are prebuilt in /opt/toolchains/deps.
cmake_minimum_required(VERSION 3.16)
project(mcrouter_mayhem C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(MCR ${CMAKE_CURRENT_SOURCE_DIR}/..)

if(MCR_MODE STREQUAL "fuzz")
  add_subdirectory(${MCR} mcrouter EXCLUDE_FROM_ALL)
  add_library(standalone_main OBJECT ${STANDALONE_MAIN})
  set_source_files_properties(${STANDALONE_MAIN} PROPERTIES LANGUAGE C)
  set(HARNESSES fuzz_lz4immutable fuzz_config_preprocessor fuzz_server_parser fuzz_client_parser)
  foreach(h ${HARNESSES})
    add_executable(${h} harnesses/${h}.cpp ${LSAN_OFF_OBJ})
    target_link_libraries(${h} PRIVATE mcrouter_lib)
    target_link_options(${h} PRIVATE ${FUZZ_ENGINE})
    add_executable(${h}-standalone harnesses/${h}.cpp ${LSAN_OFF_OBJ} $<TARGET_OBJECTS:standalone_main>)
    target_link_libraries(${h}-standalone PRIVATE mcrouter_lib)
  endforeach()
else()
  find_package(folly CONFIG REQUIRED)
  find_package(OpenSSL REQUIRED)
  find_package(Boost REQUIRED COMPONENTS filesystem)
  find_package(GTest REQUIRED)
  add_library(mcrouter_parsers STATIC
    ${MCR}/mcrouter/lib/Lz4Immutable.cpp
    ${MCR}/mcrouter/lib/IovecCursor.cpp
    ${MCR}/mcrouter/lib/config/ConfigPreprocessor.cpp
    ${MCR}/mcrouter/lib/config/RendezvousHash.cpp
    ${MCR}/mcrouter/lib/fbi/cpp/util.cpp
    ${MCR}/mcrouter/lib/fbi/network.c
  )
  target_include_directories(mcrouter_parsers PUBLIC ${MCR})
  target_compile_definitions(mcrouter_parsers PUBLIC MCROUTER_OSS_BUILD=1 LIBMC_FBTRACE_DISABLE DISABLE_COMPRESSION)
  target_link_libraries(mcrouter_parsers PUBLIC Folly::folly OpenSSL::Crypto Boost::filesystem lz4)
  add_executable(mcrouter_unit_tests
    ${MCR}/mcrouter/lib/test/Lz4ImmutableTest.cpp
    ${MCR}/mcrouter/lib/test/IovecCursorTest.cpp
    ${MCR}/mcrouter/lib/config/test/config_preprocessor_test.cpp
    ${MCR}/mcrouter/lib/config/test/rendezvous_hash_test.cpp
    ${MCR}/mcrouter/lib/config/test/main.cpp
  )
  target_link_libraries(mcrouter_unit_tests PRIVATE mcrouter_parsers GTest::gtest GTest::gtest_main)
endif()
