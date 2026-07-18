cmake_minimum_required(VERSION 3.16)
project(trino-cpp
    VERSION 0.1.5
    LANGUAGES CXX
)

# ── Locate tcmake build environment ───────────────────────────────────────────

if(NOT DEFINED TCMAKE_HOME)
    if(DEFINED ENV{TCMAKE_HOME})
        set(TCMAKE_HOME "$ENV{TCMAKE_HOME}")
    else()
        get_filename_component(TCMAKE_HOME "${CMAKE_CURRENT_SOURCE_DIR}/../tcmake" ABSOLUTE)
    endif()
endif()
list(APPEND CMAKE_MODULE_PATH "${TCMAKE_HOME}/cmake")

# Optional: override C++ standard before including tcmake
set(TCMAKE_CXX_STANDARD 17)

include(tcmake)

# ── Output directories ────────────────────────────────────────────────────────

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── Dependencies ──────────────────────────────────────────────────────────────

tcmake_require(LIBCURL)

# ── Library: trinoquery ──────────────────────────────────────────────────────

tcmake_add_library(trino-query
    SOURCES         src/trino-query.cpp
    PUBLIC_INCLUDES include
    TYPE            STATIC
)

# ── Executable: trinoquery ─────────────────────────────────────────────────

tcmake_add_executable(trinoquery
    SOURCES  src/main.cpp
    LIBS     trino-query
)
