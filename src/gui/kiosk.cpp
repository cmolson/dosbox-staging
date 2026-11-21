// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/kiosk.h"

#include <algorithm>
#include <unordered_map>

#include "config/setup.h"
#include "dosbox.h"
#include "gui/common.h"
#include "gui/mapper.h"
#include "hardware/timer.h"
#include "misc/logging.h"
#include "misc/messages.h"
#include "misc/support.h"
#include "utils/checks.h"
#include "utils/fs_utils.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

namespace Kiosk {

struct GameInfo {
	std::string title             = {};
	std::string image_path        = {};
	PanelSide side                = PanelSide::Left;
	std::vector<Control> controls = {};
};

static struct {
	bool enabled                        = false;
	bool show_overlay                   = false;
	int idle_timeout_s                  = 0;
	int idle_display_s                  = 0;
	int kick_warning_s                  = 0;
	std::vector<std::string> menu_names = {};
	std::string menu_hint               = {};
} settings = {};

static struct {
	int64_t last_input_ms = 0;
	std::string program   = {};
	bool is_at_menu       = false;

	// Keyed by upper-case program name without extension
	std::unordered_map<std::string, GameInfo> games = {};
} state = {};

static std::string to_program_name(const std::string& segment_name,
                                   const std::string& canonical_name)
{
	auto name = canonical_name;

	if (const auto pos = name.find_last_of("\\/:"); pos != std::string::npos) {
		name = name.substr(pos + 1);
	}
	if (name.empty()) {
		name = segment_name;
	}

	for (auto& c : name) {
		if (!is_extended_printable_ascii(c)) {
			c = '?';
		}
	}
	trim(name);
	upcase(name);

	return name;
}

static std::string strip_extension(const std::string& name)
{
	return name.substr(0, name.find('.'));
}

static Control parse_control(const std::string& value)
{
	const auto colon = value.find(':');
	if (colon == std::string::npos) {
		return {"", value};
	}
	Control control = {value.substr(0, colon), value.substr(colon + 1)};
	trim(control.key);
	trim(control.action);
	return control;
}

// One section per program name, e.g.:
//
//   [DOOM]
//   title    = Doom (shareware)
//   image    = images/doom.png
//   panel    = right
//   controls = Arrows: move
//   controls = Ctrl: fire
//
static std::unordered_map<std::string, GameInfo> parse_games_file(
        const std::vector<std::string>& lines, const std_fs::path& base_dir)
{
	std::unordered_map<std::string, GameInfo> games = {};
	std::string section                             = {};

	for (auto line : lines) {
		trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';') {
			continue;
		}

		if (line.front() == '[' && line.back() == ']') {
			section = line.substr(1, line.size() - 2);
			trim(section);
			upcase(section);
			section = strip_extension(section);
			continue;
		}

		const auto eq = line.find('=');
		if (section.empty() || eq == std::string::npos) {
			continue;
		}

		auto key   = line.substr(0, eq);
		auto value = line.substr(eq + 1);
		trim(key);
		trim(value);
		lowcase(key);

		auto& game = games[section];

		if (key == "title") {
			game.title = value;
		} else if (key == "image") {
			game.image_path = (base_dir / value).string();
		} else if (key == "panel") {
			game.side = iequals(value, "right") ? PanelSide::Right
			                                    : PanelSide::Left;
		} else if (key == "controls") {
			game.controls.emplace_back(parse_control(value));
		}
	}
	return games;
}

static void load_games_file(const std::string& filename)
{
	if (filename.empty()) {
		return;
	}

	const auto path  = std_fs::absolute(filename);
	const auto lines = get_lines(path);
	if (!lines) {
		LOG_WARNING("KIOSK: Games file '%s' not found",
		            path.string().c_str());
		return;
	}

	state.games = parse_games_file(*lines, path.parent_path());
}

static GameInfo find_game_info()
{
	const auto it = state.games.find(strip_extension(state.program));
	return it == state.games.end() ? GameInfo{} : it->second;
}

static int idle_seconds()
{
	return static_cast<int>((GetTicks() - state.last_input_ms) / 1000);
}

static bool is_kick_enabled()
{
	return settings.enabled && settings.idle_timeout_s > 0;
}

static bool is_kick_pending()
{
	return is_kick_enabled() && !state.is_at_menu &&
	       idle_seconds() >= settings.idle_timeout_s;
}

static int seconds_until_kick()
{
	return settings.idle_timeout_s + settings.kick_warning_s - idle_seconds();
}

static bool is_overlay_visible()
{
	return settings.enabled && settings.show_overlay && !state.is_at_menu;
}

static std::optional<PanelSide> current_panel_side()
{
	if (!is_overlay_visible()) {
		return {};
	}
	return find_game_info().side;
}

static void return_to_menu(const std::string& reason)
{
	LOG_MSG("KIOSK: Returning to the menu from '%s' (%s)",
	        state.program.c_str(),
	        reason.c_str());
	DOSBOX_Restart();
}

static void menu_hotkey_handler(const bool pressed)
{
	if (pressed && !state.is_at_menu) {
		return_to_menu("hotkey");
	}
}

static void add_messages()
{
	MSG_Add("KIOSK_CONTROLS", "CONTROLS");
	MSG_Add("KIOSK_IDLE", "Idle %s of %s");
	MSG_Add("KIOSK_BACK_TO_MENU", "Back to menu in %ds");
}

