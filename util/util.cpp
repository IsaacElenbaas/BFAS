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
bool bezier_right(const point& pnt, const bezier& bez) {
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
	for(int i = sols; i >= 0; i--) {
		if(x[i-1] < 0 || x[i-1] > 1) {
			x[i-1] = x[sols-1];
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
				if((dot_sign(*bez.h1-*bez.a1, (*bez.a2-*bez.a1).right()) > 0) ^ (bez.a2->x < bez.a1->x)) {
					return pnt.x < x[0] || (x[1] < pnt.x && pnt.x < x[2]);
				}
				else {
					return (x[0] < pnt.x && pnt.x < x[1]) || x[2] < pnt.x;
				}
			}
		case 2:
			{
				x[0] = bez[x[0]].x;
				x[1] = bez[x[1]].x;
				if(x[1] < x[0]) std::swap(x[0], x[1]);
				point right = (*bez.a2-*bez.a1).right();
				if(
					(dot_sign(pnt-*bez.a1, right) > 0 && dot_sign(*bez.h1-*bez.a1, right) > 0) ||
					(dot_sign(pnt-*bez.a2, right) > 0 && dot_sign(*bez.h2-*bez.a2, right) > 0)
				) {
					return pnt.x < x[0] || x[1] < pnt.x;
				}
				return x[0] < pnt.x && pnt.x < x[1];
			}
		case 1:
			if(x[0] < 0 || x[0] > 1) return false;
			return (bez.a1->y < bez.a2->y) ^ (bez[x[0]].x < pnt.x);
		case 0:
			{
				point right = (*bez.a2-*bez.a1).right();
				return dot_sign(pnt-*bez.a1, right) > 0 ||
				       dot_sign(pnt-*bez.a2, right) > 0;
			}
		default: return false;
	}
}
