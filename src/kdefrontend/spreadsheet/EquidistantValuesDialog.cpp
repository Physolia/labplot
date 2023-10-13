/*
	File                 : EquidistantValuesDialog.cpp
	Project              : LabPlot
	Description          : Dialog for generating equidistant numbers
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2014-2023 Alexander Semke <alexander.semke@web.de>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "EquidistantValuesDialog.h"
#include "backend/core/Settings.h"
#include "backend/core/column/Column.h"
#include "backend/core/datatypes/DateTime2StringFilter.h"
#include "backend/lib/macros.h"
#include "backend/spreadsheet/Spreadsheet.h"

#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QWindow>

#include <KLocalizedString>
#include <KWindowConfig>

/*!
	\class EquidistantValuesDialog
	\brief Dialog for equidistant values.

	\ingroup kdefrontend
 */

EquidistantValuesDialog::EquidistantValuesDialog(Spreadsheet* s, QWidget* parent)
	: QDialog(parent)
	, m_spreadsheet(s) {
	Q_ASSERT(m_spreadsheet);
	setWindowTitle(i18nc("@title:window", "Equidistant Values"));

	auto* mainWidget = new QWidget(this);
	ui.setupUi(mainWidget);
	auto* layout = new QVBoxLayout(this);

	auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	ui.gridLayout->addWidget(buttonBox);
	m_okButton = buttonBox->button(QDialogButtonBox::Ok);
	m_okButton->setText(i18n("&Generate"));
	m_okButton->setToolTip(i18n("Generate equidistant values"));

	connect(buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &EquidistantValuesDialog::close);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &EquidistantValuesDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &EquidistantValuesDialog::reject);

	layout->addWidget(mainWidget);
	layout->addWidget(buttonBox);
	setLayout(layout);
	setAttribute(Qt::WA_DeleteOnClose);

	ui.cbType->addItem(i18n("Number"), static_cast<int>(Type::FixedNumber));
	ui.cbType->addItem(i18n("Increment"), static_cast<int>(Type::FixedIncrement));

	ui.cbIncrementDateTimeUnit->addItem(i18n("Years"), static_cast<int>(DateTimeUnit::Year));
	ui.cbIncrementDateTimeUnit->addItem(i18n("Months"), static_cast<int>(DateTimeUnit::Month));
	ui.cbIncrementDateTimeUnit->addItem(i18n("Days"), static_cast<int>(DateTimeUnit::Day));
	ui.cbIncrementDateTimeUnit->addItem(i18n("Hours"), static_cast<int>(DateTimeUnit::Hour));
	ui.cbIncrementDateTimeUnit->addItem(i18n("Minutes"), static_cast<int>(DateTimeUnit::Minute));
	ui.cbIncrementDateTimeUnit->addItem(i18n("Seconds"), static_cast<int>(DateTimeUnit::Second));
	ui.cbIncrementDateTimeUnit->addItem(i18n("Milliseconds"), static_cast<int>(DateTimeUnit::Millisecond));

	ui.leFrom->setClearButtonEnabled(true);
	ui.leTo->setClearButtonEnabled(true);
	ui.leIncrement->setClearButtonEnabled(true);
	ui.leNumber->setClearButtonEnabled(true);

	ui.leFrom->setValidator(new QDoubleValidator(ui.leFrom));
	ui.leTo->setValidator(new QDoubleValidator(ui.leTo));
	ui.leIncrement->setValidator(new QDoubleValidator(ui.leIncrement));
	ui.leNumber->setValidator(new QIntValidator(ui.leNumber));
	ui.leIncrementDateTime->setValidator(new QIntValidator(ui.leIncrementDateTime));

	connect(ui.cbType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &EquidistantValuesDialog::typeChanged);
	connect(ui.leFrom, &QLineEdit::textChanged, this, &EquidistantValuesDialog::checkValues);
	connect(ui.leTo, &QLineEdit::textChanged, this, &EquidistantValuesDialog::checkValues);
	connect(ui.leNumber, &QLineEdit::textChanged, this, &EquidistantValuesDialog::checkValues);
	connect(ui.leIncrement, &QLineEdit::textChanged, this, &EquidistantValuesDialog::checkValues);
	connect(m_okButton, &QPushButton::clicked, this, &EquidistantValuesDialog::generate);


	// restore saved settings if available
	create(); // ensure there's a window created
	KConfigGroup conf = Settings::group(QStringLiteral("EquidistantValuesDialog"));
	if (conf.exists()) {
		KWindowConfig::restoreWindowSize(windowHandle(), conf);
		resize(windowHandle()->size()); // workaround for QTBUG-40584
	} else
		resize(QSize(300, 0).expandedTo(minimumSize()));

	ui.cbType->setCurrentIndex(conf.readEntry("Type", 0));
	// this->typeChanged(ui.cbType->currentIndex());

	// settings for numeric
	ui.leFrom->setText(QString::number(conf.readEntry("From", 1)));
	ui.leTo->setText(QString::number(conf.readEntry("To", 100)));
	ui.leIncrement->setText(QLocale().toString(conf.readEntry("Increment", 1.)));

	// settings for datetime
	qint64 now = QDateTime::currentDateTime().toMSecsSinceEpoch();
	ui.dteFrom->setMSecsSinceEpochUTC(conf.readEntry("FromDateTime", now));
	ui.dteTo->setMSecsSinceEpochUTC(conf.readEntry("ToDateTime", now));
	ui.leIncrementDateTime->setText(QLocale().toString(conf.readEntry("IncrementDateTime", 1)));
	ui.cbIncrementDateTimeUnit->setCurrentIndex(conf.readEntry("DateTimeUnit", 0));
}

