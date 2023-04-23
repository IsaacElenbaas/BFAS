#ifndef MAIN_H
#define MAIN_H
#include <QPaintEvent>
#include <QPainter>
#include <QWidget>
#include "../PFAS.h"

extern QPainter* painter;
extern QImage* pixels;

class Canvas : public QWidget {
	Q_OBJECT
public:
	Canvas(QWidget* obj=0) : QWidget(obj) {
		w = width();
		h = height();
		painter = new QPainter();
		pixels = new QImage(w, h, QImage::Format_ARGB32);
		setMouseTracking(true);
	}
	~Canvas() {
		delete painter;
		delete pixels;
	}
	virtual void paintEvent(QPaintEvent* event);
	virtual void wheelEvent(QWheelEvent* event);
	virtual void keyReleaseEvent(QKeyEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);
	virtual void mouseDoubleClickEvent(QMouseEvent* event);
	virtual void resizeEvent(QResizeEvent*) {
		w = width();
		h = height();
		delete pixels;
		pixels = new QImage(w, h, QImage::Format_ARGB32);
		zoom(0);
	}
};
#endif
