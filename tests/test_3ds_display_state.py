"""Configuration regression for the native PICA -> Citro2D owner handoff.

Citro2D's C2D_Prepare deliberately does not set framebuffer blending. Its
foreground has zero-alpha holes; inheriting the native opaque ONE/ZERO mode
paints those holes black. This gate checks the real native restore function;
accuracy-enabled emulator capture remains the GPU integration check.
"""
from pathlib import Path
import re
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "platform/3ds/reference/display_3ds.cpp"


def restores_alpha(source):
    body = source.split("void Display3ds::prepare_draw_state() {", 1)[1].split("\n}", 1)[0]
    body = re.sub(r"//[^\n]*", "", body)
    call = re.search(r"C3D_AlphaBlend\((.*?)\);", body, re.S)
    return bool(call and re.sub(r"\s+", "", call[1]) ==
                "GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,"
                "GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA" and call.end() < body.index("C2D_Prepare();"))


class DisplayStateTests(unittest.TestCase):
    def test_native_handoff_restores_straight_alpha(self):
        self.assertTrue(restores_alpha(SOURCE.read_text()))

    def test_opaque_replacement_regression_is_detected(self):
        source = SOURCE.read_text()
        self.assertFalse(restores_alpha(source.replace(
            "GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA", "GPU_ONE, GPU_ZERO")))


if __name__ == "__main__":
    unittest.main()
