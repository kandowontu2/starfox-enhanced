/* Opt-in host diagnostics, not an interop ABI or rendering optimization.
 * The submit lock already serializes sample writes. No per-submit logging,
 * waits, extra queues, resource retention, or changed fence ownership. */
typedef struct StarfoxVulkanSubmitSample {
    Uint64 lock, prepare, end, fence, queue, tail;
    bool present;
} StarfoxVulkanSubmitSample;
typedef struct StarfoxVulkanSubmitTimings {
    Uint32 count, dropped;
    StarfoxVulkanSubmitSample samples[8192];
} StarfoxVulkanSubmitTimings;

static void Starfox_Vulkan_CreateSubmitTimings(VulkanRenderer *renderer)
{
    if (SDL_getenv("STARFOX_TRACE_VULKAN_SUBMIT_TIMESTAMPS"))
        renderer->starfoxSubmitTimings = SDL_calloc(1, sizeof(StarfoxVulkanSubmitTimings));
}
static void Starfox_Vulkan_RecordSubmitTimings(VulkanRenderer *renderer, const Uint64 marks[7], bool present)
{
    StarfoxVulkanSubmitTimings *timings = (StarfoxVulkanSubmitTimings *)renderer->starfoxSubmitTimings;
    if (!timings) return;
    if (timings->count < SDL_arraysize(timings->samples)) {
        StarfoxVulkanSubmitSample *sample = &timings->samples[timings->count++];
        sample->lock = marks[1] - marks[0];
        sample->prepare = marks[2] - marks[1];
        sample->end = marks[3] - marks[2];
        sample->fence = marks[4] - marks[3];
        sample->queue = marks[5] - marks[4];
        sample->tail = marks[6] - marks[5];
        sample->present = present;
    } else ++timings->dropped;
}
static void Starfox_Vulkan_DestroySubmitTimings(VulkanRenderer *renderer)
{
    StarfoxVulkanSubmitTimings *timings = (StarfoxVulkanSubmitTimings *)renderer->starfoxSubmitTimings;
    if (!timings) return;
    SDL_Log("vulkan-submit-timestamps: samples=%u dropped=%u", timings->count, timings->dropped);
    for (Uint32 i = 0; i < timings->count; ++i) {
        const StarfoxVulkanSubmitSample *sample = &timings->samples[i];
        SDL_Log("vulkan-submit-time: serial=%u present=%u lock-us=%.3f prepare-us=%.3f end-us=%.3f fence-us=%.3f queue-us=%.3f tail-us=%.3f",
            i + 1, (unsigned)sample->present, (double)sample->lock / 1000., (double)sample->prepare / 1000.,
            (double)sample->end / 1000., (double)sample->fence / 1000., (double)sample->queue / 1000., (double)sample->tail / 1000.);
    }
    SDL_free(timings);renderer->starfoxSubmitTimings = NULL;
}
