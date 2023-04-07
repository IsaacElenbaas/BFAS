#include "PFAS.h"
#include "quadtree.h"
#include "resource.h"

int w, h;

// TODO: everything else should include UI.h
#include "UI/UI.cpp"

/*{{{ point paint_prep(point p)*/
point paint_prep(point p) {
	point p2;
	p2.x = round((w-1)*((double)p.x/cmax));
	p2.y = round((h-1)*((double)p.y/cmax));
	return p2;
}
/*}}}*/

/*{{{ void paint_bezier(bezier b)*/
void paint_bezier(bezier b) {
	// "sticky" - slowly subdivides less instead of ASAP - but no stack and works well in all cases
	uint t = 0;
	double max = std::numeric_limits<uint>::max();
	double step = (uint)max/2+1;
	point left = paint_prep(b[0]);
	while(true) {
		set_pixel(left, 255, 0, 0, 255);
		point center = paint_prep(b[(t+step)/max]);
		if(center == left) {
			t += (double)2*step;
			if(t == 0) break;
			if((t/(uint)step)%4 == 0) step *= 2;
			left = paint_prep(b[t/max]);
		}
		else {
			step /= 2;
			if(step < 1) break;
		}
	}
	set_pixel(paint_prep(b[1]), 255, 0, 0, 255);
}
/*}}}*/

void paint() {
	// TODO: only do if resized or UI event that should have done it
	Resource<point> points_RS;
	QuadTree<point> points_QT(25);
	for(int i = 0; i < w; i++) {
		for(int j = 0; j < h; j++) {
			if(i%10+j%10 != 0) continue;
			point* p = points_RS.get();
			p->x = i; p->y = j;
			points_QT.insert(p);
		}
	}
	QuadTree<point>::Region region = points_QT.region({0, 0}, {w, h});
	for(point* p = region.next(false); p != NULL; p = region.next(false)) {
		set_pixel(*p, 0, 255, 0, 255);
	}
	region = points_QT.region({0, 0}, {w, h});
	for(point* p = region.next(true); p != NULL; p = region.next(true)) {
		points_RS.release(p);
	}

	point ul = {0, 0};
	point ur = {cmax, 0};
	point br = {cmax, cmax};
	point bl = {0, cmax};
	bezier b = {&ul, &ur, &br, &bl};
	paint_bezier(b);
}
