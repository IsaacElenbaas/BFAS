R"(
#version 460

uniform float u_zoom;
uniform vec2 u_tl;
in vec2 a_position;
out vec2 real_position;

void main() {
	gl_Position = vec4(
		 2*(a_position.x-u_tl.x)*u_zoom-1,
		-2*(a_position.y-u_tl.y)*u_zoom+1,
	0, 1);
	real_position = vec2(
		 2*a_position.x-1,
		-2*a_position.y+1
	);
}
)"
