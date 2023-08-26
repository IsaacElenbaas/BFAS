#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include "BFAS.h"
#include "quadtree.h"
#include "resource.h"
#include "settings.h"
#include "shapes.h"
#include "util.h"
// TODO: remove
#include <iostream>

#include <bit>
template<typename T>
constexpr T hton(T value) noexcept {
	if(std::endian::native == std::endian::big) return value;
	static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
	auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
	std::ranges::reverse(value_representation);
	return std::bit_cast<T>(value_representation);
}
#define ntoh(value) hton(value)

// TODO: everything else should include UI.h
#include "UI.cpp"

extern void load_aspect_ratio();

// for temporary beziers to point to
point t_a1, t_h1, t_a2, t_h2;

int w, h;
point mouse;
State state;
Settings settings;
Resource<point> point_RS;
Resource<bezier> bezier_RS;
QuadTree<point> point_QT(25);
QuadTree<bezier> bezier_QT(25);

/*{{{ void paint_bezier(bezier b)*/
// NOTE: this CANNOT PAINT beziers generated with .split() unless they are deep cloned
//       .split() uses t_[ah][12] and this will override them when checking whether to draw sections
void paint_bezier(bezier b, bool highlight) {
	if(!b.intersecting(state.tl, state.br)) return;
	// "sticky" - slowly subdivides less instead of ASAP - but no stack and works well in all cases
	// unsigned to have higher precision than canvas, as lines can be diagonal
	unsigned int t = 0;
	double max = std::numeric_limits<uint>::max();
	double step = (uint)max/2+1;
	point left = a2s(b[0]);
	while(true) {
		point center = a2s(b[(t+step)/max]);
		if(center != left) {
			// if last point was outside the canvas, subdivide if that entire section isn't outside it (cheap and fast check)
			if(left.in({0, 0}, {w-1, h-1}) || center.in({0, 0}, {w-1, h-1}) ||
				b.split(t/max, false).split(step/(max-t), true).intersecting(state.tl, state.br)
			) {
				step /= 2;
				if(step <= 1) step = 1;
				else continue;
			}
			// will finish out its subsection then slowly expand as usual
			else if(step >= 2 && (
				b[(t+2*step)/max].in(state.tl, state.br) ||
				b.split((t+step)/max, false).split(step/(max-(t+step)), true).intersecting(state.tl, state.br)
			)) {
				t += step;
				step /= 2;
				continue;
			}
		}
		set_pixel(left, 255, (highlight) ? 0 : 127, 0, (highlight) ? 255 : 64);
		t += 2*step;
		if(t <= 0) break;
		if((t/(uint)step)%4 == 0) step *= 2;
		left = a2s(b[t/max]);
	}
	set_pixel(a2s(b[1]), 255, (highlight) ? 0 : 127, 0, (highlight) ? 255 : 64);
}
/*}}}*/

void paint() {
	if(state.view_final) return;
	// fill shape with set_pixel
	/*for(auto shape_shape = shapes.begin(); shape_shape != shapes.end(); ++shape_shape) {
		auto shape = (*shape_shape).first;
		for(int y = 0; y < h; y++) {
			for(int x = 0; x < w; x++) {
				int crosses = 0;
				for(auto i = shape->begin(); i != shape->end(); ++i) {
					crosses += bezier_crosses(s2a({x, y}), **std::get<0>(*i));
				}
				if((crosses) & 1)
					set_pixel({x, y}, 0, 255, 0, 64);
			}
		}
	}//*/
	{
		QuadTree<bezier>::Region region = bezier_QT.region(state.tl, state.br);
		for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
			//for(int x = 0; x < w; x++) {
			//	for(int y = 0; y < h; y++) {
			//		int res = bezier_inc_exc(s2a({x, y}), *b, false);
			//		if(res == 1)
			//			set_pixel({x, y}, 0, 255, 0, 64);
			//		else if(res == -1)
			//			set_pixel({x, y}, 255, 0, 0, 64);
			//	}
			//}
			paint_bezier(*b, !b->used || (state.s != NULL && state.s->beziers.find(b) != state.s->beziers.end()));
		}
	}

	// end of output, start of UI
	paint_flush();

	{
		QuadTree<bezier>::Region region = bezier_QT.region(state.tl, state.br);
		for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
			if(!b->used || (state.s != NULL && state.s->beziers.find(b) != state.s->beziers.end())) {
				paint_line(a2s(*b->a1), a2s(*b->h1));
				paint_line(a2s(*b->a2), a2s(*b->h2));
			}
		}
	}
	{
		QuadTree<point>::Region region = point_QT.region(state.tl, state.br);
		for(point* p = region.next(false); p != NULL; p = region.next(false)) {
			if(p->visible) paint_handle(a2s(*p));
		}
		if(state.s != NULL) {
			for(auto p = state.s->color_coords.begin(); p != state.s->color_coords.end(); ++p) {
				paint_handle(a2s(**p));
			}
		}
	}
}

