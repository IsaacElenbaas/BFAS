// TODO: don't include .h
#include <QPainter>

QImage* pixels;

inline void set_pixel(point p, int r, int g, int b, int a) {
	if(
		p.x < 0 || p.x >= w ||
		p.y < 0 || p.y >= h
	) return;
	pixels->setPixelColor(p.x, p.y, qRgba(r, g, b, a));
}
