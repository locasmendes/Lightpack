/*
 * CalibrationPage.cpp — Phase 3 calibration UI (temporary tab until Phase 5).
 */

#include "CalibrationPage.hpp"
#include "Settings.hpp"
#include "SettingsDefaults.hpp"
#include "debug.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QMessageBox>
#include <QSettings>
#include <cmath>

using namespace SettingsScope;

namespace {

struct PatternItem {
	CalibrationPatterns::PatternId id;
	const char *label;
};

const PatternItem kPatterns[] = {
	{ CalibrationPatterns::PatternId::White100, QT_TRANSLATE_NOOP("CalibrationPage", "White 100%") },
	{ CalibrationPatterns::PatternId::White75,  QT_TRANSLATE_NOOP("CalibrationPage", "White 75%") },
	{ CalibrationPatterns::PatternId::White50,  QT_TRANSLATE_NOOP("CalibrationPage", "White 50%") },
	{ CalibrationPatterns::PatternId::White25,  QT_TRANSLATE_NOOP("CalibrationPage", "White 25%") },
	{ CalibrationPatterns::PatternId::Red,      QT_TRANSLATE_NOOP("CalibrationPage", "Red") },
	{ CalibrationPatterns::PatternId::Green,    QT_TRANSLATE_NOOP("CalibrationPage", "Green") },
	{ CalibrationPatterns::PatternId::Blue,     QT_TRANSLATE_NOOP("CalibrationPage", "Blue") },
	{ CalibrationPatterns::PatternId::Cyan,     QT_TRANSLATE_NOOP("CalibrationPage", "Cyan") },
	{ CalibrationPatterns::PatternId::Magenta,  QT_TRANSLATE_NOOP("CalibrationPage", "Magenta") },
	{ CalibrationPatterns::PatternId::Yellow,   QT_TRANSLATE_NOOP("CalibrationPage", "Yellow") },
	{ CalibrationPatterns::PatternId::GrayRamp, QT_TRANSLATE_NOOP("CalibrationPage", "Gray ramp") },
	{ CalibrationPatterns::PatternId::ColorBars,QT_TRANSLATE_NOOP("CalibrationPage", "Color bars (per LED)") },
	{ CalibrationPatterns::PatternId::ChaseIdentify, QT_TRANSLATE_NOOP("CalibrationPage", "Identify LED (chase)") },
};

} // namespace

CalibrationPage::CalibrationPage(QWidget *parent)
	: QWidget(parent)
{
	buildUi();
	connect(&m_keepAlive, &QTimer::timeout, this, &CalibrationPage::onKeepAlive);
	using namespace std::chrono_literals;
	m_keepAlive.setInterval(200ms);
}

