cmake_minimum_required(VERSION 3.22)
project(starfox_leia_sr LANGUAGES CXX)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The SR SDK adapter requires 64-bit MSVC; the game itself does not")
endif()
include(FetchContent)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
FetchContent_Declare(starfox_srlib
    URL https://github.com/bo3b/SR-lib/archive/0c80cc01f671e1491ea70f8f9c2358ff938a6503.tar.gz
    URL_HASH SHA256=bc270bb61eef12581169601b57c8585ad821f46cc1cc004edcd30c6355040056
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(starfox_srlib)
if(NOT SRLIB_FOUND)
    message(FATAL_ERROR "Pinned SR-lib SDK/import libraries are missing")
endif()
add_library(starfox_leia_sr SHARED adapter.cpp)
target_include_directories(starfox_leia_sr PRIVATE ../../include)
target_link_libraries(starfox_leia_sr PRIVATE SRLib::SDK)
srlib_apply_delayload(starfox_leia_sr)
target_compile_features(starfox_leia_sr PRIVATE cxx_std_17)
# Vendor delay-load faults and SDK exceptions must stay inside this DLL.
target_compile_options(starfox_leia_sr PRIVATE /EHa)
install(TARGETS starfox_leia_sr RUNTIME DESTINATION .)
install(FILES ../../third_party/leia-sr/SR-lib-LICENSE.txt DESTINATION licenses/leia-sr)
install(FILES "${starfox_srlib_SOURCE_DIR}/SR-SDK-1.34.10/License.txt" DESTINATION licenses/leia-sr RENAME SR-SDK-LICENSE.txt)
# The SDK's system runtime/service is not bundled. In particular, do not copy
# its hand-tracking components or optional third-party runtime DLLs.
