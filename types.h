#ifndef TYPES_H
#define TYPES_H
#include <cmath>
#include <limits>
#include <tuple>

#define cmax std::numeric_limits<int>::max()

struct point {
	int x;
	int y;
	bool visible = false;
	bool operator==(const point& rhs) {
		return std::tie(x, y) == std::tie(rhs.x, rhs.y);
	}
	point operator+(const point& rhs) {
		return {x+rhs.x, y+rhs.y};
	}
	point operator-(const point& rhs) {
		return {x-rhs.x, y-rhs.y};
	}
	point operator*(int rhs) {
		return {rhs*x, rhs*y};
	}
	point operator/(double rhs) {
		return {(int)round(x/rhs), (int)round(y/rhs)};
	}
};

struct bezier {
	point *a1, *h1, *a2, *h2;
	bool visible = false;
	point operator[](double t) const {
		return {
			(int)round(pow(1-t,3)*a1->x+3*t*pow(1-t,2)*h1->x+3*pow(t,2)*(1-t)*h2->x+pow(t,3)*a2->x),
			(int)round(pow(1-t,3)*a1->y+3*t*pow(1-t,2)*h1->y+3*pow(t,2)*(1-t)*h2->y+pow(t,3)*a2->y)
		};
	}
};
#endif
