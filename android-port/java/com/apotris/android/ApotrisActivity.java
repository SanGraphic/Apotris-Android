package com.apotris.android;

import android.os.Bundle;

import org.libsdl.app.SDLActivity;

/**
 * Apotris Android uses a single native library: {@code libmain.so} (see {@code meson.build},
 * {@code shared_library('main', ...)}). SDL2 and the game are linked into that .so — the same
 * layout as the working {@code com.example.apotris} build on device (no {@code libSDL2.so}).
 * <p>
 * Stock {@link SDLActivity#getLibraries()} still requests {@code SDL2} then {@code main}, which
 * would require a separate {@code libSDL2.so}. This subclass keeps the working {@code libmain.so}
 * setup by loading only {@code main}.
 */
public class ApotrisActivity extends SDLActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        ApotrisAssetLoader.syncIfNeeded(this);
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }
}
