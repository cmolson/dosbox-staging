// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_KIOSK_H
#define DOSBOX_KIOSK_H

#include <optional>
#include <string>
#include <vector>

#include "config/config.h"

// Kiosk mode restarts DOSBox, and with it the launcher in [autoexec], when a
// game has been left idle for too long.

namespace Kiosk {

enum class PanelSide { Left, Right };

struct Control {
	std::string key    = {};
	std::string action = {};
};

struct OverlayContent {
	PanelSide side = PanelSide::Left;

	std::string title      = {};
	std::string image_path = {};
	std::string hint       = {};

	std::vector<Control> controls = {};

	bool show_idle     = false;
	int idle_s         = 0;
	int idle_timeout_s = 0;

	std::optional<int> kick_in_s = {};
};

} // namespace Kiosk

void KIOSK_AddConfigSection(const ConfigPtr& conf);
void KIOSK_Init();

void KIOSK_NotifyProgramName(const std::string& segment_name,
                             const std::string& canonical_name);

void KIOSK_NotifyUserInput();

// Called from the main loop; may restart the emulator
void KIOSK_Update();

int KIOSK_CalcImageX(const int canvas_width_px, const int image_width_px,
                     const int centred_x_px);

std::optional<Kiosk::OverlayContent> KIOSK_GetOverlayContent();

#endif // DOSBOX_KIOSK_H