void detect_shapes() {
	// TODO: not sure if this should be the whole canvas or just the screen for shape detection or not - make it a setting?
	// TODO: (in regards to lots of graphics objects being created and destroyed while zooming)
	QuadTree<bezier>::Region region = bezier_QT.region(state.tl, state.br);
	for(bezier* b = region.next(false); b != NULL; b = region.next(false)) { b->used = false; }
	region = bezier_QT.region(state.tl, state.br);
	std::unordered_map<bezier*, bool> searched;
	std::unordered_map<point*, bool> shape_points;
	std::forward_list<std::tuple<decltype(point().used_by.begin()), bool, decltype(point().used_by.end())>> shape;
	// DFS for loops (shapes) from each bezier
	// doesn't use a bezier if it was searched from previously
	for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
		shape_points[b->a1] = true;
		auto start = b->a2->used_by.begin();
		while(*start != b) { ++start; }
		shape.push_front({start, false, (decltype(point().used_by.begin()))NULL});
		bool backtracked = false;
		// single beziers may be a loop
		bezier* last_b;
		bool last_left;
		point* anchor;
		decltype(point().used_by.begin()) next_b;
		goto checkshape;
		while(true) {
			last_b = *std::get<0>(shape.front());
			last_left = std::get<1>(shape.front());
			anchor = (*last_b).anchor(last_left);
			next_b = anchor->used_by.begin();
			while(
				next_b != anchor->used_by.end() &&
				(
					// don't go backwards along same bezier
					*next_b == last_b ||
					searched.find(*next_b) != searched.end() ||
					// handles have used_by too, don't want to follow them
					(anchor != (*next_b)->a1 && anchor != (*next_b)->a2)
				)
			) { ++next_b; }
			// go deeper if not in the process of backtracking a step and there is somewhere to go
			if(!backtracked && next_b != anchor->used_by.end() && *next_b != b) {
				bool next_left = !(anchor == (*next_b)->a1);
				auto next_end = anchor->used_by.end();
				shape_points[anchor] = true;
				shape.push_front({next_b, next_left, next_end});
				// if this branch is already bad (next point already in use), go sideways
				point* next_anchor = (**next_b).anchor(next_left);
				if(
					shape_points.find(next_anchor) != shape_points.end() &&
					// need to check solutions for whether they are a solution though lol
					next_anchor != b->a1
				) {
					backtracked = true;
					continue;
				}
			}
			else {
				if(++shape.begin() != shape.end())
					anchor = (**std::get<0>(*(++shape.begin()))).anchor(std::get<1>(*(++shape.begin())));
				auto alt_b = ++std::get<0>(shape.front());
				while(
					alt_b != std::get<2>(shape.front()) &&
					(
						// don't go backwards along same bezier
						*alt_b == last_b ||
						searched.find(*alt_b) != searched.end() ||
						// handles have used_by too, don't want to follow them
						(anchor != (*alt_b)->a1 && anchor != (*alt_b)->a2)
					)
				) { ++alt_b; }
				if(
					alt_b != std::get<2>(shape.front()) &&
					// prevent going sideways from start
					++shape.begin() != shape.end()
				) {
					bool alt_left = !((*last_b).anchor(!last_left) == (*alt_b)->a1);
					// have to store this before pop
					auto alt_end = std::get<2>(shape.front());
					shape.pop_front();
					shape.push_front({alt_b, alt_left, alt_end});
					// run through this conditional again to go sideways or backwards if just added bezier is invalid
					point* alt_anchor = (**alt_b).anchor(alt_left);
					if(
						shape_points.find(alt_anchor) != shape_points.end() &&
						// need to check solutions for whether they are a solution though lol
						alt_anchor != b->a1
					) {
						backtracked = true;
						continue;
					}
					backtracked = false;
				}
				else {
					// go backwards
					shape_points.erase((*last_b).anchor(!last_left));
					shape.pop_front();
					if(shape.empty()) break;
					backtracked = true;
					continue;
				}
			}
	checkshape: // note last_b and last_left aren't initialized here
			// check for closed shape
			if((**std::get<0>(shape.front())).anchor(std::get<1>(shape.front())) == b->a1) {
				for(auto i = shape.begin(); i != shape.end(); ++i) {
					(*std::get<0>(*i))->used = true;
				}
				// put upper-leftmost bezier first before storing, try to account for different first shape discovery points
				// TODO: doesn't account for direction, so shapes being found from a different (reversed) bezier doesn't find any shapes and then rematches them all
				decltype(shape) normalized_shape = shape;
				bezier* min = *std::get<0>(normalized_shape.front());
				auto before_min = normalized_shape.before_begin();
				// TODO: this will break if the upper-leftmost point is used by two beziers in the shape and rematch the shape
				// TODO: improve - pretty common state
				for(auto i = ++normalized_shape.begin(), before_i = normalized_shape.begin(); i != normalized_shape.end(); before_i = i++) {
					if(
						(*std::get<0>(*i))->a1->x < min->a1->x ||
						(
							(*std::get<0>(*i))->a1->x <= min->a1->x &&
							(*std::get<0>(*i))->a1->y <  min->a1->y
						)
					) {
						min = *std::get<0>(*i);
						before_min = before_i;
					}
				}
				if(before_min != normalized_shape.before_begin())
					normalized_shape.splice_after(normalized_shape.before_begin(), normalized_shape, before_min, normalized_shape.end());
				auto shape_place = shapes.find(&normalized_shape);
				if(shape_place != shapes.end() && !(*shape_place).second->depth_update)
					(*shape_place).second->stale = false;
				else
					Shape::add(normalized_shape);
			}
		}
		shape_points.erase(b->a1);
		searched.insert({b, true});
	}
}

void zoom(double amount) {
	if(settings.invert_scroll) amount *= -1;
	point mouse_abs = s2a(mouse);
	double last = state.zoom;
	double next_linear = std::max(0.0, state.zoom_linear+amount);
	state.zoom = pow(2, next_linear);
	if(errno != ERANGE) state.zoom_linear = next_linear;
	else state.zoom = last;
	state.tl += mouse_abs-s2a(mouse);
	if(state.tl.x < 0) state.tl.x = 0;
	if(state.tl.y < 0) state.tl.y = 0;
	state.br = s2a({w-1, h-1}, true);
	state.tl.x = std::min(cmax-state.br.x, state.tl.x);
	state.tl.y = std::min(cmax-state.br.y, state.tl.y);
	state.br = s2a({w-1, h-1}, false);
	repaint(true);
}

