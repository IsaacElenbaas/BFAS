#include <QApplication>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <cmath>
#include "BFAS.h"
#include "UI.h"
#include "color_picker.h"
#include "settings.h"
// TODO: remove
#include <iostream>

void ColorPickerWindow::keyReleaseEvent(QKeyEvent* event) { key_release(event->key()); }
static double lightness = 1;
static double opacity = 1;
void ColorPickerSlider::valueChangedEvent(int value) {
	if(this == &(((ColorPickerWindow*)parent())->lightness_slider)) {
		lightness = value/(double)maximum();
		color_picker->repaint();
	}
	else {
		opacity = value/(double)maximum();
		if(state.s != NULL) {
			auto p = state.s->color_coords.begin();
			auto color_dest = state.s->colors.begin();
			for( ; p != state.s->color_coords.end(); ++p, std::advance(color_dest, 4)) {
				if(*p == state.last_color) {
					std::advance(color_dest, 3);
					*(color_dest++) = opacity;
					state.s->color_update = true;
					::repaint(true);
					break;
				}
			}
		}
	}
}

ColorPicker* color_picker;
static QOpenGLShaderProgram* program;
static GLuint u_circle;
static GLuint u_disable_shrinking;
static GLuint u_zoom; static double color_picker_zoom = 1, zoom_linear = 0;
static GLuint u_tl; static point tl = {0, 0};
static GLuint u_lightness;
static GLuint vbo;
void ColorPicker::initializeGL() {
	QOpenGLFunctions* f = context()->functions();
	f->glClearColor(0, 0, 0, 0);

	program = new QOpenGLShaderProgram;
	program->addShaderFromSourceCode(QOpenGLShader::Vertex,
		#include "color_picker.vert"
	);
	program->addShaderFromSourceCode(QOpenGLShader::Fragment,
		#include "color_picker.frag"
	);
	program->bindAttributeLocation("a_position", 0);
	program->link();
	program->bind();
	u_circle = program->uniformLocation("u_circle");
	u_disable_shrinking = program->uniformLocation("u_disable_shrinking");
	u_zoom = program->uniformLocation("u_zoom");
	u_tl = program->uniformLocation("u_tl");
	u_lightness = program->uniformLocation("u_lightness");
	f->initializeOpenGLFunctions();
	GLfloat vertices[] = {
		0, 0,
		1, 0,
		0, 1,
		0, 1,
		1, 0,
		1, 1
	};
	f->glGenBuffers(1, &vbo);
	f->glBindBuffer(GL_ARRAY_BUFFER, vbo);
	f->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	program->release();
	// TODO: glDeleteBuffers in deconstructors
}

void ColorPicker::paintGL() {
	program->bind();
	QOpenGLFunctions* f = context()->functions();
	//f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//f->glEnable(GL_DEPTH_TEST);
	program->setUniformValue(u_circle, settings.color_picker_circle);
	program->setUniformValue(u_disable_shrinking, settings.color_picker_disable_shrinking);
	program->setUniformValue(u_zoom, (GLfloat)color_picker_zoom);
	program->setUniformValue(u_tl, (GLfloat)(tl.x/(double)cmax), (GLfloat)(tl.y/(double)cmax));
	program->setUniformValue(u_lightness, (GLfloat)lightness);
	f->glBindBuffer(GL_ARRAY_BUFFER, vbo);
	f->glEnableVertexAttribArray(0);
	f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	f->glDrawArrays(GL_TRIANGLES, 0, 6);
	{GLenum err;
	while((err = f->glGetError()) != GL_NO_ERROR) {
		std::cerr << "Color Picker Error: " << err << std::endl;
	}}
	program->release();
}

void ColorPicker::wheelEvent(QWheelEvent* event) {
	double last = color_picker_zoom;
	int invert = (!settings.invert_scroll) ? 1 : -1;
	double next_linear = std::max(0.0, zoom_linear+invert*2*event->angleDelta().y()/8.0/360);
	color_picker_zoom = pow(2, next_linear);
	if(errno != ERANGE) zoom_linear = next_linear;
	else color_picker_zoom = last;
	double lost = cmax/last-cmax/color_picker_zoom;
	tl += (point){
		(ctype)((event->position().x()/width())*lost),
		(ctype)((event->position().y()/width())*lost),
	};
	if(tl.x < 0) tl.x = 0;
	if(tl.y < 0) tl.y = 0;
	if((ctype)(cmax/color_picker_zoom) > cmax-tl.x) tl.x = cmax-(ctype)(cmax/color_picker_zoom);
	if((ctype)(cmax/color_picker_zoom) > cmax-tl.y) tl.y = cmax-(ctype)(cmax/color_picker_zoom);
	color_picker->repaint();
}

static bool dragging = false;
void ColorPicker::mousePressEvent(QMouseEvent* event) {
	if((event->button() & Qt::LeftButton) == 0) return;
	dragging = true;
	mouseMoveEvent(event);
}
void ColorPicker::mouseReleaseEvent(QMouseEvent* event) {
	if((event->button() & Qt::LeftButton) == 0) return;
	dragging = false;
}
void ColorPicker::mouseMoveEvent(QMouseEvent* event) {
	if(!dragging) return;
	QRgb color;
	QImage image = grab().toImage();
	if(image.width() <= 1)
		color = QColor(lightness, lightness, lightness).Rgb;
	else {
		int x = event->x()-image.width()/2;
		int y = event->y()-image.width()/2;
		if(x != 0 || y != 0) {
			double div = sqrt(x*x+y*y)/(image.width()/2);
			div = std::copysign(std::max(1.0, abs(div)), div);
			x = x/div+image.width()/2;
			y = y/div+image.width()/2;
		}
		else {
			x = image.width()/2;
			y = image.width()/2;
		}
		color = image.pixel(x, y);
		QRgb bad = image.pixel(0, 0);
		int x2 = x; int y2 = y;
		while(color == bad) {
			if(abs((image.width()/2-x2)/(double)std::max(1, image.width()/2-x)) >= abs((image.width()/2-y2)/(double)std::max(1, image.width()/2-y)))
				x2 += (x2 < image.width()/2) ? 1 : -1;
			else
				y2 += (y2 < image.width()/2) ? 1 : -1;
			color = image.pixel(x2, y2);
		}
	}
	if(state.s != NULL) {
		auto p = state.s->color_coords.begin();
		auto color_dest = state.s->colors.begin();
		for( ; p != state.s->color_coords.end(); ++p, std::advance(color_dest, 4)) {
			if(*p == state.last_color) {
				*(color_dest++) = qRed(color)/255.0;
				*(color_dest++) = qGreen(color)/255.0;
				*(color_dest++) = qBlue(color)/255.0;
				*(color_dest++) = opacity;
				state.s->color_update = true;
				::repaint(true);
				break;
			}
		}
	}
}
