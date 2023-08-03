#include <QDir>
#include <QFileDialog>
#include <QOpenGLWidget>
#include "UI.h"
#include "color_picker.h"
#include "settings.h"
#include "settings_UI.h"

extern QOpenGLWidget* canvas;

SettingsWindow::SettingsWindow(QWidget* parent) : QWidget(parent) {
	setWindowFlags(Qt::Tool);

	hbox.addLayout(&left_vbox);
	hbox.addLayout(&right_vbox);

	left_vbox.addWidget(&invert_scroll);
	invert_scroll.setText("Invert Scroll");
	left_vbox.addWidget(&color_picker_circle);
	color_picker_circle.setText("Circular Color Picker");
	left_vbox.addWidget(&color_picker_disable_shrinking);
	color_picker_disable_shrinking.setText("Disable Color Picker Shrinking");
	left_vbox.addWidget(&enable_voronoi);
	enable_voronoi.setText("Voronoi (TEST)");
	left_vbox.addWidget(&enable_influenced);
	enable_influenced.setText("Influenced (TEST)");
	left_vbox.addWidget(&enable_fireworks);
	enable_fireworks.setText("Fireworks (FUN)");

	right_vbox.addWidget(&ratio_label);
	ratio_label.setText("Render Resolution / Aspect Ratio");
	ratio_label.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	right_vbox.addLayout(&ratio_hbox);
		ratio_hbox.addLayout(&ratio_label_vbox);
			ratio_label_vbox.addWidget(&ratio_width_label);
			ratio_width_label.setText("Width:");
			ratio_label_vbox.addWidget(&ratio_height_label);
			ratio_height_label.setText("Height:");
		ratio_hbox.addLayout(&ratio_width_height_vbox);
			ratio_width_height_vbox.addWidget(&ratio_width);
			ratio_width.setMinimum(1);
			ratio_width.setMaximum(99999);
			ratio_width_height_vbox.addWidget(&ratio_height);
			ratio_height.setMinimum(1);
			ratio_height.setMaximum(99999);
	right_vbox.addWidget(&ratio_apply);
	ratio_apply.setText("Apply");
	right_vbox.addLayout(&save_load_hbox);
		save_load_hbox.addWidget(&save);
		save.setText("Save");
		save_load_hbox.addWidget(&load);
		load.setText("Load");
	right_vbox.addWidget(&render);
	render.setText("Render / Export");
	right_vbox.addWidget(&empty_expand);

	setLayout(&hbox);

	connect(&invert_scroll,                  SIGNAL(stateChanged(int)), this, SLOT(stateChangedEvent()));
	connect(&color_picker_circle,            SIGNAL(stateChanged(int)), this, SLOT(stateChangedEvent()));
	connect(&color_picker_disable_shrinking, SIGNAL(stateChanged(int)), this, SLOT(stateChangedEvent()));
	connect(&enable_voronoi,                 SIGNAL(stateChanged(int)), this, SLOT(stateChangedEvent()));
	connect(&enable_influenced,              SIGNAL(stateChanged(int)), this, SLOT(stateChangedEvent()));
	connect(&enable_fireworks,               SIGNAL(stateChanged(int)), this, SLOT(stateChangedEvent()));

	connect(&ratio_apply, SIGNAL(clicked()), this, SLOT(applyAspectRatioEvent()));
	connect(&save, SIGNAL(clicked()), this, SLOT(saveEvent()));
	connect(&load, SIGNAL(clicked()), this, SLOT(loadEvent()));
	connect(&render, SIGNAL(clicked()), this, SLOT(renderEvent()));
}

void SettingsWindow::stateChangedEvent() {
	settings.invert_scroll = invert_scroll.isChecked();
	settings.color_picker_circle = color_picker_circle.isChecked();
	settings.color_picker_disable_shrinking = color_picker_disable_shrinking.isChecked();
	settings.enable_voronoi = enable_voronoi.isChecked();
	settings.enable_influenced = enable_influenced.isChecked();
	settings.enable_fireworks = enable_fireworks.isChecked();
	color_picker->repaint();
	::repaint(true);
}

void SettingsWindow::applyAspectRatioEvent() {
	// TODO: move items on canvas to not stretch contents
	// TODO: add tools to scale/move everything
	settings.ratio_width = ratio_width.text().toInt();
	settings.ratio_height = ratio_height.text().toInt();
	// trigger resize event
	canvas->setMaximumSize(1, 1);
}

extern void save(const char* const path);
void SettingsWindow::saveEvent() {
	QString path = QFileDialog::getSaveFileName(this, "Save", QDir::homePath(), "PFAS Files (*.pfas)");
	if(path.isEmpty()) return;
	if(!path.endsWith(".pfas", Qt::CaseInsensitive))
		path += ".pfas";
	::save(path.toUtf8().constData());
}
extern void load(const char* const path);
void SettingsWindow::loadEvent() {
	QString path = QFileDialog::getOpenFileName(this, "Load", QDir::homePath(), "PFAS Files (*.pfas)");
	if(path.isEmpty()) return;
	::load(path.toUtf8().constData());
}

extern void render(const char* const path);
void SettingsWindow::renderEvent() {
	QString path = QFileDialog::getSaveFileName(this, "Render", QDir::homePath(), "Image Files (*.png)");
	if(path.isEmpty()) return;
	if(!path.endsWith(".png", Qt::CaseInsensitive))
		path += ".png";
	::render(path.toUtf8().constData());
}
