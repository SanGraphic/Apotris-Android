package com.apotris.android;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.ImageDecoder;
import android.graphics.drawable.AnimationDrawable;
import android.graphics.drawable.AnimatedImageDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.core.splashscreen.SplashScreen;
import androidx.core.splashscreen.SplashScreenViewProvider;

/**
 * Restored from the shipped {@code com.example.apotris} APK (splash + transition to the SDL game).
 * Uses the same assets under {@code res/drawable-nodpi/} as the decompiled build.
 */
public final class SplashActivity extends Activity {

    private static final long SPLASH_REMOVE_DELAY_MS = 2000L;

    private boolean ready;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        SplashScreen splashScreen = SplashScreen.installSplashScreen(this);
        super.onCreate(savedInstanceState);
        splashScreen.setKeepOnScreenCondition(() -> !ready);
        splashScreen.setOnExitAnimationListener(this::onSplashExit);
        startActivity(new Intent(this, ApotrisActivity.class));
        new Handler(Looper.getMainLooper()).post(() -> ready = true);
    }

    private void onSplashExit(SplashScreenViewProvider splashScreenView) {
        View iconView = splashScreenView.getIconView();
        if (iconView != null) {
            iconView.setVisibility(View.INVISIBLE);
        }
        FrameLayout container = new FrameLayout(this);
        container.setBackgroundColor(Color.BLACK);
        ImageView imageView = new ImageView(this);
        imageView.setScaleType(ImageView.ScaleType.FIT_CENTER);
        container.addView(imageView, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        getWindow().addContentView(container, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            try {
                ImageDecoder.Source source = ImageDecoder.createSource(getResources(), R.drawable.splash_animated);
                Drawable drawable = ImageDecoder.decodeDrawable(source);
                imageView.setImageDrawable(drawable);
                if (drawable instanceof AnimatedImageDrawable) {
                    ((AnimatedImageDrawable) drawable).start();
                }
            } catch (Exception ignored) {
                fallbackAnimation(imageView);
            }
        } else {
            fallbackAnimation(imageView);
        }

        new Handler(Looper.getMainLooper()).postDelayed(() -> {
            splashScreenView.remove();
            finish();
        }, SPLASH_REMOVE_DELAY_MS);
    }

    private void fallbackAnimation(ImageView imageView) {
        imageView.setImageResource(R.drawable.splash_animation);
        Drawable d = imageView.getDrawable();
        if (d instanceof AnimationDrawable) {
            ((AnimationDrawable) d).start();
        }
    }
}