EquidistantValuesDialog::~EquidistantValuesDialog() {
	// save current settings
	KConfigGroup conf = Settings::group(QStringLiteral("EquidistantValuesDialog"));
	KWindowConfig::saveWindowSize(windowHandle(), conf);

	conf.writeEntry("Type", ui.cbType->currentIndex());

	// settings for numeric
	const auto numberLocale = QLocale();
	conf.writeEntry("From", numberLocale.toInt(ui.leFrom->text()));
	conf.writeEntry("To", numberLocale.toInt(ui.leTo->text()));
	conf.writeEntry("Increment", numberLocale.toDouble(ui.leIncrement->text()));

	// settings for datetime
	conf.writeEntry("FromDateTime", ui.dteFrom->dateTime().toMSecsSinceEpoch());
	conf.writeEntry("ToDateTime", ui.dteTo->dateTime().toMSecsSinceEpoch());
	conf.writeEntry("IncrementDateTime", numberLocale.toDouble(ui.leIncrement->text()));
	conf.writeEntry("DateTimeUnit", ui.cbIncrementDateTimeUnit->currentIndex());
}

void EquidistantValuesDialog::setColumns(const QVector<Column*>& columns) {
	m_columns = columns;
	ui.leNumber->setText(QLocale().toString(m_columns.first()->rowCount()));

	for (auto* col : m_columns) {
		if (col->isNumeric()) {
			m_hasNumeric = true;
			break;
		}
	}

	QString dateTimeFormat;
	for (auto* col : m_columns) {
		if (col->columnMode() == AbstractColumn::ColumnMode::DateTime) {
			m_hasDateTime = true;
			auto* filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
			dateTimeFormat = filter->format();
			break;
		}
	}

	ui.lIncrement->setVisible(m_hasNumeric);
	ui.leIncrement->setVisible(m_hasNumeric);
	ui.lFrom->setVisible(m_hasNumeric);
	ui.leFrom->setVisible(m_hasNumeric);
	ui.lTo->setVisible(m_hasNumeric);
	ui.leTo->setVisible(m_hasNumeric);

	ui.lIncrementDateTime->setVisible(m_hasDateTime);
	ui.leIncrementDateTime->setVisible(m_hasDateTime);
	ui.cbIncrementDateTimeUnit->setVisible(m_hasDateTime);
	ui.lFromDateTime->setVisible(m_hasDateTime);
	ui.dteFrom->setVisible(m_hasDateTime);
	ui.lToDateTime->setVisible(m_hasDateTime);
	ui.dteTo->setVisible(m_hasDateTime);

	ui.lNumeric->setVisible(m_hasNumeric && m_hasDateTime);
	ui.lDateTime->setVisible(m_hasNumeric && m_hasDateTime);

	if (m_hasDateTime) {
		ui.dteFrom->setDisplayFormat(dateTimeFormat);
		ui.dteTo->setDisplayFormat(dateTimeFormat);
	}

	// resize the dialog to have the minimum height
	layout()->activate();
	resize(QSize(this->width(), 0).expandedTo(minimumSize()));
}

