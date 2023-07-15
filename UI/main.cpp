#include <QApplication>
#include <QOpenGLFunctions>
#include "main.h"
#include "shapes.h"
// TODO: remove
#include <iostream>

QOpenGLWidget* canvas;

GLuint u_zoom;
GLuint u_tl;
GLuint vbos[2];
/* TODO
Interpolating colors:
	https://gwlucastrig.github.io/TinfourDocs/NaturalNeighborTinfourAlgorithm/index.html
	If inside shape, natural neighbor interpolation
	Else do natural neighbor of closest hull perimeter point
*/
void Canvas::initializeGL() {
	// TODO: use this instead of doing it in painter
	glClearColor(0, 0, 0.5, 1);

	program = new QOpenGLShaderProgram;
	program->addShaderFromSourceCode(QOpenGLShader::Vertex,
		#include "../shape.vert"
	);
	program->addShaderFromSourceCode(QOpenGLShader::Fragment,
		#include "../shape.frag"
	);
	program->bindAttributeLocation("a_position", 0);
	program->bindAttributeLocation("a_bezier_data", 1);
	program->bindAttributeLocation("a_color", 2);
	program->link();
	program->bind();
	u_zoom = program->uniformLocation("u_zoom");
	u_tl = program->uniformLocation("u_tl");

	// TODO: Do for each shape (will probably have to be a class)
	// TODO: Use this instead of context() when outside of class
	//QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
	QOpenGLFunctions* f = context()->functions();
	f->initializeOpenGLFunctions();
	shape_collections.push_front(new OpenGLShapeCollection());
	//QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
	GLfloat vertices[] = {
		0.25, 0.25, 0,
		0.25, 1, 0,
		1, 0.25, 0,
	};
	f->glGenBuffers(1, &vbos[0]);
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
	f->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	GLfloat colors[] = {
		0, 0, 0,
		0, 0, 0,
		0, 0, 0
	};
	f->glGenBuffers(1, &vbos[1]);
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
	f->glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);

	program->release();
	// TODO: glDeleteBuffers in deconstructors
}

#include <stdio.h>
void Canvas::paintGL() {
	program->bind();
	QOpenGLFunctions* f = context()->functions();
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
	f->glEnableVertexAttribArray(0);
	f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	f->glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
	f->glEnableVertexAttribArray(1);
	f->glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	//glEnable(GL_DEPTH_TEST);
	program->setUniformValue(u_zoom, (GLfloat)state.zoom);
	program->setUniformValue(u_tl, (GLfloat)(((double)state.tl.x)/cmax), (GLfloat)(((double)state.tl.y)/cmax));
	glDrawArrays(GL_TRIANGLES, 0, 3); // last param is vertex count
	program->release();

	// unbind vertex buffer, required for painter to still work
	// do in initialize, maybe here too
	f->glBindBuffer(GL_ARRAY_BUFFER, 0);
	GLenum err;
	while((err = glGetError()) != GL_NO_ERROR) {
		std::cerr << "Error: " << err << std::endl;
	}
	// TODO: make a detect shapes function, do that here (want to do paint after drawing shapes but currently am detecting shapes in it)
	painter->begin(this);
	paint();
	painter->end();
	program->bind();
	for(auto i = shape_collections.begin(); i != shape_collections.end(); ++i) { (*i)->draw(); }
	program->release();
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
	return app.exec();
}
