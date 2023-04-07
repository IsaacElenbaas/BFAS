#include <QApplication>
#include "main.h"

void PointDrawer::paintEvent(QPaintEvent*) {
	pixels->fill(qRgba(0, 0, 0, 0));
	paint();
	p.begin(this);
	p.drawImage(0, 0, *pixels);
	p.end();
}

int main(int argc, char* argv[]) {
	QApplication a(argc, argv);
	PointDrawer drawer;
	//drawer.resize(512, 512);
	drawer.show();
	return a.exec();
}
