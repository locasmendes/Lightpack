/*
 * SettingsWindowPhase5.cpp — Phase 5 IA, palette-derived QSS, responsiveness hooks.
 */

#include "SettingsWindow.hpp"
#include "ui_SettingsWindow.h"

#include <QAbstractSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidgetItem>
#include <QScreen>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QStatusBar>
#include <QTabBar>
#include <QVBoxLayout>
#include <QWindow>

namespace {

void takeFromLayout(QWidget *w)
{
	if (!w)
		return;
	if (QWidget *p = w->parentWidget()) {
		if (QLayout *l = p->layout())
			l->removeWidget(w);
	}
	w->setParent(nullptr);
}

QScrollArea *wrapInScrollArea(QWidget *content)
{
	auto *scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(content);
	scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	return scroll;
}

QWidget *makeTabShell(QScrollArea *scroll)
{
	auto *page = new QWidget;
	auto *lay = new QVBoxLayout(page);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(scroll);
	page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	return page;
}

QListWidgetItem *makeNavItem(const QString &text, const QString &iconPath)
{
	auto *item = new QListWidgetItem(text);
	item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
	item->setIcon(QIcon(iconPath));
	return item;
}

} // namespace

void SettingsWindow::applyPhase5Ui()
{
	applyResponsiveShell();
	applyPaletteDerivedTheme();
	rebuildInformationArchitecture();
	connectScreenGeometryHooks();
}

QString SettingsWindow::paletteDerivedStyleSheet() const
{
	// Accent only — lime / teal from Halo Desk. Surfaces come from QPalette.
	const QString lime = QStringLiteral("#A6D400");
	const QString teal = QStringLiteral("#1FA7A0");

	return QStringLiteral(
		"QMainWindow, QDialog {"
		"  background-color: palette(window);"
		"  color: palette(window-text);"
		"  font-family: system-ui, 'Segoe UI', sans-serif;"
		"}"
		"QListWidget#listWidget {"
		"  background-color: palette(base);"
		"  border: 1px solid palette(mid);"
		"  outline: none;"
		"}"
		"QListWidget#listWidget::item {"
		"  padding: 6px 2px;"
		"  color: palette(text);"
		"}"
		"QListWidget#listWidget::item:selected {"
		"  background-color: palette(highlight);"
		"  color: palette(highlighted-text);"
		"  border-left: 3px solid %1;"
		"}"
		"QListWidget#listWidget::item:hover:!selected {"
		"  background-color: palette(alternate-base);"
		"}"
		"QGroupBox {"
		"  font-weight: 600;"
		"  border: 1px solid palette(mid);"
		"  border-radius: 4px;"
		"  margin-top: 0.8em;"
		"  padding-top: 0.4em;"
		"}"
		"QGroupBox::title {"
		"  subcontrol-origin: margin;"
		"  left: 8px;"
		"  padding: 0 4px;"
		"  color: palette(window-text);"
		"}"
		"QPushButton {"
		"  background-color: palette(button);"
		"  color: palette(button-text);"
		"  border: 1px solid palette(mid);"
		"  border-radius: 3px;"
		"  padding: 4px 10px;"
		"}"
		"QPushButton:hover {"
		"  border-color: %2;"
		"}"
		"QPushButton:focus {"
		"  border: 1px solid %1;"
		"}"
		"QPushButton:disabled {"
		"  color: palette(mid);"
		"}"
		"QPushButton:checked {"
		"  background-color: %1;"
		"  color: #0F1C24;"
		"  border-color: %2;"
		"}"
		"QSlider::groove:horizontal {"
		"  height: 4px;"
		"  background: palette(mid);"
		"  border-radius: 2px;"
		"}"
		"QSlider::handle:horizontal {"
		"  width: 12px;"
		"  margin: -5px 0;"
		"  border-radius: 6px;"
		"  background: %1;"
		"  border: 1px solid %2;"
		"}"
		"QSlider::handle:horizontal:disabled {"
		"  background: palette(mid);"
		"  border-color: palette(dark);"
		"}"
		"QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {"
		"  background-color: palette(base);"
		"  color: palette(text);"
		"  border: 1px solid palette(mid);"
		"  border-radius: 3px;"
		"  padding: 2px 4px;"
		"  selection-background-color: palette(highlight);"
		"  selection-color: palette(highlighted-text);"
		"}"
		"QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {"
		"  border-color: %1;"
		"}"
		"QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled {"
		"  color: palette(mid);"
		"  background-color: palette(window);"
		"}"
		"QComboBox::drop-down {"
		"  border: none;"
		"  width: 1.2em;"
		"}"
		"QComboBox::down-arrow {"
		"  width: 0; height: 0;"
		"  border-left: 4px solid transparent;"
		"  border-right: 4px solid transparent;"
		"  border-top: 5px solid palette(text);"
		"  margin-right: 4px;"
		"}"
		"QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {"
		"  background: palette(button);"
		"  border: none;"
		"  width: 14px;"
		"}"
		"QScrollArea { background: transparent; border: none; }"
		"QStatusBar {"
		"  border-top: 1px solid palette(midlight);"
		"}"
		"QFrame#frame_VirtualLeds {"
		"  border: 1px solid %2;"
		"  border-radius: 4px;"
		"  background-color: palette(base);"
		"}"
		"QCheckBox#checkBox_AdvancedMode:checked {"
		"  color: %2;"
		"}"
	).arg(lime, teal);
}

