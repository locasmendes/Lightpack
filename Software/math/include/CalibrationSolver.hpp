/*
 * CalibrationSolver.hpp — pure ΔE-minimizing WB gain solver (Phase 3.4).
 *
 * No QObject / Settings / widgets — same pattern as BulkResize.
 */

#pragma once

#include "colorspace_types.h"
#include <QRgb>
#include <QtGlobal>

namespace CalibrationSolver
{

struct Gains {
	double r = 1.0;
	double g = 1.0;
	double b = 1.0;
};

struct LabF {
	double L = 0;
	double a = 0;
	double b = 0;
};

struct SolveResult {
	Gains gains;
	double deltaEBefore = 0;
	double deltaEAfter = 0;
};

/*! CIE 1931 xy (+ optional luminance Y) → XYZ. Y defaults to 1. */
StructXyz xyToXyz(double x, double y, double Y = 1.0);

LabF xyzToLab(const StructXyz &xyz);
LabF linearRgbToLab(double r, double g, double b);
double deltaE76(const LabF &a, const LabF &b);

/*!
 * Channel gains that map measured linear RGB toward target linear RGB,
 * peak-normalized so max(component) == 1. Physically: LED_out *= gains.
 */
Gains gainsFromLinear(double measR, double measG, double measB,
	double tgtR, double tgtG, double tgtB);

SolveResult solveFromMeasuredRgb(QRgb measured, quint16 targetKelvin);
SolveResult solveFromMeasuredXy(double x, double y, double Y, quint16 targetKelvin);

/*! True if any channel differs from 1.0 by more than threshold (default 5%). */
bool coefsDeviateFromNeutral(const Gains &g, double threshold = 0.05);

} // namespace CalibrationSolver
