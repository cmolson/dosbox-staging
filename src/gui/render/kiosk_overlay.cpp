// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "kiosk_overlay.h"

#if C_OPENGL

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <SDL.h>
#include <SDL_image.h>

#include "gui/kiosk.h"
#include "ints/int10.h"
#include "misc/logging.h"
#include "misc/messages.h"
#include "utils/checks.h"
#include "utils/string_utils.h"

CHECK_NARROWING();

namespace {

// The panel is drawn with the VGA 8x16 font at 1x, then scaled up by an
// integer factor. All sizes below are in unscaled pixels.
constexpr int GlyphWidth  = 8;
constexpr int GlyphHeight = 16;

constexpr int PaddingPx     = 12;
constexpr int SectionGapPx  = 10;
constexpr int ProgressBarPx = 8;

constexpr int MinColumns = 22;

using Rgba = std::array<uint8_t, 4>;

constexpr Rgba BackgroundColour = {0x00, 0x00, 0x00, 0xff};
constexpr Rgba TitleColour      = {0xff, 0xff, 0xff, 0xff};
constexpr Rgba TextColour       = {0xc8, 0xc8, 0xc8, 0xff};
constexpr Rgba DimColour        = {0x80, 0x80, 0x88, 0xff};
constexpr Rgba SeparatorColour  = {0x40, 0x40, 0x48, 0xff};
constexpr Rgba BarTrackColour   = {0x26, 0x26, 0x2c, 0xff};
constexpr Rgba GreenColour      = {0x4c, 0xc0, 0x5a, 0xff};
constexpr Rgba YellowColour     = {0xf0, 0xc0, 0x30, 0xff};
constexpr Rgba RedColour        = {0xf0, 0x50, 0x40, 0xff};
constexpr Rgba KeycapFill       = {0x30, 0x30, 0x38, 0xff};
constexpr Rgba KeycapEdge       = {0x78, 0x78, 0x84, 0xff};
constexpr Rgba KeycapShadow     = {0x18, 0x18, 0x1c, 0xff};

constexpr int KeycapPaddingPx = 4;
constexpr int KeycapGapPx     = 3;
constexpr int KeycapHeightPx  = GlyphHeight + 4;

class Canvas {
public:
	Canvas(const int width, const int height)
	        : width(width),
	          height(height),
	          pixels(static_cast<size_t>(width * height), BackgroundColour)
	{}

	void FillRect(const int x, const int y, const int w, const int h,
	              const Rgba colour)
	{
		for (auto py = std::max(y, 0); py < std::min(y + h, height); ++py) {
			for (auto px = std::max(x, 0); px < std::min(x + w, width);
			     ++px) {
				pixels[static_cast<size_t>(py * width + px)] = colour;
			}
		}
	}

	void DrawFrame(const int x, const int y, const int w, const int h,
	               const Rgba colour)
	{
		FillRect(x, y, w, 1, colour);
		FillRect(x, y + h - 1, w, 1, colour);
		FillRect(x, y, 1, h, colour);
		FillRect(x + w - 1, y, 1, h, colour);
	}

	void DrawText(const std::string& text, const int x, const int y,
	              const Rgba colour, const int size = 1)
	{
		auto pen_x = x;
		for (const auto c : text) {
			DrawGlyph(static_cast<uint8_t>(c), pen_x, y, colour, size);
			pen_x += GlyphWidth * size;
		}
	}

	const int width  = 0;
	const int height = 0;

