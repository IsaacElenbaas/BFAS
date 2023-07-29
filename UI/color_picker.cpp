#include <QApplication>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include "PFAS.h"
#include "UI.h"
#include "color_picker.h"
// TODO: remove
#include <iostream>

void ColorPickerWindow::keyReleaseEvent(QKeyEvent* event) { key_release(event->key()); }
static double lightness = 1;
void ColorPickerSlider::valueChangedEvent(int value) {
	lightness = value/(double)maximum();
	color_picker->repaint();
}

ColorPicker* color_picker;
static QOpenGLShaderProgram* program;
static GLuint u_zoom; static double color_picker_zoom = 1, zoom_linear = 0;
static GLuint u_tl; static point tl = {0, 0};
static GLuint u_lightness;
static GLuint vbo;
void ColorPicker::initializeGL() {
	glClearColor(0, 0, 0, 0);

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
	u_zoom = program->uniformLocation("u_zoom");
	u_tl = program->uniformLocation("u_tl");
	u_lightness = program->uniformLocation("u_lightness");
	QOpenGLFunctions* f = context()->functions();
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
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//glEnable(GL_DEPTH_TEST);
	program->setUniformValue(u_zoom, (GLfloat)color_picker_zoom);
	program->setUniformValue(u_tl, (GLfloat)(tl.x/(double)cmax), (GLfloat)(tl.y/(double)cmax));
	program->setUniformValue(u_lightness, (GLfloat)lightness);
	f->glBindBuffer(GL_ARRAY_BUFFER, vbo);
	f->glEnableVertexAttribArray(0);
	f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	{GLenum err;
	while((err = glGetError()) != GL_NO_ERROR) {
		std::cerr << "Error: " << err << std::endl;
	}}
	program->release();
}

void ColorPicker::wheelEvent(QWheelEvent* event) {
	double last = color_picker_zoom;
	double next_linear = std::max(0.0, zoom_linear+2*event->angleDelta().y()/8.0/360);
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
	QRgb color = grab().toImage().pixel(event->x(), event->y());
	if(state.s != NULL) {
		auto p = state.s->color_coords.begin();
		auto color_dest = state.s->colors.begin();
		for( ; p != state.s->color_coords.end(); ++p, std::advance(color_dest, 4)) {
			if(*p == state.last_color) {
				*(color_dest++) = qRed(color)/255.0;
				*(color_dest++) = qGreen(color)/255.0;
				*(color_dest++) = qBlue(color)/255.0;
				*(color_dest++) = 1;
				state.s->color_update = true;
				::repaint(true);
				break;
			}
		}
	}
}
