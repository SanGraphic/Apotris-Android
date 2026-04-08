package com.apotris.android

import android.content.Context
import android.hardware.input.InputManager
import android.view.InputDevice
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Counts gamepad-class devices (matches shipped APK InputManager listener behavior).
 */
class ControllerMonitor(context: Context) : InputManager.InputDeviceListener {

    private val inputManager = context.getSystemService(Context.INPUT_SERVICE) as InputManager
    private val _connectedCount = MutableStateFlow(countConnectedGamepads())
    val connectedCount: StateFlow<Int> = _connectedCount.asStateFlow()

    override fun onInputDeviceAdded(deviceId: Int) {
        val device = inputManager.getInputDevice(deviceId) ?: return
        if (isGamepad(device)) {
            _connectedCount.value = _connectedCount.value + 1
        }
    }

    override fun onInputDeviceRemoved(deviceId: Int) {
        _connectedCount.value = (_connectedCount.value - 1).coerceAtLeast(0)
    }

    override fun onInputDeviceChanged(deviceId: Int) {
        _connectedCount.value = countConnectedGamepads()
    }

    @Suppress("UNUSED_PARAMETER")
    fun register(context: Context) {
        inputManager.registerInputDeviceListener(this, null)
        _connectedCount.value = countConnectedGamepads()
    }

    @Suppress("UNUSED_PARAMETER")
    fun unregister(context: Context) {
        inputManager.unregisterInputDeviceListener(this)
    }

    private fun isGamepad(device: InputDevice): Boolean {
        val s = device.sources
        return (s and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
            || (s and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK
    }

    private fun countConnectedGamepads(): Int {
        var n = 0
        for (id in inputManager.inputDeviceIds) {
            val d = inputManager.getInputDevice(id) ?: continue
            if (isGamepad(d)) n++
        }
        return n
    }
}