	std::vector<Rgba> pixels = {};

private:
	void DrawGlyph(const uint8_t glyph, const int x, const int y,
	               const Rgba colour, const int size)
	{
		for (int row = 0; row < GlyphHeight; ++row) {
			const auto bits = int10_font_16[glyph * GlyphHeight + row];

			for (int col = 0; col < GlyphWidth; ++col) {
				if (bits & (0x80 >> col)) {
					FillRect(x + col * size,
					         y + row * size,
					         size,
					         size,
					         colour);
				}
			}
		}
	}
};

std::vector<std::string> word_wrap(const std::string& text, const int max_cols)
{
	std::vector<std::string> lines = {};
	std::string curr               = {};

	for (auto word : split(text, " ")) {
		while (std::ssize(word) > max_cols) {
			if (!curr.empty()) {
				lines.emplace_back(curr);
				curr.clear();
			}
			lines.emplace_back(
			        word.substr(0, static_cast<size_t>(max_cols)));
			word.erase(0, static_cast<size_t>(max_cols));
		}
		if (word.empty()) {
			continue;
		}

		if (curr.empty()) {
			curr = word;
		} else if (std::ssize(curr) + 1 + std::ssize(word) <= max_cols) {
			curr += " " + word;
		} else {
			lines.emplace_back(curr);
			curr = word;
		}
	}
	if (!curr.empty()) {
		lines.emplace_back(curr);
	}
	return lines;
}

// "Alt+Arrows" -> "Alt", "<-", "^", "v", "->" (code page 437 arrows)
std::vector<std::string> to_keycap_labels(const std::string& key)
{
	std::vector<std::string> labels = {};

	for (auto token : split(key, "+")) {
		trim(token);
		auto name = token;
		lowcase(name);

		if (name == "arrows") {
			for (const auto arrow : {"\x1b", "\x18", "\x19", "\x1a"}) {
				labels.emplace_back(arrow);
			}
		} else if (name == "up") {
			labels.emplace_back("\x18");
		} else if (name == "down") {
			labels.emplace_back("\x19");
		} else if (name == "left") {
			labels.emplace_back("\x1b");
		} else if (name == "right") {
			labels.emplace_back("\x1a");
		} else if (!token.empty()) {
			labels.emplace_back(token);
		}
	}
	return labels;
}

int keycap_width(const std::string& label)
{
	return static_cast<int>(label.size()) * GlyphWidth + 2 * KeycapPaddingPx;
}

int keycaps_width(const std::vector<std::string>& labels)
{
	int width = 0;
	for (const auto& label : labels) {
		width += keycap_width(label) + KeycapGapPx;
	}
	return std::max(width - KeycapGapPx, 0);
}

void draw_keycaps(Canvas& canvas, const std::vector<std::string>& labels,
                  const int x, const int y)
{
	auto cap_x = x;
	for (const auto& label : labels) {
		const auto w = keycap_width(label);

		canvas.FillRect(cap_x, y, w, KeycapHeightPx, KeycapFill);
		canvas.DrawFrame(cap_x, y, w, KeycapHeightPx, KeycapEdge);
		canvas.FillRect(cap_x + 1, y + KeycapHeightPx - 2, w - 2, 1, KeycapShadow);
		canvas.DrawText(label, cap_x + KeycapPaddingPx, y + 1, TitleColour);

		cap_x += w + KeycapGapPx;
	}
}

std::string format_duration(const int total_s)
{
	return format_str("%d:%02d", total_s / 60, total_s % 60);
}

Rgba progress_colour(const float fraction)
{
	if (fraction < 0.6f) {
		return GreenColour;
	}
	return (fraction < 0.85f) ? YellowColour : RedColour;
}

// The panel fills the border on one side of the image. Scale is 2x at 1080p
// and 4x at 2160p, reduced if the border is too narrow for MinColumns.
struct Layout {
	int window_x_px = 0;
	int width_px    = 0;
	int height_px   = 0;
	int scale       = 1;
};

std::optional<Layout> calc_layout(const DosBox::Rect canvas_px,
                                  const DosBox::Rect viewport_px,
                                  const Kiosk::PanelSide side)
{
	const auto canvas_w = static_cast<int>(canvas_px.w);
	const auto canvas_h = static_cast<int>(canvas_px.h);
	const auto image_x2 = static_cast<int>(viewport_px.x + viewport_px.w);

	const auto [window_x_px,
	            border_w] = (side == Kiosk::PanelSide::Left)
	                              ? std::pair{0,
	                                          static_cast<int>(viewport_px.x)}
	                              : std::pair{image_x2, canvas_w - image_x2};

	constexpr auto MinWidth = MinColumns * GlyphWidth + 2 * PaddingPx;

	const auto scale = std::min(std::max(canvas_h / 540, 1), border_w / MinWidth);
	if (scale < 1) {
		return {};
	}
	return Layout{window_x_px, border_w / scale, canvas_h / scale, scale};
}

// Returns where the image goes, if there is one
std::optional<DosBox::Rect> draw_top(Canvas& canvas,
                                     const Kiosk::OverlayContent& content,
                                     const int image_w, const int image_h,
                                     const int max_y)
{
	auto draw_line = [&](const std::string& text,
	                     const int x,
	                     const int y,
	                     const Rgba colour,
	                     const int size = 1) {
		if (y + GlyphHeight * size <= max_y) {
			canvas.DrawText(text, x, y, colour, size);
		}
	};

	const auto inner_w = canvas.width - 2 * PaddingPx;
	const auto cols    = inner_w / GlyphWidth;

	auto y = PaddingPx;

	std::optional<DosBox::Rect> image_rect = {};
	if (image_w > 0 && image_h > 0) {
		auto w = inner_w;
		auto h = w * image_h / image_w;

		if (const auto max_h = canvas.height * 2 / 5; h > max_h) {
			h = max_h;
			w = h * image_w / image_h;
		}
		const auto x = PaddingPx + (inner_w - w) / 2;

		canvas.DrawFrame(x - 1, y - 1, w + 2, h + 2, SeparatorColour);
		image_rect = DosBox::Rect{x, y, w, h};

		y += h + SectionGapPx + 4;
	}

	for (const auto& line : word_wrap(content.title, cols / 2)) {
		draw_line(line, PaddingPx, y, TitleColour, 2);
		y += GlyphHeight * 2;
	}

	y += SectionGapPx / 2;
	canvas.FillRect(PaddingPx, y, inner_w, 1, SeparatorColour);
	y += SectionGapPx;

	if (content.controls.empty()) {
		return image_rect;
	}

	draw_line(MSG_Get("KIOSK_CONTROLS"), PaddingPx, y, DimColour);
	y += GlyphHeight + 4;

	int keys_w = 0;
	for (const auto& control : content.controls) {
		keys_w = std::max(keys_w,
		                  keycaps_width(to_keycap_labels(control.key)));
	}
	keys_w = std::min(keys_w, inner_w / 2);

	const auto action_x    = PaddingPx + keys_w + GlyphWidth;
	const auto action_cols = (inner_w - keys_w - GlyphWidth) / GlyphWidth;

	for (const auto& control : content.controls) {
		const auto lines = word_wrap(control.action, action_cols);
		const auto row_h = std::max(KeycapHeightPx,
		                            static_cast<int>(lines.size()) *
		                                    GlyphHeight);
		if (y + row_h > max_y) {
			break;
		}

		draw_keycaps(canvas, to_keycap_labels(control.key), PaddingPx, y);

		auto line_y = y + 2;
		for (const auto& line : lines) {
			canvas.DrawText(line, action_x, line_y, TextColour);
			line_y += GlyphHeight;
		}
		y += row_h + 4;
	}
	return image_rect;
}

// Drawn bottom-up; returns the top edge
int draw_bottom(Canvas& canvas, const Kiosk::OverlayContent& content)
{
	const auto inner_w = canvas.width - 2 * PaddingPx;
	const auto cols    = inner_w / GlyphWidth;

	auto y = canvas.height - PaddingPx;

	const auto hint_lines = word_wrap(content.hint, cols);
	y -= static_cast<int>(hint_lines.size()) * GlyphHeight;

	auto line_y = y;
	for (const auto& line : hint_lines) {
		canvas.DrawText(line, PaddingPx, line_y, DimColour);
		line_y += GlyphHeight;
	}

	if (!hint_lines.empty()) {
		y -= SectionGapPx;
		canvas.FillRect(PaddingPx, y, inner_w, 1, SeparatorColour);
		y -= SectionGapPx;
	}

	if (!content.show_idle) {
		return y;
	}

	const auto fraction = std::min(static_cast<float>(content.idle_s) /
	                                       static_cast<float>(content.idle_timeout_s),
	                               1.0f);
	y -= ProgressBarPx;
	canvas.FillRect(PaddingPx, y, inner_w, ProgressBarPx, BarTrackColour);
	canvas.FillRect(PaddingPx,
	                y,
	                static_cast<int>(static_cast<float>(inner_w) * fraction),
	                ProgressBarPx,
	                progress_colour(fraction));
	y -= 6;

	y -= GlyphHeight;
	canvas.DrawText(format_str(MSG_Get("KIOSK_IDLE"),
	                           format_duration(content.idle_s).c_str(),
	                           format_duration(content.idle_timeout_s).c_str()),
	                PaddingPx,
	                y,
	                TextColour);

	if (content.kick_in_s) {
		const auto colour = (*content.kick_in_s % 2 == 0) ? RedColour
		                                                  : YellowColour;

		const auto big_lines = word_wrap(format_str(MSG_Get("KIOSK_BACK_TO_MENU"),
		                                            *content.kick_in_s),
		                                 cols / 2);
		y -= SectionGapPx +
		     static_cast<int>(big_lines.size()) * GlyphHeight * 2;

		auto line_y = y;
		for (const auto& line : big_lines) {
			canvas.DrawText(line, PaddingPx, line_y, colour, 2);
			line_y += GlyphHeight * 2;
		}
	}
	return y;
}

DosBox::Rect to_window_rect(const DosBox::Rect r, const Layout& layout)
{
	return r.Copy()
	        .Scale(static_cast<float>(layout.scale))
	        .Translate(static_cast<float>(layout.window_x_px), 0.0f);
}

void upload(GLuint& texture, GLuint& fbo, const int width, const int height,
            const void* pixels, const int row_length_px)
{
	if (!texture) {
		glGenTextures(1, &texture);
		glGenFramebuffers(1, &fbo);
	}

	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length_px);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER,
	                       GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D,
	                       texture,
	                       0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void free_texture(GLuint& texture, GLuint& fbo)
{
	if (fbo) {
		glDeleteFramebuffers(1, &fbo);
		fbo = 0;
	}
	if (texture) {
		glDeleteTextures(1, &texture);
		texture = 0;
	}
}

} // namespace

