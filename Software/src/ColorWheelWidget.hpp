/*
 * ColorWheelWidget.hpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 */

#pragma once

#include <QWidget>
#include <QImage>

// A circular hue/saturation picker (angle = hue, distance from center = saturation),
// in the style of Google Home's color picker. Value/lightness is deliberately not part
// of this widget - pair it with a separate slider (see ColorWheelDialog).
class ColorWheelWidget : public QWidget
{
	Q_OBJECT

public:
	explicit ColorWheelWidget(QWidget *parent = nullptr);

	int hue() const { return m_hue; }
	int saturation() const { return m_saturation; }
	void setHueSaturation(int hue, int saturation);

signals:
	void hueSaturationChanged(int hue, int saturation);

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;

private:
	double radius() const;
	QPointF center() const;
	void pickFromMouse(const QPoint &widgetPos);
	void rebuildWheelImage();

	int m_hue = 0;
	int m_saturation = 0;
	QImage m_wheelImage;
};
