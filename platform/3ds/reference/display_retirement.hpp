#pragma once

namespace starfox::platform_3ds {

// Called outside an acquired frame, after the BG2 worker has joined. Detaching
// a linked Citro3D output joins the command queue before Citro2D frees its
// vertex/index buffers. FrameSync waits for VBlank, not command completion.
// Partial construction without a linked output has not submitted a frame.
// Keep this order shared with the host resource-lifetime regression test.
template<class Target, class DetachOutput, class Retire2d, class Retire3d>
void retire_display_context(bool& c3d_ready, bool& c2d_ready,
    Target*& left, Target*& right, DetachOutput detach_output,
    Retire2d retire_2d, Retire3d retire_3d) {
    if (c3d_ready) {
        if (left != nullptr && left->linked) detach_output(left);
        if (right != nullptr && right->linked) detach_output(right);
    }
    // C2D's shader parking hook still needs a live Citro3D context here.
    if (c2d_ready) retire_2d();
    c2d_ready = false;
    if (c3d_ready) retire_3d();
    c3d_ready = false;
    // C3D_Fini owns target deletion, including unlinked partial construction.
    left = right = nullptr;
}

} // namespace starfox::platform_3ds
