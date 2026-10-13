# Exact upstream Citro2D base renderer; no private SDK snapshot or installed
# library with unknown flush behavior. The reference port uses these fixes.
include(FetchContent)
FetchContent_Declare(starfox_citro2d
    URL https://codeload.github.com/devkitPro/citro2d/tar.gz/refs/tags/v1.7.0
    URL_HASH SHA256=f311d1decb64668980b7a7a917a9b936a40025265df921e251f5f0537b94b86b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _starfox_no_upstream_project)
FetchContent_MakeAvailable(starfox_citro2d)
file(READ "${starfox_citro2d_SOURCE_DIR}/source/base.c" c2d_base)
set(c2d_draw "\tC3D_DrawElements(GPU_TRIANGLES, len, C3D_UNSIGNED_SHORT, &ctx->idxBuf[ctx->idxBufLastPos]);")
string(FIND "${c2d_base}" "${c2d_draw}" c2d_draw_offset)
if(c2d_draw_offset LESS 0)
    message(FATAL_ERROR "Pinned Citro2D draw flush patch no longer matches")
endif()
string(REPLACE "${c2d_draw}"
    "\tif (R_FAILED(GSPGPU_FlushDataCache(ctx->vtxBuf, ctx->vtxBufPos*sizeof(C2Di_Vertex))) || R_FAILED(GSPGPU_FlushDataCache(ctx->idxBuf, ctx->idxBufPos*sizeof(u16)))) {\n\t\tResult vertices = svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)ctx->vtxBuf, ctx->vtxBufSize*sizeof(C2Di_Vertex));\n\t\tResult indices = svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)ctx->idxBuf, ctx->idxBufSize*sizeof(u16));\n\t\tif (R_FAILED(vertices) || R_FAILED(indices)) { svcBreak(USERBREAK_PANIC); return; }\n\t}\n${c2d_draw}"
    c2d_base "${c2d_base}")
string(REPLACE "C3D_FrameSplit(0);" "C3D_FrameSplit(GX_CMDLIST_FLUSH);" c2d_base "${c2d_base}")
set(c2d_generated "${CMAKE_CURRENT_BINARY_DIR}/reference-citro2d")
file(MAKE_DIRECTORY "${c2d_generated}")
file(WRITE "${c2d_generated}/base.c" "${c2d_base}")
file(WRITE "${c2d_generated}/render2d_shbin.h" "extern const unsigned char render2d_shbin[];\nextern const unsigned int render2d_shbin_size;\n")
set(c2d_shader "${c2d_generated}/render2d.shbin")
ctr_add_shader_library(starfox_3ds_c2d_shader OUTPUT "${c2d_shader}" "${starfox_citro2d_SOURCE_DIR}/source/render2d.v.pica")
add_custom_command(OUTPUT "${c2d_generated}/shader.c"
    COMMAND "${CMAKE_COMMAND}" "-DINPUT=${c2d_shader}" "-DOUTPUT=${c2d_generated}/shader.c" -P "${CMAKE_CURRENT_LIST_DIR}/embed_c2d_shader.cmake"
    DEPENDS starfox_3ds_c2d_shader "${c2d_shader}" "${CMAKE_CURRENT_LIST_DIR}/embed_c2d_shader.cmake" VERBATIM)
add_library(starfox_3ds_citro2d STATIC "${c2d_generated}/base.c" "${c2d_generated}/shader.c")
target_include_directories(starfox_3ds_citro2d PUBLIC "${starfox_citro2d_SOURCE_DIR}/include"
    PRIVATE "${starfox_citro2d_SOURCE_DIR}/source" "${c2d_generated}")
target_include_directories(starfox_3ds_citro2d SYSTEM PRIVATE "${STARFOX_CTRU_INCLUDE}" "${STARFOX_CITRO3D_INCLUDE}")
target_link_libraries(starfox_3ds_citro2d PUBLIC "${STARFOX_CITRO3D_LIBRARY}" "${STARFOX_CTRU_LIBRARY}" m)
target_compile_options(starfox_3ds_citro2d PRIVATE -O3 -ffunction-sections -fdata-sections)
