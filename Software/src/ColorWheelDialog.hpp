/*
 * ColorWheelDialog.hpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 */

#pragma once

#include <QDialog>

class ColorWheelWidget;
class QSlider;
class QLabel;

// Replaces QColorDialog as the picker opened by ColorButton::click() - a
// Google-Home-style circular hue/saturation wheel plus a separate value/lightness slider.
class ColorWheelDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ColorWheelDialog(const QColor &initialColor, QWidget *parent = nullptr);

	QColor selectedColor() const { return m_color; }

signals:
	void currentColorChanged(QColor color);

private slots:
	void onWheelChanged(int hue, int saturation);
	void onValueChanged(int value);

private:
	void updateColor();
	void updatePreview();

	ColorWheelWidget *m_wheel;
	QSlider *m_valueSlider;
	QLabel *m_preview;
	QColor m_color;
};
