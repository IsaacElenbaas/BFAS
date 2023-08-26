#ifndef MAIN_H
#define MAIN_H
#include <QHBoxLayout>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPaintEvent>
#include <QPainter>
#include "BFAS.h"

extern QPainter* painter;
extern QImage* pixels;
extern bool repaint_later;

class CanvasWindow : public QWidget {
	Q_OBJECT
public:
	virtual void keyReleaseEvent(QKeyEvent* event);
};

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
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);
	virtual void mouseDoubleClickEvent(QMouseEvent* event);
	virtual void resizeGL(int w, int h);
};
#endif
