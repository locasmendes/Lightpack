/*
 * CalibrationSolver.cpp
 */

#include "CalibrationSolver.hpp"
#include "ColorOps.hpp"
#include "PrismatikMath.hpp"
#include <algorithm>
#include <cmath>

namespace CalibrationSolver
{
namespace {

constexpr double kRefX = 95.047;
constexpr double kRefY = 100.000;
constexpr double kRefZ = 108.883;

double clampPositive(double v, double eps = 1e-8)
{
	return v < eps ? eps : v;
}

void peakNormalize(Gains &g)
{
	const double m = std::max({g.r, g.g, g.b, 1e-12});
	g.r /= m;
	g.g /= m;
	g.b /= m;
}

// sRGB linear → XYZ (D65), matching PrismatikMath::toXyz scale (Y in 0..100).
StructXyz linearRgbToXyz(double r, double g, double b)
{
	r *= 100.0;
	g *= 100.0;
	b *= 100.0;
	StructXyz xyz;
	xyz.x = r * 0.4124 + g * 0.3576 + b * 0.1805;
	xyz.y = r * 0.2126 + g * 0.7152 + b * 0.0722;
	xyz.z = r * 0.0193 + g * 0.1192 + b * 0.9505;
	return xyz;
}

void xyzToLinearRgb(const StructXyz &xyz, double &r, double &g, double &b)
{
	const double x = xyz.x / 100.0;
	const double y = xyz.y / 100.0;
	const double z = xyz.z / 100.0;
	r = x * 3.2406 + y * -1.5372 + z * -0.4986;
	g = x * -0.9689 + y * 1.8758 + z * 0.0415;
	b = x * 0.0557 + y * -0.2040 + z * 1.0570;
	r = std::max(0.0, r);
	g = std::max(0.0, g);
	b = std::max(0.0, b);
}

LinearRgbF targetLinearFromKelvin(quint16 kelvin)
{
	return ColorOps::linearWhitePoint(kelvin);
}

LabF peakNormLab(double r, double g, double b)
{
	const double m = std::max({r, g, b, 1e-12});
	return linearRgbToLab(r / m, g / m, b / m);
}

SolveResult finishSolve(double mr, double mg, double mb, double tr, double tg, double tb)
{
	SolveResult out;
	out.gains = gainsFromLinear(mr, mg, mb, tr, tg, tb);

	// Compare chromaticity only (peak-normalized): WB gains are scale-free after
	// peak-normalize, so luminance must not dominate ΔE before/after.
	const LabF labMeas = peakNormLab(mr, mg, mb);
	const LabF labTgt = peakNormLab(tr, tg, tb);
	out.deltaEBefore = deltaE76(labMeas, labTgt);

	const LabF labAfter = peakNormLab(mr * out.gains.r, mg * out.gains.g, mb * out.gains.b);
	out.deltaEAfter = deltaE76(labAfter, labTgt);
	return out;
}

} // namespace

StructXyz xyToXyz(double x, double y, double Y)
{
	StructXyz xyz;
	if (y <= 1e-12) {
		xyz.x = xyz.y = xyz.z = 0;
		return xyz;
	}
	xyz.y = Y;
	xyz.x = x * Y / y;
	xyz.z = (1.0 - x - y) * Y / y;
	return xyz;
}

LabF xyzToLab(const StructXyz &xyz)
{
	auto f = [](double t) {
		return (t > 0.008856) ? std::cbrt(t) : (7.787 * t + 16.0 / 116.0);
	};
	const double fx = f(xyz.x / kRefX);
	const double fy = f(xyz.y / kRefY);
	const double fz = f(xyz.z / kRefZ);
	LabF lab;
	lab.L = 116.0 * fy - 16.0;
	lab.a = 500.0 * (fx - fy);
	lab.b = 200.0 * (fy - fz);
	return lab;
}

LabF linearRgbToLab(double r, double g, double b)
{
	return xyzToLab(linearRgbToXyz(r, g, b));
}

double deltaE76(const LabF &a, const LabF &b)
{
	const double dL = a.L - b.L;
	const double da = a.a - b.a;
	const double db = a.b - b.b;
	return std::sqrt(dL * dL + da * da + db * db);
}

Gains gainsFromLinear(double measR, double measG, double measB,
	double tgtR, double tgtG, double tgtB)
{
	Gains g;
	g.r = tgtR / clampPositive(measR);
	g.g = tgtG / clampPositive(measG);
	g.b = tgtB / clampPositive(measB);
	peakNormalize(g);
	return g;
}

SolveResult solveFromMeasuredRgb(QRgb measured, quint16 targetKelvin)
{
	const LinearRgbF m = ColorOps::srgbDecode(measured);
	const LinearRgbF t = targetLinearFromKelvin(targetKelvin);
	return finishSolve(m.r, m.g, m.b, t.r, t.g, t.b);
}

SolveResult solveFromMeasuredXy(double x, double y, double Y, quint16 targetKelvin)
{
	double mr = 0, mg = 0, mb = 0;
	xyzToLinearRgb(xyToXyz(x, y, Y), mr, mg, mb);
	// Peak-normalize measured so it is comparable to linearWhitePoint.
	const double mm = std::max({mr, mg, mb, 1e-12});
	mr /= mm;
	mg /= mm;
	mb /= mm;
	const LinearRgbF t = targetLinearFromKelvin(targetKelvin);
	return finishSolve(mr, mg, mb, t.r, t.g, t.b);
}

bool coefsDeviateFromNeutral(const Gains &g, double threshold)
{
	return std::fabs(g.r - 1.0) > threshold
		|| std::fabs(g.g - 1.0) > threshold
		|| std::fabs(g.b - 1.0) > threshold;
}

} // namespace CalibrationSolver
