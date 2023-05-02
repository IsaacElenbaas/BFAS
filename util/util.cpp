#include <algorithm>
#define _USE_MATH_DEFINES
#include <cmath>
#include "util.h"

bool line_right(const point& p, const point& a, const point& b) {
	return (double)(b.x-a.x)*(p.y-a.y) > (double)(b.y-a.y)*(p.x-a.x);
}

// https://stackoverflow.com/questions/7348009/y-coordinate-for-a-given-x-cubic-bezier#comment41435696_17546429
static inline double crt(double n) {
	if(n < 0) return -pow(-n, 1.0/3);
	return pow(n, 1.0/3);
}
// TODO: can precalculate things in here
static bool bezier_right(const point& pnt, const bezier& bez) {
	double a1 = bez.a1->y-pnt.y, h1 = bez.h1->y-pnt.y, a2 = bez.a2->y-pnt.y, h2 = bez.h2->y-pnt.y;
	// get parametric (t^3 + at^2 + bt + c) form
	double d = -a1+3*h1-3*h2+a2, a = (3*a1-6*h1+3*h2)/d, b = (-3*a1+3*h1)/d, c = a1/d;
	double p = (3*b - a*a)/3;
	double q = (2*a*a*a - 9*a*b + 27*c)/27, q2 = q/2;
	double discriminant = pow(q2, 2)+pow(p/3, 3);
	double x[3];
	int sols;

	// if the discriminant is negative, use polar coordinates to get around square roots of negative numbers
	if(discriminant < 0) {
		double r = sqrt(pow(-p/3, 3));
		double phi = acos(std::clamp(-q/(2*r), -1.0, 1.0));
		double t1 = 2*crt(r);
		x[0] = t1*cos(phi/3)-a/3;
		x[1] = t1*cos((phi+2*M_PI)/3)-a/3;
		x[2] = t1*cos((phi+4*M_PI)/3)-a/3;
		sols = 3;
	}
	else if(discriminant == 0) {
		// TODO: gives bad data
		//       this causes the flickering line in sols=2
		double u1 = copysign(crt(q2), -1);
		x[0] = 2*u1-a/3;
		x[1] = -u1-a/3;
		sols = 2;
	}
	else {
		double sd = sqrt(discriminant);
		x[0] = crt(-q2+sd)-crt(q2+sd)-a/3;
		sols = 1;
	}
	for(int i = sols-1; i >= 0; i--) {
		if(x[i] < 0 || x[i] > 1) {
			x[i] = x[sols-1];
			sols--;
		}
	}
	switch(sols) {
		case 3:
			{
				x[0] = bez[x[0]].x;
				x[1] = bez[x[1]].x;
				x[2] = bez[x[2]].x;
				if(x[1] < x[0]) std::swap(x[0], x[1]);
				if(x[2] < x[1]) std::swap(x[1], x[2]);
				// TODO: breaks when handle enters triangle
				//       see screenshots
				if(line_right(*bez.h1, *bez.a1, *bez.a2) ^ (bez.a2->x < bez.a1->x))
					return pnt.x < x[0] || (x[1] < pnt.x && pnt.x < x[2]);
				else
					return (x[0] < pnt.x && pnt.x < x[1]) || x[2] < pnt.x;
			}
		case 2:
			{
				x[0] = bez[x[0]].x;
				x[1] = bez[x[1]].x;
				if(x[1] < x[0]) std::swap(x[0], x[1]);
				if(line_right(pnt, *bez.a1, *bez.a2) && (line_right(*bez.h1, *bez.a1, *bez.a2) || line_right(*bez.h2, *bez.a1, *bez.a2)))
					return pnt.x < x[0] || x[1] < pnt.x;
				return x[0] < pnt.x && pnt.x < x[1];
			}
		case 1:
			if(x[0] < 0 || x[0] > 1) return false;
			return (bez.a1->y < bez.a2->y) ^ (bez[x[0]].x < pnt.x);
		case 0:
			return line_right(pnt, *bez.a1, *bez.a2);
		default: return false;
	}
}

// TODO: include anything in green or self
//       exclude anything in own bounding box but not self
//       exclude anything in the two spaces made by own handles and neighbors'
//           this will need to be handled differently when handles are in right area
static point a1, h1, a2, h2;
static bool h1_r, h2_r;
int bezier_inc_exc(const point& p, const bezier& b, bool left) {
	if(*b.h1 == a1 && *b.h2 == a2)
		return (left ^ line_right(p, a1, a2)) ? 1 : 0;
	if(p.in(&b))
		return (bezier_right(p, b) != left) ? 1 : -1;
	// update stored variables
	if((!left)
		? a1 != *b.a1 || h1 != *b.h1 || a2 != *b.a2 || h2 != *b.h2
		: a1 != *b.a2 || h1 != *b.h2 || a2 != *b.a1 || h2 != *b.h1
	) {
		if(!left) { a1 = *b.a1; h1 = *b.h1; a2 = *b.a2; h2 = *b.h2; }
		else      { a1 = *b.a2; h1 = *b.h2; a2 = *b.a1; h2 = *b.h1; }
		if     (h1 == a1) h1 = h2;
		else if(h2 == a2) h2 = h1;
		h1_r = line_right(h1, a1, a2);
		h2_r = line_right(h2, a1, a2);
	}
	// right/left of handle
	bool roh =  line_right(p, a1, h1);
	bool loh = !line_right(p, a2, h2);
	// inside handles
	if((h1_r ^ roh) && (h2_r ^ loh)) {
		if(line_right(p, a1, a2))
			return 1;
		// ignore counter-clockwise and S curve exclusions
		else if(!h1_r && !h2_r)
			return -1;
	}
	// above with handles crossed, only exclude needs to be extended
	if(!h1_r && line_right(p, a1, h2) && !h2_r && !line_right(p, a2, h1) &&
	   !line_right(p, a1, a2)
	) return -1;
	// outside handles on counter-clockwise curves
	if(h1_r && h2_r && (roh || loh)) return 1;
	// right side of S curve, cropping by other line on closing end
	if((h1_r ^ h2_r) && (
		(roh || (h2_r && dot_sign(h2-a2, (h1-a1).right()) == 1)) &&
		(loh || (h1_r && dot_sign(h2-a2, (h1-a1).right()) == 1))
	)) return 1;
	return 0;
}
