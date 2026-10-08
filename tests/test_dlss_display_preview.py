"""Negative controls for the captured DLSS menu-artwork regression checker."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import sys

try:
    import numpy as np
    from PIL import Image
except ImportError:
    np = Image = None

spec = importlib.util.spec_from_file_location(
    "dlss_preview", Path(__file__).resolve().parents[1] / "tools/check_dlss_display_preview.py")
preview = importlib.util.module_from_spec(spec) if np is not None else None
if preview is not None:
    spec.loader.exec_module(preview)


@unittest.skipIf(np is None, "Captured-image checks require Pillow and NumPy")
class PreviewArtworkTests(unittest.TestCase):
    def setUp(self):
        self.reference = np.full((224, 800, 3), (20, 30, 40), dtype=np.uint8)
        self.inks = [(255, 255, 255), (188, 209, 224), (255, 230, 67)]
        for x, color in zip((310, 330, 350), self.inks):
            self.reference[32:42, x:x+8] = color
        self.reference[190:196, 20:30] = (255, 255, 255)
        self.regions = [(.38, .10, .50, .35)]

    def measure(self, image):
        with tempfile.TemporaryDirectory(prefix="sfe-dlss-check-") as directory:
            output = Path(directory)
            for frame in (17, 21):
                Image.fromarray(image).save(output / f"lava.bmp.frame-{frame}.bmp")
            return preview.check(self.reference, output, range(17, 22, 4), self.regions)

    def test_all_authored_inks_are_checked(self):
        result = self.measure(self.reference)
        self.assertEqual(sorted(result["glyph_colors"]), sorted(map(list, self.inks)))
        self.assertEqual(result["glyph_pixels"], 240)
        self.assertEqual(result["text_max_rounding_error"], 0)

    def test_single_code_value_rounding_is_allowed(self):
        actual = self.reference.copy()
        actual[32:42, 330:338, 0] += 1
        self.assertEqual(self.measure(actual)["text_max_rounding_error"], 1)

    def test_blur_in_each_ink_is_rejected(self):
        for x, color in zip((310, 330, 350), self.inks):
            with self.subTest(ink=color):
                actual = self.reference.copy()
                actual[32, x] = (np.array(color) + np.array((20, 30, 40))) // 2
                with self.assertRaisesRegex(AssertionError, "moved/softened"):
                    self.measure(actual)

    def test_gray_only_movement_is_rejected(self):
        actual = self.reference.copy()
        actual[32:42, 330:338] = (20, 30, 40)
        actual[32:42, 331:339] = self.inks[1]
        with self.assertRaisesRegex(AssertionError, "moved/softened"):
            self.measure(actual)

    def test_changed_output_extent_is_rejected(self):
        with self.assertRaisesRegex(AssertionError, "extent changed"):
            self.measure(self.reference[:, :-1])

    def test_physical_model_variation_is_not_hidden_by_downsampling(self):
        reference = np.repeat(np.repeat(self.reference, 4, axis=0), 4, axis=1)
        moving = reference.copy()
        moving[440, 1600] = (255, 255, 255)
        with tempfile.TemporaryDirectory(prefix="sfe-dlss-check-") as directory:
            output = Path(directory)
            for frame, image in ((17, reference), (18, moving)):
                Image.fromarray(image).save(output / f"lava.bmp.frame-{frame}.bmp")
            result = preview.check(reference, output, (17, 18), self.regions)
        self.assertEqual(result["ship_physical_max_change_0_255"], 235)
        self.assertGreater(result["ship_physical_mean_change_0_255"], 0)
        self.assertLess(result["ship_mean_change_0_255"], .01)

    def test_stationary_model_blur_is_rejected_against_native_reference(self):
        reference = self.reference.copy()
        reference[100:110, 400:410] = (100, 180, 250)
        blurred = reference.copy()
        blurred[100, 400] = (60, 105, 145)
        with tempfile.TemporaryDirectory(prefix="sfe-dlss-check-") as directory:
            output = Path(directory)
            for frame in (17, 18):
                Image.fromarray(blurred).save(output / f"lava.bmp.frame-{frame}.bmp")
            with self.assertRaisesRegex(AssertionError, "models differ/blurred"):
                preview.check(reference, output, (17, 18), self.regions, native_models=True)
            for frame in (17, 18):
                Image.fromarray(reference).save(output / f"lava.bmp.frame-{frame}.bmp")
            result = preview.check(reference, output, (17, 18), self.regions, native_models=True)
            self.assertEqual(result["native_model_max_reference_error"], 0)
            self.assertEqual(result["ship_physical_max_change_0_255"], 0)

    def test_real_reconstruction_is_distinguished_from_native_bypass(self):
        actual=self.reference.copy()
        actual[100:104,400:404]=(120,150,180)
        measured=self.measure(actual)
        self.assertEqual(measured["model_changed_pixels_from_native"],16)
        self.assertGreater(measured["native_model_max_reference_error"],1)
        self.assertEqual(measured["ship_physical_max_change_0_255"],0)
        self.assertEqual(self.measure(self.reference)["model_changed_pixels_from_native"],0)

    def test_held_model_rejects_bypass_and_continued_wobble(self):
        reconstructed=self.reference.copy()
        reconstructed[100:104,400:404]=(120,150,180)
        with tempfile.TemporaryDirectory(prefix="sfe-dlss-check-") as directory:
            output=Path(directory)
            for frame in (17,18):
                Image.fromarray(self.reference).save(output / f"lava.bmp.frame-{frame}.bmp")
            with self.assertRaisesRegex(AssertionError,"native bypass"):
                preview.check(self.reference,output,(17,18),self.regions,held_models=True)
            for frame in (17,18):
                Image.fromarray(reconstructed).save(output / f"lava.bmp.frame-{frame}.bmp")
            preview.check(self.reference,output,(17,18),self.regions,held_models=True)
            moving=reconstructed.copy()
            moving[100,400]=(50,60,70)
            Image.fromarray(moving).save(output / "lava.bmp.frame-18.bmp")
            with self.assertRaisesRegex(AssertionError,"still moves"):
                preview.check(self.reference,output,(17,18),self.regions,held_models=True)

    def test_letterboxed_drawable_checks_the_content_not_black_bars(self):
        boxed = np.zeros((448, 800, 3), dtype=np.uint8)
        boxed[112:336] = self.reference
        with tempfile.TemporaryDirectory(prefix="sfe-dlss-check-") as directory:
            output = Path(directory)
            (output / "runtime.log").write_text(
                "presentation-drawable-capture: 800x448 raster=1600x448 viewport=0,112,800,224\n")
            extent, viewport = preview.drawable_viewport(output)
            self.assertEqual(extent, (800, 448))
            for frame in (17, 18):
                Image.fromarray(boxed).save(output / f"lava.bmp.frame-{frame}.bmp")
            result = preview.check(boxed, output, (17, 18), self.regions, viewport)
            self.assertEqual(result["glyph_pixels"], 240)
            self.assertEqual(result["text_max_rounding_error"], 0)
            blurred = boxed.copy()
            blurred[112+32, 330] = (20, 30, 40)
            Image.fromarray(blurred).save(output / "lava.bmp.frame-18.bmp")
            with self.assertRaisesRegex(AssertionError, "moved/softened"):
                preview.check(boxed, output, (17, 18), self.regions, viewport)

    def test_drawable_evidence_is_required_and_bounds_are_checked(self):
        with tempfile.TemporaryDirectory(prefix="sfe-dlss-check-") as directory:
            output = Path(directory)
            log = output / "runtime.log"
            log.write_text("Only an internal render capture\n")
            with self.assertRaisesRegex(AssertionError, "Missing actual drawable"):
                preview.drawable_viewport(output)
            for viewport in ("0,112,801,224", "0,112,800,400", "0,0,0,224", "-1,0,800,224"):
                with self.subTest(viewport=viewport):
                    log.write_text("presentation-drawable-capture: 800x448 raster=1600x448 viewport="+viewport+"\n")
                    with self.assertRaisesRegex(AssertionError, "Invalid drawable viewport"):
                        preview.drawable_viewport(output)


if __name__ == "__main__":
    if np is None:
        print("SKIP: captured-image checks require Pillow and NumPy")
        sys.exit(77)
    unittest.main()
