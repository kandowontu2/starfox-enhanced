#pragma once
#include <stdint.h>

/* C boundary between the MinGW runtime and the MSVC-only SR SDK. All calls
 * run on the present thread. No exception or SDK C++ object crosses this ABI.
 * The optional module is loaded only after the user selects SR Platform. */
#ifdef __cplusplus
extern "C" {
#endif
typedef struct StarfoxLeiaSrApiV1 {
    uint32_t version;
    uint32_t size;
    void *(*create)(void *d3d12_device, void *hwnd);
    int (*weave)(void *host, void *command_list, void *source,
        uint32_t width, uint32_t height, uint32_t dxgi_format);
    /* NULL also drains a failed factory's partial context. Returns zero if
     * vendor teardown failed: keep module/host resident. */
    int (*destroy)(void *host);
} StarfoxLeiaSrApiV1;
typedef const StarfoxLeiaSrApiV1 *(*StarfoxLeiaSrGetApi)(uint32_t version);
/* Optional separate diagnostic export; V1 layout/version stay unchanged.
 * Borrowed text is valid until the next adapter call, on the present thread. */
typedef const char *(*StarfoxLeiaSrGetStatus)(void);
#ifdef __cplusplus
}
#endif
