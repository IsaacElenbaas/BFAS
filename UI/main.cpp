#include <QApplication>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include "color_picker.h"
#include "main.h"
#include "shapes.h"
// TODO: remove
#include <iostream>

QOpenGLWidget* canvas;

static QOpenGLShaderProgram* program;
static GLuint u_zoom;
static GLuint u_tl;
void Canvas::initializeGL() {
	// TODO: use this instead of doing it in painter
	glClearColor(0, 0, 0, 1);

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
	u_zoom = program->uniformLocation("u_zoom");
	u_tl = program->uniformLocation("u_tl");
	context()->functions()->initializeOpenGLFunctions();
	shape_collections.push_front(new OpenGLShapeCollection());
	program->release();
	// TODO: glDeleteBuffers in deconstructors
}

void Canvas::paintGL() {
	program->bind();
	QOpenGLFunctions* f = context()->functions();
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//glEnable(GL_DEPTH_TEST);
	program->setUniformValue(u_zoom, (GLfloat)state.zoom);
	program->setUniformValue(u_tl, (GLfloat)(state.tl.x/(double)cmax), (GLfloat)(state.tl.y/(double)cmax));
	detect_shapes();
	Shape::apply_adds();
	for(auto i = shape_collections.begin(); i != shape_collections.end(); ++i) { (*i)->draw(); }
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
		canvas->repaint();
	}
}

void Canvas::wheelEvent(QWheelEvent* event) { zoom(2*event->angleDelta().y()/8.0/360); }

void Canvas::keyReleaseEvent(QKeyEvent* event) { key_release(event->key()); }

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

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);
	QCoreApplication::setApplicationName("PFAS");
	QCoreApplication::setOrganizationName("Programmatic Flat Art Studio");
	//resize(512, 512);
	Canvas canvas;
	::canvas = &canvas;
	canvas.show();

	ColorPickerWindow color_picker_window(&canvas);
	color_picker_window.show();
	color_picker_window.resize(300, 300);

	return app.exec();
}
