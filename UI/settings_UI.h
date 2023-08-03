#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class SettingsWindow : public QWidget {
	Q_OBJECT
	QHBoxLayout hbox;
	QVBoxLayout left_vbox;
	QVBoxLayout right_vbox;
	QCheckBox invert_scroll;
	QCheckBox color_picker_circle;
	QCheckBox color_picker_disable_shrinking;
	QCheckBox enable_voronoi;
	QCheckBox enable_influenced;
	QCheckBox enable_fireworks;
	QLabel ratio_label;
	QHBoxLayout ratio_hbox;
		QVBoxLayout ratio_label_vbox;
			QLabel ratio_width_label;
			QLabel ratio_height_label;
		QVBoxLayout ratio_width_height_vbox;
			QSpinBox ratio_width;
			QSpinBox ratio_height;
	QPushButton ratio_apply;
	QHBoxLayout save_load_hbox;
		QPushButton save;
		QPushButton load;
	QPushButton render;
	QWidget empty_expand;
public:
	SettingsWindow(QWidget* parent = NULL);
public Q_SLOTS:
	void stateChangedEvent();
	void applyAspectRatioEvent();
	void saveEvent();
	void loadEvent();
	void renderEvent();
};
#endif
