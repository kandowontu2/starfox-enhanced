#pragma once
/* Private, opt-in pinned SDL extension. It permits shader reads (not storage
 * writes) of single-mip/layer 2D multisample colour attachments on D3D12 and
 * standard-sample-location Vulkan devices. Ordinary SDL validation is intact.
 * Set the texture-create property only after checking the device capability.
 * Shader bindings must use Texture2DMS, never ordinary filtered Texture2D.
 */
#define STARFOX_SDL_MULTISAMPLE_READ "starfox.gpu.multisample-read.v1"
