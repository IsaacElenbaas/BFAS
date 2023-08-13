R"(
struct Bezier {
	vec2 a1;
	vec2 h1;
	vec2 a2;
	vec2 h2;
};
Bezier bezier_crosses_bez = Bezier(vec2(0, 0), vec2(0, 0), vec2(0, 0), vec2(0, 0));

float crt(float n) { return (n < 0) ? -pow(-n, 1.0/3.0) : pow(n, 1.0/3.0); }
vec2 bezier_crosses_bez_pos(float t) {
	return vec2(
		(1-t)*(1-t)*(1-t)*bezier_crosses_bez.a1.x+3*t*(1-t)*(1-t)*bezier_crosses_bez.h1.x+3*(1-t)*t*t*bezier_crosses_bez.h2.x+t*t*t*bezier_crosses_bez.a2.x,
		(1-t)*(1-t)*(1-t)*bezier_crosses_bez.a1.y+3*t*(1-t)*(1-t)*bezier_crosses_bez.h1.y+3*(1-t)*t*t*bezier_crosses_bez.h2.y+t*t*t*bezier_crosses_bez.a2.y
	);
}
int bezier_crosses() {
	float a1 = bezier_crosses_bez.a1.y-position.y;
	float h1 = bezier_crosses_bez.h1.y-position.y;
	float a2 = bezier_crosses_bez.a2.y-position.y;
	float h2 = bezier_crosses_bez.h2.y-position.y;
	// get parametric (t^3 + at^2 + bt + c) form
	float d = -a1+3*h1-3*h2+a2;
	float a = (3*a1-6*h1+3*h2)/d;
	float b = (-3*a1+3*h1)/d;
	float c = a1/d;
	float p = (3*b-a*a)/3;
	float q = (2*a*a*a-9*a*b+27*c)/27;
	float q2 = q/2;
	// GLSL doesn't return negative for pow(-X, 3) - or even 3 if X is a float, converts
	// was the cause of many headaches
	float discriminant = q2*q2+p*p*p/27;
	float x[3];
	int sols = 0;

	// if the discriminant is negative, use polar coordinates to get around square roots of negative numbers
	if(discriminant < 0) {
		float r = sqrt(-p*p*p/27);
		float phi = acos(clamp(-q/(2*r), -1, 1));
		float t1 = 2*crt(r);
		x[0] = t1*cos(phi/3)-a/3;
		x[1] = t1*cos((phi+2*M_PI)/3)-a/3;
		x[2] = t1*cos((phi+4*M_PI)/3)-a/3;
		sols = 3;
	}
	else if(discriminant == 0) {
		float u1 = -crt(q2);
		x[0] = 2*u1-a/3;
		x[1] = -u1-a/3;
		sols = 2;
	}
	else {
		float sd = sqrt(discriminant);
		x[0] = crt(-q2+sd)-crt(q2+sd)-a/3;
		sols = 1;
	}
	for(int i = sols-1; i >= 0; --i) {
		float t = x[i];
		vec2 p = bezier_crosses_bez_pos(x[i]);
		if(t < 0 || t > 1) {
			x[i] = x[sols-1];
			--sols;
		}
		else x[i] = p.x;
	}
	switch(sols) {
		case 3:
			return int(position.x <= x[0])+int(position.x <= x[1])+int(position.x <= x[2]);
		case 2:
			return int(position.x <= x[0])+int(position.x <= x[1]);
		case 1:
			return int(position.x <= x[0]);
	}
	return 0;
}
)"
