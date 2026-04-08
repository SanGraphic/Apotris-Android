package com.apotris.android

import android.app.Activity
import android.content.res.Configuration
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import com.swordfish.radialgamepad.library.RadialGamePad
import com.swordfish.radialgamepad.library.config.ButtonConfig
import com.swordfish.radialgamepad.library.config.CrossConfig
import com.swordfish.radialgamepad.library.config.PrimaryDialConfig
import com.swordfish.radialgamepad.library.config.RadialGamePadConfig
import com.swordfish.radialgamepad.library.config.SecondaryDialConfig
import com.swordfish.radialgamepad.library.event.Event
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.merge
import kotlinx.coroutines.flow.onEach
import org.libsdl.app.SDLActivity

/**
 * Virtual controls + physical controller visibility (parity with shipped com.example.apotris).
 */
class TouchOverlayManager(private val activity: Activity) {

    private val controllerMonitor = ControllerMonitor(activity)
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    private val landscapeEditor = OverlayEditor(activity, "landscape")
    private val portraitEditor = OverlayEditor(activity, "portrait")

    private companion object Btn {
        const val BTN_A = 0 // SDL_CONTROLLER_BUTTON_A
        const val BTN_B = 1
        const val BTN_SELECT = 4
        const val BTN_START = 6
        const val BTN_L = 9
        const val BTN_R = 10
        const val BTN_UP = 11
        const val BTN_DOWN = 12
        const val BTN_LEFT = 13
        const val BTN_RIGHT = 14
    }

    private val leftConfig = RadialGamePadConfig(
        sockets = 12,
        primaryDial = PrimaryDialConfig.Cross(CrossConfig(id = 0)),
        secondaryDials = listOf(
            SecondaryDialConfig.SingleButton(4, 1f, 0f, ButtonConfig(BTN_L, "L")),
            SecondaryDialConfig.SingleButton(2, 1f, 0f, ButtonConfig(BTN_SELECT, "SELECT"))
        )
    )

    private val rightConfig = RadialGamePadConfig(
        sockets = 12,
        primaryDial = PrimaryDialConfig.PrimaryButtons(
            dials = listOf(
                ButtonConfig(BTN_A, "A"),
                ButtonConfig(BTN_B, "B")
            ),
            rotationInDegrees = -30f
        ),
        secondaryDials = listOf(
            SecondaryDialConfig.SingleButton(2, 1f, 0f, ButtonConfig(BTN_R, "R")),
            SecondaryDialConfig.SingleButton(4, 1f, 0f, ButtonConfig(BTN_START, "START"))
        )
    )

    private var controllerActive = false
    private var overlayRoot: FrameLayout? = null
    private var leftPad: RadialGamePad? = null
    private var rightPad: RadialGamePad? = null
    private var currentEditor: OverlayEditor? = null

    fun attach(parentLayout: ViewGroup) {
        val ctx = activity
        val isPortrait = ctx.resources.configuration.orientation == android.content.res.Configuration.ORIENTATION_PORTRAIT

        val root = FrameLayout(ctx).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        }
        overlayRoot = root

        val left = RadialGamePad(leftConfig, 8f, ctx).apply {
            gravityX = -1f
            gravityY = 1f
        }
        val right = RadialGamePad(rightConfig, 8f, ctx).apply {
            gravityX = 1f
            gravityY = 1f
        }
        leftPad = left
        rightPad = right

        val leftContainer = FrameLayout(ctx).apply {
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f)
        }
        val rightContainer = FrameLayout(ctx).apply {
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f)
        }
        leftContainer.addView(left, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        ))
        rightContainer.addView(right, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        ))

        val row = LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        }
        row.addView(leftContainer)
        row.addView(rightContainer)
        root.addView(row)
        parentLayout.addView(root, ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        ))

        val editor = if (isPortrait) portraitEditor else landscapeEditor
        currentEditor = editor
        editor.load()
        editor.applyTo(left, right)

        if (controllerActive) {
            root.visibility = View.GONE
        } else {
            root.visibility = View.VISIBLE
        }

        merge(left.events(), right.events())
            .onEach { event -> handleEvent(event, root, left, right) }
            .launchIn(scope)
    }

    private fun handleEvent(event: Event, root: FrameLayout, left: RadialGamePad, right: RadialGamePad) {
        val editor = currentEditor ?: return
        when (event) {
            is Event.Button -> {
                if (editor.isEditing()) return
                if (event.action == KeyEvent.ACTION_DOWN) {
                    editor.onButtonDown {
                        editor.enterEditMode(root, left, right)
                    }
                    SDLActivity.nativeVirtualButtonEvent(event.id, true)
                } else {
                    editor.onButtonUp()
                    SDLActivity.nativeVirtualButtonEvent(event.id, false)
                }
            }
            is Event.Direction -> {
                if (editor.isEditing()) return
                listOf(BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT).forEach {
                    SDLActivity.nativeVirtualButtonEvent(it, false)
                }
                if (event.yAxis < -0.5f) SDLActivity.nativeVirtualButtonEvent(BTN_UP, true)
                if (event.yAxis > 0.5f) SDLActivity.nativeVirtualButtonEvent(BTN_DOWN, true)
                if (event.xAxis < -0.5f) SDLActivity.nativeVirtualButtonEvent(BTN_LEFT, true)
                if (event.xAxis > 0.5f) SDLActivity.nativeVirtualButtonEvent(BTN_RIGHT, true)
            }
            else -> {}
        }
    }

    fun start() {
        controllerMonitor.register(activity)
        controllerActive = controllerMonitor.connectedCount.value > 0
        updateVisibility()
        controllerMonitor.connectedCount
            .onEach { count ->
                controllerActive = count > 0
                updateVisibility()
            }
            .launchIn(scope)
    }

    fun stop() {
        controllerMonitor.unregister(activity)
    }

    fun onScreenTouch() {
        if (controllerActive) {
            controllerActive = false
            activity.runOnUiThread { show() }
        }
    }

    fun onControllerInput() {
        if (!controllerActive) {
            controllerActive = true
            activity.runOnUiThread { hide() }
        }
    }

    private fun updateVisibility() {
        if (controllerActive) hide() else show()
    }

    fun show() {
        overlayRoot?.visibility = View.VISIBLE
    }

    fun hide() {
        overlayRoot?.visibility = View.GONE
    }

    fun detach() {
        val r = overlayRoot ?: return
        (r.parent as? ViewGroup)?.removeView(r)
        overlayRoot = null
        leftPad = null
        rightPad = null
        currentEditor = null
    }
}
