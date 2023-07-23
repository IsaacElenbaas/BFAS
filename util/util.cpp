#include <algorithm>
#define _USE_MATH_DEFINES
#include <cmath>
#include "util.h"

bool line_right(const point& p, const point& a, const point& b) {
	return (double)(b.x-a.x)*(p.y-a.y) > (double)(b.y-a.y)*(p.x-a.x);
}

// https://stackoverflow.com/questions/7348009/y-coordinate-for-a-given-x-cubic-bezier#comment41435696_17546429
#define crt(n) ((n < 0) ? -pow(-n, 1.0/3) : pow(n, 1.0/3))
int bezier_crosses(const point& pnt, const bezier& bez) {
	double a1 = bez.a1->y-pnt.y, h1 = bez.h1->y-pnt.y, a2 = bez.a2->y-pnt.y, h2 = bez.h2->y-pnt.y;
	// get parametric (t^3 + at^2 + bt + c) form
	double d = -a1+3*h1-3*h2+a2, a = (3*a1-6*h1+3*h2)/d, b = (-3*a1+3*h1)/d, c = a1/d;
	double p = (3*b - a*a)/3;
	double q = (2*a*a*a - 9*a*b + 27*c)/27, q2 = q/2;
	double discriminant = pow(q2, 2)+pow(p/3, 3);
	double x[3];
	int sols = 0;

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
		double u1 = -crt(q2);
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
		double t = x[i];
		point p = bez[x[i]];
		// p comparisons fix horizontal lines at anchor intersections
		if(t <= 0 || t >= 1) {
			x[i] = x[sols-1];
			sols--;
		}
		else x[i] = p.x;
	}
	switch(sols) {
		case 3:
			return (pnt.x <= x[0])+(pnt.x <= x[1])+(pnt.x <= x[2]);
		case 2:
			return (pnt.x <= x[0])+(pnt.x <= x[1]);
		case 1:
			return  pnt.x <= x[0];
	}
	return 0;
}