void CalibrationPage::buildUi()
{
	auto *root = new QVBoxLayout(this);

	auto *note = new QLabel(tr(
		"<b>Calibration</b> — temporary tab until Phase 5 IA. Patterns go host→device "
		"(grabber bypass). Live zone colors stay forced on while this page is active."));
	note->setWordWrap(true);
	root->addWidget(note);

	// --- Patterns ---
	auto *patternBox = new QGroupBox(tr("Test patterns"), this);
	auto *patternLay = new QHBoxLayout(patternBox);
	m_patternCombo = new QComboBox(patternBox);
	for (const PatternItem &p : kPatterns)
		m_patternCombo->addItem(tr(p.label), static_cast<int>(p.id));
	m_chaseSpin = new QSpinBox(patternBox);
	m_chaseSpin->setMinimum(0);
	m_chaseSpin->setMaximum(499);
	m_chaseSpin->setPrefix(tr("LED "));
	m_chaseSpin->setEnabled(false);
	auto *applyBtn = new QPushButton(tr("Apply pattern"), patternBox);
	patternLay->addWidget(m_patternCombo, 1);
	patternLay->addWidget(m_chaseSpin);
	patternLay->addWidget(applyBtn);
	root->addWidget(patternBox);

	// --- Controls (R3 owners) ---
	auto *ctrlBox = new QGroupBox(tr("Controls"), this);
	auto *ctrlForm = new QFormLayout(ctrlBox);

	m_wpPreset = new QComboBox(ctrlBox);
	m_wpPreset->addItem(tr("D65 (6500 K)"), 6500);
	m_wpPreset->addItem(tr("D93 (9300 K)"), 9300);
	m_wpPreset->addItem(tr("Warm (4000 K)"), 4000);
	m_wpPreset->addItem(tr("Cool (7500 K)"), 7500);
	m_wpPreset->addItem(tr("Manual Kelvin"), -1);
	m_kelvinSpin = new QSpinBox(ctrlBox);
	m_kelvinSpin->setRange(1000, 10000);
	m_kelvinSpin->setSuffix(QStringLiteral(" K"));
	m_kelvinSpin->setValue(6500);

	m_gammaSpin = new QDoubleSpinBox(ctrlBox);
	m_gammaSpin->setRange(Profile::Device::OutputGammaUiMin, Profile::Device::OutputGammaUiMax);
	m_gammaSpin->setSingleStep(0.01);
	m_gammaSpin->setDecimals(2);
	m_gammaSpin->setToolTip(tr("1.00 = Linear (physical). 1.32 = Classic (migrated default)."));

	m_abToggle = new QCheckBox(tr("A/B: bypass per-LED coefficients (neutral 1.0)"), ctrlBox);
	auto *resetBtn = new QPushButton(tr("Reset calibration defaults"), ctrlBox);
	m_coefWarnLabel = new QLabel(ctrlBox);
	m_coefWarnLabel->setWordWrap(true);
	m_coefWarnLabel->setStyleSheet(QStringLiteral("color: palette(dark);"));

	ctrlForm->addRow(tr("White point"), m_wpPreset);
	ctrlForm->addRow(tr("Kelvin"), m_kelvinSpin);
	ctrlForm->addRow(tr("Output gamma"), m_gammaSpin);
	ctrlForm->addRow(QString(), m_abToggle);
	ctrlForm->addRow(QString(), resetBtn);
	ctrlForm->addRow(QString(), m_coefWarnLabel);
	root->addWidget(ctrlBox);

	// --- Assisted measurement ---
	auto *assistBox = new QGroupBox(tr("Assisted measurement"), this);
	auto *assistLay = new QVBoxLayout(assistBox);
	auto *modeRow = new QHBoxLayout;
	m_inputXy = new QRadioButton(tr("CIE xy"), assistBox);
	m_inputRgb = new QRadioButton(tr("Measured RGB"), assistBox);
	m_inputXy->setChecked(true);
	modeRow->addWidget(m_inputXy);
	modeRow->addWidget(m_inputRgb);
	modeRow->addStretch(1);
	assistLay->addLayout(modeRow);

	auto *xyRow = new QHBoxLayout;
	m_xSpin = new QDoubleSpinBox(assistBox);
	m_ySpin = new QDoubleSpinBox(assistBox);
	m_xSpin->setRange(0.0, 0.8);
	m_ySpin->setRange(0.0, 0.9);
	m_xSpin->setDecimals(4);
	m_ySpin->setDecimals(4);
	m_xSpin->setSingleStep(0.001);
	m_ySpin->setSingleStep(0.001);
	m_xSpin->setValue(0.3127);
	m_ySpin->setValue(0.3290);
	xyRow->addWidget(new QLabel(tr("x"), assistBox));
	xyRow->addWidget(m_xSpin);
	xyRow->addWidget(new QLabel(tr("y"), assistBox));
	xyRow->addWidget(m_ySpin);
	assistLay->addLayout(xyRow);

	auto *rgbRow = new QHBoxLayout;
	m_rSpin = new QSpinBox(assistBox);
	m_gSpin = new QSpinBox(assistBox);
	m_bSpin = new QSpinBox(assistBox);
	for (QSpinBox *s : { m_rSpin, m_gSpin, m_bSpin }) {
		s->setRange(0, 255);
		s->setValue(255);
	}
	rgbRow->addWidget(new QLabel(tr("R"), assistBox));
	rgbRow->addWidget(m_rSpin);
	rgbRow->addWidget(new QLabel(tr("G"), assistBox));
	rgbRow->addWidget(m_gSpin);
	rgbRow->addWidget(new QLabel(tr("B"), assistBox));
	rgbRow->addWidget(m_bSpin);
	assistLay->addLayout(rgbRow);

	m_deltaELabel = new QLabel(tr("ΔE before/after: —"), assistBox);
	auto *solveRow = new QHBoxLayout;
	auto *solveBtn = new QPushButton(tr("Solve gains"), assistBox);
	auto *applyGainsBtn = new QPushButton(tr("Apply solved gains to all LEDs"), assistBox);
	solveRow->addWidget(solveBtn);
	solveRow->addWidget(applyGainsBtn);
	assistLay->addWidget(m_deltaELabel);
	assistLay->addLayout(solveRow);
	root->addWidget(assistBox);

	// --- Named profile (same app settings file for now — TODO Phase 5 separation) ---
	auto *profBox = new QGroupBox(tr("Named calibration snapshot"), this);
	auto *profV = new QVBoxLayout(profBox);
	auto *profH = new QHBoxLayout;
	m_profileName = new QLineEdit(profBox);
	m_profileName->setPlaceholderText(tr("Name (stored under CalibrationProfiles/…)"));
	auto *saveBtn = new QPushButton(tr("Save"), profBox);
	auto *loadBtn = new QPushButton(tr("Load"), profBox);
	profH->addWidget(m_profileName, 1);
	profH->addWidget(saveBtn);
	profH->addWidget(loadBtn);
	auto *todo = new QLabel(tr(
		"TODO(Phase 5): split calibration profiles from layout profiles completely. "
		"Snapshots currently live in the application settings file, not the LED layout profile."),
		profBox);
	todo->setWordWrap(true);
	profV->addLayout(profH);
	profV->addWidget(todo);
	root->addWidget(profBox);

	root->addStretch(1);

	connect(m_patternCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &CalibrationPage::onPatternChanged);
	connect(applyBtn, &QPushButton::clicked, this, &CalibrationPage::onApplyPattern);
	connect(m_chaseSpin, qOverload<int>(&QSpinBox::valueChanged), this, &CalibrationPage::onChaseIndexChanged);
	connect(m_wpPreset, qOverload<int>(&QComboBox::currentIndexChanged), this, &CalibrationPage::onWhitePointPreset);
	connect(m_kelvinSpin, qOverload<int>(&QSpinBox::valueChanged), this, &CalibrationPage::onKelvinChanged);
	connect(m_gammaSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &CalibrationPage::onOutputGammaChanged);
	connect(m_abToggle, &QCheckBox::toggled, this, &CalibrationPage::onAbToggled);
	connect(resetBtn, &QPushButton::clicked, this, &CalibrationPage::onResetClicked);
	connect(solveBtn, &QPushButton::clicked, this, &CalibrationPage::onSolveClicked);
	connect(applyGainsBtn, &QPushButton::clicked, this, &CalibrationPage::onApplySolvedGains);
	connect(saveBtn, &QPushButton::clicked, this, &CalibrationPage::onSaveNamedProfile);
	connect(loadBtn, &QPushButton::clicked, this, &CalibrationPage::onLoadNamedProfile);
}

