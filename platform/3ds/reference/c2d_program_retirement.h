// Included once by the patched Citro2D base translation unit.
// Citro3D 1.7.1 reads its previous program on the NEXT BindProgram call,
// even after Fini/Init. Keep just that shader alive, not GPU buffers/textures.
#include <string.h>
static shaderProgram_s starfox_c2d_retired_program;
static DVLB_s* starfox_c2d_retired_shader;

// Call only AFTER another valid shader was successfully bound.
void starfox_c2d_release_retired_program(void) {
    if (!starfox_c2d_retired_shader) return;
    shaderProgramFree(&starfox_c2d_retired_program);
    DVLB_Free(starfox_c2d_retired_shader);
    memset(&starfox_c2d_retired_program, 0, sizeof(starfox_c2d_retired_program));
    starfox_c2d_retired_shader = NULL;
}

// The queue must be idle and Citro3D still initialized. Move the shader
// ownership to a stable process-lifetime address before freeing C2D buffers.
static void starfox_c2d_retire_program(shaderProgram_s* program, DVLB_s** shader) {
    if (!*shader) return;
    C3D_BindProgram(program);
    starfox_c2d_release_retired_program();
    starfox_c2d_retired_program = *program;
    starfox_c2d_retired_shader = *shader;
    C3D_BindProgram(&starfox_c2d_retired_program);
    memset(program, 0, sizeof(*program));
    *shader = NULL;
}
