package com.starfox.enhanced.quest;

import android.app.Activity;
import android.content.Context;
import java.util.concurrent.atomic.AtomicBoolean;

/** Blocking worker-thread entry. SDL Android initialization is the host's job. */
public final class QuestBridge {
    private QuestBridge() {}
    public static native void validateBundle(String path);
    public static native void prepareInput(String sourcePath, String destinationPath);
    public static native int run(Activity activity, Context applicationContext,
        String romPath, String symbolsPath, AtomicBoolean stopRequested);
}
