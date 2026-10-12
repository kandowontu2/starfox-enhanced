#pragma once
#include "starfox/vr/vulkan_eye_commands.hpp"
#include "starfox/vr/vulkan_scene_buffer.hpp"

namespace starfox::vr {
// Transient transition layers may still be referenced by the last eye draw
// when the host cancels or an exception unwinds the frame driver. Members are
// destroyed in reverse order: retire commands before any buffer or pipeline.
// The other scene/target owners must also outlive this frame resource owner.
struct VulkanFrameResources {
    VulkanScenePipeline circle_pipeline;
    VulkanSceneBuffer circle_vertices;
    VulkanScenePipeline shutter_pipeline;
    VulkanSceneBuffer shutter_vertices;
    VulkanEyeCommands commands;
};
}
