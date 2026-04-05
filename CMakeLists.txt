cmake_minimum_required(VERSION 3.16)
project(cpp_backtester LANGUAGES CXX)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

option(CPP_BACKTESTER_ENABLE_DLIB "Enable future dlib-based optimization integration when dlib is available" ON)

add_library(cpp_backtester_core
    src/config.cpp
    src/io.cpp
    src/price_generator.cpp
    src/simulator.cpp
    src/monte_carlo.cpp
    src/optimizer.cpp
)

target_include_directories(cpp_backtester_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (CPP_BACKTESTER_ENABLE_DLIB)
    find_package(dlib QUIET)
    if (dlib_FOUND)
        message(STATUS "dlib detected; enabling future optimizer integration hooks")
        target_compile_definitions(cpp_backtester_core PUBLIC CPP_BACKTESTER_HAS_DLIB=1)
        target_link_libraries(cpp_backtester_core PUBLIC dlib::dlib)
    else()
        message(STATUS "dlib not detected; building with placeholder optimizer backend")
    endif()
endif()

find_package(Eigen3 QUIET)
if (Eigen3_FOUND)
    target_link_libraries(cpp_backtester_core PUBLIC Eigen3::Eigen)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cpp_backtester_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cpp_backtester src/main.cpp)
target_link_libraries(cpp_backtester PRIVATE cpp_backtester_core)

install(TARGETS cpp_backtester
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
