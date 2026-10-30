package org.xraymobile.soc;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

/**
 * Launches OpenXRay in Shadow of Chernobyl mode.
 *
 * Game data lives in the app's external files dir (Android/data/org.xraymobile.soc/files/).
 * The user only copies their own gamedata.db* archives there; engine resources
 * (GL shaders, fsgame.ltx) ship in the APK and are unpacked next to them.
 */
public class StalkerActivity extends SDLActivity {
    private static final String TAG = "StalkerActivity";
    private static final String STAMP_FILE = ".engine-resources";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Must finish before SDL starts the engine thread.
        installEngineResources();
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        // Order matters: dependencies first, the library holding SDL_main last.
        return new String[] { "c++_shared", "SDL2", "openal", "main" };
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus)
            enterImmersiveMode();
    }

    /** Hide the status and navigation bars; a swipe from the edge shows them temporarily. */
    @SuppressWarnings("deprecation")
    private void enterImmersiveMode() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            // Draw under the camera cutout too, like any fullscreen game
            WindowManager.LayoutParams attrs = getWindow().getAttributes();
            attrs.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(attrs);
        }
    }

    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        // The engine's window is resizable and sets no hint, which makes SDL request
        // FULL_USER and override the manifest; the game is landscape-only.
        super.setOrientationBis(w, h, resizable, "LandscapeLeft LandscapeRight");
    }

    @Override
    protected String[] getArguments() {
        File fsltx = new File(getExternalFilesDir(null), "fsgame.ltx");
        java.util.List<String> args = new java.util.ArrayList<>(
            java.util.Arrays.asList("-shoc", "-fsltx", fsltx.getAbsolutePath()));
        // Extra engine arguments for testing, e.g.
        //   adb shell am start -n org.xraymobile.soc/.StalkerActivity \
        //       --es args "-start server(all/single/alife/new) client(localhost)"
        String extra = getIntent() != null ? getIntent().getStringExtra("args") : null;
        if (extra != null && !extra.trim().isEmpty())
            args.addAll(java.util.Arrays.asList(extra.trim().split("\\s+")));
        return args.toArray(new String[0]);
    }

    /** Unpacks the bundled engine resources whenever the APK has been (re)installed. */
    private void installEngineResources() {
        File root = getExternalFilesDir(null);
        if (root == null) {
            Log.e(TAG, "external files dir unavailable");
            return;
        }
        String stamp;
        try {
            stamp = Long.toString(getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime);
        } catch (PackageManager.NameNotFoundException e) {
            stamp = "unknown";
        }
        File stampFile = new File(root, STAMP_FILE);
        if (stamp.equals(readSmallFile(stampFile)))
            return;

        try {
            copyAssetTree(getAssets(), "gamedata", new File(root, "gamedata"));
            File fsltx = new File(root, "fsgame.ltx");
            if (!fsltx.exists()) // keep user edits
                copyAsset(getAssets(), "fsgame.ltx", fsltx);
            writeSmallFile(stampFile, stamp);
            Log.i(TAG, "engine resources installed to " + root);
        } catch (IOException e) {
            Log.e(TAG, "failed to install engine resources", e);
        }
    }

    private static void copyAssetTree(AssetManager assets, String path, File dest) throws IOException {
        String[] children = assets.list(path);
        if (children == null || children.length == 0) {
            copyAsset(assets, path, dest);
            return;
        }
        if (!dest.isDirectory() && !dest.mkdirs())
            throw new IOException("cannot create " + dest);
        for (String child : children)
            copyAssetTree(assets, path + "/" + child, new File(dest, child));
    }

    private static void copyAsset(AssetManager assets, String path, File dest) throws IOException {
        try (InputStream in = assets.open(path); OutputStream out = new FileOutputStream(dest)) {
            byte[] buffer = new byte[64 * 1024];
            int n;
            while ((n = in.read(buffer)) > 0)
                out.write(buffer, 0, n);
        }
    }

    private static String readSmallFile(File file) {
        try (InputStream in = new java.io.FileInputStream(file)) {
            byte[] data = new byte[64];
            int n = in.read(data);
            return n > 0 ? new String(data, 0, n) : "";
        } catch (IOException e) {
            return "";
        }
    }

    private static void writeSmallFile(File file, String text) throws IOException {
        try (OutputStream out = new FileOutputStream(file)) {
            out.write(text.getBytes());
        }
    }
}
