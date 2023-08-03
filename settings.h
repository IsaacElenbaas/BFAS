#ifndef SETTINGS_H
#define SETTINGS_H
struct Settings {
	bool invert_scroll = false;
	bool color_picker_circle = false;
	bool color_picker_disable_shrinking = false;
	bool enable_voronoi = false;
	bool enable_influenced = false;
	bool enable_fireworks = false;
	unsigned int ratio_width = 1;
	unsigned int ratio_height = 1;
};
extern Settings settings;
#endif