void CalibrationPage::enterSession()
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO;
	if (m_sessionActive)
		return;
	m_sessionActive = true;
	m_coefWarningShown = false;
	syncControlsFromSettings();
	emit showLedWidgets(true);
	emit setColorFeedbackForced(true);
	emit sessionActiveChanged(true);
	pushCurrentPattern();
	m_keepAlive.start();
	maybeWarnCoefDeviation();
}

void CalibrationPage::leaveSession()
{
	DEBUG_LOW_LEVEL << Q_FUNC_INFO;
	if (!m_sessionActive)
		return;
	m_keepAlive.stop();
	if (m_abBypass) {
		m_abToggle->blockSignals(true);
		m_abToggle->setChecked(false);
		m_abToggle->blockSignals(false);
		restoreCoefs(m_savedCoefs);
		m_abBypass = false;
	}
	m_sessionActive = false;
	emit setColorFeedbackForced(false);
	emit sessionActiveChanged(false);
}

void CalibrationPage::syncControlsFromSettings()
{
	m_kelvinSpin->blockSignals(true);
	m_kelvinSpin->setValue(Settings::getGrabColorTemperature());
	m_kelvinSpin->blockSignals(false);

	m_gammaSpin->blockSignals(true);
	m_gammaSpin->setValue(Settings::getDeviceOutputGamma());
	m_gammaSpin->blockSignals(false);

	m_chaseSpin->setMaximum(std::max(0, ledCount() - 1));
}

int CalibrationPage::ledCount() const
{
	return Settings::getNumberOfLeds(Settings::getConnectedDevice());
}

