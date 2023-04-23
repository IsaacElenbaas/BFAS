#include <algorithm>
#include <cerrno>
#include <vector>
#include "PFAS.h"
#include "quadtree.h"
#include "resource.h"
#include "util.h"
// TODO: remove
#include <iostream>

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
		set_pixel(left, (highlight) ? 0 : 255, (highlight) ? 255 : 0, 0, 255);
		t += 2*step;
		if(t <= 0) break;
		if((t/(uint)step)%4 == 0) step *= 2;
		left = a2s(b[t/max]);
	}
	set_pixel(a2s(b[1]), 255, 0, 0, 255);
}
/*}}}*/

void paint() {
	{
		QuadTree<bezier>::Region region = bezier_QT.region(state.tl, state.br);
		for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
			//bool line_right(const point& p, const point& a, const point& b)
			bool right = true;
			point a1 = *b->a1, h1 = *b->h1, a2 = *b->a2, h2 = *b->h2;
			if(a1 == h1 && a2 == h2) {
				for(int x = 0; x < w; x++) {
					for(int y = 0; y < h; y++) {
						if(line_right(s2a({x, y}), a1, a2)) set_pixel({x, y}, 0, 255, 0, 64);
					}
				}
			}
			else {
				// TODO: include anything in green or self
				//       exclude anything in own bounding box but not self
				//       exclude anything in the two spaces made by own handles and neighbors'
				//           this will need to be handled differently when making corners
				if(h1 == a1) h1 = h2;
				else if(h2 == a2) h2 = h1;
				for(int x = 0; x < w; x++) {
					for(int y = 0; y < h; y++) {
						point p = s2a({x, y});
						if(p.in(b)) {
							if(right == bezier_right(p, *b))
								set_pixel({x, y}, 0, 0, 255, 64);
						}
						else {
							bool center = (line_right(p, a1, h1) == line_right(a2, a1, h1)) &&
							              (line_right(p, a2, h2) == line_right(a1, a2, h2));
							if(center && (right == line_right(p, a1, a2))) {
								set_pixel({x, y}, 0, 255, 0, 64);
								continue;
							}
							point right = (a2-a1).right();
							bool h1_r = dot_sign(h1-a1, right) > 0;
							bool h2_r = dot_sign(h2-a2, right) > 0;
							// right/left of handle
							bool roh =  line_right(p, a1, h1);
							bool loh = !line_right(p, a2, h2);
							// crop by other line (sometimes) unless is an S curve
							if((h1_r && h2_r && (roh || loh)) ||
								(h1_r && roh && (loh || dot_sign(h1-a1, (h2-a2).right()) < 0)) ||
								(h2_r && loh && (roh || dot_sign(h2-a2, (h1-a1).right()) > 0))
							)
								set_pixel({x, y}, 0, 128, 128, 64);
						}
					}
				}
			}
			paint_bezier(*b, false);
		}
	}
	// end of output, start of UI
	paint_flush();

	{
		QuadTree<bezier>::Region region = bezier_QT.region(state.tl, state.br);
		for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
			paint_line(a2s(*b->a1), a2s(*b->h1));
			paint_line(a2s(*b->a2), a2s(*b->h2));
		}
	}
	{
		QuadTree<point>::Region region = point_QT.region(state.tl, state.br);
		for(point* p = region.next(false); p != NULL; p = region.next(false)) {
			paint_handle(a2s(*p));
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
			if(state.action == Actions::Idle || state.action == Actions::PlacingBezier) {
				if(state.action == Actions::Idle) {
					state.b = bezier_RS.get();
					state.b->a1 = point_RS.get();
					*state.b->a1 = s2a(mouse);
					state.b->a1->use_count = 1;
					point_QT.insert(state.b->a1);
				}
				else {
					bezier* last = state.b;
					mouse_press(true, mouse.x, mouse.y);
					mouse_release(true, mouse.x, mouse.y);
					state.b = bezier_RS.get();
					state.b->a1 = last->a2;
					last->a2->use_count++;
				}
				state.b->h1 = state.b->a1;
				state.b->h1->use_count++;
				state.b->a2 = point_RS.get();
				*state.b->a2 = s2a(mouse);
				state.b->a2->use_count = 1;
				point_QT.insert(state.b->a2);
				state.b->h2 = state.b->a2;
				state.b->h2->use_count++;
				bezier_QT.insert(state.b);
				state.p = state.b->a2;
				state.p_o = {0, 0};
				state.action = Actions::PlacingBezier;
			}
			repaint();
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
						QuadTree<bezier>::Region region;
						if(state.p_prefer == NULL) {
							region = bezier_QT.region(tl, br);
							for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
								if(     b->a1 == state.p && b->h1 == state.p) { p1 = &b->a1; p2 = &b->h1; break; }
								else if(b->a2 == state.p && b->h2 == state.p) { p1 = &b->a2; p2 = &b->h2; break; }
							}
						}
						// TODO: once colors are done check for those between handles and anchors
						if(p1 == NULL) {
							if(state.p_prefer != NULL) p2 = state.p_prefer;
							else {
								region = bezier_QT.region(tl, br);
								for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
									if(     b->h1 == state.p) { p2 = &b->h1; break; }
									else if(b->h2 == state.p) { p2 = &b->h2; break; }
								}
								if(p2 == NULL) {
									region = bezier_QT.region(tl, br);
									for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
										if(     b->a1 == state.p) { p2 = &b->a1; break; }
										else if(b->a2 == state.p) { p2 = &b->a2; break; }
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
						(*p1)->use_count--;
						*p2 = point_RS.get();
						// position will be updated below
						(*p2)->use_count = 1;
						point_QT.insert(*p2);
						state.p = *p2;
					}
				}
				else {
					std::vector<point*> close;
					{
						QuadTree<point>::Region region = point_QT.region(tl, br);
						for(point* p = region.next(false); p != NULL; p = region.next(false)) {
							if(p == state.p) continue;
							if(pow(p->x-state.p->x, 2)+pow(p->y-state.p->y, 2) < pow((8.0/w)*cmax, 2)+pow((8.0/h)*cmax, 2)) {
								close.push_back(p);
							}
						}
					}
					sort(close.begin(), close.end());
					if(!close.empty()) {
						QuadTree<bezier>::Region region = bezier_QT.region(tl, br);
						for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
							if(b->a1 == state.p) { b->a1 = close[0]; close[0]->use_count++; state.p_prefer = &b->a1; break; }
							if(b->h1 == state.p) { b->h1 = close[0]; close[0]->use_count++; state.p_prefer = &b->h1; break; }
							if(b->a2 == state.p) { b->a2 = close[0]; close[0]->use_count++; state.p_prefer = &b->a2; break; }
							if(b->h2 == state.p) { b->h2 = close[0]; close[0]->use_count++; state.p_prefer = &b->h2; break; }
							// TODO: colors
						}
						point_QT.remove(state.p);
						point_RS.release(state.p);
						state.p = close[0];
						state.p_last = *close[0];
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
				QuadTree<bezier>::Region region = bezier_QT.region(tl, br);
				for(bezier* b = region.next(false); b != NULL; b = region.next(false)) {
					if(b->a1 == state.p || b->h1 == state.p || b->a2 == state.p || b->h2 == state.p) {
						// TODO: if split but haven't ever moved will stay split unless they go outside this forbidden range before releasing
						if((*b->a1).in(*b->a2, *b->h1, *b->h2) || (*b->a2).in(*b->a1, *b->h1, *b->h2)) {
							*state.p = last;
							point_QT.insert(state.p);
							// don't update mouse so this can repeat
							return;
						}
					}
				}
				point_QT.insert(state.p);
				// TODO: adjust paint_bezier or something to only repaint over where it used to be and where it is going
				//       slow rendering is fine, but bad when having to do it so often while moving things
				//       actually maybe fine now with paint_bezier improvements, probably going to have to do color rendering on GPU anyway
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
						if(pow(p2.x-x, 2)+pow(p2.y-y, 2) < pow(8, 2)+pow(8, 2)) {
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
						if(pow(p2.x-x, 2)+pow(p2.y-y, 2) < pow(8, 2)+pow(8, 2)) {
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