static void init_config_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;

	auto enabled = section.AddBool("kiosk_enabled", OnlyAtStart, false);
	enabled->SetHelp(
	        "Restart DOSBox when a game has been left idle, so the launcher in\n"
	        "[autoexec] comes back up for the next player ('off' by default).");

	auto menu = section.AddString("kiosk_menu_program", OnlyAtStart, "RLOADER UI");
	menu->SetHelp(
	        "Space-separated program names (without extension) that make up the\n"
	        "launcher ('RLOADER UI' by default). These are never restarted.");

	auto timeout = section.AddInt("kiosk_idle_timeout", OnlyAtStart, 60);
	timeout->SetMinMax(0, 24 * 60 * 60);
	timeout->SetHelp(
	        "Seconds without keyboard, mouse or game controller input before the\n"
	        "return-to-menu warning appears (60 by default). 0 disables it.");

	auto warning = section.AddInt("kiosk_kick_warning", OnlyAtStart, 10);
	warning->SetMinMax(0, 600);
	warning->SetHelp(
	        "Seconds the return-to-menu warning is shown before restarting (10 by\n"
	        "default). Any input cancels it.");

	auto idle_display = section.AddInt("kiosk_idle_display", OnlyAtStart, 20);
	idle_display->SetMinMax(0, 24 * 60 * 60);
	idle_display->SetHelp(
	        "Show the idle timer in the overlay during the last this many seconds\n"
	        "before the warning (20 by default).");

	auto overlay = section.AddBool("kiosk_overlay", OnlyAtStart, true);
	overlay->SetHelp(
	        "Show the game title, controls and idle timer next to the image, which is\n"
	        "moved to the other edge to make room ('on' by default). The panel is on\n"
	        "the left unless the games file says otherwise. Not shown while the\n"
	        "launcher runs.");

	auto games = section.AddString("kiosk_games_file",
	                               OnlyAtStart,
	                               "kiosk-games.conf");
	games->SetHelp(
	        "Titles, images, panel sides and controls for the overlay, relative to the\n"
	        "working directory ('kiosk-games.conf' by default).");

	auto hint = section.AddString("kiosk_menu_hint",
	                              OnlyAtStart,
	                              "Press F12 to return to the game menu");
	hint->SetHelp(
	        "Text at the bottom of the overlay. Match it to the 'Kiosk Menu'\n"
	        "binding in the key mapper (F12 by default).");
}

} // namespace Kiosk

using namespace Kiosk;

void KIOSK_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("kiosk");
	init_config_settings(*section);
	add_messages();
}

void KIOSK_Init()
{
	const auto section = get_section("kiosk");
	assert(section);

	settings.enabled        = section->GetBool("kiosk_enabled");
	settings.show_overlay   = section->GetBool("kiosk_overlay");
	settings.idle_timeout_s = section->GetInt("kiosk_idle_timeout");
	settings.idle_display_s = section->GetInt("kiosk_idle_display");
	settings.kick_warning_s = section->GetInt("kiosk_kick_warning");
	settings.menu_hint      = section->GetString("kiosk_menu_hint");

	auto menu_programs = section->GetString("kiosk_menu_program");
	upcase(menu_programs);
	settings.menu_names = split(menu_programs);

	state.last_input_ms = GetTicks();

	if (!settings.enabled) {
		return;
	}

	load_games_file(section->GetString("kiosk_games_file"));

	MAPPER_AddHandler(menu_hotkey_handler, SDL_SCANCODE_F12, 0, "kioskmenu", "Kiosk Menu");
}

void KIOSK_NotifyProgramName(const std::string& segment_name,
                             const std::string& canonical_name)
{
	const auto prev_side = current_panel_side();

	state.program    = to_program_name(segment_name, canonical_name);
	state.is_at_menu = contains(settings.menu_names,
	                            strip_extension(state.program));

	if (current_panel_side() != prev_side) {
		GFX_RefreshViewport();
	}
}

void KIOSK_NotifyUserInput()
{
	state.last_input_ms = GetTicks();
}

void KIOSK_Update()
{
	if (is_kick_pending() && seconds_until_kick() <= 0) {
		return_to_menu(format_str("idle for %d s", idle_seconds()));
	}
}

int KIOSK_CalcImageX(const int canvas_width_px, const int image_width_px,
                     const int centred_x_px)
{
	const auto side = current_panel_side();
	if (!side) {
		return centred_x_px;
	}
	return (*side == PanelSide::Left) ? canvas_width_px - image_width_px : 0;
}

std::optional<OverlayContent> KIOSK_GetOverlayContent()
{
	if (!is_overlay_visible()) {
		return {};
	}

	const auto info = find_game_info();

	OverlayContent content = {};

	content.side       = info.side;
	content.title      = info.title.empty() ? state.program : info.title;
	content.image_path = info.image_path;
	content.controls   = info.controls;
	content.hint       = settings.menu_hint;

	if (is_kick_enabled()) {
		content.idle_s         = idle_seconds();
		content.idle_timeout_s = settings.idle_timeout_s;
		content.show_idle = settings.idle_timeout_s - content.idle_s <=
		                    settings.idle_display_s;
	}
	if (is_kick_pending()) {
		content.kick_in_s = std::max(seconds_until_kick(), 0);
	}

	return content;
}
