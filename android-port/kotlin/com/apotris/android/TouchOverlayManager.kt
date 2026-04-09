package com.apotris.android

import android.app.Activity
import android.content.res.Configuration
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import com.swordfish.radialgamepad.library.RadialGamePad
import com.swordfish.radialgamepad.library.event.Event
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.merge
import kotlinx.coroutines.flow.onEach
import kotlin.jvm.JvmOverloads
import org.libsdl.app.SDLActivity

/**
 * Virtual controls + physical controller visibility (parity with shipped com.example.apotris).
 * Portrait: left/right pads sit in two equal-width columns (same dial sizing as classic layout);
 * parents use clipChildren=false so controls can extend slightly into the center without hard clipping.
 */
class TouchOverlayManager(private val activity: Activity) {

    private val controllerMonitor = ControllerMonitor(activity)
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    private val landscapeEditor = OverlayEditor(activity, "landscape")
    private val portraitEditor = OverlayEditor(activity, "portrait")

    private var controllerActive = false
    private var overlayRoot: FrameLayout? = null
    private var leftPadHost: FrameLayout? = null
    private var rightPadHost: FrameLayout? = null
    private var padsRow: LinearLayout? = null
    private var leftPad: RadialGamePad? = null
    private var rightPad: RadialGamePad? = null
    private var currentEditor: OverlayEditor? = null
    private var eventsJob: Job? = null
    /** Portrait: over game surface — edit UI (spinner, sliders) is shown here. */
    private var portraitGameEditHost: FrameLayout? = null

    private val landscapeConfigs = GamepadLayoutFactory.landscapeDefaults()

    init {
        portraitEditor.onPortraitPadsRebuild = {
            activity.runOnUiThread {
                rebuildPortraitPadsIfNeeded()
            }
        }
    }

    @JvmOverloads
    fun attach(parentLayout: ViewGroup, portraitGameEditHost: FrameLayout? = null) {
        val ctx = activity
        val isPortrait = ctx.resources.configuration.orientation == Configuration.ORIENTATION_PORTRAIT

        this.portraitGameEditHost = if (isPortrait) portraitGameEditHost else null

        val root = FrameLayout(ctx).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            clipChildren = false
        }
        overlayRoot = root

