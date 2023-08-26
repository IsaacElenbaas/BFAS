#ifndef UTIL_H
#define UTIL_H
#include <cmath>
#include "BFAS.h"

/*{{{ point a2s/s2a(point p)*/
inline point a2s(const point& p, bool only_scale = false) {
	return {
		(int)round(state.zoom*(w-1)*((double)(p.x-((only_scale) ? 0 : state.tl.x))/cmax)),
		(int)round(state.zoom*(h-1)*((double)(p.y-((only_scale) ? 0 : state.tl.y))/cmax))
	};
}
inline point s2a(const point& p, bool only_scale = false) {
	return point(
		(int)round(((double)p.x/(w-1))*(cmax/state.zoom))+((only_scale) ? 0 : state.tl.x),
		(int)round(((double)p.y/(h-1))*(cmax/state.zoom))+((only_scale) ? 0 : state.tl.y)
	);
}
/*}}}*/

inline int dot_sign(const point& p1, const point& p2) {
	return std::copysign(1.0, (double)p1.x*p2.x+(double)p1.y*p2.y);
}

bool line_right(const point& p, const point& a, const point& b);
int bezier_crosses(const point& pnt, const bezier& bez);
#endif
