package com.starfox.enhanced.quest;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;
import android.util.AtomicFile;
import android.view.View;
import android.view.WindowManager;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.TextView;
import android.widget.Button;
import android.widget.LinearLayout;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.concurrent.atomic.AtomicBoolean;
import org.libsdl.app.SDL;

/** Development host: OpenXR owns the eye images, not an SDL window. */
public class QuestActivity extends Activity {
    private static final SessionGate sessions = new SessionGate();
    private final AtomicBoolean stop = new AtomicBoolean();
    private TextView status;
    private boolean sdlContextReady;
    private Button importBundle, start, stopButton;
    private boolean busy;
    private boolean pickerOffered;
    private boolean awaitingDownloadsPermission;
    protected boolean isImportPanel() {return false;}
    private static final int PICK_BUNDLE = 14;

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        pickerOffered = state != null && state.getBoolean("pickerOffered", false);
        awaitingDownloadsPermission = state != null && state.getBoolean("awaitingDownloadsPermission", false);
        // A VR-category activity has no visible Android panel until OpenXR
        // submits frames. Import must run in a separate ordinary 2D activity.
        if (!isImportPanel() && !inputsReady()) {
            startActivity(new Intent(this, AssetImportActivity.class));
            finish();
            return;
        }
        hideBars();
        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        int padding = Math.round(24 * getResources().getDisplayMetrics().density);
        panel.setPadding(padding, padding, padding, padding);
        status = new TextView(this);status.setTextSize(18);
        panel.addView(status);
        importBundle = button(panel, "Import ROM (.sfc/.smc) or Starfox-Assets.BIN", () -> pick(PICK_BUNDLE));
        button(panel, "Import ROM or BIN from Downloads", this::importDownloads);
        start = button(panel, "Start VR", this::startSession);
        stopButton = button(panel, "Stop VR", () -> {stop.set(true);stopButton.setEnabled(false);});
        setContentView(panel);
        refreshInputs();
        if (inputsReady() && !isImportPanel()) startSession();
        else status.setText("Select your own unmodified Star Fox/Starwing ROM (.sfc/.smc, extracted from ZIP) or Starfox-Assets.BIN. Original and EX assets are prepared on this headset. Then choose Start VR.");
    }

    private Button button(LinearLayout panel, String label, Runnable action) {
        Button button = new Button(this);button.setText(label);
        button.setOnClickListener(view -> action.run());panel.addView(button);return button;
    }
    private File inputFile(String name) {
        File directory = getExternalFilesDir(null);
        if (directory == null) throw new IllegalStateException("App storage unavailable");
        return new File(directory, name);
    }
    private boolean inputsReady() {
        try {return inputFile("Starfox-Assets.BIN").isFile();}
        catch (IllegalStateException error) {return false;}
    }
    private void refreshInputs() {
        importBundle.setEnabled(!busy);
        start.setEnabled(!busy && inputsReady());stopButton.setEnabled(false);
    }
    @SuppressWarnings("deprecation") private void pick(int request) {
        if (busy) return;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);intent.setType("*/*");
        try {startActivityForResult(intent, request);}
        catch (RuntimeException error) {
            status.setText("This headset has no usable file picker. Put Starfox.sfc, Starfox.smc or Starfox-Assets.BIN in Download, then choose Import from Downloads. Android will ask for file access; only the selected named file is read.");
        }
    }
    private void importDownloads() {
        if (busy) return;
        if (Build.VERSION.SDK_INT < 30) {
            status.setText("Downloads fallback requires Android 11 or later. Use the file picker or copy Starfox-Assets.BIN into the app's files folder.");
            return;
        }
        if (!Environment.isExternalStorageManager()) {
            awaitingDownloadsPermission = true;
            status.setText("Allow file access in Android settings, then return. Choose Starfox.sfc, Starfox.smc or Starfox-Assets.BIN from Download; you may revoke access after import.");
            try {
                startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName())));
            } catch (RuntimeException error) {
                awaitingDownloadsPermission = false;
                status.setText("Cannot open file-access settings. Copy Starfox-Assets.BIN into Android/data/" + getPackageName() + "/files instead.");
            }
            return;
        }
        File downloads = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        java.util.ArrayList<String> candidates = new java.util.ArrayList<>();
        for (String name : new String[]{"Starfox.sfc", "Starfox.smc", "Starfox-Assets.BIN"}) {
            if (new File(downloads, name).isFile()) candidates.add(name);
        }
        if (candidates.isEmpty()) {
            status.setText("Not found: Download/Starfox.sfc, Starfox.smc or Starfox-Assets.BIN. Extract your ROM from ZIP, rename it Starfox.sfc and copy it to Download.");
            return;
        }
        if (candidates.size() == 1) importSelected(Uri.fromFile(new File(downloads, candidates.get(0))));
        else new AlertDialog.Builder(this).setTitle("Choose input")
            .setItems(candidates.toArray(new String[0]), (dialog, index) ->
                importSelected(Uri.fromFile(new File(downloads, candidates.get(index)))))
            .setNegativeButton("Cancel", null).show();
    }
    @Override @SuppressWarnings("deprecation") protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_BUNDLE
            || result != RESULT_OK || data == null || data.getData() == null) return;
        importSelected(data.getData());
    }
    private void importSelected(final Uri uri) {
        if (busy) return;
        final SessionGate.Lease lease = sessions.tryAcquire();
        if (lease == null) {status.setText("A VR session or import is still finishing. Please try again.");return;}
        final String name = "Starfox-Assets.BIN";
        busy = true;refreshInputs();status.setText("Checking input and preparing Original + EX assets...");
        try {
            new Thread(() -> {
                String message;
                File staging = null, prepared = null;
                try {
                    staging = inputFile(name + ".import");
                    prepared = inputFile(name + ".prepared");
                    AtomicFile target = new AtomicFile(staging);
                    FileOutputStream output = null;
                    try {
                        try (InputStream input = getContentResolver().openInputStream(uri)) {
                            if (input == null) throw new java.io.IOException("Selected file cannot be opened");
                            output = target.startWrite();
                            InputCopy.copy(input, output, 64L * 1024 * 1024);
                        }
                        target.finishWrite(output);output = null;
                    } finally {if (output != null) target.failWrite(output);}
                    System.loadLibrary("SDL3");System.loadLibrary("starfox_quest");
                    QuestBridge.prepareInput(staging.getAbsolutePath(), prepared.getAbsolutePath());
                    AtomicFile installed = new AtomicFile(inputFile(name));
                    output = null;
                    try {
                        output = installed.startWrite();
                        try (InputStream input = new java.io.FileInputStream(prepared)) {
                            InputCopy.copy(input, output, 64L * 1024 * 1024);
                        }
                        installed.finishWrite(output);output = null;
                    } finally {if (output != null) installed.failWrite(output);}
                    message = "Imported and validated " + name + ". Original and EX are ready.";
                } catch (Throwable error) {message = "Import failed; previous input retained: " + error;}
                finally {
                    if (staging != null) new AtomicFile(staging).delete();
                    if (prepared != null) prepared.delete();
                    lease.close(() -> {});
                }
                final String resultMessage = message;
                runOnUiThread(() -> {if (!isDestroyed()) {busy = false;refreshInputs();status.setText(resultMessage);}});
            }, "StarFox-Quest-Import").start();
        } catch (Throwable error) {
            lease.close(() -> {});busy = false;refreshInputs();status.setText("Import could not start: " + error);
        }
    }

    private void startSession() {
        if (busy) return;
        if (isImportPanel()) {
            if (!inputsReady()) return;
            startActivity(new Intent(this, QuestActivity.class));
            finish();
            return;
        }
        final SessionGate.Lease lease = sessions.tryAcquire();
        if (lease == null) {
            status.setText("The previous VR session or import is still closing. Please try Start VR shortly.");
            return;
        }
        busy = true;stop.set(false);refreshInputs();stopButton.setEnabled(true);
        status.setText("Starting experimental Quest renderer...");
        try {
            File directory = getExternalFilesDir(null);
            if (directory == null) throw new IllegalStateException("App storage unavailable");
            File rom = new File(directory, "Starfox-Assets.BIN");
            if (!rom.isFile())
                throw new IllegalStateException("Import Starfox-Assets.BIN first");
            System.loadLibrary("SDL3");
            System.loadLibrary("starfox_quest");
            SDL.setupJNI();
            SDL.initialize();
            SDL.setContext(this);
            sdlContextReady = true;
            new Thread(() -> {
                String result;
                try {
                    int code = QuestBridge.run(this, getApplicationContext(),
                        rom.getAbsolutePath(), null, stop);
                    result = "VR session ended (code " + code + ").";
                } catch (Throwable error) {
                    result = "VR startup failed: " + error;
                } finally {
                    // JNI has returned and native GPU/audio resources are gone.
                    // Release before posting UI work, which may run much later.
                    lease.close(this::releaseSdlContext);
                }
                final String message = result;
                runOnUiThread(() -> {
                    if (!isDestroyed()) {busy = false;refreshInputs();status.setText(message);}
                });
            }, "StarFox-Quest-Render").start();
        } catch (Throwable error) {
            // Also handles Thread.start failure after SDL acquired this Activity.
            lease.close(this::releaseSdlContext);
            busy = false;refreshInputs();
            status.setText(error.toString());
        }
    }

    private void releaseSdlContext() {
        if (sdlContextReady) {
            SDL.setContext(null);
            sdlContextReady = false;
        }
    }

    @SuppressWarnings("deprecation") private void hideBars() {
        Window window = getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        window.clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        WindowManager.LayoutParams attributes = window.getAttributes();
        attributes.layoutInDisplayCutoutMode = Build.VERSION.SDK_INT >= 30
            ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
            : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        window.setAttributes(attributes);
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false);
            // Quest's PhoneWindow dereferences its decor in getInsetsController
            // before setContentView. Obtain the decor first; an unattached view
            // safely returns null and onResume retries after attachment.
            WindowInsetsController controller = window.getDecorView().getWindowInsetsController();
            if (controller != null) {
                controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                controller.hide(WindowInsets.Type.systemBars());
            }
        } else {
            window.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }
    @Override protected void onResume() {
        super.onResume();
        if (isFinishing()) return;
        if (awaitingDownloadsPermission) {
            awaitingDownloadsPermission = false;
            if (Build.VERSION.SDK_INT >= 30 && Environment.isExternalStorageManager()) importDownloads();
            else status.setText("File access was not granted. You can retry Import from Downloads or use the app-folder copy method.");
        }
        hideBars();
        // The insets controller may not exist until the decor attaches. Also
        // restore immersive mode after returning from a document provider.
        getWindow().getDecorView().post(() -> {if (!isDestroyed()) hideBars();});
        if (isImportPanel() && !inputsReady() && !pickerOffered) {
            pickerOffered = true;
            getWindow().getDecorView().post(() -> {
                if (!isDestroyed() && !isFinishing()) pick(PICK_BUNDLE);
            });
        }
    }
    @Override protected void onSaveInstanceState(Bundle state) {
        state.putBoolean("pickerOffered", pickerOffered);
        state.putBoolean("awaitingDownloadsPermission", awaitingDownloadsPermission);
        super.onSaveInstanceState(state);
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused) hideBars();
    }
    @Override protected void onDestroy() {
        stop.set(true); // Never block the Android UI thread waiting for a GPU fence.
        super.onDestroy();
    }
}