void EquidistantValuesDialog::typeChanged(int index) {
	if (index == 0) { // fixed number
		ui.lNumber->show();
		ui.leNumber->show();
		ui.lIncrement->hide();
		ui.leIncrement->hide();
		ui.lIncrementDateTime->hide();
		ui.leIncrementDateTime->hide();
		ui.cbIncrementDateTimeUnit->hide();
	} else { // fixed increment
		ui.lNumber->hide();
		ui.leNumber->hide();
		if (m_hasNumeric) {
			ui.lIncrement->show();
			ui.leIncrement->show();
		}
		if (m_hasDateTime) {
			ui.lIncrementDateTime->show();
			ui.leIncrementDateTime->show();
			ui.cbIncrementDateTimeUnit->show();
		}

	}
}

void EquidistantValuesDialog::checkValues() {
	// check the validness of the user input for numeric values
	const auto numberLocale = QLocale();
	bool ok;
	if (m_hasNumeric) {
		const double start = numberLocale.toDouble(ui.leFrom->text(), &ok);
		if (!ok) {
			m_okButton->setToolTip(i18n("Invalid start value"));
			m_okButton->setEnabled(false);
			return;
		}

		const double end =numberLocale.toDouble(ui.leTo->text(), &ok);
		if (!ok || end < start) {
			m_okButton->setToolTip(i18n("Invalid end value, must be bigger than the start value"));
			m_okButton->setEnabled(false);
			return;
		}
	}

	if (ui.cbType->currentIndex() == 0) { // fixed number
		// check whether a valid integer value biger than 1 was provided
		const int number = numberLocale.toDouble(ui.leNumber->text(), &ok);
		if (!ok || number < 1) {
			m_okButton->setToolTip(i18n("The number of values to be generated must be bigger than one"));
			m_okButton->setEnabled(false);
			return;
		}
	} else { // fixed increment
		if (m_hasNumeric) {
			const double increment = numberLocale.toDouble(ui.leIncrement->text(), &ok);
			if (!ok || increment == 0.) {
				m_okButton->setToolTip(i18n("Invalid numeric increment value, must be bigger than zero"));
				m_okButton->setEnabled(false);
				return;
			}
		}

		if (m_hasDateTime) {
			const int increment = numberLocale.toInt(ui.leIncrementDateTime->text(), &ok);
			if (!ok || increment == 0) {
				m_okButton->setToolTip(i18n("Invalid Date&Time increment value, must be bigger than zero"));
				m_okButton->setEnabled(false);
				return;
			}
		}
	}

	m_okButton->setToolTip(QString());
	m_okButton->setEnabled(true);
}

void EquidistantValuesDialog::generate() {
	QVector<double> newData;
	QVector<QDateTime> newDataDateTime;

	WAIT_CURSOR;
	bool rc = true;
	if (m_hasNumeric)
		rc = generateNumericData(newData);
	if (!rc) {
		RESET_CURSOR;
		return;
	}

	if (m_hasDateTime)
		rc = generateDateTimeData(newDataDateTime);
	if (!rc) {
		RESET_CURSOR;
		return;
	}

	m_spreadsheet->beginMacro(
		i18np("%1: fill column with equidistant numbers", "%1: fill columns with equidistant numbers", m_spreadsheet->name(), m_columns.size()));

	int rowCount = std::max(newData.size(), newDataDateTime.size());
	if (m_spreadsheet->rowCount() < rowCount)
		m_spreadsheet->setRowCount(rowCount);

	for (auto* col : m_columns) {
		if (col->columnMode() == AbstractColumn::ColumnMode::Double)
			col->setValues(newData);
		else if (col->columnMode() == AbstractColumn::ColumnMode::DateTime)
			col->setDateTimes(newDataDateTime);
	}

	m_spreadsheet->endMacro();
	RESET_CURSOR;
}

