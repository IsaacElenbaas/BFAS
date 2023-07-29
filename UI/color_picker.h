#ifndef COLOR_PICKER_H
#define COLOR_PICKER_H
#include <QHBoxLayout>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPaintEvent>
#include <QPainter>
#include <QSlider>
#include <QWidget>

class ColorPicker : public QOpenGLWidget {
	Q_OBJECT
public:
	ColorPicker(QWidget* parent = NULL) : QOpenGLWidget(parent) {
		setMouseTracking(true);
		setAttribute(Qt::WA_AlwaysStackOnTop);
	}
	virtual void initializeGL();
	virtual void paintGL();
	virtual void wheelEvent(QWheelEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);
};
extern ColorPicker* color_picker;

class ColorPickerSlider : public QSlider {
	Q_OBJECT
public:
	ColorPickerSlider(QWidget* parent = NULL) : QSlider(Qt::Vertical, parent) {
		setTracking(true);
		setValue(maximum());
		connect(this, SIGNAL(valueChanged(int)), this, SLOT(valueChangedEvent(int)));
	};
public Q_SLOTS:
	void valueChangedEvent(int value);
};

class ColorPickerWindow : public QWidget {
	Q_OBJECT
	QHBoxLayout layout;
	ColorPicker color_picker;
	ColorPickerSlider slider;
public:
	ColorPickerWindow(QWidget* parent = NULL) : QWidget(parent) {
		::color_picker = &color_picker;
		setWindowFlags(Qt::Tool);
		layout.addWidget(&color_picker);
		layout.addWidget(&slider);
		setLayout(&layout);
	}
	virtual void keyReleaseEvent(QKeyEvent* event);
};
#endif
