#pragma once

namespace starfox::render {
// Dual-screen frontends select source OBJ groups before composition. Never
// recover a HUD by erasing a rectangle from the finished world image.
enum class SpriteSelection { all, world_only, configurable_hud_only };
} // namespace starfox::render
