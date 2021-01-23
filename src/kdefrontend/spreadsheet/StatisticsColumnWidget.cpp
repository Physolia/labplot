/***************************************************************************
	File                 : StatisticsColumnWidget.cpp
    Project              : LabPlot
	Description          : Widget showing statistics for column values
    --------------------------------------------------------------------
	Copyright            : (C) 2021 by Alexander Semke (alexander.semke@web.de)

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

#include "StatisticsColumnWidget.h"
#include "backend/core/column/Column.h"
#include "backend/core/Project.h"
#include "backend/worksheet/Worksheet.h"
#include "backend/worksheet/TextLabel.h"
#include "backend/worksheet/plots/cartesian/CartesianPlot.h"
#include "backend/worksheet/plots/cartesian/Axis.h"
#include "backend/lib/macros.h"

#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include <KLocalizedString>

#include <cmath>

StatisticsColumnWidget::StatisticsColumnWidget(const Column* column, QWidget* parent) : QWidget(parent),
	m_column(column),
	m_project(new Project),
	m_tabWidget(new QTabWidget)
{

	auto* layout = new QVBoxLayout;
	layout->addWidget(m_tabWidget);
	setLayout(layout);

	const QString htmlColor = (palette().color(QPalette::Base).lightness() < 128) ? QLatin1String("#5f5f5f") : QLatin1String("#D1D1D1");
	m_htmlText = QString("<table border=0 width=100%>"
	                     "<tr>"
	                     "<td colspan=2 align=center bgcolor=" + htmlColor + "><b><big>"
	                     + i18n("Location measures")+
	                     "</big><b></td>"
	                     "</tr>"
// 	                     "<tr></tr>"
	                     "<tr>"
	                     "<td width=70%><b>"
	                     + i18n("Count")+
	                     "<b></td>"
	                     "<td>%1</td>"
	                     "</tr>"
						 "<tr>"
	                     "<td><b>"
	                     + i18n("Minimum")+
	                     "<b></td>"
	                     "<td>%2</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Maximum")+
	                     "<b></td>"
	                     "<td>%3</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Arithmetic mean")+
	                     "<b></td>"
	                     "<td>%4</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Geometric mean")+
	                     "<b></td>"
	                     "<td>%5</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Harmonic mean")+
	                     "<b></td>"
	                     "<td>%6</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Contraharmonic mean")+
	                     "<b></td>"
	                     "<td>%7</td>"
	                     "</tr>"
						"<tr>"
	                     "<td><b>"
	                     + i18n("Mode")+
	                     "<b></td>"
	                     "<td>%8</td>"
	                     "</tr>"
						 "<tr>"
	                     "<td><b>"
	                     + i18n("First Quartile")+
	                     "<b></td>"
	                     "<td>%9</td>"
	                     "</tr>"
						 "<tr>"
	                     "<td><b>"
	                     + i18n("Median")+
	                     "<b></td>"
	                     "<td>%10</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Third Quartile")+
	                     "<b></td>"
	                     "<td>%11</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Trimean")+
	                     "<b></td>"
	                     "<td>%12</td>"
	                     "</tr>"
	                     "<tr></tr>"
	                     "<tr>"
	                     "<td colspan=2 align=center bgcolor=" + htmlColor + "><b><big>"
	                     + i18n("Dispersion measures")+
	                     "</big></b></td>"
	                     "</tr>"
// 	                     "<tr></tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Variance")+
	                     "<b></td>"
						 "<td>%13</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Standard deviation")+
	                     "<b></td>"
						 "<td>%14</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Mean absolute deviation around mean")+
	                     "<b></td>"
						 "<td>%15</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Mean absolute deviation around median")+
	                     "<b></td>"
						 "<td>%16</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Median absolute deviation")+
	                     "<b></td>"
						 "<td>%17</td>"
	                     "</tr>"
						 "<tr>"
						  "<td><b>"
						  + i18n("Interquartile Range")+
						  "<b></td>"
						  "<td>%18</td>"
						  "</tr>"
	                     "<tr></tr>"
	                     "<tr>"
	                     "<td colspan=2 align=center bgcolor=" + htmlColor + "><b><big>"
	                     + i18n("Shape measures")+
	                     "</big></b></td>"
	                     "</tr>"
// 	                     "<tr></tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Skewness")+
	                     "<b></td>"
	                     "<td>%19</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Kurtosis")+
	                     "<b></td>"
	                     "<td>%20</td>"
	                     "</tr>"
	                     "<tr>"
	                     "<td><b>"
	                     + i18n("Entropy")+
	                     "<b></td>"
	                     "<td>%21</td>"
	                     "</tr>"
	                     "</table>");

	//create tab widgets for every column and show the initial text with the placeholders
	m_teOverview = new QTextEdit(this);
	m_teOverview->setReadOnly(true);
	m_teOverview->setHtml(m_htmlText.arg(QLatin1String("-"), QLatin1String("-"), QLatin1String("-"), QLatin1String("-"),
									QLatin1String("-"), QLatin1String("-"), QLatin1String("-"), QLatin1String("-"), QLatin1String("-")).
									arg(QLatin1String("-"), QLatin1String("-"), QLatin1String("-"), QLatin1String("-"), QLatin1String("-"),
										QLatin1String("-"), QLatin1String("-"), QLatin1String("-"), QLatin1String("-")).
									arg(QLatin1String("-"), QLatin1String("-"), QLatin1String("-")));

	m_tabWidget->addTab(m_teOverview, i18n("Overview"));
	m_tabWidget->addTab(&m_histogramWidget, i18n("Histogram"));
	m_tabWidget->addTab(&m_kdePlotWidget, i18n("KDE Plot"));
	m_tabWidget->addTab(&m_qqPlotWidget, i18n("Normal Q-Q Plot"));
	m_tabWidget->addTab(&m_boxPlotWidget, i18n("Box Plot"));

	connect(m_tabWidget, &QTabWidget::currentChanged, this, &StatisticsColumnWidget::currentTabChanged);
}

StatisticsColumnWidget::~StatisticsColumnWidget() = default;

void StatisticsColumnWidget::showStatistics() {
	if (!m_overviewInitialized) {
		QApplication::processEvents(QEventLoop::AllEvents, 0);
		QTimer::singleShot(0, this, [=] () {currentTabChanged(0);});
	}
}

void StatisticsColumnWidget::currentTabChanged(int index) {
	if (index == 0 && !m_overviewInitialized)
		showOverview();
	else if (index == 1 && !m_histogramInitialized)
		showHistogram();
	else if (index == 2 && !m_kdePlotInitialized)
		showKDEPlot();
	else if (index == 3 && !m_qqPlotInitialized)
		showQQPlot();
	else if (index == 4 && !m_boxPlotInitialized)
		showBoxPlot();
}

//helpers
const QString StatisticsColumnWidget::isNanValue(const double value) {
	SET_NUMBER_LOCALE
	return (std::isnan(value) ? QLatin1String("-") : numberLocale.toString(value,'f'));
}

QString modeValue(const Column* column, double value) {
	if (std::isnan(value))
		return QLatin1String("-");

	SET_NUMBER_LOCALE
	switch (column->columnMode()) {
	case AbstractColumn::ColumnMode::Integer:
		return numberLocale.toString((int)value);
	case AbstractColumn::ColumnMode::BigInt:
		return numberLocale.toString((qint64)value);
	case AbstractColumn::ColumnMode::Text:
		//TODO
	case AbstractColumn::ColumnMode::DateTime:
		//TODO
	case AbstractColumn::ColumnMode::Day:
		//TODO
	case AbstractColumn::ColumnMode::Month:
		//TODO
	case AbstractColumn::ColumnMode::Numeric:
		return numberLocale.toString(value, 'f');
	}

	return QString();
}

void StatisticsColumnWidget::showOverview() {
	WAIT_CURSOR;
	const Column::ColumnStatistics& statistics = m_column->statistics();

	m_teOverview->setHtml(m_htmlText.arg(QString::number(statistics.size),
									isNanValue(statistics.minimum == INFINITY ? NAN : statistics.minimum),
									isNanValue(statistics.maximum == -INFINITY ? NAN : statistics.maximum),
									isNanValue(statistics.arithmeticMean),
									isNanValue(statistics.geometricMean),
									isNanValue(statistics.harmonicMean),
									isNanValue(statistics.contraharmonicMean),
									modeValue(m_column, statistics.mode),
									isNanValue(statistics.firstQuartile)).
						arg(isNanValue(statistics.median),
							isNanValue(statistics.thirdQuartile),
							isNanValue(statistics.trimean),
							isNanValue(statistics.variance),
							isNanValue(statistics.standardDeviation),
							isNanValue(statistics.meanDeviation),
							isNanValue(statistics.meanDeviationAroundMedian),
							isNanValue(statistics.medianDeviation),
							isNanValue(statistics.iqr)).
						arg(isNanValue(statistics.skewness),
							isNanValue(statistics.kurtosis),
							isNanValue(statistics.entropy)));
	RESET_CURSOR;

	m_overviewInitialized = true;
}

void StatisticsColumnWidget::showHistogram() {
	Worksheet* worksheet = new Worksheet(QString());
	worksheet->setUseViewSize(true);
	worksheet->setLayoutTopMargin(0.);
	worksheet->setLayoutBottomMargin(0.);
	worksheet->setLayoutLeftMargin(0.);
	worksheet->setLayoutRightMargin(0.);
	worksheet->setTheme(QLatin1String("Bright"));
	m_project->addChild(worksheet);

	CartesianPlot* plot = new CartesianPlot(QString());
	plot->setType(CartesianPlot::Type::TwoAxes);
	plot->setSymmetricPadding(false);
	double padding = Worksheet::convertToSceneUnits(1.0, Worksheet::Unit::Centimeter);
	plot->setRightPadding(padding);
	plot->setVerticalPadding(padding);

	auto axes = plot->children<Axis>();
	for (auto* axis : axes) {
		if (axis->orientation() == Axis::Orientation::Horizontal)
			axis->title()->setText(m_column->name());
		else
			axis->title()->setText(i18n("Frequency"));

		axis->setMinorTicksDirection(Axis::noTicks);
		//QPen pen = axis->line
	}
	worksheet->addChild(plot);

	worksheet->setPlotsLocked(true);

	Histogram* histogram = new Histogram(QString());
	plot->addChild(histogram);
	histogram->setDataColumn(m_column);

	auto* layout = new QVBoxLayout;
	layout->setSpacing(0);
	layout->addWidget(worksheet->view());
	m_histogramWidget.setLayout(layout);

	m_histogramInitialized = true;
}

void StatisticsColumnWidget::showKDEPlot() {
	m_kdePlotInitialized = true;
}

void StatisticsColumnWidget::showQQPlot() {
	m_qqPlotInitialized = true;
}

void StatisticsColumnWidget::showBoxPlot() {
	m_boxPlotInitialized = true;
}