void SettingsWindow::applyPaletteDerivedTheme()
{
	setStyleSheet(paletteDerivedStyleSheet());

	// Drop redundant inline sheets that fight the central theme (keep functional ones elsewhere).
	ui->tabDeviceOptions->setStyleSheet(QString());
	statusBar()->setStyleSheet(QStringLiteral("QStatusBar{border-top: 1px solid; border-color: palette(midlight);} QLabel{margin:0.2em}"));
}

void SettingsWindow::applyResponsiveShell()
{
	setWindowFlags(Qt::Window
		| Qt::WindowCloseButtonHint
		| Qt::WindowMinimizeButtonHint
		| Qt::WindowMaximizeButtonHint);

	setMinimumSize(420, 480);
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	ui->listWidget->setMinimumSize(96, 0);
	ui->listWidget->setMaximumWidth(160);
	ui->listWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	ui->listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	ui->listWidget->setSpacing(0);
	const int cellW = qMax(100, fontMetrics().horizontalAdvance(QStringLiteral("Calibration")) + 24);
	const int cellH = qMax(72, fontMetrics().height() * 2 + 48);
	ui->listWidget->setGridSize(QSize(cellW, cellH));

	for (QWidget *tab : {ui->tabSoftwareOptions, ui->tabDeviceOptions, ui->tabProfiles, ui->tabPlugins, ui->tabExpert, ui->tabHelp, ui->tabAbout}) {
		if (tab)
			tab->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	}

	ui->comboBox_LightpackModes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	ui->comboBox_LightpackModes->setMinimumWidth(120);
	ui->comboBox_LightpackModes->setMaximumWidth(16777215);

	// Keep numeric spin floors; drop decorative fixed widths where safe.
	for (QWidget *w : findChildren<QWidget*>()) {
		if (w->objectName().startsWith(QStringLiteral("pushButton_")) && w->maximumWidth() == 16)
			continue; // help icons
		if (qobject_cast<QAbstractSpinBox*>(w)) {
			if (w->minimumWidth() > 80)
				w->setMinimumWidth(50);
			continue;
		}
		if (w->minimumWidth() >= 150 && w->minimumWidth() <= 220
			&& (qobject_cast<QSlider*>(w) || w->objectName().contains(QStringLiteral("Slider")))) {
			w->setMinimumWidth(80);
		}
	}
}

void SettingsWindow::connectScreenGeometryHooks()
{
	auto hookScreen = [this](QScreen *screen) {
		if (!screen)
			return;
		connect(screen, &QScreen::geometryChanged, this, &SettingsWindow::onScreenGeometryOrDpiChanged, Qt::UniqueConnection);
		connect(screen, &QScreen::logicalDotsPerInchChanged, this, &SettingsWindow::onScreenGeometryOrDpiChanged, Qt::UniqueConnection);
	};

	for (QScreen *s : QGuiApplication::screens())
		hookScreen(s);

	connect(qApp, &QGuiApplication::screenAdded, this, [hookScreen](QScreen *s) { hookScreen(s); });
	connect(qApp, &QGuiApplication::primaryScreenChanged, this, &SettingsWindow::onScreenGeometryOrDpiChanged);

	if (windowHandle()) {
		connect(windowHandle(), &QWindow::screenChanged, this, &SettingsWindow::onScreenGeometryOrDpiChanged, Qt::UniqueConnection);
	}
}