void CalibrationPage::onPatternChanged()
{
	const auto id = static_cast<CalibrationPatterns::PatternId>(
		m_patternCombo->currentData().toInt());
	m_chaseSpin->setEnabled(id == CalibrationPatterns::PatternId::ChaseIdentify);
	if (m_sessionActive)
		pushCurrentPattern();
}

void CalibrationPage::onApplyPattern()
{
	pushCurrentPattern();
}

void CalibrationPage::onChaseIndexChanged(int)
{
	if (m_sessionActive)
		pushCurrentPattern();
}

void CalibrationPage::pushCurrentPattern()
{
	const int n = ledCount();
	if (n <= 0)
		return;
	const auto id = static_cast<CalibrationPatterns::PatternId>(
		m_patternCombo->currentData().toInt());
	m_lastColors = CalibrationPatterns::generate(id, n, m_chaseSpin->value());
	emit updateLedsColors(m_lastColors);
}

void CalibrationPage::onKeepAlive()
{
	if (!m_lastColors.isEmpty())
		emit updateLedsColors(m_lastColors);
}

void CalibrationPage::onWhitePointPreset(int index)
{
	const int k = m_wpPreset->itemData(index).toInt();
	if (k > 0) {
		m_kelvinSpin->blockSignals(true);
		m_kelvinSpin->setValue(k);
		m_kelvinSpin->blockSignals(false);
		onKelvinChanged(k);
	}
}

void CalibrationPage::onKelvinChanged(int kelvin)
{
	Settings::setGrabColorTemperature(kelvin);
	Settings::setGrabApplyColorTemperatureEnabled(true);
	maybeWarnCoefDeviation();
	if (m_sessionActive)
		pushCurrentPattern();
}

void CalibrationPage::onOutputGammaChanged(double gamma)
{
	Settings::setDeviceOutputGamma(gamma);
	if (m_sessionActive)
		pushCurrentPattern();
}

void CalibrationPage::snapshotCoefs()
{
	m_savedCoefs = Settings::getLedCoefs();
}

void CalibrationPage::restoreCoefs(const QList<WBAdjustment> &coefs)
{
	for (int i = 0; i < coefs.size(); ++i) {
		Settings::setLedCoefRed(i, coefs[i].red);
		Settings::setLedCoefGreen(i, coefs[i].green);
		Settings::setLedCoefBlue(i, coefs[i].blue);
	}
}

void CalibrationPage::applyGainsToAllLeds(const CalibrationSolver::Gains &g)
{
	const int n = ledCount();
	for (int i = 0; i < n; ++i) {
		Settings::setLedCoefRed(i, g.r);
		Settings::setLedCoefGreen(i, g.g);
		Settings::setLedCoefBlue(i, g.b);
	}
}

void CalibrationPage::onAbToggled(bool bypass)
{
	if (bypass) {
		snapshotCoefs();
		CalibrationSolver::Gains neutral{ 1.0, 1.0, 1.0 };
		applyGainsToAllLeds(neutral);
		m_abBypass = true;
	} else if (m_abBypass) {
		restoreCoefs(m_savedCoefs);
		m_abBypass = false;
	}
	if (m_sessionActive)
		pushCurrentPattern();
}

void CalibrationPage::onResetClicked()
{
	Settings::setGrabColorTemperature(6500);
	Settings::setGrabApplyColorTemperatureEnabled(true);
	Settings::setDeviceOutputGamma(1.32);
	CalibrationSolver::Gains neutral{ 1.0, 1.0, 1.0 };
	applyGainsToAllLeds(neutral);
	syncControlsFromSettings();
	m_coefWarnLabel->clear();
	if (m_sessionActive)
		pushCurrentPattern();
}

void CalibrationPage::maybeWarnCoefDeviation()
{
	if (m_coefWarningShown)
		return;
	const QList<WBAdjustment> coefs = Settings::getLedCoefs();
	bool deviant = false;
	for (const WBAdjustment &c : coefs) {
		CalibrationSolver::Gains g{ c.red, c.green, c.blue };
		if (CalibrationSolver::coefsDeviateFromNeutral(g, 0.05)) {
			deviant = true;
			break;
		}
	}
	if (!deviant)
		return;
	m_coefWarningShown = true;
	const QString msg = tr(
		"Per-LED white-balance coefficients already deviate more than 5%% from neutral. "
		"Temperature is applied separately and will not overwrite those coefficients.");
	m_coefWarnLabel->setText(msg);
	QMessageBox::information(this, tr("Color temperature"), msg);
}

