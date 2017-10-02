/***************************************************************************
    File                 : EquidistantValuesDialog.cpp
    Project              : LabPlot
    Description          : Dialog for generating equidistant numbers
    --------------------------------------------------------------------
    Copyright            : (C) 2014 by Alexander Semke (alexander.semke@web.de)

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor,                    *
 *   Boston, MA  02110-1301  USA                                           *
 *                                                                         *
 ***************************************************************************/
#include "EquidistantValuesDialog.h"
#include "backend/core/column/Column.h"
#include "backend/lib/macros.h"
#include "backend/spreadsheet/Spreadsheet.h"

#include <KLocalizedString>

/*!
	\class EquidistantValuesDialog
	\brief Dialog for equidistant values.

	\ingroup kdefrontend
 */

EquidistantValuesDialog::EquidistantValuesDialog(Spreadsheet* s, QWidget* parent, Qt::WFlags fl) : KDialog(parent, fl), m_spreadsheet(s) {

	setWindowTitle(i18n("Equidistant values"));

	QWidget* mainWidget = new QWidget(this);
	ui.setupUi(mainWidget);
	setMainWidget( mainWidget );
    setAttribute(Qt::WA_DeleteOnClose);
	ui.cbType->addItem(i18n("Number"));
	ui.cbType->addItem(i18n("Increment"));

	setButtons( KDialog::Ok | KDialog::Cancel );
	setButtonText(KDialog::Ok, i18n("&Generate"));
	setButtonToolTip(KDialog::Ok, i18n("Generate equidistant values"));

	ui.leFrom->setClearButtonEnabled(true);
	ui.leTo->setClearButtonEnabled(true);
	ui.leIncrement->setClearButtonEnabled(true);
	ui.leNumber->setClearButtonEnabled(true);

	ui.leFrom->setValidator( new QDoubleValidator(ui.leFrom) );
	ui.leTo->setValidator( new QDoubleValidator(ui.leTo) );
	ui.leIncrement->setValidator( new QDoubleValidator(ui.leIncrement) );
	ui.leNumber->setValidator( new QIntValidator(ui.leNumber) );

	ui.leFrom->setText("1");
	ui.leTo->setText("100");
	ui.leIncrement->setText("1");

	connect( ui.cbType, SIGNAL(currentIndexChanged(int)), SLOT(typeChanged(int)) );
	connect( ui.leFrom, SIGNAL(textChanged(QString)), this, SLOT(checkValues()) );
	connect( ui.leTo, SIGNAL(textChanged(QString)), this, SLOT(checkValues()) );
	connect( ui.leNumber, SIGNAL(textChanged(QString)), this, SLOT(checkValues()) );
	connect( ui.leIncrement, SIGNAL(textChanged(QString)), this, SLOT(checkValues()) );
	connect(this, SIGNAL(okClicked()), this, SLOT(generate()));

	//generated data the  default
	this->typeChanged(0);

	resize( QSize(300,0).expandedTo(minimumSize()) );
}

void EquidistantValuesDialog::setColumns(QVector<Column*> columns) {
	m_columns = columns;
	ui.leNumber->setText( QString::number(m_columns.first()->rowCount()) );
}

void EquidistantValuesDialog::typeChanged(int index) {
	if (index==0) { //fixed number
		ui.lIncrement->hide();
		ui.leIncrement->hide();
		ui.lNumber->show();
		ui.leNumber->show();
	} else { //fixed increment
		ui.lIncrement->show();
		ui.leIncrement->show();
		ui.lNumber->hide();
		ui.leNumber->hide();
	}
}

void EquidistantValuesDialog::checkValues() {
	if (ui.leFrom->text().simplified().isEmpty()) {
		enableButton(KDialog::Ok, false);
		return;
	}

	if (ui.leTo->text().simplified().isEmpty()) {
		enableButton(KDialog::Ok, false);
		return;
	}

	if (ui.cbType->currentIndex() == 0) {
		if (ui.leNumber->text().simplified().isEmpty() || ui.leNumber->text().simplified().toInt()==0) {
			enableButton(KDialog::Ok, false);
			return;
		}
	} else {
		if (ui.leIncrement->text().simplified().isEmpty() || qFuzzyIsNull(ui.leIncrement->text().simplified().toDouble())) {
			enableButton(KDialog::Ok, false);
			return;
		}
	}

	enableButton(KDialog::Ok, true);
}

void EquidistantValuesDialog::generate() {
	Q_ASSERT(m_spreadsheet);

	WAIT_CURSOR;
	m_spreadsheet->beginMacro(i18np("%1: fill column with equidistant numbers",
									"%1: fill columns with equidistant numbers",
									m_spreadsheet->name(),
									m_columns.size()));

	double start  = ui.leFrom->text().toDouble();
	double end  = ui.leTo->text().toDouble();
	int number;
	double dist;
	if (ui.cbType->currentIndex()==0) { //fixed number
		number = ui.leNumber->text().toInt();
		if (number!=1)
			dist = (end - start)/ (number - 1);
		else
			dist = 0;
	} else { //fixed increment
		dist = ui.leIncrement->text().toDouble();
		number = (end-start)/dist + 1;
	}

	if (m_spreadsheet->rowCount()<number)
		m_spreadsheet->setRowCount(number);

	for (auto* col : m_columns) {
		col->setSuppressDataChangedSignal(true);

		if (m_spreadsheet->rowCount()>number)
			col->clear();

		for (int i=0; i<number; ++i) {
			col->setValueAt(i, start + dist*i);
		}

		col->setSuppressDataChangedSignal(false);
		col->setChanged();
	}

	m_spreadsheet->endMacro();
	RESET_CURSOR;
}
