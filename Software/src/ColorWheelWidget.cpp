/*
 * ColorWheelWidget.cpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 */

#include "ColorWheelWidget.hpp"
#include "PrismatikMath.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>

namespace {
	const int MarkerRadius = 6;
	const int Margin = MarkerRadius + 2; // keeps the marker fully inside the widget at full saturation
}

ColorWheelWidget::ColorWheelWidget(QWidget *parent) : QWidget(parent)
{
	setMinimumSize(120, 120);
}

void ColorWheelWidget::setHueSaturation(int hue, int saturation)
{
	hue = ((hue % 360) + 360) % 360;
	saturation = qBound(0, saturation, 100);
	if (hue == m_hue && saturation == m_saturation)
		return;

	m_hue = hue;
	m_saturation = saturation;
	update();
	emit hueSaturationChanged(m_hue, m_saturation);
}

double ColorWheelWidget::radius() const
{
	return qMax(0, qMin(width(), height()) / 2 - Margin);
}

QPointF ColorWheelWidget::center() const
{
	return QPointF(width() / 2.0, height() / 2.0);
}

void ColorWheelWidget::rebuildWheelImage()
{
	const int w = width();
	const int h = height();
	if (w <= 0 || h <= 0) {
		m_wheelImage = QImage();
		return;
	}

	QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);

	const double r = radius();
	const QPointF c = center();

	for (int y = 0; y < h; ++y) {
		QRgb *line = reinterpret_cast<QRgb*>(image.scanLine(y));
		for (int x = 0; x < w; ++x) {
			int hue, sat;
			const bool inside = PrismatikMath::pointToHueSat(QPointF(x, y) - c, r, hue, sat);
			// Fully opaque (alpha=255) or fully transparent (alpha=0) only - premultiplied
			// equals straight RGBA at both those extremes, so no separate premultiply step needed.
			line[x] = inside ? QColor::fromHsv(hue, sat * 255 / 100, 255).rgba() : 0;
		}
	}

	m_wheelImage = image;
}

void ColorWheelWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	rebuildWheelImage();
}

void ColorWheelWidget::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	if (m_wheelImage.isNull())
		rebuildWheelImage();
	painter.drawImage(0, 0, m_wheelImage);

	const QPointF markerPos = center() + PrismatikMath::hueSatToPoint(m_hue, m_saturation, radius());
	painter.setPen(QPen(Qt::white, 2));
	painter.setBrush(QColor::fromHsv(m_hue, m_saturation * 255 / 100, 255));
	painter.drawEllipse(markerPos, MarkerRadius, MarkerRadius);
}

void ColorWheelWidget::pickFromMouse(const QPoint &widgetPos)
{
	int hue, sat;
	PrismatikMath::pointToHueSat(QPointF(widgetPos) - center(), radius(), hue, sat);
	setHueSaturation(hue, sat);
}

void ColorWheelWidget::mousePressEvent(QMouseEvent *event)
{
	pickFromMouse(event->pos());
}

void ColorWheelWidget::mouseMoveEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::LeftButton)
		pickFromMouse(event->pos());
}
