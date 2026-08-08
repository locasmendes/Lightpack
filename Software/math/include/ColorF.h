/*
 * ColorF.h — working-space color types for the unified float pipeline.
 *
 * Three distinct structs (not QVector3D): domain mismatches become compile errors.
 * float (not double) in the working space; double only in full-strip accumulators.
 *
 *	Created on: 08.08.2026
 *		Project: Lightpack
 */

#pragma once

#include <QList>
#include <QMetaType>

/*! Linear light (scene-referred). Nominal 0..1; >1 allowed before clamp. */
struct LinearRgbF {
	float r = 0.f;
	float g = 0.f;
	float b = 0.f;
};

/*! sRGB-encoded (display-referred), 0..1. Argument type for perceptual ops only. */
struct EncodedRgbF {
	float r = 0.f;
	float g = 0.f;
	float b = 0.f;
};

/*! Post render-transform; ∝ PWM duty / emitted light, 0..1. */
struct WireRgbF {
	float r = 0.f;
	float g = 0.f;
	float b = 0.f;
};

Q_DECLARE_METATYPE(LinearRgbF)
Q_DECLARE_METATYPE(EncodedRgbF)
Q_DECLARE_METATYPE(WireRgbF)
Q_DECLARE_METATYPE(QList<LinearRgbF>)