void key_release(int key) {
	switch(key) {
		case 'A':
			// TODO: if over an existing visible point, attach to it
			if(state.action != Actions::Idle && state.action != Actions::PlacingBezier) break;
			if(state.action == Actions::Idle) {
				state.b = bezier_RS.get();
				state.b->a1 = point_RS.get();
				*state.b->a1 = s2a(mouse);
				state.b->a1->use_count = 0;
				(*state.b->a1).add_to(state.b);
				point_QT.insert(state.b->a1);
			}
			else {
				bezier* last = state.b;
				mouse_press(true, mouse.x, mouse.y);
				mouse_release(true, mouse.x, mouse.y);
				state.b = bezier_RS.get();
				state.b->a1 = last->a2;
				(*last->a2).add_to(state.b);
			}
			state.b->h1 = state.b->a1;
			(*state.b->h1).add_to(state.b);
			state.b->a2 = point_RS.get();
			*state.b->a2 = s2a(mouse);
			state.b->a2->use_count = 0;
			(*state.b->a2).add_to(state.b);
			point_QT.insert(state.b->a2);
			state.b->h2 = state.b->a2;
			(*state.b->h2).add_to(state.b);
			bezier_QT.insert(state.b);
			state.p = state.b->a2;
			state.p_o = {0, 0};
			state.action = Actions::PlacingBezier;
			repaint(true);
			break;
		case 'G':
			{
				if(state.action != Actions::Idle) break;
				point* remove_p = NULL;
				QuadTree<point>::Region region = point_QT.region(
					s2a({mouse.x-8, mouse.y-8})-point(1, 1),
					s2a({mouse.x+8, mouse.y+8})+point(1, 1)
				);
				for(point* p = region.next(false); p != NULL; p = region.next(false)) {
					point p2 = a2s(*p);
					if(p->visible && pow(p2.x-mouse.x, 2)+pow(p2.y-mouse.y, 2) < pow(8, 2)+pow(8, 2)) {
						remove_p = p;
						break;
					}
				}
				if(remove_p == NULL && state.s != NULL) {
					auto p = state.s->color_coords.begin();
					auto before_p = state.s->color_coords.before_begin();
					auto before_color = state.s->colors.before_begin();
					for( ; p != state.s->color_coords.end(); ++p, ++before_p, std::advance(before_color, 4)) {
						point p2 = a2s(**p);
						if(pow(p2.x-mouse.x, 2)+pow(p2.y-mouse.y, 2) < pow(8, 2)+pow(8, 2)) {
							state.s->color_coords.erase_after(before_p);
							for(int i = 0; i < 4; i++) { state.s->colors.erase_after(before_color); }
							state.s->color_count--;
							state.s->color_update = true;
							repaint(true);
							break;
						}
					}
				}
				if(remove_p == NULL) break;
				size_t use_count = 0;
				for(auto b = remove_p->used_by.begin(); b != remove_p->used_by.end(); ++b) {
					if((*b)->a1 == remove_p || (*b)->a2 == remove_p) {
						use_count++;
						if(use_count > 2) break;
					}
				}
				// TODO: check remove_p's use_count afterwards and possibly remove from point QT and release?
				// TODO: maybe already is / should be done in bezier release for >=1 though
				if(use_count > 2) break;
				if(use_count == 0) {
					for(auto b = remove_p->used_by.begin(); b != remove_p->used_by.end(); ++b) {
						if((*b)->h1 == remove_p) { remove_p->remove_from(*b); (*b)->h1 = (*b)->a1; (*b)->a1->add_to(*b); }
						if((*b)->h2 == remove_p) { remove_p->remove_from(*b); (*b)->h2 = (*b)->a2; (*b)->a2->add_to(*b); }
					}
				}
				else if(use_count == 1) {
					bezier* b = remove_p->used_by.front();
					bezier_QT.remove(b);
					(*b).release();
				}
				else {
					bezier* b = remove_p->used_by.front();
					bezier* b2 = *(++(remove_p->used_by.begin()));
					point** a2;
					point** h2;
					if(remove_p == b->a2) { a2 = &b->a2; h2 = &b->h2; } else { a2 = &b->a1; h2 = &b->h1; }
					(**a2).remove_from(b); if((*a2)->use_count == 0) (**a2).release();
					(**h2).remove_from(b); if((*h2)->use_count == 0) (**h2).release();
					*a2 = (*b2).anchor(remove_p == b2->a2); (**a2).add_to(b);
					*h2 = (*b2).handle(remove_p == b2->a2); (**h2).add_to(b);
					bezier_QT.remove(b2);
					(*b2).release();
				}
				repaint(true);
			}
			break;
		case 'S':
			{
				Shape* last_s = state.s;
				std::forward_list<Shape*> under_cursor;
				for(auto shape_shape = shapes.begin(); shape_shape != shapes.end(); ++shape_shape) {
					auto shape = (*shape_shape).first;
					int crosses = 0;
					for(auto i = shape->begin(); i != shape->end(); ++i) {
						crosses += bezier_crosses(s2a(mouse), **std::get<0>(*i));
					}
					if(crosses & 1)
						under_cursor.push_front((*shape_shape).second);
				}
				for(auto shape = under_cursor.begin(); shape != under_cursor.end(); ++shape) {
					if(*shape == state.s) {
						if(++shape != under_cursor.end())
							state.s = *shape;
						else
							state.s = under_cursor.front();
						break;
					}
				}
				if(state.s == last_s) {
					// try to select a colored shape first
					for(auto shape = under_cursor.begin(); shape != under_cursor.end(); ++shape) {
						if((*shape)->color_count != 0) {
							state.s = *shape;
							break;
						}
					}
					if(state.s == last_s)
						state.s = (!under_cursor.empty()) ? under_cursor.front() : NULL;
				}
				// cycle through the colored shapes first, then uncolored
				else if((state.s->color_count != 0) != (last_s->color_count != 0)) {
					auto shape = under_cursor.begin();
					for(; *shape != last_s; ++shape) {}
					for(++shape; shape != under_cursor.end(); ++shape) {
						if(((*shape)->color_count != 0) == (last_s->color_count != 0)) {
							state.s = *shape;
							break;
						}
					}
					if(shape == under_cursor.end()) {
						for(auto shape = under_cursor.begin(); shape != under_cursor.end(); ++shape) {
							if(((*shape)->color_count != 0) != (last_s->color_count != 0)) {
								state.s = *shape;
								break;
							}
						}
					}
				}
				// TODO: make a setting
				//if(state.s == last_s) state.s = NULL;
				repaint(true);
			}
			break;
		case 'D':
			state.s = NULL;
			repaint(true);
			break;
		case 'F':
			state.view_final = !state.view_final;
			repaint(true);
			break;
		case 'C':
			if(state.action != Actions::Idle) break;
			if(state.s != NULL) {
				point* p = point_RS.get();
				*p = s2a(mouse);
				state.s->color_coords.push_front(p);
				state.s->colors.push_front(1); // a
				state.s->colors.push_front(1); // b
				state.s->colors.push_front(1); // g
				state.s->colors.push_front(1); // r
				state.s->color_count++;
				state.s->color_update = true;
				state.last_color = p;
				repaint(true);
			}
			else {
				// TODO: sort by depth, add to top
				/*for(auto shape_shape = shapes.begin(); shape_shape != shapes.end(); ++shape_shape) {
					auto shape = (*shape_shape).first;
					int crosses = 0;
					for(auto i = shape->begin(); i != shape->end(); ++i) {
						crosses += bezier_crosses(s2a(mouse), **std::get<0>(*i));
					}
					if(crosses & 1) {
						Shape* shape = (*shape_shape).second;
						point* p = point_RS.get();
						*p = s2a(mouse);
						shape->color_coords.push_front(p);
						shape->colors.push_front(0);
						shape->colors.push_front((rand()%256)/255.0);
						shape->colors.push_front((rand()%256)/255.0);
						shape->colors.push_front((rand()%256)/255.0);
						shape->color_count++;
						shape->color_update = true;
						repaint(true);
						break;
					}
				}*/
			}
			break;
		// TODO: make these inc/dec depth just past the next bezier in the region made by its TL and BR
		case 'E':
			if(state.s == NULL) break;
			if(state.s->depth == 0) {
				for(auto shape = shapes.begin(); shape != shapes.end(); ++shape) { (*shape).second->depth++; }
				for(auto shape_collection = shape_collections.begin(); shape_collection != shape_collections.end(); ++shape_collection) { (*shape_collection)->depth++; }
			}
			state.s->depth--;
			state.s->depth_update = true;
			repaint(true);
			break;
		case 'R':
			if(state.s == NULL) break;
			state.s->depth++;
			state.s->depth_update = true;
			repaint(true);
			break;
		//case 16777216:
		//	(*a).quit();
		//	break;
	}
}

