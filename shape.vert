R"(
#version 460

uniform float u_depth;
uniform float u_zoom;
uniform vec2 u_tl;
in vec3 a_position;
in vec2 a_bezier_data;
in vec3 a_color;
out vec3 color;

void main() {
	gl_Position = vec4(
		 2*(a_position.x-u_tl.x)*u_zoom-1,
		-2*(a_position.y-u_tl.y)*u_zoom+1,
		a_position.z,
	1.0);
	color = a_color;
}
)"
