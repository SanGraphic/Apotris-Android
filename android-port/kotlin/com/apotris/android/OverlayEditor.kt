package com.apotris.android

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Color
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import androidx.core.view.GravityCompat
import com.swordfish.radialgamepad.library.RadialGamePad
import kotlin.math.roundToInt

/**
 * 4s hold to enter edit mode, then drag/scale pads; SharedPreferences [overlay_layout] (shipped keys).
 */
class OverlayEditor(private val context: Context, private val orientationKey: String) {

    class PadState(
        var tx: Float = 0f,
        var ty: Float = 0f,
        var scale: Float = 1f
    )

    private val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    val leftState = PadState()
    val rightState = PadState()

    private val handler = Handler(Looper.getMainLooper())
    private var holdCheckRunnable: Runnable? = null
    private var holdStartTime = 0L
    private var editOverlay: FrameLayout? = null
    private var sizeSlider: SeekBar? = null
    private var sliderLabel: TextView? = null
    private var selectedPad: String? = null

    fun load() {
        leftState.tx = prefs.getFloat("left_tx_$orientationKey", 0f)
        leftState.ty = prefs.getFloat("left_ty_$orientationKey", 0f)
        leftState.scale = prefs.getFloat("left_scale_$orientationKey", 1f)
        rightState.tx = prefs.getFloat("right_tx_$orientationKey", 0f)
        rightState.ty = prefs.getFloat("right_ty_$orientationKey", 0f)
        rightState.scale = prefs.getFloat("right_scale_$orientationKey", 1f)
    }

    fun save() {
        prefs.edit()
            .putFloat("left_tx_$orientationKey", leftState.tx)
            .putFloat("left_ty_$orientationKey", leftState.ty)
            .putFloat("left_scale_$orientationKey", leftState.scale)
            .putFloat("right_tx_$orientationKey", rightState.tx)
            .putFloat("right_ty_$orientationKey", rightState.ty)
            .putFloat("right_scale_$orientationKey", rightState.scale)
            .apply()
    }

    fun applyTo(leftPad: RadialGamePad, rightPad: RadialGamePad) {
        leftPad.translationX = leftState.tx
        leftPad.translationY = leftState.ty
        leftPad.scaleX = leftState.scale
        leftPad.scaleY = leftState.scale
        rightPad.translationX = rightState.tx
        rightPad.translationY = rightState.ty
        rightPad.scaleX = rightState.scale
        rightPad.scaleY = rightState.scale
    }

    fun onButtonDown(onEditMode: () -> Unit) {
        holdStartTime = System.currentTimeMillis()
        holdCheckRunnable?.let { handler.removeCallbacks(it) }
        val r = Runnable {
            if (System.currentTimeMillis() - holdStartTime >= HOLD_MS) {
                onEditMode()
            }
        }
        holdCheckRunnable = r
        handler.postDelayed(r, HOLD_MS)
    }

    fun onButtonUp() {
        holdCheckRunnable?.let { handler.removeCallbacks(it) }
        holdCheckRunnable = null
    }

    @SuppressLint("ClickableViewAccessibility")
    fun enterEditMode(root: FrameLayout, leftPad: RadialGamePad, rightPad: RadialGamePad) {
        if (editOverlay != null) return

        val overlay = FrameLayout(context).apply {
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            setBackgroundColor(Color.argb(80, 0, 0, 0))
        }
        editOverlay = overlay

        setupDragInterceptor(overlay, leftPad, leftState, PAD_LEFT)
        setupDragInterceptor(overlay, rightPad, rightState, PAD_RIGHT)

        leftPad.alpha = 0.7f
        rightPad.alpha = 0.7f

        val panel = FrameLayout(context).apply {
            layoutParams = FrameLayout.LayoutParams(
                dpToPx(220),
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER
            )
            setBackgroundColor(Color.argb(220, 30, 30, 30))
            setPadding(dpToPx(12), dpToPx(12), dpToPx(12), dpToPx(12))
        }
        val inner = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }
        val label = TextView(context).apply {
            text = "Tap a pad to adjust size"
            setTextColor(Color.WHITE)
            textSize = 14f
            gravity = Gravity.CENTER
        }
        sliderLabel = label