void mouse_move(int x, int y) {
	// needs to find things near last position
	// using mouse here prevents not finding things -> prevents segfaults / failing to remove point from where it is used
	point tl = s2a({mouse.x-8, mouse.y-8})-point(1, 1);
	point br = s2a({mouse.x+8, mouse.y+8})+point(1, 1);
	switch(state.action) {
		case Actions::MovingPointUnMerge:
			{
				if(state.p->use_count > 1) {
					if(pow(state.p_last.x-state.p->x, 2)+pow(state.p_last.y-state.p->y, 2) > pow((8.0/w)*cmax/state.zoom, 2)+pow((8.0/h)*cmax/state.zoom, 2)) {
						point **p1 = NULL, **p2 = NULL;
						bezier* p2_b = NULL;
						QuadTree<bezier>::Region region;
						if(state.p_prefer == NULL) {
							region = bezier_QT.region(tl, br);
							for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
								if(     b->a1 == state.p && b->h1 == state.p) { p1 = &b->a1; p2 = &b->h1; p2_b = b; break; }
								else if(b->a2 == state.p && b->h2 == state.p) { p1 = &b->a2; p2 = &b->h2; p2_b = b; break; }
							}
						}
						// TODO: once colors are done check for those between handles and anchors
						if(p1 == NULL) {
							if(state.p_prefer != NULL) { p2 = state.p_prefer; p2_b = (*p2)->used_by.front(); }
							else {
								region = bezier_QT.region(tl, br);
								for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
									if(     b->h1 == state.p) { p2 = &b->h1; p2_b = b; break; }
									else if(b->h2 == state.p) { p2 = &b->h2; p2_b = b; break; }
								}
								if(p2 == NULL) {
									region = bezier_QT.region(tl, br);
									for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
										if(     b->a1 == state.p) { p2 = &b->a1; p2_b = b; break; }
										else if(b->a2 == state.p) { p2 = &b->a2; p2_b = b; break; }
									}
								}
							}
							region = bezier_QT.region(tl, br);
							for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
								if(     b->h1 == state.p && p2 != &b->h1) { p1 = &b->h1; break; }
								else if(b->h2 == state.p && p2 != &b->h2) { p1 = &b->h2; break; }
							}
							if(p1 == NULL) {
								region = bezier_QT.region(tl, br);
								for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
									if(     b->a1 == state.p && p2 != &b->a1) { p1 = &b->a1; break; }
									else if(b->a2 == state.p && p2 != &b->a2) { p1 = &b->a2; break; }
								}
							}
						}
						**p1 = state.p_last;
						*p2 = point_RS.get();
						(**p1).remove_from(p2_b);
						// position will be updated below
						(*p2)->use_count = 0;
						(**p2).add_to(p2_b);
						point_QT.insert(*p2);
						state.p = *p2;
					}
				}
				else {
					std::vector<std::pair<point*, double>> close;
					{
						QuadTree<point>::Region region = point_QT.region(tl, br);
						for(point* p = region.next(false); p != NULL; p = region.next(false)) {
							if(p == state.p) continue;
							double dist2 = pow(p->x-state.p->x, 2)+pow(p->y-state.p->y, 2);
							if(p->visible && dist2 < pow((8.0/w)*cmax/state.zoom, 2)+pow((8.0/h)*cmax/state.zoom, 2)) {
								close.push_back({p, dist2});
							}
						}
					}
					// TODO: . . . why, exactly, are we sorting by pointer here?
					std::sort(close.begin(), close.end(), [](auto &lhs, auto &rhs) { return lhs.second < rhs.second; });
					if(!close.empty()) {
						bezier* b = state.p->used_by.front();
						     if(b->a1 == state.p) { b->a1 = close[0].first; state.p_prefer = &b->a1; }
						else if(b->h1 == state.p) { b->h1 = close[0].first; state.p_prefer = &b->h1; }
						else if(b->a2 == state.p) { b->a2 = close[0].first; state.p_prefer = &b->a2; }
						else if(b->h2 == state.p) { b->h2 = close[0].first; state.p_prefer = &b->h2; }
						// TODO: colors
						(*state.p).remove_from(b);
						point_QT.remove(state.p);
						(*state.p).release();
						(*(close[0].first)).add_to(b);
						state.p = close[0].first;
						state.p_last = *(state.p);
					}
				}
			}
			[[fallthrough]];
		case Actions::PlacingBezier:
		case Actions::MovingPoint:
			{
				// TODO: remove and readd beziers using point
				// TODO: do not allow moving if will put an anchor inside the triangle made from handles and other anchor
				//           not sure how that will work with the above falling through if the moving bit is just ignored
				point_QT.remove(state.p);
				point last = *state.p;
				*state.p = s2a({x, y})+state.p_o;
				point_QT.insert(state.p);
				// TODO: adjust paint_bezier or something to only repaint over where it used to be and where it is going
				//       slow rendering is fine, but bad when having to do it so often while moving things
				//       actually maybe fine now with paint_bezier improvements, going to have to do color rendering on GPU anyway
				if(state.moving_color && state.s != NULL) state.s->color_update = true;
				repaint(true);
			}
			break;
		case Actions::Idle:
			break;
	}
	// NOTE: MovingPoint above may return, don't put anything else important here without refactoring
	mouse.x = x; mouse.y = y;
}

