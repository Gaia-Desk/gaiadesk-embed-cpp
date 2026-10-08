# SPDX-License-Identifier: MIT
# Standalone: cmake -S examples/c-minimal -B build-c -DCMAKE_PREFIX_PATH=<unpacked release archive>
cmake_minimum_required(VERSION 3.18)
project(gaiadesk_c_minimal LANGUAGES C)
if(NOT TARGET GaiaDesk::embed)
  find_package(GaiaDeskEmbed 0.1 REQUIRED)
endif()

add_executable(c-minimal main.c)
set_target_properties(c-minimal PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
target_link_libraries(c-minimal PRIVATE GaiaDesk::embed)
if(APPLE)
  target_link_libraries(c-minimal PRIVATE "-framework CoreFoundation")
elseif(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(c-minimal PRIVATE Threads::Threads)
endif()
if(MSVC)
  target_compile_options(c-minimal PRIVATE /W4 /WX)
else()
  target_compile_options(c-minimal PRIVATE -Wall -Wextra -Werror)
endif()
gaiadesk_embed_deploy(c-minimal)

if(BUILD_TESTING OR GAIADESK_EMBED_BUILD_TESTS)
  # A bad token is refused before anything starts (no dialog: test consent).
  add_test(NAME example_c_minimal_refuses_a_bad_token COMMAND c-minimal)
  set_tests_properties(example_c_minimal_refuses_a_bad_token PROPERTIES
    ENVIRONMENT "GAIADESK_EMBED_TOKEN=gdemb_bad;GAIADESK_DEMO_TEST_CONSENT=1"
    PASS_REGULAR_EXPRESSION "refused: .*\"code\":\"bad_token\""
    LABELS "live;example" TIMEOUT 60)
endif()
