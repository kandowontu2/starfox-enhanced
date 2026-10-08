#pragma once
#include <stdint.h>
#include <stddef.h>

/* Private C ABI between the MinGW game and optional MSVC-built SDK adapter.
 * No STL, SDK types, ownership transfer or exceptions cross this boundary.
 * Initialize the trusted SDK before DXGI; set its device before these calls.
 * The caller owns command submission, presentation hooks and GPU completion.
 * All five resources must belong to the command-list device and remain alive
 * through GPU completion. Matrices are unjittered, row-major Streamline layout.
 * Input motion is previous-minus-current in render pixels, not jittered. */
#if defined(STARFOX_DLSS_EXPORTS)
#define SF_DLSS_API __declspec(dllexport)
#elif defined(_WIN32)
#define SF_DLSS_API __declspec(dllimport)
#else
#define SF_DLSS_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct StarfoxDlssFrameV1 {
    uint32_t size, viewport, frame_index, width, height, output_width, output_height, reset;
    void *command, *color, *depth, *motion, *output, *exposure;
    uint32_t states[5]; /* color, depth, motion, output, exposure */
    float view_to_clip[16], clip_to_view[16], clip_to_previous[16], previous_to_clip[16];
    float camera_position[3], camera_up[3], camera_right[3], camera_forward[3];
    float near_plane, far_plane, vertical_fov, aspect, jitter[2], pinhole[2];
} StarfoxDlssFrameV1;
/* Native-eye extension. R8_UNORM input-sized bias: 1 rejects history locally,
 * 0 preserves ordinary reconstruction. Same device/lifetime as the V1 guides.
 * V1 remains available to the mono host; no layout change to its ABI. */
typedef struct StarfoxDlssFrameV2 {
    uint32_t size;
    StarfoxDlssFrameV1 frame;
    void *current_color_bias;
    uint32_t bias_state;
} StarfoxDlssFrameV2;

/* Single rendering thread / one SDK instance per process. Open must precede
 * DXGI/SDL GPU creation. binary_directory is an absolute official SDK bin/x64
 * path; no download or fallback DLL search. Close only after all evaluated GPU
 * work has completed and viewports have been released. The adapter retains the
 * bound device through SDK shutdown. Failed open never publishes a handle. */
SF_DLSS_API int starfox_dlss_open_v1(const wchar_t *binary_directory, void **module,
    char *error, uint32_t error_capacity);
SF_DLSS_API int starfox_dlss_bind_device_v1(void *module, void *device,
    char *error, uint32_t error_capacity);
SF_DLSS_API int starfox_dlss_release_viewport_v1(void *module, uint32_t viewport,
    char *error, uint32_t error_capacity);
SF_DLSS_API int starfox_dlss_close_v1(void *module, char *error, uint32_t error_capacity);
// Swapchain3 owned-reference replacement; restore every upgraded chain before
// SDK shutdown. Restore is a no-op for chains not upgraded by this adapter.
SF_DLSS_API int starfox_dlss_swapchain_v1(void *module, void **swapchain3, uint32_t restore,
    char *error, uint32_t error_capacity);
/* Frame-end bookkeeping without the SDK's DXGI proxy. The mono SDL owner calls
 * once after native presentation/submission or cancellation, preserving its
 * existing ordered-queue resource lifetime. An OpenXR owner calls once for the
 * whole pair after its retained native work drains, including held pairs.
 * Runs verified sl.common before/after-present hooks; no dummy swapchain.
 * Never also call for proxied DXGI: that path already invokes these hooks. */
SF_DLSS_API int starfox_dlss_finish_frame_v1(void *module,
    char *error, uint32_t error_capacity);

/* 0 on success, nonzero with a bounded diagnostic on failure. Module is the
 * already initialized official sl.interposer.dll. Modes: 1 quality, 2 balanced,
 * 3 performance, 4 DLAA. Configuration returns recommended input dimensions. */
SF_DLSS_API int starfox_dlss_configure_v1(void *module, uint32_t viewport, uint32_t mode,
    uint32_t output_width, uint32_t output_height, uint32_t *width, uint32_t *height,
    char *error, uint32_t error_capacity);
/* v2 selects the model explicitly: 0=standard first-generation transformer
 * (K), 1=DLSS 4.5 second-generation transformer (M). They share NVIDIA's
 * official runtime, but are independent UI preferences. v1 remains standard. */
SF_DLSS_API int starfox_dlss_configure_v2(void *module, uint32_t viewport, uint32_t mode,
    uint32_t model, uint32_t output_width, uint32_t output_height, uint32_t *width, uint32_t *height,
    char *error, uint32_t error_capacity);
SF_DLSS_API int starfox_dlss_evaluate_v1(void *module, const StarfoxDlssFrameV1 *frame,
    char *error, uint32_t error_capacity);
SF_DLSS_API int starfox_dlss_evaluate_v2(void *module, const StarfoxDlssFrameV2 *frame,
    char *error, uint32_t error_capacity);
#ifdef __cplusplus
}
#endif