void CalibrationPage::onSolveClicked()
{
	const int kelvin = m_kelvinSpin->value();
	CalibrationSolver::SolveResult r;
	if (m_inputXy->isChecked()) {
		r = CalibrationSolver::solveFromMeasuredXy(m_xSpin->value(), m_ySpin->value(), 1.0, static_cast<quint16>(kelvin));
	} else {
		r = CalibrationSolver::solveFromMeasuredRgb(
			qRgb(m_rSpin->value(), m_gSpin->value(), m_bSpin->value()),
			static_cast<quint16>(kelvin));
	}
	m_lastSolved = r.gains;
	m_hasSolved = true;
	m_deltaELabel->setText(tr("ΔE before: %1   after: %2   gains R=%3 G=%4 B=%5")
		.arg(r.deltaEBefore, 0, 'f', 2)
		.arg(r.deltaEAfter, 0, 'f', 2)
		.arg(r.gains.r, 0, 'f', 3)
		.arg(r.gains.g, 0, 'f', 3)
		.arg(r.gains.b, 0, 'f', 3));
}

void CalibrationPage::onApplySolvedGains()
{
	if (!m_hasSolved) {
		QMessageBox::information(this, tr("Calibration"), tr("Solve gains first."));
		return;
	}
	applyGainsToAllLeds(m_lastSolved);
	if (m_sessionActive)
		pushCurrentPattern();
}

void CalibrationPage::onSaveNamedProfile()
{
	const QString name = m_profileName->text().trimmed();
	if (name.isEmpty()) {
		QMessageBox::warning(this, tr("Calibration"), tr("Enter a profile name."));
		return;
	}
	QSettings store;
	store.beginGroup(QStringLiteral("CalibrationProfiles"));
	store.beginGroup(name);
	store.setValue(QStringLiteral("ColorTemperature"), Settings::getGrabColorTemperature());
	store.setValue(QStringLiteral("OutputGamma"), Settings::getDeviceOutputGamma());
	const QList<WBAdjustment> coefs = Settings::getLedCoefs();
	store.setValue(QStringLiteral("LedCount"), coefs.size());
	for (int i = 0; i < coefs.size(); ++i) {
		store.setValue(QStringLiteral("Led%1/R").arg(i), coefs[i].red);
		store.setValue(QStringLiteral("Led%1/G").arg(i), coefs[i].green);
		store.setValue(QStringLiteral("Led%1/B").arg(i), coefs[i].blue);
	}
	store.endGroup();
	store.endGroup();
	QMessageBox::information(this, tr("Calibration"), tr("Saved snapshot \"%1\".").arg(name));
}

void CalibrationPage::onLoadNamedProfile()
{
	const QString name = m_profileName->text().trimmed();
	if (name.isEmpty()) {
		QMessageBox::warning(this, tr("Calibration"), tr("Enter a profile name."));
		return;
	}
	QSettings store;
	store.beginGroup(QStringLiteral("CalibrationProfiles"));
	store.beginGroup(name);
	if (!store.contains(QStringLiteral("OutputGamma"))) {
		QMessageBox::warning(this, tr("Calibration"), tr("Snapshot \"%1\" not found.").arg(name));
		store.endGroup();
		store.endGroup();
		return;
	}
	Settings::setGrabColorTemperature(store.value(QStringLiteral("ColorTemperature"), 6500).toInt());
	Settings::setGrabApplyColorTemperatureEnabled(true);
	Settings::setDeviceOutputGamma(store.value(QStringLiteral("OutputGamma"), 1.32).toDouble());
	const int n = store.value(QStringLiteral("LedCount"), 0).toInt();
	for (int i = 0; i < n; ++i) {
		Settings::setLedCoefRed(i, store.value(QStringLiteral("Led%1/R").arg(i), 1.0).toDouble());
		Settings::setLedCoefGreen(i, store.value(QStringLiteral("Led%1/G").arg(i), 1.0).toDouble());
		Settings::setLedCoefBlue(i, store.value(QStringLiteral("Led%1/B").arg(i), 1.0).toDouble());
	}
	store.endGroup();
	store.endGroup();
	syncControlsFromSettings();
	if (m_sessionActive)
		pushCurrentPattern();
	QMessageBox::information(this, tr("Calibration"), tr("Loaded snapshot \"%1\".").arg(name));
}
