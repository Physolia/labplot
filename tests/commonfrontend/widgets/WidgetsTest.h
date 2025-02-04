/*
	File                 : WidgetsTest.h
	Project              : LabPlot
	Description          : Tests for widgets
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2022 Martin Marmsoler <martin.marmsoler@gmail.com>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef WIDGETSTEST_H
#define WIDGETSTEST_H

#include "tests/CommonTest.h"

class WidgetsTest : public CommonTest {
	Q_OBJECT

private Q_SLOTS:
	void initTestCase() {
		QLocale::setDefault(QLocale(QLocale::Language::English));
	}
	void numberSpinBoxProperties();
	void numberSpinBoxCreateStringNumber();
	void numberSpinBoxChangingValueKeyPress();
	void numberSpinBoxLimit();
	void numberSpinBoxPrefixSuffix();
	void numberSpinBoxSuffixFrontToBackSelection();
	void numberSpinBoxSuffixSetCursor();
	void numberSpinBoxSuffixBackToFrontSelection();
	void numberSpinBoxPrefixFrontToBackSelection();
	void numberSpinBoxPrefixBackToFrontSelection();
	void numberSpinBoxPrefixSetCursorPosition();
	void numberSpinBoxEnterNumber();
	void numberSpinBoxFeedback();
	void numberSpinBoxFeedback2();
	void numberSpinBoxFeedbackCursorPosition();
	void numberSpinBoxFeedbackCursorPosition2();
	void numberSpinBoxDecimals2();
	void numberSpinBoxScrollingNegToPos();
	void numberSpinBoxScrollingNegToPos2();
	void numberSpinBoxScrollingNegativeValues();
	void numberSpinBoxMinimumFeedback();
	void numberSpinBoxDecimalsMinMax();

	void thousandSeparator();
	void thousandSeparatorNegative();
	void thousandSeparatorScrolling();
	void thousandSeparatorScrolling2();
	void thousandSeparatorScrollingSeparatorPosition();
};
#endif // WIDGETSTEST_H
