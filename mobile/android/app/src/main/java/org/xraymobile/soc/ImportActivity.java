package org.xraymobile.soc;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

/**
 * Launcher: starts the game when the player's data is in place, otherwise asks for the game folder
 * (anything the system file picker can open: phone storage, SD card, USB drive) and copies the
 * gamedata.db* archives into the app's own folder, where the engine reads them.
 */
public class ImportActivity extends Activity {
    private static final String TAG = "ImportActivity";
    private static final int PICK_FOLDER = 1;
    // Present while an import is unfinished, so a half-copied set doesn't count as installed
    private static final String IMPORT_MARKER = ".import-incomplete";

    private TextView status;
    private ProgressBar progress;
    private Button pickButton;
    private volatile boolean copying;

    private static final class Archive {
        final Uri uri;
        final String name;
        final long size;

        Archive(Uri uri, String name, long size) {
            this.uri = uri;
            this.name = name;
            this.size = size;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (hasGameData(this)) {
            startGame();
            return;
        }
        buildUi();
    }

    static boolean hasGameData(Activity activity) {
        File root = activity.getExternalFilesDir(null);
        return root != null && new File(root, "gamedata.db0").isFile() && !new File(root, IMPORT_MARKER).exists();
    }

    private void startGame() {
        startActivity(new Intent(this, StalkerActivity.class));
        finish();
    }

    private void buildUi() {
        int pad = (int) (24 * getResources().getDisplayMetrics().density);

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(pad * 2, pad, pad * 2, pad);
        layout.setBackgroundColor(Color.BLACK);

        TextView title = new TextView(this);
        title.setText("S.T.A.L.K.E.R.: Shadow of Chernobyl");
        title.setTextColor(Color.WHITE);
        title.setTextSize(22);
        title.setGravity(Gravity.CENTER);
        layout.addView(title);

        status = new TextView(this);
        status.setText("This app runs your own copy of the game. Copy the game folder (or just its "
            + "gamedata.db0 … gamedata.dbd files) to this device, an SD card or a USB drive, then pick "
            + "that folder. About 5.6 GB is copied into the app; the copy you picked can be deleted after.");
        status.setTextColor(Color.LTGRAY);
        status.setTextSize(15);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, pad, 0, pad);
        layout.addView(status);

        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(1000);
        progress.setVisibility(View.GONE);
        layout.addView(progress, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT));

        pickButton = new Button(this);
        pickButton.setText("Choose game folder");
        pickButton.setOnClickListener(v -> pickFolder());
        layout.addView(pickButton);

        setContentView(layout);
    }

    private void pickFolder() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        startActivityForResult(intent, PICK_FOLDER);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_FOLDER || resultCode != RESULT_OK || data == null || data.getData() == null)
            return;

        Uri tree = data.getData();
        List<Archive> archives = findArchives(tree);
        if (archives.isEmpty()) {
            status.setText("No gamedata.db* files in that folder or the folders directly inside it. "
                + "Pick the folder the game is installed in.");
            return;
        }
        startCopy(archives);
    }

    /** gamedata.db* in the picked folder, or else in its direct subfolders. */
    private List<Archive> findArchives(Uri tree) {
        String rootId = DocumentsContract.getTreeDocumentId(tree);
        List<Archive> found = new ArrayList<>();
        List<String> subfolders = new ArrayList<>();
        listChildren(tree, rootId, found, subfolders);
        for (int i = 0; found.isEmpty() && i < subfolders.size(); ++i)
            listChildren(tree, subfolders.get(i), found, null);
        return found;
    }

    private void listChildren(Uri tree, String parentId, List<Archive> archives, List<String> subfolders) {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, parentId);
        String[] columns = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_SIZE,
        };
        try (Cursor c = getContentResolver().query(children, columns, null, null, null)) {
            while (c != null && c.moveToNext()) {
                String id = c.getString(0);
                String name = c.getString(1);
                String mime = c.getString(2);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                    if (subfolders != null)
                        subfolders.add(id);
                } else if (name != null && name.toLowerCase(Locale.ROOT).matches("gamedata\\.db[0-9a-z]")) {
                    archives.add(new Archive(DocumentsContract.buildDocumentUriUsingTree(tree, id),
                        name.toLowerCase(Locale.ROOT), c.getLong(3)));
                }
            }
        } catch (RuntimeException e) {
            Log.e(TAG, "listing " + parentId + " failed", e);
        }
    }

    private void startCopy(List<Archive> archives) {
        File root = getExternalFilesDir(null);
        if (root == null) {
            status.setText("App storage is unavailable.");
            return;
        }

        long total = 0, needed = 0;
        for (Archive a : archives) {
            total += a.size;
            File dest = new File(root, a.name);
            if (!dest.isFile() || dest.length() != a.size)
                needed += a.size;
        }
        if (root.getUsableSpace() < needed) {
            status.setText(String.format(Locale.ROOT, "Not enough free space: %.1f GB needed, %.1f GB free.",
                needed / 1e9, root.getUsableSpace() / 1e9));
            return;
        }

        copying = true;
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        pickButton.setEnabled(false);
        progress.setVisibility(View.VISIBLE);
        final long totalBytes = total;
        new Thread(() -> copyAll(root, archives, totalBytes), "gamedata-import").start();
    }

    private void copyAll(File root, List<Archive> archives, long total) {
        File marker = new File(root, IMPORT_MARKER);
        try {
            marker.createNewFile();
            ContentResolver resolver = getContentResolver();
            byte[] buffer = new byte[1 << 20];
            long done = 0;
            long lastUpdate = 0;
            for (Archive a : archives) {
                File dest = new File(root, a.name);
                if (dest.isFile() && dest.length() == a.size) { // already there from an earlier attempt
                    done += a.size;
                    continue;
                }
                File partial = new File(root, a.name + ".part");
                try (InputStream in = resolver.openInputStream(a.uri);
                     OutputStream out = new FileOutputStream(partial)) {
                    if (in == null)
                        throw new IOException("cannot open " + a.name);
                    int n;
                    while ((n = in.read(buffer)) > 0) {
                        out.write(buffer, 0, n);
                        done += n;
                        long now = System.currentTimeMillis();
                        if (now - lastUpdate > 200) {
                            lastUpdate = now;
                            final long d = done;
                            final String file = a.name;
                            runOnUiThread(() -> showProgress(file, d, total));
                        }
                    }
                }
                if (!partial.renameTo(dest))
                    throw new IOException("cannot finish " + a.name);
            }
            marker.delete();
            runOnUiThread(this::startGame);
        } catch (IOException e) {
            Log.e(TAG, "import failed", e);
            runOnUiThread(() -> {
                status.setText("Copying failed: " + e.getMessage() + ". Pick the folder again to resume.");
                pickButton.setEnabled(true);
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            });
        } finally {
            copying = false;
        }
    }

    private void showProgress(String file, long done, long total) {
        progress.setProgress((int) (1000 * done / Math.max(total, 1)));
        status.setText(String.format(Locale.ROOT, "Copying %s … %.1f / %.1f GB", file, done / 1e9, total / 1e9));
    }

    @Override
    public void onBackPressed() {
        if (!copying) // leaving mid-copy would just restart it on the next launch
            super.onBackPressed();
    }
}
