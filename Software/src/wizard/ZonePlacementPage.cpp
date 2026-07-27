/*
 * ZoneConfiguration.cpp
 *
 *	Created on: 10/25/2013
 *		Project: Prismatik
 *
 *	Copyright (c) 2013 Tim
 *
 *	Lightpack is an open-source, USB content-driving ambient lighting
 *	hardware.
 *
 *	Prismatik is a free, open-source software: you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as published
 *	by the Free Software Foundation, either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	Prismatik and Lightpack files is distributed in the hope that it will be
 *	useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 *	General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "ZonePlacementPage.hpp"
#include "ui_ZonePlacementPage.h"
#include "AbstractLedDevice.hpp"
#include "Settings.hpp"
#include "CustomDistributor.hpp"
#include "LayoutRecipeGenerator.hpp"
#include "GrabWidget.hpp"
#include "LedDeviceLightpack.hpp"
#include "MonitorIdForm.hpp"
#include "BulkResize.hpp"
#include "LedGroupRuntime.hpp"
#include "ColorButton.hpp"
#include <QJsonArray>
#include <QListWidgetItem>
#include <QSet>
#include <QPixmap>
#include <QIcon>

using SettingsScope::LedGroup;


ZonePlacementPage::ZonePlacementPage(bool isInitFromSettings, TransientSettings *ts, QWidget *parent):
	WizardPageUsingDevice(isInitFromSettings, ts, parent),
	_ui(new Ui::ZonePlacementPage)
{
	_ui->setupUi(this);
	_ledNumberUpdate.setSingleShot(true);
}

ZonePlacementPage::~ZonePlacementPage()
{
	delete _ui;
}

void ZonePlacementPage::initializePage()
{
	using namespace SettingsScope;

	int i = 0;
	for (const QScreen* const screen : QGuiApplication::screens()) {
		const QString& displayName = QStringLiteral("Display %1").arg(QString::number(i + 1));
		MonitorIdForm* const monitorIdForm = new MonitorIdForm(displayName, screen->geometry());
		monitorIdForm->show();
		_monitorForms.append(monitorIdForm);

		_ui->cbMonitorSelect->addItem(displayName, i);
		MonitorSettings settings;
		settings.screen = screen;
		saveMonitorSettings(settings);
		_screens.insert(i, settings);
		++i;
	}
	this->activateWindow();
	_monitorForms[_ui->cbMonitorSelect->currentIndex()]->setActive(true);
	resetNewAreaRect();

	device()->setSmoothSlowdown(70);

	connect(_ui->pbAndromeda, &QPushButton::clicked, this, &ZonePlacementPage::onAndromeda_clicked);
	connect(_ui->pbCassiopeia, &QPushButton::clicked, this, &ZonePlacementPage::onCassiopeia_clicked);
	connect(_ui->pbPegasus, &QPushButton::clicked, this, &ZonePlacementPage::onPegasus_clicked);
	connect(_ui->pbApply, &QPushButton::clicked, this, &ZonePlacementPage::onApply_clicked);
	connect(_ui->pbClearDisplay, &QPushButton::clicked, this, &ZonePlacementPage::onClearDisplay_clicked);
	connect(_ui->cbMonitorSelect, qOverload<int>(&QComboBox::currentIndexChanged), this, &ZonePlacementPage::onMonitor_currentIndexChanged);

	connect(_ui->sbNumberOfLeds, qOverload<int>(&QSpinBox::valueChanged), this, &ZonePlacementPage::onNumberOfLeds_valueChanged);
	connect(_ui->sbTopLeds, qOverload<int>(&QSpinBox::valueChanged), this, &ZonePlacementPage::onTopLeds_valueChanged);
	connect(_ui->sbSideLeds, qOverload<int>(&QSpinBox::valueChanged), this, &ZonePlacementPage::onSideLeds_valueChanged);

	connect(_ui->pushButton_ResizeAllApply, &QPushButton::clicked, this, &ZonePlacementPage::onResizeAllApply_clicked);
	connect(_ui->comboBox_GroupEdge, qOverload<int>(&QComboBox::currentIndexChanged), this, &ZonePlacementPage::onGroupEdge_currentIndexChanged);
	connect(_ui->pushButton_GroupApply, &QPushButton::clicked, this, &ZonePlacementPage::onGroupApply_clicked);
	connect(_ui->pushButton_GroupRemove, &QPushButton::clicked, this, &ZonePlacementPage::onGroupRemove_clicked);
	connect(_ui->listWidget_LedGroups, &QListWidget::itemClicked, this, &ZonePlacementPage::onGroupListItem_clicked);
	connect(_ui->checkBox_GroupHasColor, &QCheckBox::toggled, this, &ZonePlacementPage::onGroupHasColor_toggled);
	updateGroupEdgeControlsVisibility();
	refreshGroupList();

	_ui->sbNumberOfLeds->setMaximum(device()->maxLedsCount());
	_ui->sbStartingLed->setMaximum(device()->maxLedsCount() - 1);
	_ui->sbNumberOfLeds->blockSignals(true);

	int monitorLedCount = 0;
	if (_isInitFromSettings) {
		const int ledCount = Settings::getNumberOfLeds(Settings::getConnectedDevice());
		_transSettings->ledCount = ledCount;
		_zonePool.reserve(_transSettings->ledCount);
		for (MonitorSettings& settings : _screens) {
			int startingLed = -1;
			for (int i = 0; i < ledCount; i++) {
				const QRect areaGeometry(Settings::getLedPosition(i), Settings::getLedSize(i));
				if (settings.screen->geometry().contains(areaGeometry.center())) {
					addGrabArea(settings.grabAreas, i, areaGeometry, Settings::isLedEnabled(i));
					startingLed = startingLed < 0 ? i : std::min(i, startingLed);
				}
			}
			settings.startingLed = startingLed;
		}
		monitorLedCount = _screens[_ui->cbMonitorSelect->currentIndex()].grabAreas.count();
		if (monitorLedCount == 0)
			monitorLedCount = device()->defaultLedsCount();
		_ui->sbStartingLed->setValue(_screens[_ui->cbMonitorSelect->currentIndex()].startingLed + 1);
	} else {
		monitorLedCount = device()->defaultLedsCount();
		_transSettings->ledCount = device()->defaultLedsCount();
		onAndromeda_clicked();
	}
	_ui->sbNumberOfLeds->setValue(monitorLedCount);
	_ui->sbNumberOfLeds->blockSignals(false);
	resetDeviceSettings();
	turnLightsOff();
	checkZoneIssues();

	connect(&_ledNumberUpdate, &QTimer::timeout, this, &ZonePlacementPage::onNumberOfLeds_timeout);
}

void ZonePlacementPage::cleanupPage()
{
	qDeleteAll(_zonePool);
	cleanupGrabAreas();
	cleanupMonitors();
}

void ZonePlacementPage::cleanupMonitors()
{
	qDeleteAll(_monitorForms);
	_monitorForms.clear();
}

void ZonePlacementPage::cleanupGrabAreas(int idx)
{
	QMap<int, MonitorSettings>::iterator it = _screens.begin();
	while (it != _screens.end()) {
		if (idx < 0 || idx == it.key()) {
			qDeleteAll(it.value().grabAreas);
			it.value().grabAreas.clear();
		}
		++it;
	}
}

void ZonePlacementPage::onClearDisplay_clicked()
{
	cleanupGrabAreas(_ui->cbMonitorSelect->currentIndex());
	checkZoneIssues();
}

bool ZonePlacementPage::checkZoneIssues()
{
	QMultiMap<int, std::nullptr_t> ids;
	// get all IDs (even repeating ones)
	for (const MonitorSettings& settings : _screens) {
		for (const GrabWidget* const widget : settings.grabAreas)
			ids.insert(widget->getId(), nullptr);
	}

	// build gap string list and gather overlaping IDs (no repeats)
	QStringList gapStrs;
	int prevId = -1;
	QList<int> overlapIds;
	QMultiMap<int, std::nullptr_t>::const_iterator idIt = ids.constBegin();
	while (idIt != ids.constEnd()) {
		const int id = idIt.key();
		const int delta = id - prevId;
		if (delta == 2)
			gapStrs << QString::number(id);
		else if (delta > 2)
			gapStrs << QStringLiteral("%1-%2").arg(QString::number(id - delta + 2), QString::number(id));
		else if (delta == 0)
			overlapIds << id;
		prevId = id;
		++idIt;
	}

	// condense overlapping IDs into "X-Y" ranges when possible and build the string list
	QStringList overlapStrs;
	prevId = -1;
	int overlapStart = -1;
	auto addOverlap = [&overlapStrs](const int overlapStart, const int prevId) {
		if (overlapStart == prevId)
			overlapStrs << QString::number(prevId + 1);
		else
			overlapStrs << QStringLiteral("%1-%2").arg(QString::number(overlapStart + 1), QString::number(prevId + 1));
	};
	for (const int id : overlapIds) {
		if (prevId > -1) {
			if (id - prevId > 1) {
				addOverlap(overlapStart, prevId);
				overlapStart = id;
			}
		} else
			overlapStart = id;
		prevId = id;
	}
	if (prevId > -1 && overlapStart > -1)
		addOverlap(overlapStart, prevId);

	QStringList errors;
	if (!gapStrs.isEmpty())
		errors << QStringLiteral("Missing LEDs: %1").arg(gapStrs.join(QStringLiteral(", ")));
	if (!overlapStrs.isEmpty())
		errors << QStringLiteral("LEDs on multiple displays: %1").arg(overlapStrs.join(QStringLiteral(", ")));
	if (!errors.isEmpty())
		errors << QStringLiteral("Adjust the number of LEDs and/or the starting LED for concerned displays");
	_ui->labelZoneIssues->setText(errors.join('\n'));
	return errors.isEmpty();
}

bool ZonePlacementPage::validatePage()
{
	if (!checkZoneIssues())
		return false;

	_transSettings->zonePositions.clear();
	_transSettings->zoneSizes.clear();
	_transSettings->zoneEnabled.clear();
	_transSettings->ledCount = 0;
	for (const MonitorSettings& settings : _screens) {
		for (const GrabWidget* const grabArea : settings.grabAreas) {
			_transSettings->zonePositions.insert(grabArea->getId(), grabArea->geometry().topLeft());
			_transSettings->zoneSizes.insert(grabArea->getId(), grabArea->geometry().size());
			_transSettings->zoneEnabled.insert(grabArea->getId(), grabArea->isAreaEnabled());
			_transSettings->ledCount = std::max(grabArea->getId() + 1, _transSettings->ledCount);
		}
	}

	// Persist a layout recipe (see docs/plans/presets-aspect-ratio.md) for every
	// monitor that has zones, so content-aspect presets can regenerate them
	// later without re-running the wizard. Make sure the currently selected
	// monitor's settings reflect the live UI controls first.
	saveMonitorSettings(_screens[_ui->cbMonitorSelect->currentIndex()]);
	QJsonArray recipe;
	for (const MonitorSettings& settings : _screens) {
		if (settings.grabAreas.isEmpty())
			continue;

		LayoutRecipeGenerator::MonitorRecipe monitorRecipe;
		monitorRecipe.startingLed = settings.startingLed;
		monitorRecipe.topLeds = settings.topLeds;
		monitorRecipe.sideLeds = settings.sideLeds;
		monitorRecipe.bottomLeds = std::max(0, (int)settings.grabAreas.count() - settings.topLeds - 2 * settings.sideLeds);
		monitorRecipe.thicknessPercent = settings.thickness;
		monitorRecipe.standWidthPercent = settings.standWidth;
		monitorRecipe.skipCorners = settings.skipCorners;
		monitorRecipe.invertOrder = settings.invertOrder;
		monitorRecipe.numberingOffset = settings.offset;
		monitorRecipe.baseRect = marginAdjustedRect(settings.screen->geometry(), settings.topMargin, settings.sideMargin, settings.bottomMargin);

		recipe.append(LayoutRecipeGenerator::toJson(monitorRecipe));
	}
	_transSettings->layoutRecipe = recipe;

	cleanupPage();
	return true;
}

void ZonePlacementPage::resetNewAreaRect()
{
	const QRect screen = screenRect();
	_newAreaRect.setX(screen.left() + 150);
	_newAreaRect.setY(screen.top() + 150);
	_newAreaRect.setWidth(100);
	_newAreaRect.setHeight(100);
}

void ZonePlacementPage::distributeAreas(AreaDistributor *distributor, bool invertIds, int idOffset)
{
	QList<GrabWidget*>& grabAreas = _screens[_ui->cbMonitorSelect->currentIndex()].grabAreas;
	const int startId = _ui->sbStartingLed->value() - 1;

	for (GrabWidget* const widget : grabAreas)
		widget->hide();
	_zonePool.append(grabAreas);

	grabAreas.clear();
	const QMap<int, QRect> rects = LayoutRecipeGenerator::generate(*distributor, invertIds, idOffset, startId);
	grabAreas.reserve(rects.size());
	for (auto it = rects.constBegin(); it != rects.constEnd(); ++it)
		addGrabArea(grabAreas, it.key(), it.value());
	resetNewAreaRect();

	_transSettings->ledCount = 0;
	for (const MonitorSettings& settings : _screens) {
		for (const GrabWidget* const widget : settings.grabAreas)
			_transSettings->ledCount = std::max(widget->getId() + 1, _transSettings->ledCount);
	}
	checkZoneIssues();
}

void ZonePlacementPage::addGrabArea(QList<GrabWidget*>& list, int id, const QRect &r, const bool enabled)
{
	const bool reuse = !_zonePool.isEmpty();
	GrabWidget * const zone = reuse ? _zonePool.takeLast() : new GrabWidget(id, DimUntilInteractedWith | AllowEnableConfig | AllowMove | AllowResize, &list);

	zone->move(r.topLeft());
	zone->resize(r.size());
	zone->setAreaEnabled(enabled);
	if (reuse) {
		zone->setId(id);
		zone->setFellows(&list);
	} else {
		connect(zone, &GrabWidget::resizeOrMoveStarted, this, &ZonePlacementPage::turnLightOn);
		connect(zone, &GrabWidget::resizeOrMoveCompleted, this, qOverload<>(&ZonePlacementPage::turnLightsOff));
		connect(zone, &GrabWidget::mouseRightButtonClicked, this, &ZonePlacementPage::onGrabWidgetRightClicked);
	}
	zone->show();
	list.append(zone);
}

void ZonePlacementPage::removeLastGrabArea()
{
	QList<GrabWidget*>& grabAreas = _screens[_ui->cbMonitorSelect->currentIndex()].grabAreas;
	_zonePool.append(grabAreas.takeLast());
	_zonePool.last()->hide();
}

QRect ZonePlacementPage::screenRect() const
{
	const QRect screen = QGuiApplication::screens().value(_ui->cbMonitorSelect->currentIndex(), QGuiApplication::primaryScreen())->geometry();
	return marginAdjustedRect(screen,
		_ui->doubleSpinBox_topMargin->value(),
		_ui->doubleSpinBox_sideMargin->value(),
		_ui->doubleSpinBox_bottomMargin->value());
}

QRect ZonePlacementPage::marginAdjustedRect(const QRect& screen, double topMarginPct, double sideMarginPct, double bottomMarginPct)
{
	QRect result = screen;
	const int topMargin = std::floor(screen.height() * topMarginPct / 100.0);
	const int sideMargin = std::floor(screen.width() * sideMarginPct / 100.0);
	const int bottomMargin = std::floor(screen.height() * bottomMarginPct / 100.0);
	result.setTopLeft(QPoint(screen.left() + sideMargin, screen.top() + topMargin));
	result.setBottomRight(QPoint(screen.right() - sideMargin, screen.bottom() - bottomMargin));
	return result;
}

void ZonePlacementPage::onAndromeda_clicked()
{
	const QRect screen = screenRect();
	const int bottomWidth = screen.width() * (1.0 - _ui->sbStandWidth->value() / 100.0);
	const int perimeter = screen.width() + screen.height() * 2 + bottomWidth;
	const int ledSize = perimeter / _ui->sbNumberOfLeds->value();

	const int bottomLeds = ((bottomWidth / ledSize) + 1) & ~1;//round up / down to next even number
	const int sideLeds = screen.height() / ledSize;
	const int topLeds = _ui->sbNumberOfLeds->value() - bottomLeds - sideLeds * 2;
	CustomDistributor custom(
		screen,
		topLeds,
		sideLeds,
		bottomLeds,
		_ui->sbThickness->value() / 100.0,
		_ui->sbStandWidth->value() / 100.0,
		_ui->checkBox_skipCorners->isChecked()
	);

	distributeAreas(&custom, _ui->cbInvertOrder->isChecked(), _ui->sbNumberingOffset->value());
	_ui->sbTopLeds->setValue(topLeds);
	_ui->sbSideLeds->setValue(sideLeds);
	_ui->sbBottomLeds->setValue(bottomLeds);

	MonitorSettings& settings = _screens[_ui->cbMonitorSelect->currentIndex()];
	saveMonitorSettings(settings);
}

void ZonePlacementPage::onCassiopeia_clicked()
{
	const QRect screen = screenRect();
	const int perimeter = screen.width() + screen.height() * 2;
	const int ledSize = perimeter / _ui->sbNumberOfLeds->value();
	const int sideLeds = screen.height() / ledSize;
	const int topLeds = _ui->sbNumberOfLeds->value() - sideLeds * 2;
	CustomDistributor custom(
		screen,
		topLeds,
		sideLeds,
		0,
		_ui->sbThickness->value() / 100.0,
		_ui->sbStandWidth->value() / 100.0,
		_ui->checkBox_skipCorners->isChecked()
	);

	distributeAreas(&custom, _ui->cbInvertOrder->isChecked(), _ui->sbNumberingOffset->value());
	_ui->sbTopLeds->setValue(topLeds);
	_ui->sbSideLeds->setValue(sideLeds);
	_ui->sbBottomLeds->setValue(0);

	MonitorSettings& settings = _screens[_ui->cbMonitorSelect->currentIndex()];
	saveMonitorSettings(settings);
}

void ZonePlacementPage::onPegasus_clicked()
{
	const QRect screen = screenRect();
	const int sideLeds = _ui->sbNumberOfLeds->value() / 2;
	CustomDistributor custom(
		screen,
		0,
		sideLeds,
		0,
		_ui->sbThickness->value() / 100.0,
		_ui->sbStandWidth->value() / 100.0,
		_ui->checkBox_skipCorners->isChecked()
	);

	distributeAreas(&custom, _ui->cbInvertOrder->isChecked(), _ui->sbNumberingOffset->value());
	_ui->sbTopLeds->setValue(0);
	_ui->sbSideLeds->setValue(sideLeds);
	_ui->sbBottomLeds->setValue(0);

	MonitorSettings& settings = _screens[_ui->cbMonitorSelect->currentIndex()];
	saveMonitorSettings(settings);
}


void ZonePlacementPage::onApply_clicked()
{
	const QRect screen = screenRect();
	CustomDistributor custom(
		screen,
		_ui->sbTopLeds->value(),
		_ui->sbSideLeds->value(),
		_ui->sbBottomLeds->value(),
		_ui->sbThickness->value() / 100.0,
		_ui->sbStandWidth->value() / 100.0,
		_ui->checkBox_skipCorners->isChecked()
	);

	distributeAreas(&custom, _ui->cbInvertOrder->isChecked(), _ui->sbNumberingOffset->value());

	MonitorSettings& settings = _screens[_ui->cbMonitorSelect->currentIndex()];
	_ui->sbNumberOfLeds->setValue(settings.grabAreas.size());
	saveMonitorSettings(settings);
}

void ZonePlacementPage::onNumberOfLeds_timeout()
{
	const int numOfLed = _ui->sbNumberOfLeds->value();

	QList<GrabWidget*>& grabAreas = _screens[_ui->cbMonitorSelect->currentIndex()].grabAreas;
	while (numOfLed < grabAreas.size())
		removeLastGrabArea();

	int maxId = 0;
	for (const GrabWidget* const widget : grabAreas)
		maxId = std::max(maxId, widget->getId());

	const QRect screen = screenRect();

	const int dx = 10;
	const int dy = 10;
	grabAreas.reserve(numOfLed);
	while (numOfLed > grabAreas.size()) {
		addGrabArea(grabAreas, ++maxId, _newAreaRect);
		if (_newAreaRect.right() + dx < screen.right()) {
			_newAreaRect.moveTo(_newAreaRect.x() + dx, _newAreaRect.y());
		} else if (_newAreaRect.bottom() + dy < screen.bottom()) {
			_newAreaRect.moveTo(screen.left() + 150, _newAreaRect.y() + dy);
		} else {
			_newAreaRect.moveTo(screen.left() + 150, screen.top() + 150);
		}
	}
	checkZoneIssues();
}

void ZonePlacementPage::onNumberOfLeds_valueChanged(int numOfLed)
{
	Q_UNUSED(numOfLed);
	// this delay is meant to leave the UI responsive while adjusting the number of leds
	// it has to be long enough to have the time to type 3-4 digits and/or adjust via spinbox controls
	// so the grab areas spawn/despawn has a higher chance of occurring when the user is done editing
	using namespace std::chrono_literals;
	_ledNumberUpdate.start(500ms);
}

void ZonePlacementPage::onTopLeds_valueChanged(int numOfLed)
{
	_ui->sbBottomLeds->setValue(_ui->sbNumberOfLeds->value() - numOfLed - _ui->sbSideLeds->value() * 2);
	_screens[_ui->cbMonitorSelect->currentIndex()].topLeds = numOfLed;
}

void ZonePlacementPage::onSideLeds_valueChanged(int numOfLed)
{
	_ui->sbBottomLeds->setValue(_ui->sbNumberOfLeds->value() - _ui->sbTopLeds->value() - numOfLed * 2);
	_screens[_ui->cbMonitorSelect->currentIndex()].sideLeds = numOfLed;
}

void ZonePlacementPage::onMonitor_currentIndexChanged(int idx)
{
	int i = 0;
	for (MonitorIdForm* const monitorId : _monitorForms)
		monitorId->setActive(idx == i++);

	resetNewAreaRect();

	const MonitorSettings& settings = _screens[idx];
	_ui->sbStartingLed->setValue(settings.startingLed + 1);
	_ui->sbNumberOfLeds->blockSignals(true);
	const int ledCount = settings.grabAreas.count();
	_ui->sbNumberOfLeds->setValue(ledCount > 0 ? ledCount : device()->defaultLedsCount());
	_ui->sbNumberOfLeds->blockSignals(false);
	_ui->sbTopLeds->setValue(settings.topLeds);
	_ui->sbSideLeds->setValue(settings.sideLeds);
	_ui->sbNumberingOffset->setValue(settings.offset);
	_ui->sbThickness->setValue(settings.thickness);
	_ui->sbStandWidth->setValue(settings.standWidth);
	_ui->doubleSpinBox_topMargin->setValue(settings.topMargin);
	_ui->doubleSpinBox_sideMargin->setValue(settings.sideMargin);
	_ui->doubleSpinBox_bottomMargin->setValue(settings.bottomMargin);
	_ui->cbInvertOrder->setChecked(settings.invertOrder);
	_ui->checkBox_skipCorners->setChecked(settings.skipCorners);
}

void ZonePlacementPage::saveMonitorSettings(MonitorSettings& settings)
{
	settings.startingLed = _ui->sbStartingLed->value() - 1;
	settings.topLeds = _ui->sbTopLeds->value();
	settings.sideLeds = _ui->sbSideLeds->value();
	settings.offset = _ui->sbNumberingOffset->value();
	settings.thickness = _ui->sbThickness->value();
	settings.standWidth = _ui->sbStandWidth->value();
	settings.topMargin = _ui->doubleSpinBox_topMargin->value();
	settings.sideMargin = _ui->doubleSpinBox_sideMargin->value();
	settings.bottomMargin = _ui->doubleSpinBox_bottomMargin->value();
	settings.invertOrder = _ui->cbInvertOrder->isChecked();
	settings.skipCorners = _ui->checkBox_skipCorners->isChecked();
}

QList<GrabWidget*> ZonePlacementPage::selectedForGroupEdit() const
{
	QList<GrabWidget*> result;
	for (const MonitorSettings& settings : _screens) {
		for (GrabWidget* const widget : settings.grabAreas) {
			if (widget->isSelectedForGroupEdit())
				result.append(widget);
		}
	}
	return result;
}

void ZonePlacementPage::clearGroupEditSelection()
{
	for (GrabWidget* const widget : selectedForGroupEdit())
		widget->setSelectedForGroupEdit(false);
}

void ZonePlacementPage::onGrabWidgetRightClicked(int)
{
	const int count = selectedForGroupEdit().count();
	_ui->label_GroupHint->setText(count > 0
		? tr("%1 box(es) selected for the group. Right-click to toggle.").arg(count)
		: tr("Right-click boxes to mark them as members of the group being edited."));
}

void ZonePlacementPage::updateGroupEdgeControlsVisibility()
{
	const LedGroup::Edge edge = static_cast<LedGroup::Edge>(_ui->comboBox_GroupEdge->currentIndex());
	const bool showWidth = edge == LedGroup::Edge::Left || edge == LedGroup::Edge::Right || edge == LedGroup::Edge::Custom;
	const bool showHeight = edge == LedGroup::Edge::Top || edge == LedGroup::Edge::Bottom || edge == LedGroup::Edge::Custom;

	_ui->label_GroupWidth->setVisible(showWidth);
	_ui->spinBox_GroupWidth->setVisible(showWidth);
	_ui->label_GroupHeight->setVisible(showHeight);
	_ui->spinBox_GroupHeight->setVisible(showHeight);
}

void ZonePlacementPage::onGroupEdge_currentIndexChanged(int)
{
	updateGroupEdgeControlsVisibility();
}

void ZonePlacementPage::applyResizeToWidgets(const QList<GrabWidget*>& widgets, int newWidth, int newHeight, Qt::Corner anchor)
{
	// BulkResize::resizedKeepingAnchor is pure math (unit-tested in
	// BulkResizeTest, which links without GrabWidget/QApplication) - the
	// widget-touching loop stays here since GrabWidget requires a real
	// QApplication and is not part of the test binary.
	for (GrabWidget* const widget : widgets) {
		const QRect current(widget->pos(), widget->size());
		const QRect updated = BulkResize::resizedKeepingAnchor(current, newWidth, newHeight, anchor);

		if (updated.size() != current.size())
			widget->resize(updated.size());
		if (updated.topLeft() != current.topLeft())
			widget->move(updated.topLeft());

		widget->saveSizeAndPosition();
	}
}

void ZonePlacementPage::onResizeAllApply_clicked()
{
	const QList<GrabWidget*>& grabAreas = _screens[_ui->cbMonitorSelect->currentIndex()].grabAreas;
	applyResizeToWidgets(grabAreas, _ui->spinBox_ResizeAllWidth->value(), _ui->spinBox_ResizeAllHeight->value(), Qt::TopLeftCorner);
	checkZoneIssues();
}

static QString groupEdgeName(LedGroup::Edge edge)
{
	switch (edge) {
	case LedGroup::Edge::Top: return QStringLiteral("Top");
	case LedGroup::Edge::Bottom: return QStringLiteral("Bottom");
	case LedGroup::Edge::Left: return QStringLiteral("Left");
	case LedGroup::Edge::Right: return QStringLiteral("Right");
	case LedGroup::Edge::Custom: return QStringLiteral("Custom");
	}
	return QStringLiteral("Custom");
}

void ZonePlacementPage::refreshGroupList()
{
	_ui->listWidget_LedGroups->clear();
	for (const LedGroup& group : SettingsScope::Settings::getLedGroups()) {
		QListWidgetItem* const item = new QListWidgetItem(
			tr("%1 (%2, %3 members)").arg(group.name, groupEdgeName(group.edge)).arg(group.memberIds.count()));
		item->setData(Qt::UserRole, group.name);
		if (group.hasColor) {
			QPixmap swatch(12, 12);
			swatch.fill(group.color);
			item->setIcon(QIcon(swatch));
		}
		_ui->listWidget_LedGroups->addItem(item);
	}
}

void ZonePlacementPage::onGroupHasColor_toggled(bool checked)
{
	_ui->pushButton_GroupColor->setEnabled(checked);
}

void ZonePlacementPage::onGroupApply_clicked()
{
	const QString name = _ui->lineEdit_GroupName->text().trimmed();
	if (name.isEmpty()) {
		_ui->label_GroupHint->setText(tr("Enter a name for the group before applying it."));
		return;
	}

	const QList<GrabWidget*> selected = selectedForGroupEdit();
	if (selected.isEmpty()) {
		_ui->label_GroupHint->setText(tr("Right-click at least one box before applying the group."));
		return;
	}

	LedGroup group;
	group.name = name;
	group.edge = static_cast<LedGroup::Edge>(_ui->comboBox_GroupEdge->currentIndex());
	group.width = _ui->spinBox_GroupWidth->isVisible() ? _ui->spinBox_GroupWidth->value() : -1;
	group.height = _ui->spinBox_GroupHeight->isVisible() ? _ui->spinBox_GroupHeight->value() : -1;
	group.enabled = true;
	group.hasColor = _ui->checkBox_GroupHasColor->isChecked();
	group.color = _ui->pushButton_GroupColor->getColor();
	for (GrabWidget* const widget : selected)
		group.memberIds.append(widget->getId());

	LedGroupRuntime::applyGroup(group);

	// LedGroupRuntime writes through Settings only (so it also works outside
	// the wizard, without live GrabWidgets) - reflect the result onto the
	// live wizard widgets here so validatePage() picks up the new geometry
	// instead of the stale pre-apply positions.
	for (GrabWidget* const widget : selected) {
		widget->move(SettingsScope::Settings::getLedPosition(widget->getId()));
		widget->resize(SettingsScope::Settings::getLedSize(widget->getId()));
	}

	QList<LedGroup> groups = SettingsScope::Settings::getLedGroups();
	bool replaced = false;
	for (LedGroup& existing : groups) {
		if (existing.name == group.name) {
			existing = group;
			replaced = true;
			break;
		}
	}
	if (!replaced)
		groups.append(group);
	SettingsScope::Settings::setLedGroups(groups);

	clearGroupEditSelection();
	_ui->lineEdit_GroupName->clear();
	_ui->checkBox_GroupHasColor->setChecked(false);
	_ui->label_GroupHint->setText(tr("Right-click boxes to mark them as members of the group being edited."));
	refreshGroupList();
	checkZoneIssues();
}

void ZonePlacementPage::onGroupRemove_clicked()
{
	QListWidgetItem* const item = _ui->listWidget_LedGroups->currentItem();
	if (!item)
		return;

	const QString name = item->data(Qt::UserRole).toString();
	QList<LedGroup> groups = SettingsScope::Settings::getLedGroups();
	for (int i = 0; i < groups.count(); ++i) {
		if (groups.at(i).name == name) {
			groups.removeAt(i);
			break;
		}
	}
	// Removing a group definition does not undo the resize already applied
	// to its member LEDs - only stops it from being reapplied later.
	SettingsScope::Settings::setLedGroups(groups);
	refreshGroupList();
}

void ZonePlacementPage::onGroupListItem_clicked(QListWidgetItem *item)
{
	if (!item)
		return;

	const QString name = item->data(Qt::UserRole).toString();
	LedGroup found;
	bool foundGroup = false;
	for (const LedGroup& group : SettingsScope::Settings::getLedGroups()) {
		if (group.name == name) {
			found = group;
			foundGroup = true;
			break;
		}
	}
	if (!foundGroup)
		return;

	clearGroupEditSelection();
	_ui->lineEdit_GroupName->setText(found.name);
	_ui->comboBox_GroupEdge->setCurrentIndex(static_cast<int>(found.edge));
	if (found.width > 0)
		_ui->spinBox_GroupWidth->setValue(found.width);
	if (found.height > 0)
		_ui->spinBox_GroupHeight->setValue(found.height);
	_ui->checkBox_GroupHasColor->setChecked(found.hasColor);
	if (found.hasColor)
		_ui->pushButton_GroupColor->setColor(found.color);

	const QSet<int> memberIds(found.memberIds.constBegin(), found.memberIds.constEnd());
	for (const MonitorSettings& settings : _screens) {
		for (GrabWidget* const widget : settings.grabAreas) {
			if (memberIds.contains(widget->getId()))
				widget->setSelectedForGroupEdit(true);
		}
	}
	onGrabWidgetRightClicked(0);
}
