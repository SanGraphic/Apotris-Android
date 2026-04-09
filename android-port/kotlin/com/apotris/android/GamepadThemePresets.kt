package com.apotris.android

import android.graphics.Color
import com.swordfish.radialgamepad.library.config.CrossConfig
import com.swordfish.radialgamepad.library.config.RadialGamePadTheme
import com.swordfish.radialgamepad.library.utils.Constants
import kotlin.math.roundToInt

/**
 * Named looks for portrait RadialGamePad. "Pixel / blocky" uses vector icons instead of text labels
 * (the library does not expose custom label typefaces).
 */
object GamepadThemePresets {

    const val ID_CLASSIC = "classic"
    const val ID_APOTRIS = "apotris"
    const val ID_GAME_BOY = "game_boy"
    const val ID_ARCADE = "arcade"
    const val ID_NEON = "neon"
    const val ID_OLED_GLASS = "oled_glass"
    const val ID_HIGH_CONTRAST = "high_contrast"
    const val ID_PIXEL_BLOCKS = "pixel_blocks"
    const val ID_SUNSET = "sunset"
    const val ID_CUSTOM = "custom"

    val PRESET_DISPLAY_ORDER = listOf(
        ID_CLASSIC,
        ID_APOTRIS,
        ID_GAME_BOY,
        ID_ARCADE,
        ID_NEON,
        ID_OLED_GLASS,
        ID_HIGH_CONTRAST,
        ID_PIXEL_BLOCKS,
        ID_SUNSET,
        ID_CUSTOM
    )

    fun displayName(id: String): String = when (id) {
        ID_CLASSIC -> "Classic grey"
        ID_APOTRIS -> "Apotris purple"
        ID_GAME_BOY -> "Handheld green"
        ID_ARCADE -> "Arcade cab"
        ID_NEON -> "Neon grid"
        ID_OLED_GLASS -> "OLED glass"
        ID_HIGH_CONTRAST -> "High contrast"
        ID_PIXEL_BLOCKS -> "8-bit blocks (icons)"
        ID_SUNSET -> "Sunset"
        ID_CUSTOM -> "Custom colors"
        else -> id
    }

    fun usePixelIcons(presetId: String): Boolean = presetId == ID_PIXEL_BLOCKS

    fun crossShape(presetId: String): CrossConfig.Shape = when (presetId) {
        ID_GAME_BOY, ID_PIXEL_BLOCKS, ID_ARCADE -> CrossConfig.Shape.CIRCLE
        else -> CrossConfig.Shape.STANDARD
    }

    fun themeForPreset(appear: PortraitGamepadAppear): RadialGamePadTheme {
        if (appear.presetId == ID_CUSTOM) {
            return customTheme(
                appear.customPrimary,
                appear.customPadBackground,
                appear.customStrokeDp
            )
        }
        return when (appear.presetId) {
            ID_CLASSIC -> classicFallbackTheme()
            ID_APOTRIS -> themeFromBase(
                face = Color.parseColor("#6B4EAA"),
                pressed = Color.parseColor("#9B6DFF"),
                text = Color.parseColor("#FFF5E6"),
                ring = Color.parseColor("#3D2B66"),
                strokeDp = 2.5f
            )
            ID_GAME_BOY -> themeFromBase(
                face = Color.parseColor("#8BAC0F"),
                pressed = Color.parseColor("#C4CF6A"),
                text = Color.parseColor("#0F380F"),
                ring = Color.parseColor("#306230"),
                strokeDp = 3f
            )
            ID_ARCADE -> themeFromBase(
                face = Color.parseColor("#C62828"),
                pressed = Color.parseColor("#FF7043"),
                text = Color.parseColor("#FFECB3"),
                ring = Color.parseColor("#4E342E"),
                strokeDp = 2.5f
            )
            ID_NEON -> themeFromBase(
                face = Color.parseColor("#00E5FF"),
                pressed = Color.parseColor("#FF00E5"),
                text = Color.parseColor("#0D0221"),
                ring = Color.parseColor("#1A0D40"),
                strokeDp = 2f,
                strokeFace = Color.parseColor("#7C4DFF")
            )
            ID_OLED_GLASS -> RadialGamePadTheme(
                normalColor = Color.argb(90, 180, 180, 200),
                pressedColor = Color.argb(200, 120, 140, 220),
                simulatedColor = Color.argb(90, 180, 180, 200),
                textColor = Color.argb(240, 255, 255, 255),
                backgroundColor = Color.argb(40, 40, 50, 70),
                lightColor = Color.argb(60, 100, 100, 130),
                strokeWidthDp = 1.5f,
                normalStrokeColor = Color.argb(120, 255, 255, 255),
                lightStrokeColor = Color.argb(80, 200, 200, 255),
                backgroundStrokeColor = Color.argb(60, 255, 255, 255)
            )
            ID_HIGH_CONTRAST -> RadialGamePadTheme(
                normalColor = Color.parseColor("#FFFFFF"),
                pressedColor = Color.parseColor("#FFEB3B"),
                simulatedColor = Color.parseColor("#FFFFFF"),
                textColor = Color.parseColor("#000000"),
                backgroundColor = Color.parseColor("#212121"),
                lightColor = Color.parseColor("#BDBDBD"),
                strokeWidthDp = 3f,
                normalStrokeColor = Color.parseColor("#000000"),
                lightStrokeColor = Color.parseColor("#000000"),
                backgroundStrokeColor = Color.parseColor("#FFFFFF")
            )
            ID_PIXEL_BLOCKS -> themeFromBase(
                face = Color.parseColor("#5D4037"),
                pressed = Color.parseColor("#8D6E63"),
                text = Color.parseColor("#FFCC80"),
                ring = Color.parseColor("#3E2723"),
                strokeDp = 3.5f,
                strokeFace = Color.parseColor("#212121")
            )
            ID_SUNSET -> themeFromBase(
                face = Color.parseColor("#FF7043"),
                pressed = Color.parseColor("#FFCA28"),
                text = Color.parseColor("#3E2723"),
                ring = Color.parseColor("#BF360C"),
                strokeDp = 2f
            )
            else -> classicFallbackTheme()
        }
    }

