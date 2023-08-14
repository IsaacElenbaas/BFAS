#include <QApplication>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <cstring>
#include <stdlib.h>
#include "color_picker.h"
#include "main.h"
#include "settings.h"
#include "settings_UI.h"
#include "shapes.h"
// TODO: remove
#include <iostream>

QOpenGLWidget* canvas;

static QOpenGLShaderProgram* program;
static GLuint u_enable_voronoi;
static GLuint u_enable_influenced;
static GLuint u_enable_fireworks;
static GLuint u_zoom;
static GLuint u_tl;
void Canvas::initializeGL() {
	program = new QOpenGLShaderProgram;
	program->addShaderFromSourceCode(QOpenGLShader::Vertex,
		#include "../shape.vert"
	);
	program->addShaderFromSourceCode(QOpenGLShader::Fragment,
		#include "../shape.frag"
	);
	program->bindAttributeLocation("a_position", 0);
	program->bindAttributeLocation("a_bezier_data", 1);
	program->bindAttributeLocation("a_color_data", 2);
	program->link();
	program->bind();
	u_enable_voronoi = program->uniformLocation("u_enable_voronoi");
	u_enable_influenced = program->uniformLocation("u_enable_influenced");
	u_enable_fireworks = program->uniformLocation("u_enable_fireworks");
	u_zoom = program->uniformLocation("u_zoom");
	u_tl = program->uniformLocation("u_tl");
	context()->functions()->initializeOpenGLFunctions();
	shape_collections.push_back(new OpenGLShapeCollection());
	program->release();
	// TODO: glDeleteBuffers in deconstructors
}

static char* render_path = NULL;
void render(const char* const path) {
	render_path = (char*)malloc((strlen(path)+1)*sizeof(char));
	strcpy(render_path, path);
	canvas->update();
}

void Canvas::paintGL() {
	GLint old_fbo;
	QOpenGLFramebufferObject* fbo = NULL;
	if(render_path != NULL) {
		context()->functions()->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);
		fbo = new QOpenGLFramebufferObject(8*settings.ratio_width, 8*settings.ratio_height);
		fbo->bind();
		glViewport(0, 0, 8*settings.ratio_width, 8*settings.ratio_height);
	}
	program->bind();
	QOpenGLFunctions* f = context()->functions();
	glClear(GL_COLOR_BUFFER_BIT);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	program->setUniformValue(u_enable_voronoi, settings.enable_voronoi);
	program->setUniformValue(u_enable_influenced, settings.enable_influenced);
	program->setUniformValue(u_enable_fireworks, settings.enable_fireworks);
	program->setUniformValue(u_zoom, (GLfloat)state.zoom);
	program->setUniformValue(u_tl, (GLfloat)(state.tl.x/(double)cmax), (GLfloat)(state.tl.y/(double)cmax));
	detect_shapes();
	Shape::apply_adds();
	for(auto i = shape_collections.begin(); i != shape_collections.end(); ++i) {
		if((*i)->used == 0) break;
		(*i)->draw();
	}
	if(render_path != NULL) {
		fbo->toImage().scaled(settings.ratio_width, settings.ratio_height, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(render_path);
		fbo->release();
		fbo = NULL;
		f->glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);
		glViewport(0, 0, contentsRect().width(), contentsRect().height());
		free(render_path); render_path = NULL;
		update();
		program->release();
		return;
	}
	// unbind vertex buffer, required for painter to still work
	// do in initialize, maybe here too
	f->glBindBuffer(GL_ARRAY_BUFFER, 0);
	{GLenum err;
	while((err = glGetError()) != GL_NO_ERROR) {
		std::cerr << "Error: " << err << std::endl;
	}}
	program->release();

	painter->begin(this);
	paint();
	painter->end();
	if(repaint_later) {
		repaint_later = false;
		repaint();
	}
}

void Canvas::wheelEvent(QWheelEvent* event) { zoom(2*event->angleDelta().y()/8.0/360); }

void CanvasWindow::keyReleaseEvent(QKeyEvent* event) { key_release(event->key()); }

void Canvas::mouseMoveEvent(QMouseEvent* event) { mouse_move(std::max(0, std::min(event->x(), w)), std::max(0, std::min(event->y(), h))); }

void Canvas::mousePressEvent(QMouseEvent* event) {
	if((event->button() & (Qt::LeftButton | Qt::RightButton)) == 0) return;
	mouse_press(event->button() == Qt::LeftButton, std::max(0, std::min(event->x(), w)), std::max(0, std::min(event->y(), h)));
}

void Canvas::mouseReleaseEvent(QMouseEvent* event) {
	if((event->button() & (Qt::LeftButton | Qt::RightButton)) == 0) return;
	mouse_release(event->button() == Qt::LeftButton, std::max(0, std::min(event->x(), w)), std::max(0, std::min(event->y(), h)));
}

void Canvas::mouseDoubleClickEvent(QMouseEvent* event) {
	if((event->button() & (Qt::LeftButton | Qt::RightButton)) == 0) return;
	mouse_double_click(event->button() == Qt::LeftButton, std::max(0, std::min(event->x(), w)), std::max(0, std::min(event->y(), h)));
}

void Canvas::resizeGL(int w, int h) {
	double ratio = settings.ratio_width/(double)settings.ratio_height;
	if(abs(w-ratio*h)/2 > 2 || w == 1) {
		setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
		QMargins existing = parentWidget()->layout()->contentsMargins();
		w += existing.left()+existing.right();
		h += existing.top()+existing.bottom();
		h *= ratio;
		parentWidget()->layout()->setContentsMargins(
			std::max(0, w-h)/2,
			std::max(0, (int)round((h-w)/ratio))/2,
			std::max(0, w-h)/2,
			std::max(0, (int)round((h-w)/ratio))/2
		);
		return;
	}
	else {
		::w = w; ::h = h;
		delete pixels;
		pixels = new QImage(w, h, QImage::Format_ARGB32);
		zoom(0);
	}
}

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);
	QCoreApplication::setApplicationName("PFAS");
	QCoreApplication::setOrganizationName("Programmatic Flat Art Studio");

	CanvasWindow window;
	QHBoxLayout layout;
	Canvas canvas;
	::canvas = &canvas;
	layout.addWidget(&canvas);
	window.setLayout(&layout);
	window.show();
	window.resize(800, 800);

	ColorPickerWindow color_picker_window(&canvas);
	color_picker_window.show();
	color_picker_window.resize(300, 300);

	SettingsWindow settings;
	settings.show();

	return app.exec();
}
