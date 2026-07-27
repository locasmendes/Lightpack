/*
 * ColorWheelDialog.cpp
 *
 *	Created on: 27.07.2026
 *		Project: Lightpack
 */

#include "ColorWheelDialog.hpp"
#include "ColorWheelWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QSlider>
#include <QLabel>

ColorWheelDialog::ColorWheelDialog(const QColor &initialColor, QWidget *parent) :
	QDialog(parent),
	m_color(initialColor)
{
	setWindowTitle(tr("Select Color"));

	m_wheel = new ColorWheelWidget(this);
	m_wheel->setHueSaturation(initialColor.hue() < 0 ? 0 : initialColor.hue(), initialColor.hsvSaturationF() * 100);

	m_valueSlider = new QSlider(Qt::Vertical, this);
	m_valueSlider->setRange(0, 255);
	m_valueSlider->setValue(initialColor.value());

	m_preview = new QLabel(this);
	m_preview->setMinimumSize(48, 24);
	m_preview->setAutoFillBackground(true);

	QHBoxLayout *pickerLayout = new QHBoxLayout();
	pickerLayout->addWidget(m_wheel, 1);
	pickerLayout->addWidget(m_valueSlider);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->addLayout(pickerLayout);
	mainLayout->addWidget(m_preview);
	mainLayout->addWidget(buttons);

	connect(m_wheel, &ColorWheelWidget::hueSaturationChanged, this, &ColorWheelDialog::onWheelChanged);
	connect(m_valueSlider, &QSlider::valueChanged, this, &ColorWheelDialog::onValueChanged);

	updatePreview();
}

void ColorWheelDialog::onWheelChanged(int, int)
{
	updateColor();
}

void ColorWheelDialog::onValueChanged(int)
{
	updateColor();
}

void ColorWheelDialog::updateColor()
{
	m_color = QColor::fromHsv(m_wheel->hue(), m_wheel->saturation() * 255 / 100, m_valueSlider->value());
	updatePreview();
	emit currentColorChanged(m_color);
}

void ColorWheelDialog::updatePreview()
{
	QPalette pal = m_preview->palette();
	pal.setColor(QPalette::Window, m_color);
	m_preview->setPalette(pal);
}