        val slider = SeekBar(context).apply {
            max = 100
            progress = 50
            isEnabled = false
        }
        sizeSlider = slider
        slider.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, progress: Int, fromUser: Boolean) {
                val scale = (50f + progress) / 100f
                when (selectedPad) {
                    PAD_LEFT -> {
                        leftState.scale = scale
                        leftPad.scaleX = scale
                        leftPad.scaleY = scale
                    }
                    PAD_RIGHT -> {
                        rightState.scale = scale
                        rightPad.scaleX = scale
                        rightPad.scaleY = scale
                    }
                }
                label.text = "Size: ${"%.2f".format(scale)}x"
            }

            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {}
        })

        val saveBtn = Button(context).apply {
            text = "Save"
            setOnClickListener { exitEditMode(root, leftPad, rightPad) }
        }
        val resetBtn = Button(context).apply {
            text = "Reset"
            setOnClickListener {
                leftState.tx = 0f
                leftState.ty = 0f
                leftState.scale = 1f
                rightState.tx = 0f
                rightState.ty = 0f
                rightState.scale = 1f
                applyTo(leftPad, rightPad)
                slider.progress = 50
                label.text = "Tap a pad to adjust size"
                selectedPad = null
                slider.isEnabled = false
            }
        }
        val btnRow = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
            addView(saveBtn, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            addView(resetBtn, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        }
        inner.addView(label)
        inner.addView(slider)
        inner.addView(btnRow)
        panel.addView(inner)
        overlay.addView(panel)
        root.addView(overlay)
    }

    fun exitEditMode(root: FrameLayout, leftPad: RadialGamePad, rightPad: RadialGamePad) {
        save()
        editOverlay?.let { root.removeView(it) }
        editOverlay = null
        selectedPad = null
        leftPad.alpha = 1f
        rightPad.alpha = 1f
    }

    fun isEditing(): Boolean = editOverlay != null

    @SuppressLint("ClickableViewAccessibility")
    private fun setupDragInterceptor(
        overlay: FrameLayout,
        pad: RadialGamePad,
        state: PadState,
        padId: String
    ) {
        val dm = context.resources.displayMetrics
        val v = View(context).apply { setBackgroundColor(0) }
        val lp = FrameLayout.LayoutParams(dm.widthPixels / 2, dm.heightPixels).apply {
            gravity = if (padId == PAD_LEFT) GravityCompat.START else GravityCompat.END
        }
        overlay.addView(v, lp)

        var lastX = 0f
        var lastY = 0f
        var readyToDrag = false

        v.setOnTouchListener { _, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    lastX = event.rawX
                    lastY = event.rawY
                    readyToDrag = true
                    selectedPad = padId
                    sizeSlider?.isEnabled = true
                    val p = (100 * state.scale - 50f).roundToInt().coerceIn(0, 100)
                    sizeSlider?.progress = p
                    sliderLabel?.text = "Size: ${"%.2f".format(state.scale)}x"
                    true
                }
                MotionEvent.ACTION_MOVE -> {
                    if (!readyToDrag) {
                        lastX = event.rawX
                        lastY = event.rawY
                        return@setOnTouchListener true
                    }
                    val dx = event.rawX - lastX
                    val dy = event.rawY - lastY
                    state.tx += dx
                    state.ty += dy
                    pad.translationX = state.tx
                    pad.translationY = state.ty
                    lastX = event.rawX
                    lastY = event.rawY
                    true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    readyToDrag = false
                    true
                }
                else -> false
            }
        }
    }

    private fun dpToPx(dp: Int): Int = (dp * context.resources.displayMetrics.density).roundToInt()

    private companion object {
        const val PREFS_NAME = "overlay_layout"
        const val HOLD_MS = 4000L
        const val PAD_LEFT = "left"
        const val PAD_RIGHT = "right"
    }
}
