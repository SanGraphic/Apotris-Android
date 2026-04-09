package com.apotris.android

import com.swordfish.radialgamepad.library.config.ButtonConfig
import com.swordfish.radialgamepad.library.config.CrossConfig
import com.swordfish.radialgamepad.library.config.PrimaryDialConfig
import com.swordfish.radialgamepad.library.config.RadialGamePadConfig
import com.swordfish.radialgamepad.library.config.SecondaryDialConfig
import com.swordfish.radialgamepad.library.haptics.HapticConfig

object GamepadLayoutFactory {

    fun landscapeDefaults(): Pair<RadialGamePadConfig, RadialGamePadConfig> {
        val left = RadialGamePadConfig(
            sockets = 12,
            primaryDial = PrimaryDialConfig.Cross(CrossConfig(id = 0)),
            secondaryDials = listOf(
                SecondaryDialConfig.SingleButton(4, 1f, 0f, ButtonConfig(VirtualGamepadMapping.BTN_L, "L")),
                SecondaryDialConfig.SingleButton(2, 1f, 0f, ButtonConfig(VirtualGamepadMapping.BTN_SELECT, "SELECT"))
            )
        )
        val right = RadialGamePadConfig(
            sockets = 12,
            primaryDial = PrimaryDialConfig.PrimaryButtons(
                dials = listOf(
                    ButtonConfig(VirtualGamepadMapping.BTN_A, "A"),
                    ButtonConfig(VirtualGamepadMapping.BTN_B, "B")
                ),
                rotationInDegrees = -30f
            ),
            secondaryDials = listOf(
                SecondaryDialConfig.SingleButton(2, 1f, 0f, ButtonConfig(VirtualGamepadMapping.BTN_R, "R")),
                SecondaryDialConfig.SingleButton(4, 1f, 0f, ButtonConfig(VirtualGamepadMapping.BTN_START, "START"))
            )
        )
        return left to right
    }

    fun portraitConfigs(appear: PortraitGamepadAppear): Pair<RadialGamePadConfig, RadialGamePadConfig> {
        val theme = appear.toRadialTheme()
        val icons = appear.usePixelIcons()
        val crossShape = appear.crossShape()

        val lBtn: (Int, String, String) -> ButtonConfig = { id, text, cd ->
            if (icons) {
                val res = when (id) {
                    VirtualGamepadMapping.BTN_L -> R.drawable.gamepad_pixel_l
                    VirtualGamepadMapping.BTN_SELECT -> R.drawable.gamepad_pixel_select
                    else -> error("unknown id")
                }
                ButtonConfig(id, label = null, iconId = res, contentDescription = cd)
            } else {
                ButtonConfig(id, text, contentDescription = cd)
            }
        }
        val rSide: (Int, String, String) -> ButtonConfig = { id, text, cd ->
            if (icons) {
                val res = when (id) {
                    VirtualGamepadMapping.BTN_R -> R.drawable.gamepad_pixel_r
                    VirtualGamepadMapping.BTN_START -> R.drawable.gamepad_pixel_start
                    else -> error("unknown id")
                }
                ButtonConfig(id, label = null, iconId = res, contentDescription = cd)
            } else {
                ButtonConfig(id, text, contentDescription = cd)
            }
        }

        val ab: (Int, String) -> ButtonConfig = { id, letter ->
            if (icons) {
                val res = if (id == VirtualGamepadMapping.BTN_A) R.drawable.gamepad_pixel_a else R.drawable.gamepad_pixel_b
                ButtonConfig(id, label = null, iconId = res, contentDescription = letter)
            } else {
                ButtonConfig(id, letter, contentDescription = letter)
            }
        }

        val left = RadialGamePadConfig(
            sockets = 12,
            primaryDial = PrimaryDialConfig.Cross(
                CrossConfig(id = 0, shape = crossShape, theme = theme)
            ),
            secondaryDials = listOf(
                SecondaryDialConfig.SingleButton(4, 1f, 0f, lBtn(VirtualGamepadMapping.BTN_L, "L", "L shoulder")),
                SecondaryDialConfig.SingleButton(2, 1f, 0f, lBtn(VirtualGamepadMapping.BTN_SELECT, "SELECT", "Select"))
            ),
            haptic = HapticConfig.PRESS,
            theme = theme
        )
        val right = RadialGamePadConfig(
            sockets = 12,
            primaryDial = PrimaryDialConfig.PrimaryButtons(
                dials = listOf(
                    ab(VirtualGamepadMapping.BTN_A, "A"),
                    ab(VirtualGamepadMapping.BTN_B, "B")
                ),
                rotationInDegrees = -30f,
                theme = theme
            ),
            secondaryDials = listOf(
                SecondaryDialConfig.SingleButton(2, 1f, 0f, rSide(VirtualGamepadMapping.BTN_R, "R", "R shoulder")),
                SecondaryDialConfig.SingleButton(4, 1f, 0f, rSide(VirtualGamepadMapping.BTN_START, "START", "Start"))
            ),
            haptic = HapticConfig.PRESS,
            theme = theme
        )
        return left to right
    }
}