void mouse_press(bool left, int x, int y) {
	if(left) {
		switch(state.action) {
			case Actions::Idle:
				{
					QuadTree<point>::Region region = point_QT.region(
						s2a({x-8, y-8})-point(1, 1),
						s2a({x+8, y+8})+point(1, 1)
					);
					for(point* p = region.next(false); p != NULL; p = region.next(false)) {
						point p2 = a2s(*p);
						if(p->visible && pow(p2.x-x, 2)+pow(p2.y-y, 2) < pow(8, 2)+pow(8, 2)) {
							state.p = p;
							state.p_o = s2a({p2.x-x, p2.y-y}, true);
							state.moving_color = false;
							state.action = Actions::MovingPoint;
							break;
						}
					}
					if(state.action == Actions::Idle && state.s != NULL) {
						for(auto p = state.s->color_coords.begin(); p != state.s->color_coords.end(); ++p) {
							point p2 = a2s(**p);
							if(pow(p2.x-x, 2)+pow(p2.y-y, 2) < pow(8, 2)+pow(8, 2)) {
								state.p = *p;
								state.p_o = s2a({p2.x-x, p2.y-y}, true);
								state.moving_color = true;
								state.last_color = state.p;
								state.action = Actions::MovingPoint;
								break;
							}
						}
					}
				}
				break;
			case Actions::MovingPoint:
			case Actions::MovingPointUnMerge:
			case Actions::PlacingBezier:
				break;
		}
	}
	else {
		switch(state.action) {
			case Actions::Idle:
				{
					QuadTree<point>::Region region = point_QT.region(
						s2a({x-8, y-8})-point(1, 1),
						s2a({x+8, y+8})+point(1, 1)
					);
					for(point* p = region.next(false); p != NULL; p = region.next(false)) {
						point p2 = a2s(*p);
						if(p->visible && pow(p2.x-x, 2)+pow(p2.y-y, 2) < pow(8, 2)+pow(8, 2)) {
							state.p = p;
							state.p_o = s2a({p2.x-x, p2.y-y}, true);
							state.p_last = *p;
							state.p_prefer = NULL;
							state.moving_color = false;
							state.action = Actions::MovingPointUnMerge;
							break;
						}
					}
				}
				break;
			case Actions::MovingPoint:
			case Actions::MovingPointUnMerge:
			case Actions::PlacingBezier:
				break;
		}
	}
}

void mouse_release(bool left, int x, int y) {
	if(left) {
		switch(state.action) {
			case Actions::MovingPoint:
			case Actions::PlacingBezier:
				state.action = Actions::Idle;
				break;
			case Actions::Idle:
			case Actions::MovingPointUnMerge:
				break;
		}
	}
	else {
		switch(state.action) {
			case Actions::MovingPointUnMerge:
				state.action = Actions::Idle;
				break;
			case Actions::Idle:
			case Actions::MovingPoint:
			case Actions::PlacingBezier:
				break;
		}
	}
}

