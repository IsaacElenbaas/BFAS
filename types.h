#ifndef TYPES_H
#define TYPES_H
#include <algorithm>
#include <cmath>
#include <forward_list>
#include <limits>
#include <tuple>
#include "resource.h"

#define cmax std::numeric_limits<int>::max()

struct bezier;
struct point;
extern Resource<point> point_RS;
extern Resource<bezier> bezier_RS;

struct point {
	int x, y;
	int use_count = 0;
	std::forward_list<bezier*> used_by;
	bool visible = false;
	void add_to(bezier* b) { use_count++; used_by.remove(b); used_by.push_front(b); }
	void remove_from(bezier* b);
	bool in(const point& tl, const point& br) const {
		if(
			x >= tl.x && y >= tl.y &&
			x <= br.x && y <= br.y
		) return true;
		return false;
	}
	bool in(const point& a, const point& b, const point& c) const;
	bool in(const point& a, const point& b, const point& c, const point& d) const {
		// Randolph Franklin ray test
		int n = 0;
		if(y > std::min(a.y, b.y) && y <= std::max(a.y, b.y) && x <= ((b.y-a.y != 0) ? a.x+(y-a.y)*((double)(b.x-a.x)/(b.y-a.y)) : std::max(a.x, b.x))) n++;
		if(y > std::min(b.y, c.y) && y <= std::max(b.y, c.y) && x <= ((c.y-b.y != 0) ? b.x+(y-b.y)*((double)(c.x-b.x)/(c.y-b.y)) : std::max(b.x, c.x))) n++;
		if(y > std::min(c.y, d.y) && y <= std::max(c.y, d.y) && x <= ((d.y-c.y != 0) ? c.x+(y-c.y)*((double)(d.x-c.x)/(d.y-c.y)) : std::max(c.x, d.x))) n++;
		if(y > std::min(d.y, a.y) && y <= std::max(d.y, a.y) && x <= ((a.y-d.y != 0) ? d.x+(y-d.y)*((double)(a.x-d.x)/(a.y-d.y)) : std::max(d.x, a.x))) n++;
		return n%2 != 0;
	}
	bool in(const bezier* b) const;
	point right() const { return {-this->y, this->x}; }
	point(int x1 = 0, int y1 = 0) : x(x1), y(y1), used_by() {}
	point(const point& rhs) : x(rhs.x), y(rhs.y), used_by() {}
	point& operator=(const point& rhs) {
		x = rhs.x; y = rhs.y;
		return *this;
	}
	bool operator==(const point& rhs) const {
		return std::tie(x, y) == std::tie(rhs.x, rhs.y);
	}
	bool operator!=(const point& rhs) const { return !(*this == rhs); }
	point operator+(const point& rhs) {
		return {x+rhs.x, y+rhs.y};
	}
	point operator-(const point& rhs) const {
		return {x-rhs.x, y-rhs.y};
	}
	point operator*(double rhs) const {
		return {(int)round(rhs*x), (int)round(rhs*y)};
	}
	point operator/(double rhs) const {
		return {(int)round(x/rhs), (int)round(y/rhs)};
	}
	point operator+=(const point& rhs) { *this = *this+rhs; return *this; }
	point operator-=(const point& rhs) { *this = *this-rhs; return *this; }
	point operator*=(double rhs) { *this = *this*rhs; return *this; }
	point operator/=(double rhs) { *this = *this/rhs; return *this; }
	point* clone() { point* ret = point_RS.get(); *ret = *this; return ret; }
	void release() { point_RS.release(this); }
};

inline bool line_rect_intersect(const point& a, const point& b, const point& tl, const point& br) {
	int n;
	if(b.x-a.x != 0) {
		n = a.y+(tl.x-a.x)*((double)(b.y-a.y)/(b.x-a.x));
		if(n >= tl.y && n <= br.y) return true;
		n = a.y+(br.x-a.x)*((double)(b.y-a.y)/(b.x-a.x));
		if(n >= tl.y && n <= br.y) return true;
	}
	else if(a.x == tl.x && ((a.y >= tl.y && a.y <= br.y) || (b.y >= tl.y && b.y <= br.y))) return true;
	if(b.y-a.y != 0) {
		n = a.x+(tl.y-a.y)*((double)(b.x-a.x)/(b.y-a.y));
		if(n >= tl.x && n <= br.x) return true;
		n = a.x+(br.y-a.y)*((double)(b.x-a.x)/(b.y-a.y));
		if(n >= tl.x && n <= br.x) return true;
	}
	else if(a.y == tl.y && ((a.x >= tl.x && a.x <= br.x) || (b.x >= tl.x && b.x <= br.x))) return true;
	return false;
}

