/*
 * CalibrationPage.hpp — Phase 3 calibration surface (temporary until Phase 5 IA).
 *
 * Hosted as a SettingsWindow tab until the revised IA lands. On enter: forces
 * grab-zone overlays + setColorFeedbackForced(true). Patterns bypass the grabber
 * via SettingsWindow::updateLedsColors (Mood Lamp path).
 */

#pragma once

#include <QWidget>
#include <QTimer>
#include <QList>
#include <QRgb>
#include "CalibrationPatterns.hpp"
#include "CalibrationSolver.hpp"
#include "types.h"

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QLabel;
class QCheckBox;
class QLineEdit;
class QRadioButton;
class QSlider;

class CalibrationPage : public QWidget
{
	Q_OBJECT
public:
	explicit CalibrationPage(QWidget *parent = nullptr);

	bool isSessionActive() const { return m_sessionActive; }

public slots:
	void enterSession();
	void leaveSession();

signals:
	void sessionActiveChanged(bool active);
	void updateLedsColors(const QList<QRgb> &colors);
	void showLedWidgets(bool visible);
	void setColorFeedbackForced(bool forced);

private slots:
	void onPatternChanged();
	void onApplyPattern();
	void onChaseIndexChanged(int value);
	void onKeepAlive();
	void onWhitePointPreset(int index);
	void onKelvinChanged(int kelvin);
	void onOutputGammaChanged(double gamma);
	void onAbToggled(bool bypass);
	void onResetClicked();
	void onSolveClicked();
	void onApplySolvedGains();
	void onSaveNamedProfile();
	void onLoadNamedProfile();

private:
	void buildUi();
	void pushCurrentPattern();
	void syncControlsFromSettings();
	void maybeWarnCoefDeviation();
	int ledCount() const;
	void applyGainsToAllLeds(const CalibrationSolver::Gains &g);
	void snapshotCoefs();
	void restoreCoefs(const QList<WBAdjustment> &coefs);

	bool m_sessionActive{ false };
	bool m_coefWarningShown{ false };
	bool m_abBypass{ false };
	QList<WBAdjustment> m_savedCoefs;
	QList<QRgb> m_lastColors;
	CalibrationSolver::Gains m_lastSolved;
	bool m_hasSolved{ false };

	QTimer m_keepAlive;

	QComboBox *m_patternCombo{ nullptr };
	QSpinBox *m_chaseSpin{ nullptr };
	QComboBox *m_wpPreset{ nullptr };
	QSpinBox *m_kelvinSpin{ nullptr };
	QDoubleSpinBox *m_gammaSpin{ nullptr };
	QCheckBox *m_abToggle{ nullptr };
	QLabel *m_coefWarnLabel{ nullptr };
	QLabel *m_deltaELabel{ nullptr };
	QRadioButton *m_inputXy{ nullptr };
	QRadioButton *m_inputRgb{ nullptr };
	QDoubleSpinBox *m_xSpin{ nullptr };
	QDoubleSpinBox *m_ySpin{ nullptr };
	QSpinBox *m_rSpin{ nullptr };
	QSpinBox *m_gSpin{ nullptr };
	QSpinBox *m_bSpin{ nullptr };
	QLineEdit *m_profileName{ nullptr };
};