void mouse_double_click(bool left, int x, int y) {
	
}

void save(const char* const path) {
	std::ofstream file(path, std::ios_base::binary);
	if(!file.is_open()) return;
	// TODO: make sure to account for ctype possibly being bigger in the file than on this machine when reading
	std::unordered_map<point*, size_t> points;
	file << "resolution:" << settings.ratio_width << "x" << settings.ratio_height << std::endl;
	{
		QuadTree<point>::Region region = point_QT.region({0, 0}, {cmax, cmax});
		if(region.next(false) != NULL)
			file << "points:" << sizeof(ctype) << std::endl;
	}
	{
		file.seekp(-1, std::ios_base::cur);
		size_t i = 0;
		QuadTree<point>::Region region = point_QT.region({0, 0}, {cmax, cmax});
		for(point* p = region.next(false); p != NULL; p = region.next(false), i++) {
			points.insert({p, i});
			ctype x = hton(p->x); ctype y = hton(p->y);
			file << "|";
			file.write(reinterpret_cast<const char*>(&x), sizeof(x));
			file.write(reinterpret_cast<const char*>(&y), sizeof(y));
		}
		file << std::endl;
	}
	size_t length = std::min((size_t)ceil(log2(points.size())/8), sizeof(size_t));
	{
		QuadTree<bezier>::Region region = bezier_QT.region({0, 0}, {cmax, cmax});
		if(region.next(false) != NULL)
			file << "beziers:" << length << std::endl;
	}
	{
		file.seekp(-1, std::ios_base::cur);
		QuadTree<bezier>::Region region = bezier_QT.region({0, 0}, {cmax, cmax});
		for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
			size_t a1 = hton(points.at(b->a1));
			size_t h1 = hton(points.at(b->h1));
			size_t a2 = hton(points.at(b->a2));
			size_t h2 = hton(points.at(b->h2));
			file << "|";
			file.write(reinterpret_cast<const char*>(&a1)+sizeof(size_t)-length, length);
			file.write(reinterpret_cast<const char*>(&h1)+sizeof(size_t)-length, length);
			file.write(reinterpret_cast<const char*>(&a2)+sizeof(size_t)-length, length);
			file.write(reinterpret_cast<const char*>(&h2)+sizeof(size_t)-length, length);
		}
		file << std::endl;
	}
	std::vector<Shape*> shapes;
	for(auto shape = ::shapes.begin(); shape != ::shapes.end(); ++shape) {
		shapes.push_back((*shape).second);
	}
	std::sort(shapes.begin(), shapes.end(), [](auto &lhs, auto &rhs) {
		if(lhs->shape.empty()) return rhs->shape.empty();
		if(rhs->shape.empty()) return true;
		ShapeHasher shape_hasher;
		size_t lhsh = shape_hasher(&(lhs->shape));
		size_t rhsh = shape_hasher(&(rhs->shape));
		if(lhsh < rhsh) return true;
		if(lhsh > rhsh) return false;
		if(lhs->tl.x < rhs->tl.x) return true;
		if(lhs->tl.x > rhs->tl.x) return false;
		if(lhs->tl.y < rhs->tl.y) return true;
		if(lhs->tl.y > rhs->tl.y) return false;
		if(lhs->br.x < rhs->br.x) return true;
		if(lhs->br.x > rhs->br.x) return false;
		if(lhs->br.y < rhs->br.y) return true;
		if(lhs->br.y > rhs->br.y) return false;
		if((*std::get<0>(lhs->shape.front()))->a1->x < (*std::get<0>(rhs->shape.front()))->a1->x) return true;
		if((*std::get<0>(lhs->shape.front()))->a1->x > (*std::get<0>(rhs->shape.front()))->a1->x) return false;
		if((*std::get<0>(lhs->shape.front()))->a1->y < (*std::get<0>(rhs->shape.front()))->a1->y) return true;
		if((*std::get<0>(lhs->shape.front()))->a1->y > (*std::get<0>(rhs->shape.front()))->a1->y) return false;
		// give up lol
		return true;
	});
	for(auto shape = shapes.begin(); shape != shapes.end(); ++shape) {
		if((*shape)->shape.empty()) break;
		file << "color coords:" << (*shape)->depth << "," << sizeof(ctype) << std::endl;
		file.seekp(-1, std::ios_base::cur);
		for(auto p = (*shape)->color_coords.begin(); p != (*shape)->color_coords.end(); ++p) {
			ctype x = hton((*p)->x); ctype y = hton((*p)->y);
			file << "|";
			file.write(reinterpret_cast<const char*>(&x), sizeof(x));
			file.write(reinterpret_cast<const char*>(&y), sizeof(y));
		}
		file << std::endl;
		file << "colors:" << std::endl;
		file.seekp(-1, std::ios_base::cur);
		for(auto rgba = (*shape)->colors.begin(); rgba != (*shape)->colors.end(); ) {
			file << std::setw(3) << std::setfill('0') << (int)round((*rgba)*255); ++rgba;
			file << std::setw(3) << std::setfill('0') << (int)round((*rgba)*255); ++rgba;
			file << std::setw(3) << std::setfill('0') << (int)round((*rgba)*255); ++rgba;
			file << std::setw(3) << std::setfill('0') << (int)round((*rgba)*255); ++rgba;
		}
		file << std::endl;
	}
}

