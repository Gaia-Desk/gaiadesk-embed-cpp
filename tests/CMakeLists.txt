# SPDX-License-Identifier: MIT
find_package(Threads REQUIRED)
set(_gd_test_dir "${CMAKE_BINARY_DIR}/tests/bin")

function(gd_test name)
  cmake_parse_arguments(T "" "LABEL" "SOURCES" ${ARGN})
  add_executable(${name} ${T_SOURCES})
  target_link_libraries(${name} PRIVATE GaiaDesk::embed Threads::Threads)
  set_target_properties(${name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${_gd_test_dir}"
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
    C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4 /WX /permissive- /utf-8)
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror)
  endif()
  gaiadesk_embed_deploy(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS "${T_LABEL}" TIMEOUT 60)
endfunction()

# Pure: the JSON, the config, the events (they still link the library: the
# wrapper is one header).
gd_test(test_json LABEL unit SOURCES test_json.cpp)
gd_test(test_config LABEL unit SOURCES test_config.cpp)
gd_test(test_events LABEL unit SOURCES test_events.cpp)
gd_test(test_platform LABEL unit SOURCES test_platform.cpp)
# Against the real library (refusals only; no session, no network).
gd_test(test_live LABEL live SOURCES test_live.cpp)
gd_test(test_c_api LABEL live SOURCES test_c_api.c)
