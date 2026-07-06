cmake_minimum_required(VERSION 3.18)

set(SOLUTION_NAME TSEComplexConverterSol)

project(${SOLUTION_NAME})
set(SOURCE_ROOT ${CMAKE_CURRENT_LIST_DIR})

if (MSVC)
    set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "Available build configurations" FORCE)
endif()

if (WIN32)
    set(HOME_ROOT $ENV{USERPROFILE})
    string(REPLACE "\\" "/" HOME_ROOT "${HOME_ROOT}")
else()
    set(HOME_ROOT $ENV{HOME})
endif()

set(PROJECT_HOME_ROOT "${HOME_ROOT}/GitHub/SREES_2026_Vuk_TSE")
string(FIND "${SOURCE_ROOT}" "${PROJECT_HOME_ROOT}" PROJECT_IN_HOME_GITHUB)
if (NOT PROJECT_IN_HOME_GITHUB EQUAL 0)
    message(FATAL_ERROR
        "This project must be configured from the user's HOME GitHub project folder. "
        "Move/copy it under '${PROJECT_HOME_ROOT}' and use a fresh empty build folder. "
        "If CMake was already configured with another source path, delete CMakeCache.txt "
        "or delete the whole build folder before configuring again."
    )
endif()

set(NATID_SDK_ROOT ${HOME_ROOT}/natID.SDK)
include(${NATID_SDK_ROOT}/DevEnv/Common.cmake)

include(${NATID_SDK_ROOT}/DevEnv/natGUI.cmake)
include(${NATID_SDK_ROOT}/DevEnv/MatrixLib.cmake)

include(TSEComplexConverter.cmake)
