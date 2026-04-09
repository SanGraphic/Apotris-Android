package com.apotris.android

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import kotlin.math.min
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.Spinner
import android.widget.TextView
import com.swordfish.radialgamepad.library.RadialGamePad
import kotlin.math.roundToInt

/**
 * 4s hold to enter edit mode, then drag/scale pads; SharedPreferences [overlay_layout].
 * Portrait: pick presets, custom colors, global size, opacity (see [PortraitGamepadPrefs]).
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

    /** Portrait appearance while editing (also source for live pad rebuild). */
    var portraitAppearDraft: PortraitGamepadAppear = PortraitGamepadAppear()
        private set

    /** Invoked when portrait theme/stroke/custom colors change (pads must rebuild). */
    var onPortraitPadsRebuild: (() -> Unit)? = null

    private val handler = Handler(Looper.getMainLooper())
    private var holdCheckRunnable: Runnable? = null
    private var holdStartTime = 0L
    private var editOverlay: FrameLayout? = null
    private var sizeSlider: SeekBar? = null
    private var sliderLabel: TextView? = null
    private var selectedPad: String? = null
    private var padLeftRef: RadialGamePad? = null
    private var padRightRef: RadialGamePad? = null
    private var editRoot: FrameLayout? = null
    /** Portrait: SDL game-area overlay — theme UI is placed here. */
    private var gameEditHostRef: FrameLayout? = null
    private var portraitPanelWrapper: FrameLayout? = null

    private val isPortrait get() = orientationKey == "portrait"

    fun load() {
        leftState.tx = prefs.getFloat("left_tx_$orientationKey", 0f)
        leftState.ty = prefs.getFloat("left_ty_$orientationKey", 0f)
        leftState.scale = prefs.getFloat("left_scale_$orientationKey", 1f)
        rightState.tx = prefs.getFloat("right_tx_$orientationKey", 0f)
        rightState.ty = prefs.getFloat("right_ty_$orientationKey", 0f)
        rightState.scale = prefs.getFloat("right_scale_$orientationKey", 1f)
        if (isPortrait) {
            portraitAppearDraft = PortraitGamepadPrefs.load(context)
        }
    }

    fun save() {
        val ed = prefs.edit()
            .putFloat("left_tx_$orientationKey", leftState.tx)
            .putFloat("left_ty_$orientationKey", leftState.ty)
            .putFloat("left_scale_$orientationKey", leftState.scale)
            .putFloat("right_tx_$orientationKey", rightState.tx)
            .putFloat("right_ty_$orientationKey", rightState.ty)
            .putFloat("right_scale_$orientationKey", rightState.scale)
        if (isPortrait) {
            portraitPanelWrapper?.let {
                ed.putFloat(KEY_EDIT_PANEL_TX, it.translationX)
                    .putFloat(KEY_EDIT_PANEL_TY, it.translationY)
            }
        }
        ed.apply()
    }

    private fun effectiveGlobalScale(): Float {
        if (!isPortrait) return 1f
        return if (isEditing()) portraitAppearDraft.globalScale
        else PortraitGamepadPrefs.load(context).globalScale
    }

    fun applyTo(leftPad: RadialGamePad, rightPad: RadialGamePad) {
        val g = effectiveGlobalScale()
        leftPad.translationX = leftState.tx
        leftPad.translationY = leftState.ty
        leftPad.scaleX = leftState.scale * g
        leftPad.scaleY = leftState.scale * g
        rightPad.translationX = rightState.tx
        rightPad.translationY = rightState.ty
        rightPad.scaleX = rightState.scale * g
        rightPad.scaleY = rightState.scale * g
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
    fun enterEditMode(controlsRoot: FrameLayout, gameEditHost: FrameLayout?, leftPad: RadialGamePad, rightPad: RadialGamePad) {
        if (editOverlay != null) return

        padLeftRef = leftPad
        padRightRef = rightPad
        editRoot = controlsRoot
        gameEditHostRef = gameEditHost

        if (isPortrait) {
            portraitAppearDraft = PortraitGamepadPrefs.load(context)
        }

        val controlsLayer = FrameLayout(context).apply {
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            if (isPortrait && gameEditHost != null) {
                setBackgroundColor(Color.TRANSPARENT)
            } else {
                setBackgroundColor(Color.argb(80, 0, 0, 0))
            }
        }
        editOverlay = controlsLayer

        setupDragInterceptor(controlsLayer, leftPad, leftState, PAD_LEFT)
        setupDragInterceptor(controlsLayer, rightPad, rightState, PAD_RIGHT)

        val previewAlpha = if (isPortrait) {
            (portraitAppearDraft.alpha * 0.78f).coerceAtLeast(0.35f)
        } else {
            0.7f
        }
        leftPad.alpha = previewAlpha
        rightPad.alpha = previewAlpha

        when {
            isPortrait && gameEditHost != null -> {
                gameEditHost.visibility = View.VISIBLE
                gameEditHost.removeAllViews()
                gameEditHost.addView(
                    buildPortraitGameEditShell(),
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.MATCH_PARENT
                    )
                )
                controlsRoot.addView(controlsLayer)
            }
            isPortrait -> {
                val scroll = buildPortraitEditScroll()
                scroll.layoutParams = FrameLayout.LayoutParams(
                    dpToPx(300),
                    FrameLayout.LayoutParams.WRAP_CONTENT,
                    Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
                ).apply { bottomMargin = dpToPx(8) }
                controlsLayer.addView(scroll)
                controlsRoot.addView(controlsLayer)
            }
            else -> {
                val panel = buildLandscapeEditPanel(controlsRoot, leftPad, rightPad)
                controlsLayer.addView(panel)
                controlsRoot.addView(controlsLayer)
            }
        }
    }

    private fun buildLandscapeEditPanel(root: FrameLayout, leftPad: RadialGamePad, rightPad: RadialGamePad): View {
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
        wireSizeSlider(slider, label, leftPad, rightPad)

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
        return panel
    }

    private fun wireSizeSlider(slider: SeekBar, label: TextView, leftPad: RadialGamePad, rightPad: RadialGamePad) {
        slider.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, progress: Int, fromUser: Boolean) {
                val scale = (50f + progress) / 100f
                when (selectedPad) {
                    PAD_LEFT -> {
                        leftState.scale = scale
                        applyTo(leftPad, rightPad)
                    }
                    PAD_RIGHT -> {
                        rightState.scale = scale
                        applyTo(leftPad, rightPad)
                    }
                }
                val cap = if (isPortrait) "Per-pad size" else "Size"
                label.text = "$cap: ${"%.2f".format(scale)}x"
            }

            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {}
        })
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun buildPortraitGameEditShell(): View {
        val outer = FrameLayout(context)
        val scrim = View(context).apply {
            layoutParams = FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            setBackgroundColor(Color.argb(130, 0, 0, 0))
        }
        val movable = FrameLayout(context).apply {
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.TOP or Gravity.CENTER_HORIZONTAL
            ).apply {
                topMargin = dpToPx(12)
                marginStart = dpToPx(6)
                marginEnd = dpToPx(6)
            }
            translationX = prefs.getFloat(KEY_EDIT_PANEL_TX, 0f)
            translationY = prefs.getFloat(KEY_EDIT_PANEL_TY, 0f)
        }
        portraitPanelWrapper = movable

        val dragBar = TextView(context).apply {
            text = "⇅  Drag to move panel  ⇅"
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.argb(255, 55, 48, 82))
            gravity = Gravity.CENTER
            textSize = 12f
            setPadding(dpToPx(10), dpToPx(12), dpToPx(10), dpToPx(12))
        }
        wirePanelDragHeader(dragBar, movable)

        val scroll = buildPortraitEditScroll()
        val column = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.argb(245, 22, 18, 32))
        }
        column.addView(
            dragBar,
            LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        )
        column.addView(scroll)
        movable.addView(column)
        outer.addView(scrim)
        outer.addView(movable)
        return outer
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun wirePanelDragHeader(handle: View, movable: FrameLayout) {
        var lastX = 0f
        var lastY = 0f
        handle.setOnTouchListener { _, event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    lastX = event.rawX
                    lastY = event.rawY
                    true
                }
                MotionEvent.ACTION_MOVE -> {
                    movable.translationX += event.rawX - lastX
                    movable.translationY += event.rawY - lastY
                    lastX = event.rawX
                    lastY = event.rawY
                    true
                }
                else -> true
            }
        }
    }

    private fun buildPortraitEditScroll(): ScrollView {
        val scrollH = min(
            (context.resources.displayMetrics.heightPixels * 0.52f).toInt(),
            dpToPx(440)
        ).coerceAtLeast(dpToPx(160))
        val scroll = ScrollView(context).apply {
            layoutParams = LinearLayout.LayoutParams(dpToPx(300), scrollH)
            isFillViewport = false
            setPadding(0, 0, 0, dpToPx(6))
        }
        val panel = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.argb(235, 22, 18, 32))
            setPadding(dpToPx(14), dpToPx(14), dpToPx(14), dpToPx(14))
        }

        val title = TextView(context).apply {
            text = "Portrait controls — theme, size, drag pads"
            setTextColor(Color.WHITE)
            textSize = 13f
            gravity = Gravity.CENTER
        }
        panel.addView(title)

        val presetLabel = TextView(context).apply {
            text = "Preset"
            setTextColor(0xFFB0B0C0.toInt())
            textSize = 12f
        }
        panel.addView(presetLabel)

        val presetIds = GamepadThemePresets.PRESET_DISPLAY_ORDER

        val customBlock = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            visibility = if (portraitAppearDraft.presetId == GamepadThemePresets.ID_CUSTOM) View.VISIBLE else View.GONE
        }

        fun refreshCustomVisibility() {
            customBlock.visibility =
                if (portraitAppearDraft.presetId == GamepadThemePresets.ID_CUSTOM) View.VISIBLE else View.GONE
        }

        val accentLabel = TextView(context).apply {
            text = "Custom — button face"
            setTextColor(0xFFB0B0C0.toInt())
            textSize = 11f
        }
        customBlock.addView(accentLabel)
        customBlock.addView(colorChipRow(CUSTOM_FACE_SWATCHES) { color ->
            portraitAppearDraft = portraitAppearDraft.copy(customPrimary = color)
            onPortraitPadsRebuild?.invoke()
        })

        val ringLabel = TextView(context).apply {
            text = "Custom — ring / background"
            setTextColor(0xFFB0B0C0.toInt())
            textSize = 11f
        }
        customBlock.addView(ringLabel)
        customBlock.addView(colorChipRow(CUSTOM_RING_SWATCHES) { color ->
            portraitAppearDraft = portraitAppearDraft.copy(customPadBackground = color)
            onPortraitPadsRebuild?.invoke()
        })

        val strokeCap = TextView(context).apply { text = "Outline thickness" }
        strokeCap.setTextColor(0xFFB0B0C0.toInt())
        strokeCap.textSize = 11f
        customBlock.addView(strokeCap)
        val strokeBar = SeekBar(context).apply {
            max = 40
            progress = ((portraitAppearDraft.customStrokeDp - 1f) / 4f * 40f).roundToInt().coerceIn(0, 40)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, progress: Int, fromUser: Boolean) {
                    if (!fromUser) return
                    val dp = 1f + (progress / 40f) * 4f
                    portraitAppearDraft = portraitAppearDraft.copy(customStrokeDp = dp)
                    onPortraitPadsRebuild?.invoke()
                }

                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?) {}
            })
        }
        customBlock.addView(strokeBar)

        val spinner = Spinner(context).apply {
            adapter = ArrayAdapter(
                context,
                android.R.layout.simple_spinner_dropdown_item,
                presetIds.map { GamepadThemePresets.displayName(it) }
            )
            setSelection(presetIds.indexOf(portraitAppearDraft.presetId).coerceAtLeast(0))
            onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                    val newId = presetIds[position]
                    if (newId == portraitAppearDraft.presetId) return
                    portraitAppearDraft = portraitAppearDraft.copy(presetId = newId)
                    refreshCustomVisibility()
                    onPortraitPadsRebuild?.invoke()
                }

                override fun onNothingSelected(parent: AdapterView<*>?) {}
            }
        }
        panel.addView(spinner)
        panel.addView(customBlock)

        val sizeCap = TextView(context).apply {
            text = "Overall size (both pads)"
            setTextColor(0xFFB0B0C0.toInt())
            textSize = 12f
        }
        panel.addView(sizeCap)
        val globalBar = SeekBar(context).apply {
            max = 100
            progress = globalScaleToProgress(portraitAppearDraft.globalScale)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, progress: Int, fromUser: Boolean) {
                    if (!fromUser) return
                    portraitAppearDraft = portraitAppearDraft.copy(globalScale = progressToGlobalScale(progress))
                    padLeftRef?.let { l -> padRightRef?.let { r -> applyTo(l, r) } }
                }

                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?) {}
            })
        }
        panel.addView(globalBar)

        val alphaCap = TextView(context).apply {
            text = "Opacity"
            setTextColor(0xFFB0B0C0.toInt())
            textSize = 12f
        }
        panel.addView(alphaCap)
        val alphaBar = SeekBar(context).apply {
            max = 100
            progress = alphaToProgress(portraitAppearDraft.alpha)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, progress: Int, fromUser: Boolean) {
                    if (!fromUser) return
                    val a = progressToAlpha(progress)
                    portraitAppearDraft = portraitAppearDraft.copy(alpha = a)
                    val preview = (a * 0.78f).coerceAtLeast(0.35f)
                    padLeftRef?.alpha = preview
                    padRightRef?.alpha = preview
                }

                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?) {}
            })
        }
        panel.addView(alphaBar)

        val hint = TextView(context).apply {
            text = "Tap left/right on the control strip to pick a pad, then adjust per-pad size below."
            setTextColor(0xFF888899.toInt())
            textSize = 11f
            setPadding(0, dpToPx(6), 0, 0)
        }
        panel.addView(hint)

        val label = TextView(context).apply {
            text = "Per-pad size (after selecting a side)"
            setTextColor(Color.WHITE)
            textSize = 12f
            gravity = Gravity.CENTER
        }
        sliderLabel = label
        val slider = SeekBar(context).apply {
            max = 100
            progress = 50
            isEnabled = false
        }
        sizeSlider = slider
        padLeftRef?.let { l -> padRightRef?.let { r -> wireSizeSlider(slider, label, l, r) } }

        panel.addView(label)
        panel.addView(slider)

        val saveBtn = Button(context).apply {
            text = "Save"
            setOnClickListener {
                val l = padLeftRef ?: return@setOnClickListener
                val r = padRightRef ?: return@setOnClickListener
                val er = editRoot ?: return@setOnClickListener
                exitEditMode(er, l, r)
            }
        }
        val resetLayoutBtn = Button(context).apply {
            text = "Reset layout"
            setOnClickListener {
                leftState.tx = 0f
                leftState.ty = 0f
                leftState.scale = 1f
                rightState.tx = 0f
                rightState.ty = 0f
                rightState.scale = 1f
                padLeftRef?.let { l -> padRightRef?.let { r -> applyTo(l, r) } }
                slider.progress = 50
                label.text = "Per-pad size (after selecting a side)"
                selectedPad = null
                slider.isEnabled = false
            }
        }
        val resetLookBtn = Button(context).apply {
            text = "Default look"
            setOnClickListener {
                portraitAppearDraft = PortraitGamepadAppear()
                globalBar.progress = globalScaleToProgress(portraitAppearDraft.globalScale)
                alphaBar.progress = alphaToProgress(portraitAppearDraft.alpha)
                strokeBar.progress = ((portraitAppearDraft.customStrokeDp - 1f) / 4f * 40f).roundToInt().coerceIn(0, 40)
                spinner.setSelection(presetIds.indexOf(portraitAppearDraft.presetId).coerceAtLeast(0))
                refreshCustomVisibility()
                onPortraitPadsRebuild?.invoke()
                padLeftRef?.let { l -> padRightRef?.let { r -> applyTo(l, r) } }
            }
        }
        val row1 = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            addView(saveBtn, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            addView(resetLayoutBtn, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        }
        val row2 = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            addView(resetLookBtn, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
        }
        panel.addView(row1)
        panel.addView(row2)

        scroll.addView(panel)
        return scroll
    }

    private fun colorChipRow(colors: IntArray, onPick: (Int) -> Unit): LinearLayout {
        val row = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
        }
        val size = dpToPx(32)
        val margin = dpToPx(4)
        for (c in colors) {
            val chip = View(context).apply {
                layoutParams = LinearLayout.LayoutParams(size, size).apply { setMargins(margin, margin, margin, margin) }
                background = GradientDrawable().apply {
                    shape = GradientDrawable.OVAL
                    setColor(c)
                    setStroke(dpToPx(2), Color.WHITE)
                }
                setOnClickListener { onPick(c) }
            }
            row.addView(chip)
        }
        return row
    }

    private fun globalScaleToProgress(g: Float): Int =
        ((g - 0.7f) / 0.75f * 100f).roundToInt().coerceIn(0, 100)

    private fun progressToGlobalScale(p: Int): Float =
        (0.7f + (p / 100f) * 0.75f).coerceIn(0.7f, 1.45f)

    private fun alphaToProgress(a: Float): Int =
        ((a - 0.35f) / 0.65f * 100f).roundToInt().coerceIn(0, 100)

    private fun progressToAlpha(p: Int): Float =
        (0.35f + (p / 100f) * 0.65f).coerceIn(0.35f, 1f)

    fun refreshEditPads(root: FrameLayout, leftPad: RadialGamePad, rightPad: RadialGamePad) {
        val overlay = editOverlay ?: return
        padLeftRef = leftPad
        padRightRef = rightPad
        removeDragInterceptors(overlay)
        setupDragInterceptor(overlay, leftPad, leftState, PAD_LEFT)
        setupDragInterceptor(overlay, rightPad, rightState, PAD_RIGHT)
        val previewAlpha = (portraitAppearDraft.alpha * 0.78f).coerceAtLeast(0.35f)
        leftPad.alpha = previewAlpha
        rightPad.alpha = previewAlpha
        applyTo(leftPad, rightPad)
    }

    private fun removeDragInterceptors(overlay: FrameLayout) {
        val remove = mutableListOf<View>()
        for (i in 0 until overlay.childCount) {
            val c = overlay.getChildAt(i)
            if (c.tag == TAG_DRAG_ROW) remove.add(c)
        }
        remove.forEach { overlay.removeView(it) }
    }

    fun exitEditMode(root: FrameLayout, leftPad: RadialGamePad, rightPad: RadialGamePad) {
        save()
        if (isPortrait) {
            PortraitGamepadPrefs.save(context, portraitAppearDraft)
        }
        editOverlay?.let { o ->
            if (o.parent === root) root.removeView(o)
        }
        editOverlay = null
        gameEditHostRef?.removeAllViews()
        gameEditHostRef?.visibility = View.GONE
        gameEditHostRef = null
        portraitPanelWrapper = null
        selectedPad = null
        padLeftRef = null
        padRightRef = null
        editRoot = null
        if (isPortrait) {
            val a = portraitAppearDraft.alpha
            leftPad.alpha = a
            rightPad.alpha = a
        } else {
            leftPad.alpha = 1f
            rightPad.alpha = 1f
        }
    }

    fun isEditing(): Boolean = editOverlay != null

    /** Clear edit chrome when the touch overlay is torn down (e.g. rotation). */
    fun detachCleanup() {
        holdCheckRunnable?.let { handler.removeCallbacks(it) }
        holdCheckRunnable = null
        editRoot?.let { root ->
            editOverlay?.let { o ->
                if (o.parent === root) root.removeView(o)
            }
        }
        gameEditHostRef?.removeAllViews()
        gameEditHostRef?.visibility = View.GONE
        editOverlay = null
        gameEditHostRef = null
        portraitPanelWrapper = null
        selectedPad = null
        padLeftRef = null
        padRightRef = null
        editRoot = null
        sizeSlider = null
        sliderLabel = null
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun setupDragInterceptor(
        overlay: FrameLayout,
        pad: RadialGamePad,
        state: PadState,
        padId: String
    ) {
        val row = overlay.findViewWithTag(TAG_DRAG_ROW) as? LinearLayout
        val touchHost = if (row != null) {
            if (padId == PAD_LEFT) row.getChildAt(0) else row.getChildAt(1)
        } else {
            val newRow = LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                tag = TAG_DRAG_ROW
                layoutParams = FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT
                )
            }
            val leftZone = View(context).apply { setBackgroundColor(0) }
            newRow.addView(
                leftZone,
                LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f)
            )
            val rightZone = View(context).apply { setBackgroundColor(0) }
            newRow.addView(
                rightZone,
                LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f)
            )
            overlay.addView(newRow, 0)
            if (padId == PAD_LEFT) leftZone else rightZone
        }

        var lastX = 0f
        var lastY = 0f
        var readyToDrag = false

        touchHost.setOnTouchListener { _, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    lastX = event.rawX
                    lastY = event.rawY
                    readyToDrag = true
                    selectedPad = padId
                    sizeSlider?.isEnabled = true
                    val p = (100 * state.scale - 50f).roundToInt().coerceIn(0, 100)
                    sizeSlider?.progress = p
                    sliderLabel?.text = "Per-pad size: ${"%.2f".format(state.scale)}x"
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
        const val TAG_DRAG_ROW = "apotris_drag_row"
        const val KEY_EDIT_PANEL_TX = "portrait_edit_panel_tx"
        const val KEY_EDIT_PANEL_TY = "portrait_edit_panel_ty"

        val CUSTOM_FACE_SWATCHES = intArrayOf(
            Color.parseColor("#9B6DFF"),
            Color.parseColor("#00E5FF"),
            Color.parseColor("#FF7043"),
            Color.parseColor("#8BAC0F"),
            Color.parseColor("#E040FB"),
            Color.parseColor("#FFEB3B"),
            Color.parseColor("#FFFFFF"),
            Color.parseColor("#5D4037")
        )

        val CUSTOM_RING_SWATCHES = intArrayOf(
            Color.parseColor("#2A1A45"),
            Color.parseColor("#1A0D2E"),
            Color.parseColor("#0D0221"),
            Color.parseColor("#263238"),
            Color.parseColor("#306230"),
            Color.parseColor("#3E2723"),
            Color.parseColor("#212121"),
            Color.parseColor("#000000")
        )
    }
}