        val leftContainer = FrameLayout(ctx).apply {
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f)
            clipChildren = false
        }
        val rightContainer = FrameLayout(ctx).apply {
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f)
            clipChildren = false
        }
        leftPadHost = leftContainer
        rightPadHost = rightContainer

        val row = LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            clipChildren = false
        }
        padsRow = row
        row.addView(leftContainer)
        row.addView(rightContainer)
        root.addView(row)

        parentLayout.addView(
            root,
            ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        )

        val editor = if (isPortrait) portraitEditor else landscapeEditor
        currentEditor = editor
        editor.load()

        if (isPortrait) {
            installPortraitPads()
        } else {
            installLandscapePads()
        }

        if (controllerActive) {
            root.visibility = View.GONE
        } else {
            root.visibility = View.VISIBLE
        }
    }

    private fun portraitAppearForPads(): PortraitGamepadAppear {
        return if (portraitEditor.isEditing()) {
            portraitEditor.portraitAppearDraft
        } else {
            PortraitGamepadPrefs.load(activity)
        }
    }

    private fun installLandscapePads() {
        val ctx = activity
        val leftContainer = leftPadHost ?: return
        val rightContainer = rightPadHost ?: return
        eventsJob?.cancel()
        leftContainer.removeAllViews()
        rightContainer.removeAllViews()

        val left = RadialGamePad(landscapeConfigs.first, 8f, ctx).apply {
            gravityX = -1f
            gravityY = 1f
        }
        val right = RadialGamePad(landscapeConfigs.second, 8f, ctx).apply {
            gravityX = 1f
            gravityY = 1f
        }
        leftPad = left
        rightPad = right
        leftContainer.addView(
            left,
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        )
        rightContainer.addView(
            right,
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        )
        landscapeEditor.applyTo(left, right)
        left.alpha = 1f
        right.alpha = 1f
        subscribeEvents(left, right)
    }

    private fun installPortraitPads() {
        val ctx = activity
        val leftContainer = leftPadHost ?: return
        val rightContainer = rightPadHost ?: return
        eventsJob?.cancel()
        leftContainer.removeAllViews()
        rightContainer.removeAllViews()

        val appear = portraitAppearForPads()
        val (leftConfig, rightConfig) = GamepadLayoutFactory.portraitConfigs(appear)

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
        val lp = FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        )
        leftContainer.addView(left, lp)
        rightContainer.addView(right, lp)
        portraitEditor.applyTo(left, right)
        applyPortraitIdleAlpha(left, right, appear)
        subscribeEvents(left, right)
    }

    private fun applyPortraitIdleAlpha(left: RadialGamePad, right: RadialGamePad, appear: PortraitGamepadAppear) {
        left.alpha = appear.alpha
        right.alpha = appear.alpha
    }

    private fun subscribeEvents(left: RadialGamePad, right: RadialGamePad) {
        val root = overlayRoot ?: return
        eventsJob = merge(left.events(), right.events())
            .onEach { event -> handleEvent(event, root, left, right) }
            .launchIn(scope)
    }

    fun rebuildPortraitPadsIfNeeded() {
        if (activity.resources.configuration.orientation != Configuration.ORIENTATION_PORTRAIT) return
        if (overlayRoot == null) return
        val editing = portraitEditor.isEditing()

        installPortraitPads()

        val newLeft = leftPad ?: return
        val newRight = rightPad ?: return
        if (editing) {
            portraitEditor.refreshEditPads(overlayRoot!!, newLeft, newRight)
        }
    }

    private fun handleEvent(event: Event, root: FrameLayout, left: RadialGamePad, right: RadialGamePad) {
        val editor = currentEditor ?: return
        when (event) {
            is Event.Button -> {
                if (editor.isEditing()) return
                if (event.action == KeyEvent.ACTION_DOWN) {
                    editor.onButtonDown {
                        val gameHost = portraitGameEditHost
                        if (activity.resources.configuration.orientation == Configuration.ORIENTATION_PORTRAIT && gameHost != null) {
                            editor.enterEditMode(root, gameHost, left, right)
                        } else {
                            editor.enterEditMode(root, null, left, right)
                        }
                    }
                    SDLActivity.nativeVirtualButtonEvent(event.id, true)
                } else {
                    editor.onButtonUp()
                    SDLActivity.nativeVirtualButtonEvent(event.id, false)
                }
            }
            is Event.Direction -> {
                if (editor.isEditing()) return
                listOf(
                    VirtualGamepadMapping.BTN_UP,
                    VirtualGamepadMapping.BTN_DOWN,
                    VirtualGamepadMapping.BTN_LEFT,
                    VirtualGamepadMapping.BTN_RIGHT
                ).forEach {
                    SDLActivity.nativeVirtualButtonEvent(it, false)
                }
                if (event.yAxis < -0.5f) SDLActivity.nativeVirtualButtonEvent(VirtualGamepadMapping.BTN_UP, true)
                if (event.yAxis > 0.5f) SDLActivity.nativeVirtualButtonEvent(VirtualGamepadMapping.BTN_DOWN, true)
                if (event.xAxis < -0.5f) SDLActivity.nativeVirtualButtonEvent(VirtualGamepadMapping.BTN_LEFT, true)
                if (event.xAxis > 0.5f) SDLActivity.nativeVirtualButtonEvent(VirtualGamepadMapping.BTN_RIGHT, true)
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
        portraitEditor.detachCleanup()
        landscapeEditor.detachCleanup()
        eventsJob?.cancel()
        eventsJob = null
        val r = overlayRoot ?: return
        (r.parent as? ViewGroup)?.removeView(r)
        overlayRoot = null
        leftPadHost = null
        rightPadHost = null
        padsRow = null
        leftPad = null
        rightPad = null
        currentEditor = null
        portraitGameEditHost = null
    }
}
