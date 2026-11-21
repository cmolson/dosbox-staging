// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_KIOSK_OVERLAY_H
#define DOSBOX_KIOSK_OVERLAY_H

#include "dosbox_config.h"

#if C_OPENGL

#include <string>

#include "utils/rect.h"

// Glad must be included before SDL
#include "glad/gl.h"

// Draws the kiosk panel into the border next to the DOS image. Needs a
// current OpenGL context for its whole lifetime.
class KioskOverlay {
public:
	KioskOverlay() = default;
	~KioskOverlay();

	void Render(const DosBox::Rect canvas_px, const DosBox::Rect viewport_px);

	KioskOverlay(const KioskOverlay&)            = delete;
	KioskOverlay& operator=(const KioskOverlay&) = delete;

private:
	struct Texture {
		GLuint texture = 0;
		GLuint fbo     = 0;
		int width      = 0;
		int height     = 0;
	};

	static void Blit(const Texture& src, const int canvas_height_px,
	                 const DosBox::Rect dest_px, const GLenum filter);

	void LoadImage(const std::string& path);

	Texture panel = {};
	Texture image = {};

	std::string image_path = {};
};

#endif // C_OPENGL

#endif // DOSBOX_KIOSK_OVERLAY_H
