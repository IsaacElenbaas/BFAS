R"(
#version 460

#define MAX_COLORS 128
#define M_PI 3.1415926538

uniform bool u_enable_voronoi;
uniform bool u_enable_influenced;
uniform bool u_enable_fireworks;
uniform float u_zoom;
in vec2 real_position; vec2 position = vec2(0, 0);
in vec2 bezier_data;
layout(std430, binding = 0) restrict readonly buffer Beziers {
	vec2 beziers[];
};
in vec2 color_data;
layout(std430, binding = 1) restrict readonly buffer Colors {
	vec2 colors[];
};
out vec4 fragColor;

)"
#include "bezier_crosses.glsl"
R"(

vec2 circumcenter(vec2 a, vec2 b) {
	float d = 2*(a.x*b.y-a.y*b.x);
	return vec2(
		b.y*(a.x*a.x+a.y*a.y)-a.y*(b.x*b.x+b.y*b.y),
		a.x*(b.x*b.x+b.y*b.y)-b.x*(a.x*a.x+a.y*a.y)
	)/d;
}

void main() {
	if(int(color_data[0]) == 0) discard;
	position = real_position;
	int crosses = 0;
	for(int i = 0; i < int(bezier_data[0]); ++i) {
		bezier_crosses_bez.a1 = beziers[4*(int(bezier_data[1])+i)+0];
		bezier_crosses_bez.h1 = beziers[4*(int(bezier_data[1])+i)+1];
		bezier_crosses_bez.a2 = beziers[4*(int(bezier_data[1])+i)+2];
		bezier_crosses_bez.h2 = beziers[4*(int(bezier_data[1])+i)+3];
		crosses += bezier_crosses();
	}
	if((crosses & 1) != 1)
		discard;
	for(int i = 0; i < 3*int(color_data[0]); i += 3) {
		if(length(position-colors[int(color_data[1])+i]) < 0.001/u_zoom) {
			fragColor = vec4(colors[int(color_data[1])+i+1], colors[int(color_data[1])+i+2]);
			return;
		}
	}

	// +1 to not have to modulo all the time in outside-hull checking
	int c_s[MAX_COLORS+1];
	int color_count = min(MAX_COLORS, int(color_data[0]));
	for(int i = 0; i < color_count; ++i) { c_s[i] = int(color_data[1])+3*i; }
	// sort points by angle
	for(int i = color_count-1; i >= 0; --i) {
		for(int j = 0; j < i; ++j) {
			int a = c_s[j];
			int b = c_s[j+1];
			if(!u_enable_voronoi) {
				if(
					((colors[a].y-position.y > 0) != (colors[b].y-position.y > 0))
						? colors[a].y-position.y < 0
						: cross(vec3(colors[a]-position, 0), vec3(colors[b]-position, 0)).z < 0
				) {
					c_s[j] = b; c_s[j+1] = a;
				}
			}
			else {
				if(length(colors[c_s[j+1]]-position) < length(colors[c_s[j]]-position)) {
					c_s[j] = b; c_s[j+1] = a;
				}
			}
		}
	}
	if(u_enable_voronoi) {
		fragColor = vec4(colors[c_s[0]+1], colors[c_s[0]+2]);
		return;
	}

	if(!u_enable_fireworks) {
		if(color_count >= 3) {
			bool outside_hull = false;
			// check if position is outside hull of the colors, if so it should use linear interpolation between the two points making up the closest line
			c_s[color_count] = c_s[0];
			for(int i = 0; i < color_count; ++i) {
				if(cross(vec3(colors[c_s[i+1]]-position, 0), vec3(colors[c_s[i]]-position, 0)).z >= 0) {
					outside_hull = true;
					break;
				}
			}
			// loop over the colors, deleting one if it is not close enough to interject on the points around it
			int left = 0;
			int center = 1;
			int right = 2;
			for(int prune_progress = 0; prune_progress < color_count; ) {
				while(true) {
					if(left == right) break;
					else if(c_s[left] == -1) ;
					else break;
					left = (left-1+color_count)%color_count;
				}
				while(true) {
					if(right == left) break;
					else if(c_s[right] == -1) ;
					else break;
					right = (right+1)%color_count;
				}
				// wrapped around, done
				if(left == right) break;
				// colinear, delete the longest of the three
				if((colors[c_s[left]].x-position.x)*(colors[c_s[right]].y-position.y) == (colors[c_s[right]].x-position.x)*(colors[c_s[left]].y-position.y)) {
					float len_left   = length(colors[c_s[left]]  -position);
					float len_center = length(colors[c_s[center]]-position);
					float len_right  = length(colors[c_s[right]] -position);
					// better to move forward, check that first
					if(len_right == 0 || (len_right >= max(len_center, len_left) && max(len_center, len_left) > 0))
						c_s[right] = -1;
					else if(len_left == 0 || len_left >= len_center)
						c_s[left] = -1;
					else {
						c_s[center] = -1;
						center = left;
						prune_progress = 0;
						left = (left-1+color_count)%color_count;
					}
					continue;
				}
				// left and right are less than 180 apart
				if(cross(vec3(colors[c_s[right]]-position, 0), vec3(colors[c_s[left]]-position, 0)).z < 0) {
					bool keep = false;
					if(outside_hull) {
						// on the closer side of the line made by left and right
						float cross = cross(vec3(colors[c_s[right]]-colors[c_s[left]], 0), vec3(colors[c_s[center]]-colors[c_s[left]], 0)).z;
						if(cross > 0)
							keep = true;
						else if(cross == 0) {
							c_s[left] = -1;
							left = center;
							center = right;
							right = (right+1)%color_count;
							continue;
						}
					}
					else {
						// close enough to interject on the points around it
						vec2 corner = circumcenter(colors[c_s[left]]-position, colors[c_s[right]]-position);
						if(length(colors[c_s[center]]-(corner+position)) < length(corner)) keep = true;
					}
					if(!keep) {
						c_s[center] = -1;
						// just removed the last one's right, check it again instead of moving forward
						center = left;
						// I don't see why this would need to restart when center is backtracked - one is still consumed, just on the previous round and not counted - but it does (weird horizontal artifacts)
						prune_progress = 0;
						left = (left-1+color_count)%color_count;
						continue;
					}
				}
				left = center;
				center = right;
				right = (right+1)%color_count;
				++prune_progress;
			}
			// prune points to get closest line
			if(outside_hull) {
				left = (center-1+color_count)%color_count;
				right = (center+1)%color_count;
				for(int prune_progress = 0; prune_progress < color_count; ) {
					while(true) {
						if(left == right) break;
						else if(c_s[left] == -1) ;
						else break;
						left = (left-1+color_count)%color_count;
					}
					while(true) {
						if(right == left) break;
						else if(c_s[right] == -1) ;
						else break;
						right = (right+1)%color_count;
					}
					// wrapped around, done
					if(left == right) break;
					// left and right are less than 180 apart
					if(cross(vec3(colors[c_s[right]]-position, 0), vec3(colors[c_s[left]]-position, 0)).z < 0) {
						if(outside_hull) {
							// on the closer side of the line made by left and right
							float cross = cross(vec3(colors[c_s[right]]-colors[c_s[left]], 0), vec3(colors[c_s[center]]-colors[c_s[left]], 0)).z;
							if(cross > 0) {
								float len_center = length(colors[c_s[center]]-position);
								if(dot(colors[c_s[left]] -position, colors[c_s[center]]-position)/len_center > len_center)
									c_s[left] = -1;
								if(dot(colors[c_s[right]]-position, colors[c_s[center]]-position)/len_center > len_center)
									c_s[right] = -1;
								if(c_s[left] == -1 || c_s[right] == -1) continue;
							}
						}
					}
					left = center;
					center = right;
					right = (right+1)%color_count;
					++prune_progress;
				}
			}
		}
	}

	int final_color_count = 0;
	if(u_enable_influenced) {
		fragColor = vec4(0, 0, 0, 0);
		for(int i = 0; i < color_count; ++i) {
			if(c_s[i] == -1) continue;
			fragColor.r += colors[c_s[i]+1][0];
			fragColor.g += colors[c_s[i]+1][1];
			fragColor.b += colors[c_s[i]+2][0];
			fragColor.a += colors[c_s[i]+2][1];
			++final_color_count;
		}
		fragColor = vec4(fragColor/final_color_count);
		return;
	}
	for(int i = 0; i < color_count; ++i) {
		if(c_s[i] == -1) continue;
		c_s[final_color_count] = c_s[i];
		++final_color_count;
	}

	switch(final_color_count) {
		case 1:
			fragColor = vec4(colors[c_s[0]+1], colors[c_s[0]+2]);
			break;
		case 2:
			float transition_length = length(colors[c_s[1]]-colors[c_s[0]]);
			float t = max(0, min(1, dot(position-colors[c_s[0]], colors[c_s[1]]-colors[c_s[0]])/transition_length/transition_length));
			fragColor = vec4(
				(1-t)*colors[c_s[0]+1]+t*colors[c_s[1]+1],
				(1-t)*colors[c_s[0]+2]+t*colors[c_s[1]+2]
			);
			break;
		default:
			float weights = 0;
			fragColor = vec4(0, 0, 0, 0);
			int left = 0;
			int center = 1;
			int right = 2;
			while(true) {
				if(colors[c_s[center]] == position) {
					fragColor = vec4(colors[c_s[center]+1], colors[c_s[center]+2]);
					return;
				}
				vec2 border_left  = circumcenter(colors[c_s[left]]  -position, colors[c_s[center]]-position);
				vec2 border_right = circumcenter(colors[c_s[center]]-position, colors[c_s[right]] -position);
				float weight = length(border_right-border_left)/length(colors[c_s[center]]-position);
				fragColor.r += weight*colors[c_s[center]+1][0];
				fragColor.g += weight*colors[c_s[center]+1][1];
				fragColor.b += weight*colors[c_s[center]+2][0];
				fragColor.a += weight*colors[c_s[center]+2][1];
				weights += weight;
				if(center == 0) break;
				left = center;
				center = right;
				right = (right+1)%final_color_count;
			}
			fragColor = vec4(fragColor/weights);
	}
}
)"