void SettingsWindow::onScreenGeometryOrDpiChanged()
{
	const int cellW = qMax(100, fontMetrics().horizontalAdvance(QStringLiteral("Calibration")) + 24);
	const int cellH = qMax(72, fontMetrics().height() * 2 + 48);
	ui->listWidget->setGridSize(QSize(cellW, cellH));
	resizeToFitScreen();
}

void SettingsWindow::rebuildInformationArchitecture()
{
	// ---- Color page (pipeline order §2.5) ----
	auto *colorInner = new QWidget;
	auto *colorLay = new QVBoxLayout(colorInner);
	colorLay->setContentsMargins(16, 12, 16, 12);

	auto *contentBox = new QGroupBox(tr("Content"), colorInner);
	auto *contentForm = new QFormLayout(contentBox);
	takeFromLayout(ui->checkBox_GrabIsAvgColors);
	contentForm->addRow(QString(), ui->checkBox_GrabIsAvgColors);
	colorLay->addWidget(contentBox);

	takeFromLayout(ui->groupBox_ColorAdjustments);
	ui->groupBox_ColorAdjustments->setTitle(tr("Perceptual adjustments"));
	colorLay->addWidget(ui->groupBox_ColorAdjustments);

	auto *wpBox = new QGroupBox(tr("White point / temperature"), colorInner);
	auto *wpLay = new QVBoxLayout(wpBox);
	takeFromLayout(ui->checkBox_GrabApplyBlueLightReduction);
	wpLay->addWidget(ui->checkBox_GrabApplyBlueLightReduction);
	{
		auto *row = new QHBoxLayout;
		takeFromLayout(ui->checkBox_GrabApplyColorTemperature);
		takeFromLayout(ui->pushButton_grabApplyColorTemperatureHelp);
		row->addWidget(ui->checkBox_GrabApplyColorTemperature, 1);
		row->addWidget(ui->pushButton_grabApplyColorTemperatureHelp, 0);
		wpLay->addLayout(row);
	}
	{
		auto *row = new QHBoxLayout;
		takeFromLayout(ui->horizontalSlider_GrabColorTemperature);
		takeFromLayout(ui->spinBox_GrabColorTemperature);
		row->addWidget(ui->horizontalSlider_GrabColorTemperature, 1);
		row->addWidget(ui->spinBox_GrabColorTemperature, 0);
		wpLay->addLayout(row);
	}
	colorLay->addWidget(wpBox);

	auto *obBox = new QGroupBox(tr("Over-brightening"), colorInner);
	auto *obLay = new QHBoxLayout(obBox);
	takeFromLayout(ui->label_11);
	takeFromLayout(ui->horizontalSlider_GrabOverBrighten);
	takeFromLayout(ui->spinBox_GrabOverBrighten);
	takeFromLayout(ui->pushButton_grabOverBrightenHelp);
	obLay->addWidget(ui->label_11);
	obLay->addWidget(ui->horizontalSlider_GrabOverBrighten, 1);
	obLay->addWidget(ui->spinBox_GrabOverBrighten);
	obLay->addWidget(ui->pushButton_grabOverBrightenHelp);
	colorLay->addWidget(obBox);

	auto *smoothBox = new QGroupBox(tr("Host smoothing"), colorInner);
	auto *smoothLay = new QVBoxLayout(smoothBox);
	{
		auto *row = new QHBoxLayout;
		takeFromLayout(ui->label_GrabHostSmoothing_txt);
		takeFromLayout(ui->horizontalSlider_GrabHostSmoothing);
		takeFromLayout(ui->spinBox_GrabHostSmoothing);
		takeFromLayout(ui->pushButton_grabHostSmoothingHelp);
		row->addWidget(ui->label_GrabHostSmoothing_txt);
		row->addWidget(ui->horizontalSlider_GrabHostSmoothing, 1);
		row->addWidget(ui->spinBox_GrabHostSmoothing);
		row->addWidget(ui->pushButton_grabHostSmoothingHelp);
		smoothLay->addLayout(row);
	}
	takeFromLayout(ui->label_GrabHostSmoothing_lightpackNotice);
	smoothLay->addWidget(ui->label_GrabHostSmoothing_lightpackNotice);
	colorLay->addWidget(smoothBox);

	auto *outBox = new QGroupBox(tr("Output"), colorInner);
	auto *outForm = new QFormLayout(outBox);
	{
		auto *row = new QWidget;
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		takeFromLayout(ui->horizontalSlider_GammaCorrection);
		takeFromLayout(ui->doubleSpinBox_DeviceGamma);
		takeFromLayout(ui->pushButton_GammaCorrectionHelp);
		h->addWidget(ui->horizontalSlider_GammaCorrection, 1);
		h->addWidget(ui->doubleSpinBox_DeviceGamma);
		h->addWidget(ui->pushButton_GammaCorrectionHelp);
		takeFromLayout(ui->label_GammaCorrection);
		outForm->addRow(ui->label_GammaCorrection, row);
	}
	{
		auto *row = new QWidget;
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		takeFromLayout(ui->horizontalSlider_DeviceBrightness);
		takeFromLayout(ui->spinBox_DeviceBrightness);
		h->addWidget(ui->horizontalSlider_DeviceBrightness, 1);
		h->addWidget(ui->spinBox_DeviceBrightness);
		takeFromLayout(ui->label_5);
		outForm->addRow(ui->label_5, row);
	}
	{
		auto *row = new QWidget;
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		takeFromLayout(ui->horizontalSlider_DeviceBrightnessCap);
		takeFromLayout(ui->spinBox_DeviceBrightnessCap);
		takeFromLayout(ui->pushButton_BrightnessCapHelp);
		h->addWidget(ui->horizontalSlider_DeviceBrightnessCap, 1);
		h->addWidget(ui->spinBox_DeviceBrightnessCap);
		h->addWidget(ui->pushButton_BrightnessCapHelp);
		takeFromLayout(ui->label_7);
		outForm->addRow(ui->label_7, row);
	}
	takeFromLayout(ui->checkBox_EnableDithering);
	takeFromLayout(ui->pushButton_DitheringHelp);
	{
		auto *row = new QWidget;
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(0, 0, 0, 0);
		h->addWidget(ui->checkBox_EnableDithering, 1);
		h->addWidget(ui->pushButton_DitheringHelp, 0);
		outForm->addRow(QString(), row);
	}
	colorLay->addWidget(outBox);

	takeFromLayout(ui->groupBox_3);
	ui->groupBox_3->setTitle(tr("Luminosity threshold"));
	colorLay->addWidget(ui->groupBox_3);
	colorLay->addStretch(1);

	m_scrollAreaColor = wrapInScrollArea(colorInner);
	m_tabColor = makeTabShell(m_scrollAreaColor);

	// ---- Geometry page ----
	auto *geoInner = new QWidget;
	auto *geoLay = new QVBoxLayout(geoInner);
	geoLay->setContentsMargins(16, 12, 16, 12);

	auto *stageLabel = new QLabel(tr("Live stage (post-pipeline)"), geoInner);
	stageLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
	geoLay->addWidget(stageLabel);
	takeFromLayout(ui->frame_VirtualLeds);
	geoLay->addWidget(ui->frame_VirtualLeds);

	takeFromLayout(ui->groupBox_2);
	ui->groupBox_2->setTitle(tr("Zone overlay"));
	geoLay->addWidget(ui->groupBox_2);

	takeFromLayout(ui->groupBox_ContentAspect);
	geoLay->addWidget(ui->groupBox_ContentAspect);

	{
		auto *row = new QHBoxLayout;
		takeFromLayout(ui->pushButton_ReapplyLedGroups);
		takeFromLayout(ui->pushButton_ReapplyLedGroupsHelp);
		row->addWidget(ui->pushButton_ReapplyLedGroups);
		row->addWidget(ui->pushButton_ReapplyLedGroupsHelp);
		row->addStretch(1);
		geoLay->addLayout(row);
	}
	geoLay->addStretch(1);

	m_scrollAreaGeometry = wrapInScrollArea(geoInner);
	m_tabGeometry = makeTabShell(m_scrollAreaGeometry);

	// ---- Scene: profiles under mode ----
	if (auto *sceneLay = qobject_cast<QGridLayout*>(ui->tabSoftwareOptions->layout())) {
		takeFromLayout(ui->groupBox_Profiles);
		sceneLay->addWidget(ui->groupBox_Profiles, 5, 0);
	}

	// ---- Advanced: Expert + Plugins + language ----
	if (auto *advLay = qobject_cast<QVBoxLayout*>(ui->tabExpert->layout())) {
		takeFromLayout(ui->groupBox_UserInterface);
		advLay->insertWidget(0, ui->groupBox_UserInterface);

		auto *pluginsBox = new QGroupBox(tr("Plugins"), ui->tabExpert);
		auto *pluginsLay = new QVBoxLayout(pluginsBox);
		// Move main plugins widgets from tabPlugins
		if (ui->tabPlugins->layout()) {
			while (QLayoutItem *it = ui->tabPlugins->layout()->takeAt(0)) {
				if (QWidget *w = it->widget()) {
					pluginsLay->addWidget(w);
				} else if (QLayout *sub = it->layout()) {
					pluginsLay->addLayout(sub);
				}
				delete it;
			}
		}
		advLay->addWidget(pluginsBox);
	}

	// ---- About: absorb Help ----
	if (auto *aboutLay = qobject_cast<QVBoxLayout*>(ui->tabAbout->layout())) {
		auto *helpHeading = new QLabel(tr("Help"), ui->tabAbout);
		helpHeading->setStyleSheet(QStringLiteral("font-weight:600; margin-top: 0.6em;"));
		aboutLay->addWidget(helpHeading);
		takeFromLayout(ui->textBrowserHelp);
		ui->textBrowserHelp->setMinimumHeight(180);
		aboutLay->addWidget(ui->textBrowserHelp, 1);
	}

	// ---- Calibration ----
	m_calibrationPage = new CalibrationPage(this);
	connect(m_calibrationPage, &CalibrationPage::updateLedsColors, this, &SettingsWindow::updateLedsColors);
	connect(m_calibrationPage, &CalibrationPage::showLedWidgets, this, &SettingsWindow::showLedWidgets);
	connect(m_calibrationPage, &CalibrationPage::setColorFeedbackForced, this, &SettingsWindow::setColorFeedbackForced);
	connect(m_calibrationPage, &CalibrationPage::sessionActiveChanged, this, &SettingsWindow::onCalibrationSessionActive);

	// ---- Rebuild tabWidget + listWidget (1:1) ----
	while (ui->tabWidget->count() > 0)
		ui->tabWidget->removeTab(0);

	ui->tabWidget->addTab(ui->tabSoftwareOptions, tr("Scene"));
	ui->tabWidget->addTab(m_tabGeometry, tr("Geometry"));
	ui->tabWidget->addTab(m_tabColor, tr("Color"));
	ui->tabWidget->addTab(m_calibrationPage, tr("Calibration"));
	ui->tabWidget->addTab(ui->tabDeviceOptions, tr("Device"));
	ui->tabWidget->addTab(ui->tabExpert, tr("Advanced"));
	ui->tabWidget->addTab(ui->tabAbout, tr("About"));

	m_calibrationNavIndex = 3;

	// Hide orphaned tabs (not deleted — Designer pointers remain valid)
	ui->tabProfiles->hide();
	ui->tabPlugins->hide();
	ui->tabHelp->hide();
	ui->tabDeviceVirtual->hide();

	ui->listWidget->clear();
	ui->listWidget->addItem(makeNavItem(tr("Scene"), QStringLiteral(":/icons/modes.png")));
	ui->listWidget->addItem(makeNavItem(tr("Geometry"), QStringLiteral(":/icons/profiles2.png")));
	ui->listWidget->addItem(makeNavItem(tr("Color"), QStringLiteral(":/icons/settings.png")));
	ui->listWidget->addItem(makeNavItem(tr("Calibration"), QStringLiteral(":/icons/profiles.png")));
	ui->listWidget->addItem(makeNavItem(tr("Device"), QStringLiteral(":/icons/device.png")));
	ui->listWidget->addItem(makeNavItem(tr("Advanced"), QStringLiteral(":/icons/nerd2.png")));
	ui->listWidget->addItem(makeNavItem(tr("About"), QStringLiteral(":/icons/about.png")));

	if (QTabBar *tabBar = ui->tabWidget->findChild<QTabBar*>())
		tabBar->hide();

	ui->tabWidget->setCurrentIndex(0);
	ui->listWidget->setCurrentRow(0);
}
