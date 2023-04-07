#ifndef MAIN_H
#define MAIN_H
#include <QPaintEvent>
#include <QPainter>
#include <QWidget>
#include "../PFAS.h"

extern QImage* pixels;

class PointDrawer : public QWidget {
	Q_OBJECT
	QPainter p;
public:
	PointDrawer(QWidget* obj=0) : QWidget(obj) {
		w = width();
		h = height();
		pixels = new QImage(w, h, QImage::Format_ARGB32);
	}
	~PointDrawer() {
		delete pixels;
	}
	virtual void paintEvent(QPaintEvent*);
	virtual void resizeEvent(QResizeEvent*) {
		w = width();
		h = height();
		delete pixels;
		pixels = new QImage(w, h, QImage::Format_ARGB32);
	}
};
#endif
