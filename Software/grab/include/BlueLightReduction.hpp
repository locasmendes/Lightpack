#pragma once
#include <QList>
#include <QRgb>
#include <QtGlobal>

namespace BlueLightReduction
{
	class Client
	{
		Q_DISABLE_COPY(Client)
	public:
		static bool isSupported() { Q_ASSERT_X(false, "BlueLightReduction::isSupported()", "not implemented"); return false; }
		/*! Apply display-LUT style correction in-place (GammaRamp). No-op for Kelvin sources. */
		virtual void apply(QList<QRgb>& colors) = 0;
		/*! Kelvin white-point sources (Night Light / Night Shift). 0 = not applicable. */
		virtual quint16 colorTemperatureKelvin() const { return 0; }
		virtual ~Client() = default;
		Client() = default;
	};

	Client* create();
};
