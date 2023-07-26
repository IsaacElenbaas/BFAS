#include <algorithm>
#include <cerrno>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include "PFAS.h"
#include "quadtree.h"
#include "resource.h"
#include "shapes.h"
#include "util.h"
// TODO: remove
#include <iostream>
#include <random>

// TODO: everything else should include UI.h
#include "UI/UI.cpp"

// for temporary beziers to point to
point t_a1, t_h1, t_a2, t_h2;

int w, h;
point mouse;
State state;
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
	uint t = 0;
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
		set_pixel(left, 255, 0, (!highlight) ? 0 : 127, (highlight) ? 255 : 64);
		t += 2*step;
		if(t <= 0) break;
		if((t/(uint)step)%4 == 0) step *= 2;
		left = a2s(b[t/max]);
	}
	set_pixel(a2s(b[1]), 255, 0, (!highlight) ? 0 : 127, (highlight) ? 255 : 64);
}
/*}}}*/

void paint() {
	// fill shape with set_pixel
	/*for(auto shape_shape = shapes.begin(); shape_shape != shapes.end(); ++shape_shape) {
		auto shape = (*shape_shape).first;
		for(int y = 0; y < h; y++) {
			for(int x = 0; x < w; x++) {
				int crosses = 0;
				for(auto i = shape->begin(); i != shape->end(); ++i) {
					crosses += bezier_crosses(s2a({x, y}), **std::get<0>(*i));
				}
				if(crosses & 1)
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
	}
}

void detect_shapes() {
	// TODO: not sure if this should be the whole canvas or just the screen for shape detection or not - make it a setting?
	// TODO: (in regards to lots of graphics objects being created and destroyed while zooming)
	{
		QuadTree<bezier>::Region region = bezier_QT.region(state.tl, state.br);
		std::unordered_map<bezier*, bool> used;
		std::unordered_map<point*, bool> shape_points;
		std::forward_list<std::tuple<decltype(point().used_by.begin()), bool, decltype(point().used_by.end())>> shape;
		// DFS for loops (shapes) from each bezier
		// doesn't search from a bezier if it was used in a shape previously
		for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
			if(used.find(b) != used.end()) continue;
			b->used = false;
			shape_points[b->a1] = true;
			auto start = b->a2->used_by.begin();
			while(*start != b) { ++start; }
			shape.push_front({start, false, (decltype(point().used_by.begin()))NULL});
			bool backtracked = false;
			// single beziers may be a loop
			bezier* last_b;
			bool last_left;
			point* endpoint;
			decltype(point().used_by.begin()) next_b;
			goto checkshape;
			while(true) {
				last_b = *std::get<0>(shape.front());
				last_left = std::get<1>(shape.front());
				endpoint = (*last_b).endpoint(last_left);
				next_b = endpoint->used_by.begin();
				// handles have used_by too, don't want to follow them
				while(
					next_b != endpoint->used_by.end() &&
					endpoint != (*next_b)->a1 && endpoint != (*next_b)->a2
				) { ++next_b; }
				// go deeper if not in the process of backtracking a step and there is somewhere to go
				if(!backtracked && next_b != endpoint->used_by.end() && *next_b != b) {
					bool next_left = !(endpoint == (*next_b)->a1);
					auto next_end = endpoint->used_by.end();
					shape_points[endpoint] = true;
					shape.push_front({next_b, next_left, next_end});
					// if this branch is already bad (next point already in use), go sideways
					point* next_endpoint = (**next_b).endpoint(next_left);
					if(
						shape_points.find(next_endpoint) != shape_points.end() &&
						// need to check solutions for whether they are a solution lol
						(next_endpoint != b->a1 || *next_b == b)
					) {
						backtracked = true;
						continue;
					}
				}
				else {
					if(++shape.begin() != shape.end())
						endpoint = (**std::get<0>(*(++shape.begin()))).endpoint(std::get<1>(*(++shape.begin())));
					auto alt_b = ++std::get<0>(shape.front());
					// handles have used_by too, don't want to follow them
					while(
						alt_b != std::get<2>(shape.front()) &&
						endpoint != (*alt_b)->a1 && endpoint != (*alt_b)->a2
					) { ++alt_b; }
					if(
						alt_b != std::get<2>(shape.front()) &&
						// prevent going sideways from start
						++shape.begin() != shape.end()
					) {
						bool alt_left = !((*last_b).endpoint(!last_left) == (*alt_b)->a1);
						// have to store this before pop
						auto alt_end = std::get<2>(shape.front());
						shape.pop_front();
						shape.push_front({alt_b, alt_left, alt_end});
						// run through this conditional again to go sideways or backwards if just added bezier is invalid
						point* alt_endpoint = (**alt_b).endpoint(alt_left);
						if(
							shape_points.find(alt_endpoint) != shape_points.end() &&
							// need to check solutions for whether they are a solution lol
							(alt_endpoint != b->a1 || *alt_b == b)
						) {
							backtracked = true;
							continue;
						}
						backtracked = false;
					}
					else {
						// go backwards
						shape_points.erase((*last_b).endpoint(!last_left));
						shape.pop_front();
						if(shape.empty()) break;
						backtracked = true;
						continue;
					}
				}
	checkshape: // note last_b and last_left aren't initialized here
				// check for closed shape
				if((**std::get<0>(shape.front())).endpoint(std::get<1>(shape.front())) == b->a1) {
					for(auto i = shape.begin(); i != shape.end(); ++i) {
						(*std::get<0>(*i))->used = true;
						used.insert({*std::get<0>(*i), true});
					}
					// put upper-leftmost bezier first before storing, try to account for different first shape discovery points
					typeof(shape) normalized_shape = shape;
					bezier* min = *std::get<0>(normalized_shape.front());
					auto before_min = normalized_shape.before_begin();
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
					if(shape_place != shapes.end())
						(*shape_place).second->stale = false;
					else
						// TODO: if no active shape and a new one is made, it should be set to the active shape
						// TODO: disconnected and reconnected shapes should regain old colors
						// TODO: not sure how to do either of the above when multiple shapes could be reconnected at once
						// TODO: also, how long should old colors be kept? should they be in a menu so that if a bezier is added to a shape (or a completely new one is made) they can select the stale color set and reuse it?
						Shape::add(normalized_shape);
				}
			}
			shape_points.erase(b->a1);
		}
	}
	{
		QuadTree<point>::Region region = point_QT.region(state.tl, state.br);
		for(point* p = region.next(false); p != NULL; p = region.next(false)) {
			p->visible = false;
		}
	}
	{
		QuadTree<bezier>::Region region = bezier_QT.region(state.tl, state.br);
		for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
			if(!b->used || (state.s != NULL && state.s->beziers.find(b) != state.s->beziers.end())) {
				b->a1->visible = true; b->h1->visible = true;
				b->a2->visible = true; b->h2->visible = true;
			}
		}
	}
}

void zoom(double amount) {
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
	repaint();
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
			repaint();
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
				if(state.s == last_s)
					state.s = (!under_cursor.empty()) ? under_cursor.front() : NULL;
				// TODO: make a setting
				//if(state.s == last_s) state.s = NULL;
				repaint();
			}
			break;
		case 'D':
			state.s = NULL;
			repaint();
			break;
		case 'C':
			if(state.action != Actions::Idle) break;
			if(state.s != NULL) {
				point* p = point_RS.get();
				*p = s2a(mouse);
				state.s->color_coords.push_front(p);
				state.s->colors.push_front(0);
				state.s->colors.push_front((rand()%256)/255.0);
				state.s->colors.push_front((rand()%256)/255.0);
				state.s->colors.push_front((rand()%256)/255.0);
				state.s->color_count++;
				state.s->color_update = true;
				repaint();
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
						repaint();
						break;
					}
				}*/
			}
			break;
		//case 16777216:
		//	a->quit();
		//	break;
	}
	std::cout << key << std::endl;
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
					if(pow(state.p_last.x-state.p->x, 2)+pow(state.p_last.y-state.p->y, 2) > pow((8.0/w)*cmax, 2)+pow((8.0/h)*cmax, 2)) {
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
							if(p->visible && dist2 < pow((8.0/w)*cmax, 2)+pow((8.0/h)*cmax, 2)) {
								close.push_back({p, dist2});
							}
						}
					}
					sort(close.begin(), close.end(), [](auto &lhs, auto &rhs) { return lhs.second < rhs.second; });
					if(!close.empty()) {
						bezier* b = state.p->used_by.front();
						     if(b->a1 == state.p) { b->a1 = close[0].first; state.p_prefer = &b->a1; }
						else if(b->h1 == state.p) { b->h1 = close[0].first; state.p_prefer = &b->h1; }
						else if(b->a2 == state.p) { b->a2 = close[0].first; state.p_prefer = &b->a2; }
						else if(b->h2 == state.p) { b->h2 = close[0].first; state.p_prefer = &b->h2; }
						// TODO: colors
						(*state.p).remove_from(b);
						point_QT.remove(state.p);
						state.p->release();
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
				repaint();
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
							state.action = Actions::MovingPoint;
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
