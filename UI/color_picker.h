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
	virtual void resizeGL(int w, int h) {
		if(abs(w-h)/2 > 2) {
			QMargins existing = parentWidget()->layout()->contentsMargins();
			w += existing.left()+existing.right();
			h += existing.top()+existing.bottom();
			parentWidget()->layout()->setContentsMargins(
				std::max(0, w-h)/2,
				std::max(0, h-w)/2,
				std::max(0, w-h)/2,
				std::max(0, h-w)/2
			);
		}
	}
	virtual void wheelEvent(QWheelEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);
};
extern ColorPicker* color_picker;

class ColorPickerSlider : public QSlider {
	Q_OBJECT
public:
	ColorPickerSlider(QWidget* parent = NULL) : QSlider(parent) {
		setTracking(true);
		setValue(maximum());
		connect(this, SIGNAL(valueChanged(int)), this, SLOT(valueChangedEvent(int)));
	};
public Q_SLOTS:
	void valueChangedEvent(int value);
};

class ColorPickerWindow : public QWidget {
	Q_OBJECT
	QVBoxLayout vbox;
	QHBoxLayout hbox;
	QWidget color_picker_widget;
	QHBoxLayout color_picker_layout;
	ColorPicker color_picker;
	ColorPickerSlider opacity_slider;
public:
	ColorPickerSlider lightness_slider;
	ColorPickerWindow(QWidget* parent = NULL) : QWidget(parent) {
		::color_picker = &color_picker;
		setWindowFlags(Qt::Tool);
		vbox.addLayout(&hbox);
			hbox.addWidget(&color_picker_widget);
				color_picker_layout.addWidget(&color_picker);
				color_picker_widget.setLayout(&color_picker_layout);
			hbox.addWidget(&lightness_slider);
		vbox.addWidget(&opacity_slider);
		opacity_slider.setOrientation(Qt::Horizontal);
		setLayout(&vbox);
	}
	virtual void keyReleaseEvent(QKeyEvent* event);
};
#endif
