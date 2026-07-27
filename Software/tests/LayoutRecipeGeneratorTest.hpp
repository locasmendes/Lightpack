#ifndef LAYOUTRECIPEGENERATORTEST_HPP
#define LAYOUTRECIPEGENERATORTEST_HPP

#include <QObject>

class LayoutRecipeGeneratorTest : public QObject
{
	Q_OBJECT
public:
	explicit LayoutRecipeGeneratorTest(QObject *parent = 0);

private slots:
	void testGeneratedRectsAreContainedInOffsetContentRect();
	void testLedCountMatchesTopSideBottomSum();
	void testSkipCornersPreservesLedCount();
	void testInvalidRecipeReturnsEmpty();
	void testNonPositiveContentRectReturnsEmpty();
	void testJsonRoundTrip();
	void testBottomLedsOneWithStandWidthDoesNotDivideByZero();
	void testDistributorAndRecipeEntryPointsAgree();
};

#endif // LAYOUTRECIPEGENERATORTEST_HPP
