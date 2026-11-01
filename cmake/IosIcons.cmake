include_guard(GLOBAL)

# Raw bundle PNGs work for unsigned/sideloaded IPAs too, without depending on
# asset-catalog merging into our custom Info.plist. Reuse the existing artwork.
function(starfox_add_ios_icons target)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    set(icons)
    foreach(name AppIcon60@2x.png AppIcon60@3x.png AppIcon76.png
            AppIcon76@2x.png AppIcon83.5@2x.png)
        list(APPEND icons "${CMAKE_CURRENT_BINARY_DIR}/generated/ios-icons/${name}")
    endforeach()
    file(GLOB icon_inputs CONFIGURE_DEPENDS "${root}/assets/icons/ios/*.png")
    add_custom_command(OUTPUT ${icons}
        COMMAND "${Python3_EXECUTABLE}" "${root}/tools/generate_ios_assets.py"
            --output "${CMAKE_CURRENT_BINARY_DIR}/generated/ios-icons"
        DEPENDS "${root}/tools/generate_ios_assets.py" ${icon_inputs}
        VERBATIM)
    target_sources(${target} PRIVATE ${icons})
    set_source_files_properties(${icons} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    # Keep standard PNG bytes: the IPA verifier validates their raster/CRC;
    # Xcode's optional CgBI conversion is unnecessary for these tiny icons.
    # Do not combine @2x/@3x PNGs into desktop multi-resolution TIFFs either:
    # iOS's CFBundleIconFiles entries must resolve to the original PNG files.
    set_target_properties(${target} PROPERTIES
        XCODE_ATTRIBUTE_COMPRESS_PNG_FILES NO
        XCODE_ATTRIBUTE_COMBINE_HIDPI_IMAGES NO)
    set(STARFOX_APPLE_ICON_PLIST "
  <key>CFBundleIcons</key>
  <dict><key>CFBundlePrimaryIcon</key><dict>
    <key>CFBundleIconFiles</key><array><string>AppIcon60</string></array>
    <key>UIPrerenderedIcon</key><false/>
  </dict></dict>
  <key>CFBundleIcons~ipad</key>
  <dict><key>CFBundlePrimaryIcon</key><dict>
    <key>CFBundleIconFiles</key><array>
      <string>AppIcon60</string><string>AppIcon76</string><string>AppIcon83.5</string>
    </array>
    <key>UIPrerenderedIcon</key><false/>
  </dict></dict>" PARENT_SCOPE)
endfunction()