bool EquidistantValuesDialog::generateNumericData(QVector<double>& newData) {
	int number{0};
	double increment{0};

	// check the validness of the user input for numeric values
	const auto numberLocale = QLocale();
	bool ok;
	const double start = numberLocale.toDouble(ui.leFrom->text(), &ok);
	if (!ok) {
		DEBUG("Invalid double value for 'start'!")
		return false;
	}

	const double end = numberLocale.toDouble(ui.leTo->text(), &ok);
	if (!ok) {
		DEBUG("Invalid double value for 'end'!")
		return false;
	}

	// generate equidistant numeric values
	if (ui.cbType->currentIndex() == 0) { // fixed number
		number = QLocale().toInt(ui.leNumber->text(), &ok);
		if (!ok || number == 1) {
			DEBUG("Invalid integer value for 'number'!")
			return false;
		}

		increment = (end - start) / (number - 1);
	} else { // fixed increment
		increment = QLocale().toDouble(ui.leIncrement->text(), &ok);
		if (ok)
			number = (end - start) / increment + 1;
	}

	try {
		newData.resize(number);
	} catch (std::bad_alloc&) {
		RESET_CURSOR;
		QMessageBox::critical(this, i18n("Failed to allocate memory"), i18n("Not enough memory to perform this operation."));
		return false;
	}

	for (int i = 0; i < number; ++i)
		newData[i] = start + increment * i;

	return true;
}

// generate equidistant datetime values
bool EquidistantValuesDialog::generateDateTimeData(QVector<QDateTime>& newData) {
	bool ok;
	if (ui.cbType->currentIndex() == 0) { // fixed number -> determine the increment
		const auto startValue = ui.dteFrom->dateTime().toMSecsSinceEpoch();
		const auto endValue = ui.dteFrom->dateTime().toMSecsSinceEpoch();
		const int number = QLocale().toInt(ui.leNumber->text(), &ok);
		int increment = 1;
		if (number != 1)
			increment = (endValue - startValue) / (number - 1);

		try {
			newData.resize(number);
		} catch (std::bad_alloc&) {
			RESET_CURSOR;
			QMessageBox::critical(this, i18n("Failed to allocate memory"), i18n("Not enough memory to perform this operation."));
			return false;
		}

		for (int i = 0; i < number; ++i)
			newData[i] = QDateTime::fromMSecsSinceEpoch(startValue + increment * i, Qt::UTC);

	} else { // fixed increment -> determine the number
		const auto startValue = ui.dteFrom->dateTime();
		const auto endValue = ui.dteTo->dateTime();
		const auto increment = QLocale().toInt(ui.leIncrementDateTime->text(), &ok);
		const auto unit = static_cast<DateTimeUnit>(ui.cbIncrementDateTimeUnit->currentData().toInt());
		QDateTime value = startValue;
		switch (unit) {
		case DateTimeUnit::Year:
			while (value < endValue) {
				newData << value;
				value = value.addYears(increment);
			}
			break;
		case DateTimeUnit::Month:
			while (value < endValue) {
				newData << value;
				value = value.addMonths(increment);
			}
			break;
		case DateTimeUnit::Day:
			while (value < endValue) {
				newData << value;
				value = value.addDays(increment);
			}
			break;
		case DateTimeUnit::Hour: {
			const int seconds = increment * 60 * 60;
			while (value < endValue) {
				newData << value;
				value = value.addSecs(seconds);
			}
			break;
		}
		case DateTimeUnit::Minute: {
			const int seconds = increment * 60;
			while (value < endValue) {
				newData << value;
				value = value.addSecs(seconds);
			}
			break;
		}
		case DateTimeUnit::Second:
			while (value < endValue) {
				newData << value;
				value = value.addSecs(increment);
			}
			break;
		case DateTimeUnit::Millisecond:
			while (value < endValue) {
				newData << value;
				value = value.addMSecs(increment);
			}
			break;
		}
	}

	return true;
}