KioskOverlay::~KioskOverlay()
{
	free_texture(panel.texture, panel.fbo);
	free_texture(image.texture, image.fbo);
}

void KioskOverlay::Blit(const Texture& src, const int canvas_height_px,
                        const DosBox::Rect dest_px, const GLenum filter)
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, src.fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	// Our textures are top row first; OpenGL's origin is bottom-left
	const auto x0     = static_cast<GLint>(dest_px.x);
	const auto x1     = static_cast<GLint>(dest_px.x + dest_px.w);
	const auto top    = canvas_height_px - static_cast<GLint>(dest_px.y);
	const auto bottom = top - static_cast<GLint>(dest_px.h);

	glBlitFramebuffer(0, 0, src.width, src.height, x0, top, x1, bottom, GL_COLOR_BUFFER_BIT, filter);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void KioskOverlay::LoadImage(const std::string& path)
{
	if (path == image_path) {
		return;
	}
	image_path = path;

	free_texture(image.texture, image.fbo);
	image.width  = 0;
	image.height = 0;

	if (path.empty()) {
		return;
	}

	auto loaded = IMG_Load(path.c_str());
	if (!loaded) {
		LOG_WARNING("KIOSK: Cannot load image '%s': %s",
		            path.c_str(),
		            IMG_GetError());
		return;
	}

	auto rgba = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
	SDL_FreeSurface(loaded);
	if (!rgba) {
		return;
	}

	image.width  = rgba->w;
	image.height = rgba->h;
	upload(image.texture, image.fbo, rgba->w, rgba->h, rgba->pixels, rgba->pitch / 4);

	SDL_FreeSurface(rgba);
}

void KioskOverlay::Render(const DosBox::Rect canvas_px, const DosBox::Rect viewport_px)
{
	const auto content = KIOSK_GetOverlayContent();
	if (!content) {
		return;
	}

	const auto layout = calc_layout(canvas_px, viewport_px, content->side);
	if (!layout) {
		return;
	}

	LoadImage(content->image_path);

	Canvas canvas(layout->width_px, layout->height_px);

	const auto bottom_y   = draw_bottom(canvas, *content);
	const auto image_rect = draw_top(canvas,
	                                 *content,
	                                 image.width,
	                                 image.height,
	                                 bottom_y - SectionGapPx);

	panel.width  = canvas.width;
	panel.height = canvas.height;
	upload(panel.texture,
	       panel.fbo,
	       canvas.width,
	       canvas.height,
	       canvas.pixels.data(),
	       0);

	const auto canvas_h = static_cast<int>(canvas_px.h);

	Blit(panel,
	     canvas_h,
	     to_window_rect(DosBox::Rect{canvas.width, canvas.height}, *layout),
	     GL_NEAREST);

	if (image_rect) {
		Blit(image, canvas_h, to_window_rect(*image_rect, *layout), GL_LINEAR);
	}
}

#endif // C_OPENGL