    /** Fallback when [PortraitGamepadAppear.presetId] is unknown (corrupt prefs). */
    private fun classicFallbackTheme(): RadialGamePadTheme = RadialGamePadTheme(
        normalColor = Constants.DEFAULT_COLOR_NORMAL,
        pressedColor = Constants.DEFAULT_COLOR_PRESSED,
        simulatedColor = Constants.DEFAULT_COLOR_NORMAL,
        textColor = Constants.DEFAULT_COLOR_TEXT,
        backgroundColor = Constants.DEFAULT_COLOR_BACKGROUND,
        lightColor = Constants.DEFAULT_COLOR_LIGHT,
        strokeWidthDp = 2f,
        normalStrokeColor = Constants.DEFAULT_COLOR_NORMAL_STROKE,
        lightStrokeColor = Constants.DEFAULT_COLOR_LIGHT_STROKE,
        backgroundStrokeColor = Constants.DEFAULT_COLOR_BACKGROUND_STROKE
    )

    private fun customTheme(primary: Int, padBg: Int, strokeDp: Float): RadialGamePadTheme {
        val pressed = blendArgb(primary, Color.BLACK, 0.38f)
        val text = if (luminance(primary) > 0.45f) Color.BLACK else Color.WHITE
        val light = blendArgb(primary, Color.WHITE, 0.35f)
        val stroke = blendArgb(primary, Color.BLACK, 0.55f)
        return RadialGamePadTheme(
            normalColor = primary,
            pressedColor = pressed,
            simulatedColor = primary,
            textColor = text,
            backgroundColor = padBg,
            lightColor = light,
            strokeWidthDp = strokeDp,
            normalStrokeColor = stroke,
            lightStrokeColor = blendArgb(stroke, Color.WHITE, 0.25f),
            backgroundStrokeColor = blendArgb(padBg, Color.WHITE, 0.2f)
        )
    }

    private fun themeFromBase(
        face: Int,
        pressed: Int,
        text: Int,
        ring: Int,
        strokeDp: Float,
        strokeFace: Int = blendArgb(face, Color.BLACK, 0.4f)
    ): RadialGamePadTheme {
        val light = blendArgb(face, Color.WHITE, 0.3f)
        return RadialGamePadTheme(
            normalColor = face,
            pressedColor = pressed,
            simulatedColor = face,
            textColor = text,
            backgroundColor = ring,
            lightColor = light,
            strokeWidthDp = strokeDp,
            normalStrokeColor = strokeFace,
            lightStrokeColor = blendArgb(strokeFace, Color.WHITE, 0.2f),
            backgroundStrokeColor = blendArgb(ring, Color.WHITE, 0.15f)
        )
    }

    private fun blendArgb(from: Int, to: Int, t: Float): Int {
        val r = (Color.red(from) * (1 - t) + Color.red(to) * t).roundToInt().coerceIn(0, 255)
        val g = (Color.green(from) * (1 - t) + Color.green(to) * t).roundToInt().coerceIn(0, 255)
        val b = (Color.blue(from) * (1 - t) + Color.blue(to) * t).roundToInt().coerceIn(0, 255)
        val a = (Color.alpha(from) * (1 - t) + Color.alpha(to) * t).roundToInt().coerceIn(0, 255)
        return Color.argb(a, r, g, b)
    }

    private fun luminance(c: Int): Float {
        val r = Color.red(c) / 255f
        val g = Color.green(c) / 255f
        val b = Color.blue(c) / 255f
        return 0.2126f * r + 0.7152f * g + 0.0722f * b
    }
}
