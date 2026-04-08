// The MIT License (MIT)
//
// Copyright (C) 2024 Michael Theall
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "imgui_ctru.h"

#include <imgui.h>

#include <imgui_internal.h>

extern "C" {

#include <3ds/applets/swkbd.h>
#include <3ds/os.h>
#include <3ds/services/hid.h>
#include <3ds/types.h>
}

#include <algorithm>
#include <cstring>
#include <functional>
#include <string>
#include <tuple>

#undef keysDown
#undef keysUp

namespace
{
/// \brief Clipboard
std::string s_clipboard;

ImGuiPlatformImeData s_imeData{};
ImGuiViewport *s_viewportWantIme = nullptr;

bool s_imePopupActive = false;
ImVec2 s_imePopupPos{};
constexpr ImVec2 k_imePopupSize{16, 16};

bool s_keyboardRequested = false;

constexpr ImVec2 operator+ (const ImVec2 &a, const ImVec2 &b)
{
	return {a.x + b.x, a.y + b.y};
}

void setImeData (ImGuiContext *ctx, ImGuiViewport *viewport, ImGuiPlatformImeData *data)
{
	s_imeData.WantVisible     = data->WantVisible;
	s_imeData.InputPos        = data->InputPos;
	s_imeData.InputLineHeight = data->InputLineHeight;
	if (data->WantVisible)
	{
		s_viewportWantIme = viewport;
	}
	else if (s_viewportWantIme == viewport)
	{
		s_viewportWantIme = nullptr;
	}
}

/// \brief Get clipboard text callback
/// \param context_ ImGui context
char const *getClipboardText (ImGuiContext *const context_)
{
	(void)context_;
	return s_clipboard.c_str ();
}

/// \brief Set clipboard text callback
/// \param context_ ImGui context
/// \param text_ Clipboard text
void setClipboardText (ImGuiContext *const context_, char const *const text_)
{
	(void)context_;
	s_clipboard = text_;
}

/// \brief Update touch position
/// \param io_ ImGui IO
void updateTouch (ImGuiIO &io_, bool bothScreens)
{
	// check if touchpad was released
	if (hidKeysUp () & KEY_TOUCH)
	{
		// keep mouse position for one frame for release event
		io_.AddMouseButtonEvent (0, false);
		return;
	}

	// check if touchpad is touched
	if (!(hidKeysHeld () & KEY_TOUCH))
	{
		// set mouse cursor off-screen
		io_.AddMousePosEvent (-FLT_MAX, -FLT_MAX);
		io_.AddMouseButtonEvent (0, false);
		return;
	}

	float left, right, top, bottom;
	if (bothScreens)
	{
		left   = io_.DisplaySize.x * 0.1;
		right  = io_.DisplaySize.x * 0.9;
		top    = io_.DisplaySize.y * 0.5;
		bottom = io_.DisplaySize.y;
	}
	else
	{
		left   = 0;
		right  = io_.DisplaySize.x;
		top    = 0;
		bottom = io_.DisplaySize.y;
	}

	// read touch position
	touchPosition pos;
	hidTouchRead (&pos);

	// transform to bottom-screen space
	float x = lerp (left, right, pos.px / 320.0f);
	float y = lerp (top, bottom, pos.py / 240.0f);
	if (s_imePopupActive && (hidKeysDown () & KEY_TOUCH))
	{
		if (x >= s_imePopupPos.x && x < s_imePopupPos.x + k_imePopupSize.x &&
		    y >= s_imePopupPos.y && y < s_imePopupPos.y + k_imePopupSize.y)
		{
			s_keyboardRequested = true;
			return;
		}
	}
	io_.AddMousePosEvent (x, y);
	io_.AddMouseButtonEvent (0, true);
}

/// \brief Update gamepad inputs
/// \param io_ ImGui IO
void updateGamepads (ImGuiIO &io_, bool enabled)
{
	auto const buttonMapping = {
	    // clang-format off
	    std::make_pair (KEY_A,      ImGuiKey_GamepadFaceRight),
	    std::make_pair (KEY_B,      ImGuiKey_GamepadFaceDown),
	    std::make_pair (KEY_X,      ImGuiKey_GamepadFaceUp),
	    std::make_pair (KEY_Y,      ImGuiKey_GamepadFaceLeft),
	    std::make_pair (KEY_L,      ImGuiKey_GamepadL1),
	    std::make_pair (KEY_ZL,     ImGuiKey_GamepadL1),
	    std::make_pair (KEY_R,      ImGuiKey_GamepadR1),
	    std::make_pair (KEY_ZR,     ImGuiKey_GamepadR1),
	    std::make_pair (KEY_DUP,    ImGuiKey_GamepadDpadUp),
	    std::make_pair (KEY_DRIGHT, ImGuiKey_GamepadDpadRight),
	    std::make_pair (KEY_DDOWN,  ImGuiKey_GamepadDpadDown),
	    std::make_pair (KEY_DLEFT,  ImGuiKey_GamepadDpadLeft),
	    // clang-format on
	};

	static u32 previousKeys = 0;
	static u32 currentKeys  = 0;

	// read buttons from 3DS
	previousKeys = currentKeys;
	currentKeys  = enabled ? hidKeysHeld () : 0;

	auto const keysDown = ~previousKeys & currentKeys;
	auto const keysUp   = previousKeys & ~currentKeys;
	for (auto const &[in, out] : buttonMapping)
	{
		if (keysUp & in)
			io_.AddKeyEvent (out, false);
		else if (keysDown & in)
			io_.AddKeyEvent (out, true);
	}

	// update joystick
	circlePosition cpad;
	auto const analogMapping = {
	    // clang-format off
	    std::make_tuple (std::ref (cpad.dx), ImGuiKey_GamepadLStickLeft,  -0.3f, -0.9f),
	    std::make_tuple (std::ref (cpad.dx), ImGuiKey_GamepadLStickRight, +0.3f, +0.9f),
	    std::make_tuple (std::ref (cpad.dy), ImGuiKey_GamepadLStickUp,    +0.3f, +0.9f),
	    std::make_tuple (std::ref (cpad.dy), ImGuiKey_GamepadLStickDown,  -0.3f, -0.9f),
	    // clang-format on
	};

	// read left joystick from circle pad
	if (enabled)
	{
		hidCircleRead (&cpad);
	}
	else
	{
		cpad.dx = cpad.dy = 0;
	}
	for (auto const &[in, out, min, max] : analogMapping)
	{
		auto const value = std::clamp ((in / 156.0f - min) / (max - min), 0.0f, 1.0f);
		io_.AddKeyAnalogEvent (out, value > 0.1f, value);
	}
}

/// \brief Update keyboard inputs
/// \param io_ ImGui IO
void updateKeyboard (ImGuiIO &io_)
{
	static enum {
		INACTIVE,
		KEYBOARD,
	} state = INACTIVE;

	switch (state)
	{
	case INACTIVE:
	{
		if (!s_keyboardRequested)
			return;

		s_keyboardRequested = false;

		if (!io_.WantTextInput)
			return;

		// Held mouse buttons interfere with input
		for (int i = 0; i < ImGuiMouseButton_COUNT; i++)
			if (io_.MouseDown[i])
				return;

		// Held keys also interfere with input
		for (int i = 0; i < ImGuiKey_NamedKey_COUNT; i++)
			if (io_.KeysData[i].Down)
				return;

		auto &textState = io_.Ctx->InputTextState;

		if (textState.Flags & ImGuiInputTextFlags_ReadOnly)
		{
			return;
		}

		auto firstSelected =
		    std::min (textState.GetSelectionStart (), textState.GetSelectionEnd ());
		auto selectedCount =
		    std::abs (textState.GetSelectionEnd () - textState.GetSelectionStart ());

		SwkbdState kbd;
		// Make enough room to store 1.33x the length of the original text.
		// For the worst case, assuming all existing chars take 1 UTF-8 code
		// units each and those in the new text would take 3 UTF-8 code units
		// each, which means len * 3 * 1.33 = len * 4.
		std::vector<char> inputBuffer (std::max (512, selectedCount * 4 + 1));

		memcpy (inputBuffer.data (), textState.TextA.Data + firstSelected, selectedCount);
		inputBuffer[selectedCount] = 0;

		// For the worst case, 1 UTF-16 code unit can take 3 UTF-8 code units.
		swkbdInit (&kbd, SWKBD_TYPE_NORMAL, 2, (inputBuffer.size () - 1) / 3);
		swkbdSetButton (&kbd, SWKBD_BUTTON_LEFT, "Cancel", false);
		swkbdSetButton (&kbd, SWKBD_BUTTON_RIGHT, "OK", true);
		// Data is already nul-terminated.
		swkbdSetInitialText (&kbd, inputBuffer.data ());

		u32 features = SWKBD_PREDICTIVE_INPUT;

		if (textState.Flags & ImGuiInputTextFlags_Multiline)
			features |= SWKBD_MULTILINE;

		swkbdSetFeatures (&kbd, features);

		if (textState.Flags & ImGuiInputTextFlags_Password)
			swkbdSetPasswordMode (&kbd, SWKBD_PASSWORD_HIDE_DELAY);

		auto const button = swkbdInputText (&kbd, inputBuffer.data (), inputBuffer.size ());
		if (button == SWKBD_BUTTON_RIGHT)
		{
			if (inputBuffer[0])
			{
				io_.AddInputCharactersUTF8 (inputBuffer.data ());
			}
			else
			{
				if (selectedCount)
				{
					io_.AddKeyEvent (ImGuiKey_Backspace, true);
					io_.AddKeyEvent (ImGuiKey_Backspace, false);
				}
			}
		}

		state = KEYBOARD;
		break;
	}

	case KEYBOARD:
		state = INACTIVE;
		break;
	}
}
}

