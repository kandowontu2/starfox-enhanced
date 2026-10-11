# Private opt-in candidate for the pinned SDL Metal compute constructor.
# Default SDL descriptors and all actual shader work remain unchanged.
function(starfox_patch_metal_exact_workgroup source_dir)
    set(source "${source_dir}/src/gpu/metal/SDL_gpu_metal.m")
    file(READ "${source}" code)
    set(anchor "        descriptor.computeFunction = libraryFunction.function;")
    set(replacement [=[        descriptor.computeFunction = libraryFunction.function;
        /* Star Fox exact compute workgroup v1: opt-in, no dispatch-size change. */
        if (SDL_GetBooleanProperty(createinfo->props, "starfox.gpu.compute.exact_threadgroup.v1", false)) {
            const Uint64 xy = (Uint64)createinfo->threadcount_x * createinfo->threadcount_y;
            if (xy == 0 || createinfo->threadcount_z == 0 || xy > (Uint64)NSUIntegerMax / createinfo->threadcount_z) {
                SET_ERROR_AND_RETURN("%s", "Invalid exact compute threadgroup size", NULL);
            }
            descriptor.maxTotalThreadsPerThreadgroup = (NSUInteger)(xy * createinfo->threadcount_z);
        }]=])
    set(error_anchor [=[        if (error != NULL) {
            SET_ERROR_AND_RETURN("Creating compute pipeline failed: %s", [[error description] UTF8String], NULL);
        }]=])
    set(error_replacement [=[        if (error != NULL || handle == nil) {
            SET_ERROR_AND_RETURN("Creating compute pipeline failed: %s",
                error != NULL ? [[error description] UTF8String] : "Metal returned nil without NSError", NULL);
        }]=])
    string(FIND "${code}" "/* Star Fox exact compute workgroup v1:" installed)
    if(installed LESS 0)
        string(REPLACE "${anchor}" "" without_anchor "${code}")
        string(LENGTH "${code}" original_length)
        string(LENGTH "${without_anchor}" remaining_length)
        string(LENGTH "${anchor}" anchor_length)
        math(EXPR anchor_count "(${original_length}-${remaining_length})/${anchor_length}")
        string(FIND "${code}" "${error_anchor}" error_found)
        if(NOT anchor_count EQUAL 1 OR error_found LESS 0)
            message(FATAL_ERROR "Pinned SDL Metal compute constructor changed")
        endif()
        string(REPLACE "${anchor}" "${replacement}" code "${code}")
        string(REPLACE "${error_anchor}" "${error_replacement}" code "${code}")
        file(WRITE "${source}" "${code}")
    else()
        string(FIND "${code}" "${replacement}" hint_found)
        string(FIND "${code}" "${error_replacement}" nil_found)
        if(hint_found LESS 0 OR nil_found LESS 0)
            message(FATAL_ERROR "Previously applied Metal candidate differs")
        endif()
    endif()
endfunction()
