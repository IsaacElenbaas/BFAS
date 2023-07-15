#ifndef MAIN_H
#define MAIN_H
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPaintEvent>
#include <QPainter>
#include "PFAS.h"

extern QOpenGLShaderProgram* program;
extern QPainter* painter;
extern QImage* pixels;

class Canvas : public QOpenGLWidget {
	Q_OBJECT
public:
	Canvas(QWidget* parent = NULL) : QOpenGLWidget(parent) {
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
	virtual void initializeGL();
	virtual void paintGL();
	virtual void wheelEvent(QWheelEvent* event);
	virtual void keyReleaseEvent(QKeyEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);
	virtual void mouseDoubleClickEvent(QMouseEvent* event);
	virtual void resizeGL(int w, int h) {
		::w = w; ::h = h;
		delete pixels;
		pixels = new QImage(w, h, QImage::Format_ARGB32);
		zoom(0);
	}
};
#endif