extern point t_a1, t_h1, t_a2, t_h2;
struct bezier {
	point *a1, *h1, *a2, *h2;
	bool used = false;
	point* endpoint(bool left) { return (!left) ? a2 : a1; }
	bezier split(double t, bool left) const {
		if(left) {
			// h1 may point to t_h1, would break .split().split()
			point old_h1 = *h1;
			t_a2 = (*this)[t];
			t_a1 = *a1;
			t_h1 = (*h1-*a1)*t+*a1;
			t_h2 = (((*h2-old_h1)*t+old_h1)-t_h1)*t+t_h1;
		}
		else {
			// h2 may point to t_h2, would break .split().split()
			point old_h2 = *h2;
			t_a1 = (*this)[t];
			t_a2 = *a2;
			t_h2 = (*h2-*a2)*(1-t)+*a2;
			t_h1 = (((*h1-old_h2)*(1-t)+old_h2)-t_h2)*(1-t)+t_h2;
		}
		return {&t_a1, &t_h1, &t_a2, &t_h2};
	}
	bool intersecting(const point& tl, const point& br) const {
		if(
			br.x < std::min({a1->x, h1->x, a2->x, h2->x}) ||
			br.y < std::min({a1->y, h1->y, a2->y, h2->y}) ||
			tl.x > std::max({a1->x, h1->x, a2->x, h2->x}) ||
			tl.y > std::max({a1->y, h1->y, a2->y, h2->y})
		) return false;
		if((*a1).in(tl, br)) return true;
		if(tl.in(this)) return true;
		if(line_rect_intersect(*a1, *h1, tl, br)) return true;
		if(line_rect_intersect(*a2, *h2, tl, br)) return true;
		if(line_rect_intersect(*a1, *a2, tl, br)) return true;
		if(line_rect_intersect(*h1, *h2, tl, br)) return true;
		return false;
	}
	point operator[](double t) const {
		return {
			(int)round(pow(1-t,3)*a1->x+3*t*pow(1-t,2)*h1->x+3*(1-t)*pow(t,2)*h2->x+pow(t,3)*a2->x),
			(int)round(pow(1-t,3)*a1->y+3*t*pow(1-t,2)*h1->y+3*(1-t)*pow(t,2)*h2->y+pow(t,3)*a2->y)
		};
	}
	bezier* clone() {
		bezier* ret = bezier_RS.get();
		ret->a1 = a1->clone();
		ret->h1 = h1->clone();
		ret->a2 = a2->clone();
		ret->h2 = h2->clone();
		return ret;
	}
	void release() {
		a1->remove_from(this); if(a1->use_count == 0) a1->release();
		h1->remove_from(this); if(h1->use_count == 0) h1->release();
		a2->remove_from(this); if(a2->use_count == 0) a2->release();
		h2->remove_from(this); if(h2->use_count == 0) h2->release();
		bezier_RS.release(this);
	}
};

inline void point::remove_from(bezier* b) {
	use_count--;
	if(b->a1 != this && b->h1 != this && b->a2 != this && b->h2 != this)
		used_by.remove(b);
}
inline bool point::in(const bezier* b) const { return in(*b->a1, *b->h1, *b->a2, *b->h2) || in(*b->a1, *b->h1, *b->h2, *b->a2); }
extern bool line_right(const point& p, const point& a, const point& b);
inline bool point::in(const point& a, const point& b, const point& c) const {
	if(*this == a || *this == b || *this == c) return false;
	bool first = line_right(*this, a, b);
	return first == line_right(*this, b, c) && first == line_right(*this, c, a);
}
#endif
