#include <QApplication>
#include "main.h"

QWidget* canvas;

void Canvas::paintEvent(QPaintEvent* event) {
	painter->begin(this);
	paint();
	painter->end();
}

void Canvas::wheelEvent(QWheelEvent* event) { zoom(2*event->angleDelta().y()/8.0/360); }

void Canvas::keyReleaseEvent(QKeyEvent* event) { key_release(event->key()); }

void Canvas::mouseMoveEvent(QMouseEvent* event) { mouse_move(event->x(), event->y()); }

void Canvas::mousePressEvent(QMouseEvent* event) {
	if((event->button() & (Qt::LeftButton | Qt::RightButton)) == 0) return;
	mouse_press(event->button() == Qt::LeftButton, event->x(), event->y());
}

void Canvas::mouseReleaseEvent(QMouseEvent* event) {
	if((event->button() & (Qt::LeftButton | Qt::RightButton)) == 0) return;
	mouse_release(event->button() == Qt::LeftButton, event->x(), event->y());
}

void Canvas::mouseDoubleClickEvent(QMouseEvent* event) {
	if((event->button() & (Qt::LeftButton | Qt::RightButton)) == 0) return;
	mouse_double_click(event->button() == Qt::LeftButton, event->x(), event->y());
}

int main(int argc, char* argv[]) {
	QApplication a(argc, argv);
	//resize(512, 512);
	Canvas canvas;
	::canvas = &canvas;
	canvas.show();
	return a.exec();
}
