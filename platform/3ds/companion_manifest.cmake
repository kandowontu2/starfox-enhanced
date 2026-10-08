# Build-time public checksum only. No ROM/BIN, patches, symbols or photographic
# backdrops are embedded in the original console executable by this generator.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(companion_root "${CMAKE_CURRENT_LIST_DIR}/../..")
set(companion_resources
    assets/patches/ultrastarfox-v12.bps assets/symbols/ultrastarfox.txt
    assets/patches/starfox-ex-v12.bps assets/symbols/starfox-ex.txt
    assets/patches/retail-japan-v10-to-usa-v12.bps assets/patches/retail-japan-v11-to-usa-v12.bps
    assets/patches/retail-usa-v10-to-v12.bps assets/patches/retail-usa-v11-to-v12.bps
    assets/patches/retail-europe-v10-to-usa-v12.bps assets/patches/retail-europe-v11-to-usa-v12.bps
    assets/patches/retail-germany-v10-to-usa-v12.bps)
list(TRANSFORM companion_resources PREPEND "${companion_root}/")
set(companion_header "${CMAKE_CURRENT_BINARY_DIR}/generated/companion_manifest.hpp")
add_custom_command(OUTPUT "${companion_header}"
    COMMAND "${Python3_EXECUTABLE}" "${companion_root}/tools/generate_runtime_manifest.py"
        --root "${companion_root}" --output "${companion_header}"
    DEPENDS "${companion_root}/tools/generate_runtime_manifest.py" ${companion_resources}
    VERBATIM)
