cmake_minimum_required(VERSION 3.0)

project(frequency_cutoff_plugin VERSION 1.5.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

SET(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_AUTOMOC ON)
find_package(Qt5Widgets CONFIG REQUIRED)

add_library(frequency_cutoff_plugin_21 SHARED src/api_21/plugin.cpp thirdparty/iir/liir.c)
target_include_directories(frequency_cutoff_plugin_21 PUBLIC src/include thirdparty/iir/include src/api_21/include thirdparty/teamspeak/api_21/pluginsdk/include /usr/include/qt)

add_library(frequency_cutoff_plugin_22 SHARED src/api_22/plugin.cpp thirdparty/iir/liir.c)
target_include_directories(frequency_cutoff_plugin_22 PUBLIC src/include thirdparty/iir/include src/api_22/include thirdparty/teamspeak/api_22/pluginsdk/include /usr/include/qt)