static TickCounter tick_counter;

bool imgui::ctru::init ()
{
	auto &io = ImGui::GetIO ();

	// setup config flags
	io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	// setup platform backend
	io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
	io.BackendPlatformName = "3DS";

	// enable Nintendo button layout
	io.ConfigNavSwapGamepadButtons = true;

	// disable mouse cursor
	io.MouseDrawCursor = false;

	auto &platformIO = ImGui::GetPlatformIO ();

	// clipboard callbacks
	platformIO.Platform_SetClipboardTextFn = &setClipboardText;
	platformIO.Platform_GetClipboardTextFn = &getClipboardText;
	platformIO.Platform_ClipboardUserData  = nullptr;

	platformIO.Platform_SetImeDataFn = &setImeData;

	osTickCounterStart (&tick_counter);

	// we only support touchscreen as mouse source
	io.AddMouseSourceEvent (ImGuiMouseSource_TouchScreen);

	return true;
}

void imgui::ctru::newFrame (bool topScreen, bool bottomScreen, bool enableGamepad)
{
	auto &io = ImGui::GetIO ();

	// check that font was built
	IM_ASSERT (io.Fonts->IsBuilt () &&
	           "Font atlas not built! It is generally built by the renderer back-end. Missing call "
	           "to renderer _NewFrame() function?");

	osTickCounterUpdate (&tick_counter);
	io.DeltaTime = osTickCounterRead (&tick_counter) / 1000.;

	updateTouch (io, topScreen && bottomScreen);
	updateGamepads (io, enableGamepad);
	updateKeyboard (io);
}

void imgui::ctru::beforeRender ()
{
	if (s_imeData.WantVisible)
	{
		ImVec2 vpBottomRight = {320, 240};
		if (s_viewportWantIme)
		{
			vpBottomRight = s_viewportWantIme->WorkPos + s_viewportWantIme->WorkSize;
		}
		constexpr ImVec2 popupSize = k_imePopupSize;
		ImVec2 pos                 = s_imeData.InputPos + ImVec2{0, s_imeData.InputLineHeight};
		if (pos.x + popupSize.x > vpBottomRight.x)
		{
			pos.x = vpBottomRight.x - popupSize.x;
		}
		if (pos.y + popupSize.y > vpBottomRight.y)
		{
			pos.y = s_imeData.InputPos.y - popupSize.y;
		}
		auto *draw_list = ImGui::GetForegroundDrawList ();
		draw_list->AddRectFilled (pos, pos + popupSize, IM_COL32 (255, 255, 0, 192));
		draw_list->AddText (
		    pos + ImVec2{2, 2}, IM_COL32 (0, 0, 0, 255), "\uE056"); // Return key symbol
		s_imePopupActive = true;
		s_imePopupPos    = pos;
	}
	else
	{
		s_imePopupActive = false;
	}
}
