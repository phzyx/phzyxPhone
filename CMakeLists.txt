# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.16)
project(phzyxPhone VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# phzyxPhone - a testing-focused SIP softphone built on PJSIP/pjproject + Qt
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt meta-object / resource / ui handling
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- Qt (prefer Qt6, fall back to Qt5) ------------------------------------
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
message(STATUS "Using Qt${QT_VERSION_MAJOR} (${Qt${QT_VERSION_MAJOR}_VERSION})")

# --- pjproject / PJSUA2 via pkg-config ------------------------------------
# pjproject installs 'libpjproject.pc' on `make install`. If it was built into
# the in-repo prefix by scripts/build-pjproject.sh, point pkg-config there
# automatically; otherwise fall back to whatever is on PKG_CONFIG_PATH (e.g. a
# system-wide /usr/local install).
set(PJ_LOCAL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pjproject-install")
if(EXISTS "${PJ_LOCAL_PREFIX}/lib/pkgconfig/libpjproject.pc")
    set(ENV{PKG_CONFIG_PATH} "${PJ_LOCAL_PREFIX}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    message(STATUS "Using in-repo pjproject at ${PJ_LOCAL_PREFIX}")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PJPROJECT REQUIRED IMPORTED_TARGET libpjproject)
message(STATUS "Found pjproject: ${PJPROJECT_VERSION}")

add_executable(phzyxphone
    src/main.cpp
    src/SipCore.cpp
    src/MainWindow.cpp
)

target_include_directories(phzyxphone PRIVATE ${PJPROJECT_INCLUDE_DIRS})

target_link_libraries(phzyxphone PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    PkgConfig::PJPROJECT
)

# pjproject ships its required preprocessor defines (PJ_AUTOCONF etc.) and
# extra cflags through pkg-config; apply them so headers compile correctly.
target_compile_options(phzyxphone PRIVATE ${PJPROJECT_CFLAGS_OTHER})

# Bake the pjproject library directory into the binary's RPATH so it runs
# straight from the build tree without LD_LIBRARY_PATH when using the in-repo
# prefix.
#
# Use OLD-style DT_RPATH (--disable-new-dtags) rather than the linker default
# DT_RUNPATH. The pjproject shared libs depend on each other (libpjsua2 ->
# libpjsua -> libpjsip -> ...) and were not built with their own RPATH. A
# DT_RUNPATH is only consulted for the binary's *direct* dependencies and is
# NOT inherited when resolving those libraries' own transitive deps, so the
# second-level pj libs would come up "not found". DT_RPATH, by contrast, is
# searched for the whole dependency chain, which makes the in-repo prefix work.
if(PJPROJECT_LIBRARY_DIRS)
    set_target_properties(phzyxphone PROPERTIES
        BUILD_RPATH "${PJPROJECT_LIBRARY_DIRS}"
        INSTALL_RPATH "${PJPROJECT_LIBRARY_DIRS}")
    target_link_options(phzyxphone PRIVATE "-Wl,--disable-new-dtags")
endif()

install(TARGETS phzyxphone RUNTIME DESTINATION bin)