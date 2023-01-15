// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server.h>
#include "settings.h"
#include "scene.h"
#include "util.h"
#include "window.h"

#define FONT ("Sans 10")
#define COLOR_BG (0x000000AA)
#define COLOR_FG (0xCCCCCCFF)
#define SCALE (1.0)

static struct {
	int x;
	int y;
	struct region *region;
} grab;

static void
plot_rect(cairo_t *cairo, struct dbox *box, uint32_t color, bool fill)
{
	double thickness = fill ? 0.0 : 1.0;
	cairo_save(cairo);
	set_source_u32(cairo, color);
	cairo_rectangle(cairo, box->x + thickness / 2.0, box->y + thickness / 2.0,
		box->width - thickness, box->height - thickness);
	cairo_set_line_width(cairo, thickness);
	if (fill) {
		cairo_fill(cairo);
	} else {
		cairo_stroke(cairo);
	}
	cairo_restore(cairo);
}

void
scene_update(cairo_t *cairo, struct state *state)
{
	static bool has_been_converted_from_percentage;
	if (!has_been_converted_from_percentage) {
		/*
		 * scene_update() is never called before the layer-surface is
		 * configured, so at this point the surface has width/height
		 * which is what we need to covert from percentages.
		 */
		convert_regions_from_percentage_to_pixels(state->window);
		has_been_converted_from_percentage = true;
	}

	/* Clear background */
	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	set_source_u32(cairo, 0x00000000);
	cairo_paint(cairo);
	cairo_restore(cairo);

	/* background */
	struct dbox box = {
		.width = state->window->surface->width,
		.height = state->window->surface->height,
	};
	plot_rect(cairo, &box, COLOR_BG, true);

	/* regions */
	set_source_u32(cairo, COLOR_FG);
	struct region *region;
	wl_list_for_each(region, state->config->regions, link) {
		plot_rect(cairo, &region->dbox, COLOR_FG, false);
		cairo_move_to(cairo, region->dbox.x + 5, region->dbox.y + 5);
		render_text(cairo, FONT, SCALE, region->name);
	}
}

static bool
box_empty(const struct dbox *box)
{
	return !box || box->width <= 0 || box->height <= 0;
}

static bool
box_contains_point(const struct dbox *box, double x, double y)
{
	if (box_empty(box)) {
		return false;
	}
	return x >= box->x && x < box->x + box->width
		&& y >= box->y && y < box->y + box->height;
}

void
scene_handle_cursor_motion(struct window *window, int x, int y)
{
	if (grab.region) {
		grab.region->dbox.x += x - grab.x;
		grab.region->dbox.y += y - grab.y;
		grab.x = x;
		grab.y = y;
	}
	surface_damage(window->surface);
}

void
scene_handle_button_pressed(struct state *state, int x, int y)
{
	grab.x = x;
	grab.y = y;

	struct region *region;
	wl_list_for_each(region, state->config->regions, link) {
		if (box_contains_point(&region->dbox, x, y)) {
			grab.region = region;
			return;
		}
	}
}

void
scene_handle_button_released(struct window *window, int x, int y)
{
	if (grab.region) {
		grab.region = NULL;
	} else {
		window->run_display = false;
	}
}

void
scene_handle_key(struct window *window, xkb_keysym_t keysym, uint32_t codepoint)
{
	switch (keysym) {
	case XKB_KEY_Up:
	case XKB_KEY_Down:
	case XKB_KEY_Right:
	case XKB_KEY_Left:
		break;
	case XKB_KEY_Escape:
		window->run_display = false;
		break;
	default:
		break;
	}
	surface_damage(window->surface);
}
