/*
	File                 : NumberSpinBox.g
	Project              : LabPlot
	Description          : widget for setting numbers with a spinbox
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2022 Martin Marmsoler <martin.marmsoler@gmail.com>
	SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef NUMBERSPINBOX_H
#define NUMBERSPINBOX_H

#include <QDoubleSpinBox>

class NumberSpinBox : public QDoubleSpinBox {
	Q_OBJECT

public:
	struct NumberProperties {
		QChar integerSign;
		int integer;
		int intergerDigits;

		bool fraction; // 5. is a valid number, so just setting fractionDigits to 0 is not correct
		int fractionPos;
		int fractionDigits;

		QChar exponentLetter;
		int exponentPos;
		QChar exponentSign;
		int exponent;
		int exponentDigits;
	};

	enum class Errors {
		NoError,
		NoNumber,
		Invalid,
		Min, // value smaller than min
		Max, // value larger than max
	};

public:
	NumberSpinBox(double initValue = 0, QWidget* parent = nullptr);
	QString errorToString(Errors);
	bool setValue(double);
	void setFeedback(bool enable);

Q_SIGNALS:
	void valueChanged(double);

private:
	void keyPressEvent(QKeyEvent*) override;
	void setInvalid(Errors e);
	void setInvalid(const QString& str);
	bool properties(const QString& value, NumberProperties& p) const;
	QString createStringNumber(double integerFraction, int exponent, const NumberProperties&) const;
	void stepBy(int steps) override;
	Errors step(int steps);
	QString strip(const QString& t) const;
	virtual QString textFromValue(double value) const override;
	virtual double valueFromText(const QString&) const override;
	QAbstractSpinBox::StepEnabled stepEnabled() const override;
	virtual QValidator::State validate(QString& input, int& pos) const override;
	Errors validate(QString& input, double& value, QString& valueStr) const;
	void setText(const QString&);

	Errors increaseValue();
	Errors decreaseValue();
	void valueChanged();

private:
	QString mValueStr;
	// See https://invent.kde.org/education/labplot/-/merge_requests/167
	// for explanation of the feature
	bool mFeedback{false}; // defines if the spinbox expects a feedback
	bool mWaitFeedback{false};

	friend class WidgetsTest;
};

#endif // NUMBERSPINBOX_H
