R"(
#version 460

in vec3 color;
out vec4 fragColor;

void main() {
	fragColor = vec4(color.r, 0.2, 0.2, 1.0);
}
)"