void load(const char* const path) {
	std::ifstream file(path, std::ios_base::binary);
	if(!file.is_open()) return;
	enum ReadState {
		READ_BASE,
		READ_RESOLUTION,
		READ_POINTS,
		READ_BEZIERS,
		READ_COLOR_COORDS,
		READ_COLORS
	};
	ReadState state = READ_BASE;
	int buffer_size = 100;
	char buffer[buffer_size+1];
	char* buffer_end = &buffer[buffer_size];
	// for if we try to read past EOF
	auto rewind_past = [](
		std::ifstream* file,
		const char* const buffer,
		const char* const buffer_end,
		const char* const buffer_pointer
	) {
		// casts are necessary on Windows
		if(*file)
			(*file).seekg(-(buffer_end-buffer_pointer)/(ssize_t)sizeof(char)+1, std::ios_base::cur);
		else {
			(*file).clear();
			(*file).seekg(-((*file).gcount()-(buffer_pointer-buffer)/(ssize_t)sizeof(char))+1, std::ios_base::cur);
		}
	};
	std::unordered_map<size_t, point*> points;
	size_t p_i = 0;
	std::vector<Shape*> shapes;
	decltype(shapes.begin()) shape;
	bool shapes_init = true;
	bool done = false;
	while(!done) {
		switch(state) {
			case READ_BASE:
				{
					file.read(buffer, buffer_size);
					if(file.gcount() == 0 && file.eof()) { done = true; break; }
					char* colon = std::find(buffer, buffer_end, ':');
					if(colon == buffer_end) return;
					*colon = '\0';
					rewind_past(&file, buffer, buffer_end, colon);
					     if(strcmp(buffer, "resolution"  ) == 0) state = READ_RESOLUTION;
					else if(strcmp(buffer, "points"      ) == 0) state = READ_POINTS;
					else if(strcmp(buffer, "beziers"     ) == 0) state = READ_BEZIERS;
					else if(strcmp(buffer, "color coords") == 0) state = READ_COLOR_COORDS;
					else if(strcmp(buffer, "colors"      ) == 0) state = READ_COLORS;
				}
				break;
			case READ_RESOLUTION:
				{
					file.read(buffer, buffer_size);
					char* nl = std::min(
						std::find(buffer, buffer_end, '\r'),
						std::find(buffer, buffer_end, '\n')
					);
					if(nl == buffer_end) return;
					if(*nl == '\r') { *nl = '\0'; nl++; }
					else if(*nl != '\n') return;
					*nl = '\0';
					rewind_past(&file, buffer, buffer_end, nl);
					if(sscanf(buffer, "%ux%u", &settings.ratio_width, &settings.ratio_height) != 2)
						return;
					// TODO: resize
					state = READ_BASE;
				}
				break;
			case READ_POINTS:
				{
					file.read(buffer, buffer_size);
					char* pipe = std::find(buffer, buffer_end, '|');
					if(pipe == buffer_end) return;
					*pipe = '\0';
					rewind_past(&file, buffer, buffer_end, pipe-1);
					unsigned int length;
					char length_buffer[sizeof(size_t)];
					if(sscanf(buffer, "%u", &length) != 1)
						return;
					long double length_cmax = (((size_t)1) << (8*length-1))-1;
					while(true) {
						file.read(buffer, buffer_size);
						char* i;
						// adding 2 for \r\n
						for(i = buffer; i+sizeof(char) <= buffer_end-(2*length+2*sizeof(char)); i += 2*length+sizeof(char), p_i++) {
							if(*i != '|') break;
							point* p = point_RS.get();
							memset(length_buffer, 0, sizeof(size_t));
							memcpy(length_buffer+sizeof(size_t)-length, i+sizeof(char), length);
							p->x = cmax*(ntoh(*reinterpret_cast<size_t*>(length_buffer))/length_cmax);
							memset(length_buffer, 0, sizeof(size_t));
							memcpy(length_buffer+sizeof(size_t)-length, i+sizeof(char)+length, length);
							p->y = cmax*(ntoh(*reinterpret_cast<size_t*>(length_buffer))/length_cmax);
							p->use_count = 0;
							point_QT.insert(p);
							points.insert({p_i, p});
						}
						if(*i != '|') {
							if(*i == '\r') i++;
							if(*i == '\n') {
								rewind_past(&file, buffer, buffer_end, i);
								break;
							}
							else return;
						}
						rewind_past(&file, buffer, buffer_end, i-1);
					}
					state = READ_BASE;
				}
				break;
			case READ_BEZIERS:
				{
					file.read(buffer, buffer_size);
					char* pipe = std::find(buffer, buffer_end, '|');
					if(pipe == buffer_end) return;
					*pipe = '\0';
					rewind_past(&file, buffer, buffer_end, pipe-1);
					unsigned int length;
					char length_buffer[sizeof(size_t)];
					if(sscanf(buffer, "%u", &length) != 1)
						return;
					while(true) {
						file.read(buffer, buffer_size);
						char* i;
						// adding 2 for \r\n
						for(i = buffer; i+sizeof(char) <= buffer_end-(4*length+2*sizeof(char)); i += 4*length+sizeof(char)) {
							if(*i != '|') break;
							bezier* b = bezier_RS.get();
							memset(length_buffer, 0, sizeof(size_t));
							memcpy(length_buffer+sizeof(size_t)-length, i+sizeof(char), length);
							b->a1 = points.at(ntoh(*reinterpret_cast<size_t*>(length_buffer)));
							memset(length_buffer, 0, sizeof(size_t));
							memcpy(length_buffer+sizeof(size_t)-length, i+sizeof(char)+length, length);
							b->h1 = points.at(ntoh(*reinterpret_cast<size_t*>(length_buffer)));
							memset(length_buffer, 0, sizeof(size_t));
							memcpy(length_buffer+sizeof(size_t)-length, i+sizeof(char)+2*length, length);
							b->a2 = points.at(ntoh(*reinterpret_cast<size_t*>(length_buffer)));
							memset(length_buffer, 0, sizeof(size_t));
							memcpy(length_buffer+sizeof(size_t)-length, i+sizeof(char)+3*length, length);
							b->h2 = points.at(ntoh(*reinterpret_cast<size_t*>(length_buffer)));
							(*(b->a1)).add_to(b);
							(*(b->h1)).add_to(b);
							(*(b->a2)).add_to(b);
							(*(b->h2)).add_to(b);
							bezier_QT.insert(b);
						}
						if(*i != '|') {
							if(*i == '\r') i++;
							if(*i == '\n') {
								rewind_past(&file, buffer, buffer_end, i);
								break;
							}
							else return;
						}
						rewind_past(&file, buffer, buffer_end, i-1);
					}
					state = READ_BASE;
				}
				break;
			case READ_COLOR_COORDS:
				if(shapes_init) {
					repaint(true);
					for(auto shape = ::shapes.begin(); shape != ::shapes.end(); ++shape) {
						shapes.push_back((*shape).second);
					}
					std::sort(shapes.begin(), shapes.end(), [](auto &lhs, auto &rhs) {
						ShapeHasher shape_hasher;
						size_t lhsh = shape_hasher(&(lhs->shape));
						size_t rhsh = shape_hasher(&(rhs->shape));
						if(lhsh < rhsh) return true;
						if(lhsh > rhsh) return false;
						if(lhs->tl.x < rhs->tl.x) return true;
						if(lhs->tl.x > rhs->tl.x) return false;
						if(lhs->tl.y < rhs->tl.y) return true;
						if(lhs->tl.y > rhs->tl.y) return false;
						if(lhs->br.x < rhs->br.x) return true;
						if(lhs->br.x > rhs->br.x) return false;
						if(lhs->br.y < rhs->br.y) return true;
						if(lhs->br.y > rhs->br.y) return false;
						if((*std::get<0>(lhs->shape.front()))->a1->x < (*std::get<0>(rhs->shape.front()))->a1->x) return true;
						if((*std::get<0>(lhs->shape.front()))->a1->x > (*std::get<0>(rhs->shape.front()))->a1->x) return false;
						if((*std::get<0>(lhs->shape.front()))->a1->y < (*std::get<0>(rhs->shape.front()))->a1->y) return true;
						if((*std::get<0>(lhs->shape.front()))->a1->y > (*std::get<0>(rhs->shape.front()))->a1->y) return false;
						// give up lol
						return true;
					});
					shape = shapes.begin();
					shapes_init = false;
				}
				{
					file.read(buffer, buffer_size);
					char* pipe = std::find(buffer, buffer_end, '|');
					char* nl = std::find(buffer, buffer_end, '\n');
					if(nl < pipe) {
						rewind_past(&file, buffer, buffer_end, nl);
						state = READ_BASE;
						break;
					}
					if(pipe == buffer_end) return;
					*pipe = '\0';
					rewind_past(&file, buffer, buffer_end, pipe-1);
					unsigned int length;
					char length_buffer[sizeof(size_t)];
					if(sscanf(buffer, "%zu,%u", &((*shape)->depth), &length) != 2)
						return;
					(*shape)->depth_update = true;
					long double length_cmax = (((size_t)1) << (8*length-1))-1;
					while(true) {
						file.read(buffer, buffer_size);
						char* i;
						// adding 2 for \r\n
						for(i = buffer; i+sizeof(char) <= buffer_end-(2*length+2*sizeof(char)); i += 2*length+sizeof(char), p_i++) {
							if(*i != '|') break;
							point* p = point_RS.get();
							memset(length_buffer, 0, sizeof(size_t));
							memcpy(length_buffer+sizeof(size_t)-length, i+sizeof(char), length);
							p->x = cmax*(ntoh(*reinterpret_cast<size_t*>(length_buffer))/length_cmax);
							memset(length_buffer, 0, sizeof(size_t));
							memcpy(length_buffer+sizeof(size_t)-length, i+sizeof(char)+length, length);
							p->y = cmax*(ntoh(*reinterpret_cast<size_t*>(length_buffer))/length_cmax);
							(*shape)->color_coords.push_front(p);
							(*shape)->color_count++;
						}
						if(*i != '|') {
							if(*i == '\r') i++;
							if(*i == '\n') {
								rewind_past(&file, buffer, buffer_end, i);
								break;
							}
							else return;
						}
						rewind_past(&file, buffer, buffer_end, i-1);
					}
					state = READ_BASE;
				}
				break;
			case READ_COLORS:
				if((*shape)->color_count > 0) {
					size_t count = 0;
					while(count < (*shape)->color_count) {
						file.read(buffer, buffer_size);
						char* i;
						for(i = buffer; i < buffer_end-12*sizeof(char) && count < (*shape)->color_count; i += 12*sizeof(char), count++) {
							unsigned int r, g, b, a;
							if(sscanf(i, "%03u%03u%03u%03u", &r, &g, &b, &a) != 4)
								return;
							(*shape)->colors.push_front(a/255.0);
							(*shape)->colors.push_front(b/255.0);
							(*shape)->colors.push_front(g/255.0);
							(*shape)->colors.push_front(r/255.0);
						}
						rewind_past(&file, buffer, buffer_end, i-sizeof(char));
					}
					(*shape)->color_update = true;
				}
				++shape;
				file.read(buffer, buffer_size);
				rewind_past(&file, buffer, buffer_end, (*buffer == '\n') ? buffer : buffer+1);
				state = READ_BASE;
				break;
		}
	}
	::state.s = NULL;
	load_aspect_ratio();
	repaint(true);
}
