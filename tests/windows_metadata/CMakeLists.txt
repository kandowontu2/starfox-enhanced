cmake_minimum_required(VERSION 3.22)
project(starfox_windows_metadata_tests VERSION 0.0.8.6 LANGUAGES CXX)
include(../../cmake/WindowsMetadata.cmake)
add_executable(metadata_app fixture.cpp)
add_library(metadata_adapter SHARED fixture.cpp)
starfox_windows_metadata(metadata_app "Star Fox Enhanced metadata fixture")
starfox_windows_metadata(metadata_adapter "Star Fox Enhanced adapter fixture")
# Repeated registration must not duplicate VERSIONINFO/manifest resources.
starfox_windows_metadata(metadata_app "Star Fox Enhanced metadata fixture")
