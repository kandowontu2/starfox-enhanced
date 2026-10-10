package com.starfox.enhanced.quest;

import java.io.File;
import java.nio.file.Files;
import java.util.Arrays;

/** Run with app_process on a developer device, using this APK's native libraries.
 * Include the installed APK in CLASSPATH as well as the test DEX: SDL's JNI_OnLoad
 * requires its Java classes even for an import-only test with no Activity.
 */
public final class QuestNativeImportTest {
    public static void main(String[] args) throws Exception {
        if (args.length != 3) throw new IllegalArgumentException("LIB_DIRECTORY ROM OUTPUT_BIN");
        for (String library : new String[]{"c++_shared", "SDL3", "starfox_quest"})
            System.load(new File(args[0], "lib" + library + ".so").getAbsolutePath());
        File rom = new File(args[1]), output = new File(args[2]);
        QuestBridge.prepareInput(rom.getAbsolutePath(), output.getAbsolutePath());
        QuestBridge.validateBundle(output.getAbsolutePath());
        byte[] expected = Files.readAllBytes(output.toPath());
        File fixture = new File(output.getParentFile(), "test-headered.sfc");
        File second = new File(output.getParentFile(), "test-second.bin");
        byte[] source = Files.readAllBytes(rom.toPath());
        byte[] headered = new byte[source.length + 512];
        System.arraycopy(source, 0, headered, 512, source.length);
        Files.write(fixture.toPath(), headered);
        QuestBridge.prepareInput(fixture.getAbsolutePath(), second.getAbsolutePath());
        if (!Arrays.equals(expected, Files.readAllBytes(second.toPath())))
            throw new AssertionError("Headered import differs");
        QuestBridge.prepareInput(output.getAbsolutePath(), second.getAbsolutePath());
        if (!Arrays.equals(expected, Files.readAllBytes(second.toPath())))
            throw new AssertionError("BIN reimport differs");
        source[source.length / 2] ^= 1;
        Files.write(fixture.toPath(), source);
        boolean rejected = false;
        try { QuestBridge.prepareInput(fixture.getAbsolutePath(), output.getAbsolutePath()); }
        catch (IllegalStateException expectedFailure) { rejected = true; }
        if (!rejected || !Arrays.equals(expected, Files.readAllBytes(output.toPath())))
            throw new AssertionError("Invalid import damaged previous output");
        Files.delete(fixture.toPath());
        Files.delete(second.toPath());
        System.out.println("Quest ARM64 JNI import passed: ROM, copier header, BIN, rejection preserves output");
    }
}
