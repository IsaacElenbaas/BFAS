R"(
#version 460

#define M_PI 3.1415926538

uniform bool u_circle;
uniform bool u_disable_shrinking;
uniform float u_lightness;
in vec2 real_position;
out vec4 fragColor;

void main() {
	// dividing here is important, limits output more which is helpful
	float hex_edge_t = atan(real_position.y/real_position.x)+M_PI/6;
	// modulo M_PI/3
	if(hex_edge_t < 0) hex_edge_t += M_PI/3;
	if(hex_edge_t > M_PI/3) hex_edge_t -= M_PI/3;
	// t from angle
	hex_edge_t = cos(M_PI/6)*hex_edge_t/(M_PI/3);
	float hex_dist = sqrt(pow(1-hex_edge_t/sqrt(3), 2)+hex_edge_t*hex_edge_t);
	vec2 position = real_position;
	float limit;
	if(!u_circle)
		limit = hex_dist;
	else {
		limit = 1;
		position *= hex_dist;
	}
	float lightness = u_lightness;
	if(!u_disable_shrinking) {
		if(length(real_position) > limit) discard;
		if(length(real_position) > lightness*limit)
			lightness = length(real_position)/limit;
	}
	else {
		position *= lightness;
		if(length(real_position) > limit) discard;
	}
	vec2 r = position;
	vec2 g = vec2(cos(2.0/3*M_PI)*r.x-sin(2.0/3*M_PI)*r.y,
	              sin(2.0/3*M_PI)*r.x+cos(2.0/3*M_PI)*r.y);
	vec2 b = vec2(cos(4.0/3*M_PI)*r.x-sin(4.0/3*M_PI)*r.y,
	              sin(4.0/3*M_PI)*r.x+cos(4.0/3*M_PI)*r.y);
	fragColor = vec4(
		max(0, min(lightness, r.y-abs(tan(M_PI/6)*r.x)+lightness)),
		max(0, min(lightness, g.y-abs(tan(M_PI/6)*g.x)+lightness)),
		max(0, min(lightness, b.y-abs(tan(M_PI/6)*b.x)+lightness)),
	1);
}
)"
