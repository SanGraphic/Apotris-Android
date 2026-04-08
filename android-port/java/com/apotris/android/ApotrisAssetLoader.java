package com.apotris.android;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.util.Scanner;

/**
 * Copies bundled game files from {@code assets/apotris_game_assets/} into app internal storage
 * ({@code files/game_assets/}) so native code can use {@code fopen} / {@code opendir} (SoLoud,
 * shaders). APK {@code assets/} are not a real filesystem path.
 */
public final class ApotrisAssetLoader {
    private static final String TAG = "ApotrisAssets";
    private static final String ASSET_PREFIX = "apotris_game_assets";
    private static final String OUT_DIR = "game_assets";
    /** Increment when the packaged tree or sync logic changes (forces a full re-copy). */
    private static final int ASSET_PACK_VERSION = 1;

    private ApotrisAssetLoader() {}

    public static void syncIfNeeded(Context ctx) {
        File root = new File(ctx.getFilesDir(), OUT_DIR);
        File stamp = new File(root, ".apotris_assets_version");
        try {
            int current = -1;
            if (stamp.isFile()) {
                try (Scanner s = new Scanner(stamp, "UTF-8")) {
                    if (s.hasNextInt()) {
                        current = s.nextInt();
                    }
                }
            }
            if (current == ASSET_PACK_VERSION && root.isDirectory() && !isEmptyDir(root)) {
                return;
            }
            deleteRecursiveQuiet(root);
            AssetManager am = ctx.getAssets();
            String[] top = am.list(ASSET_PREFIX);
            if (top == null || top.length == 0) {
                Log.w(TAG, "No bundled assets at " + ASSET_PREFIX + " (run build.ps1 to sync main/apotris/assets)");
                root.mkdirs();
                try (PrintWriter pw = new PrintWriter(stamp, "UTF-8")) {
                    pw.print(ASSET_PACK_VERSION);
                }
                return;
            }
            root.mkdirs();
            copyTree(am, ASSET_PREFIX, root);
            try (PrintWriter pw = new PrintWriter(stamp, "UTF-8")) {
                pw.print(ASSET_PACK_VERSION);
            }
        } catch (IOException e) {
            Log.e(TAG, "Asset sync failed", e);
        }
    }

    private static boolean isEmptyDir(File dir) {
        String[] names = dir.list();
        return names == null || names.length == 0;
    }

    private static void deleteRecursiveQuiet(File f) {
        if (f == null || !f.exists()) {
            return;
        }
        if (f.isDirectory()) {
            File[] kids = f.listFiles();
            if (kids != null) {
                for (File c : kids) {
                    deleteRecursiveQuiet(c);
                }
            }
        }
        // noinspection ResultOfMethodCallIgnored
        f.delete();
    }

    private static void copyTree(AssetManager am, String assetPath, File outPath)
            throws IOException {
        String[] children = am.list(assetPath);
        if (children == null) {
            return;
        }
        if (children.length == 0) {
            try (InputStream in = am.open(assetPath);
                    FileOutputStream out = new FileOutputStream(outPath)) {
                File parent = outPath.getParentFile();
                if (parent != null) {
                    parent.mkdirs();
                }
                pump(in, out);
            } catch (IOException e) {
                if (!outPath.mkdirs()) {
                    throw new IOException("asset path neither file nor dir: " + assetPath, e);
                }
            }
            return;
        }
        if (!outPath.isDirectory() && !outPath.mkdirs()) {
            throw new IOException("mkdir failed: " + outPath);
        }
        for (String name : children) {
            copyTree(am, assetPath + "/" + name, new File(outPath, name));
        }
    }

    private static void pump(InputStream in, OutputStream out) throws IOException {
        byte[] buf = new byte[64 * 1024];
        int n;
        while ((n = in.read(buf)) > 0) {
            out.write(buf, 0, n);
        }
    }
}
