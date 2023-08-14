R"(
#version 460

uniform float u_zoom;
uniform vec2 u_tl;
in vec2 a_position;
in vec2 a_bezier_data;
in vec2 a_color_data;
out vec2 real_position;
out vec2 bezier_data;
out vec2 color_data;

void main() {
	gl_Position = vec4(
		 2*(a_position.x-u_tl.x)*u_zoom-1,
		-2*(a_position.y-u_tl.y)*u_zoom+1,
	0, 1);
	real_position = a_position.xy;
	bezier_data = a_bezier_data;
	color_data = a_color_data;
}
)"
