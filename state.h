#ifndef STATE_H
#include "types.h"
#include "util/shapes.h"
#define STATE_H

enum class Actions {
	Idle,
	MovingPoint,
	MovingPointUnMerge,
	PlacingBezier
};
struct State {
	double zoom_linear = 0;
	double zoom = 1;
	point tl = {0, 0};
	point br;
	Actions action = Actions::Idle;
	bool view_final = false;
	point* p;
	point p_o;
	point p_last;
	point** p_prefer;
	bezier* b;
	Shape* s = NULL;
	bool moving_color;
	point* last_color = NULL;
};
extern State state;
#endif
