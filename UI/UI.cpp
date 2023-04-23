// TODO: don't include .h
#include <QPainter>
#include <QWidget>

extern QWidget* canvas;
QPainter* painter;
QImage* pixels;

// TODO: rethink - inline elsewhere, store in matrix, copy over to this in paint_flush? Just deal with importing qt stuff and inline this?
bool set_pixel(const point& p, int r, int g, int b, int a) {
	if(
		p.x < 0 || p.x >= w ||
		p.y < 0 || p.y >= h
	) return false;
	QColor last = pixels->pixelColor(p.x, p.y);
	double w = a/255.0;
	pixels->setPixelColor(p.x, p.y, qRgba(
		r*w+last.red()  *(1-w),
		g*w+last.green()*(1-w),
		b*w+last.blue() *(1-w),
		(a+last.alpha()-255 < 255) ? a+last.alpha()-255 : 255
	));
	return true;
}

void paint_line(const point& p1, const point& p2) {
	painter->setPen(QPen(Qt::blue));
	painter->drawLine(p1.x, p1.y, p2.x, p2.y);
}

void paint_handle(const point& p) {
	if(
		p.x < -8 || p.x >= w+8 ||
		p.y < -8 || p.y >= h+8
	) return;
	painter->setPen(QPen(Qt::green));
	painter->drawEllipse(QPoint(p.x, p.y), 8, 8);
}

void paint_flush() {
	painter->drawImage(0, 0, *pixels);
	pixels->fill(qRgba(0, 0, 0, 255));
}

void repaint() { canvas->repaint(); }
