# SPDX-License-Identifier: MIT
# Standalone: cmake -S examples/qt-helpdesk -B build-qt \
#   -DCMAKE_PREFIX_PATH="<unpacked release archive>;<Qt 6 prefix>"
cmake_minimum_required(VERSION 3.18)
project(gaiadesk_qt_helpdesk LANGUAGES CXX)
if(NOT TARGET GaiaDesk::embed)
  find_package(GaiaDeskEmbed 0.1 REQUIRED)
endif()
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
add_executable(qt-helpdesk MACOSX_BUNDLE main.cpp HelpDeskWindow.cpp HelpDeskWindow.h)
set_target_properties(qt-helpdesk PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
  MACOSX_BUNDLE_BUNDLE_NAME "Acme Desk"
  MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.acmedesk")
target_link_libraries(qt-helpdesk PRIVATE GaiaDesk::embed Qt6::Widgets)
if(MSVC)
  target_compile_options(qt-helpdesk PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(qt-helpdesk PRIVATE -Wall -Wextra)
endif()
# The library beside the executable (Contents/Frameworks in the .app).
gaiadesk_embed_deploy(qt-helpdesk)

if(BUILD_TESTING OR GAIADESK_EMBED_BUILD_TESTS)
  # Offscreen, autostarted with a bad token: refused, the app keeps running
  # and quits on its timer.
  add_test(NAME example_qt_helpdesk_refuses_a_bad_token COMMAND qt-helpdesk)
  set_tests_properties(example_qt_helpdesk_refuses_a_bad_token PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;GAIADESK_EMBED_TOKEN=gdemb_bad;GAIADESK_DEMO_AUTOSTART=1;GAIADESK_DEMO_TEST_CONSENT=1;GAIADESK_DEMO_EXIT_AFTER_MS=2000"
    PASS_REGULAR_EXPRESSION "refused bad_token"
    LABELS "live;example" TIMEOUT 60)
endif()
