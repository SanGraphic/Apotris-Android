package com.apotris.android

import android.content.Context
import android.graphics.Color
import com.swordfish.radialgamepad.library.config.CrossConfig
import com.swordfish.radialgamepad.library.config.RadialGamePadTheme

/**
 * Portrait-only gamepad look + global scale/alpha. Stored in [PREFS_NAME] alongside overlay_layout keys.
 */
data class PortraitGamepadAppear(
    val presetId: String = GamepadThemePresets.ID_CLASSIC,
    val customPrimary: Int = Color.parseColor("#9B6DFF"),
    val customPadBackground: Int = Color.parseColor("#2A1A45"),
    val customStrokeDp: Float = 2.5f,
    val globalScale: Float = 1f,
    val alpha: Float = 1f
) {
    fun toRadialTheme(): RadialGamePadTheme = GamepadThemePresets.themeForPreset(this)

    fun usePixelIcons(): Boolean = GamepadThemePresets.usePixelIcons(presetId)

    fun crossShape(): CrossConfig.Shape = GamepadThemePresets.crossShape(presetId)
}

object PortraitGamepadPrefs {

    const val PREFS_NAME = "overlay_layout"

    private const val KEY_PRESET = "portrait_preset_id"
    private const val KEY_CUSTOM_PRIMARY = "portrait_custom_primary"
    private const val KEY_CUSTOM_PAD_BG = "portrait_custom_pad_bg"
    private const val KEY_CUSTOM_STROKE_DP = "portrait_custom_stroke_dp"
    private const val KEY_GLOBAL_SCALE = "portrait_global_scale"
    private const val KEY_ALPHA = "portrait_overlay_alpha"

    fun load(context: Context): PortraitGamepadAppear {
        val p = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return PortraitGamepadAppear(
            presetId = p.getString(KEY_PRESET, GamepadThemePresets.ID_CLASSIC)
                ?: GamepadThemePresets.ID_CLASSIC,
            customPrimary = p.getInt(KEY_CUSTOM_PRIMARY, Color.parseColor("#9B6DFF")),
            customPadBackground = p.getInt(KEY_CUSTOM_PAD_BG, Color.parseColor("#2A1A45")),
            customStrokeDp = p.getFloat(KEY_CUSTOM_STROKE_DP, 2.5f).coerceIn(1f, 5f),
            globalScale = p.getFloat(KEY_GLOBAL_SCALE, 1f).coerceIn(0.7f, 1.45f),
            alpha = p.getFloat(KEY_ALPHA, 1f).coerceIn(0.35f, 1f)
        )
    }

    fun save(context: Context, appear: PortraitGamepadAppear) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).edit()
            .putString(KEY_PRESET, appear.presetId)
            .putInt(KEY_CUSTOM_PRIMARY, appear.customPrimary)
            .putInt(KEY_CUSTOM_PAD_BG, appear.customPadBackground)
            .putFloat(KEY_CUSTOM_STROKE_DP, appear.customStrokeDp)
            .putFloat(KEY_GLOBAL_SCALE, appear.globalScale)
            .putFloat(KEY_ALPHA, appear.alpha)
            .apply()
    }
}
