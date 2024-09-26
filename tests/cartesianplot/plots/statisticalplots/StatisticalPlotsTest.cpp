/*
	File                 : StatisticalPlotsTest.cpp
	Project              : LabPlot
	Description          : Tests for statistical plots like Q-Q plot, KDE plot, etc.
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2023 Alexander Semke <alexander.semke@web.de>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "StatisticalPlotsTest.h"
#include "backend/core/Project.h"
#include "backend/core/column/Column.h"
#include "backend/spreadsheet/Spreadsheet.h"
#include "backend/worksheet/Worksheet.h"
#include "backend/worksheet/plots/cartesian/BarPlot.h"
#include "backend/worksheet/plots/cartesian/Histogram.h"
#include "backend/worksheet/plots/cartesian/KDEPlot.h"
#include "backend/worksheet/plots/cartesian/ProcessBehaviorChart.h"
#include "backend/worksheet/plots/cartesian/QQPlot.h"
#include "backend/worksheet/plots/cartesian/XYCurve.h"

#include <gsl/gsl_randist.h>
#include <gsl/gsl_rng.h>

#include <QUndoStack>

// ##############################################################################
// ############################## Histogram #####################################
// ##############################################################################
/*!
 * \brief create and add a new Histogram, undo and redo this step
 */
void StatisticalPlotsTest::testHistogramInit() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* histogram = new Histogram(QStringLiteral("histogram"));
	p->addChild(histogram);

	auto children = p->children<Histogram>();

	QCOMPARE(children.size(), 1);

	project.undoStack()->undo();
	children = p->children<Histogram>();
	QCOMPARE(children.size(), 0);

	project.undoStack()->redo();
	children = p->children<Histogram>();
	QCOMPARE(children.size(), 1);
}

/*!
 * \brief create and add a new Histogram, duplicate it and check the number of children
 */
void StatisticalPlotsTest::testHistogramDuplicate() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* histogram = new Histogram(QStringLiteral("histogram"));
	p->addChild(histogram);

	histogram->duplicate();

	auto children = p->children<Histogram>();
	QCOMPARE(children.size(), 2);
}

/*!
 * \brief create a Histogram for 3 values check the plot ranges.
 */
void StatisticalPlotsTest::testHistogramRangeBinningTypeChanged() {
	// prepare the data
	Spreadsheet sheet(QStringLiteral("test"), false);
	sheet.setColumnCount(1);
	sheet.setRowCount(100);
	auto* column = sheet.column(0);
	column->setValueAt(0, 1.);
	column->setValueAt(1, 2.);
	column->setValueAt(2, 3.);

	// prepare the worksheet + plot
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* histogram = new Histogram(QStringLiteral("histogram"));
	histogram->setBinningMethod(Histogram::BinningMethod::ByNumber);
	histogram->setBinCount(3);
	histogram->setDataColumn(column);
	p->addChild(histogram);

	// the x-range is defined by the min and max values in the data [1, 3]
	// because of the bin count 3 we have one value in every bin and the y-range is [0,1]
	const auto& rangeX = p->range(Dimension::X);
	const auto& rangeY = p->range(Dimension::Y);
	QCOMPARE(rangeX.start(), 1);
	QCOMPARE(rangeX.end(), 3);
	QCOMPARE(rangeY.start(), 0);
	QCOMPARE(rangeY.end(), 1);

	// set the bin number to 1, the values 1 and 2 fall into the same bin
	histogram->setBinCount(1);
	QCOMPARE(rangeX.start(), 1);
	QCOMPARE(rangeX.end(), 3);
	QCOMPARE(rangeY.start(), 0);
	QCOMPARE(rangeY.end(), 2);
}

/*!
 * \brief create a Histogram for 3 values check the plot ranges after a row was removed in the source spreadsheet.
 */
void StatisticalPlotsTest::testHistogramRangeRowsChanged() {
	Project project;

	// prepare the data
	auto* sheet = new Spreadsheet(QStringLiteral("test"), false);
	sheet->setColumnCount(1);
	sheet->setRowCount(3);
	project.addChild(sheet);
	auto* column = sheet->column(0);
	column->setValueAt(0, 1.);
	column->setValueAt(1, 2.);
	column->setValueAt(2, 3.);

	// worksheet
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* histogram = new Histogram(QStringLiteral("histogram"));
	histogram->setBinningMethod(Histogram::BinningMethod::ByNumber);
	histogram->setBinCount(3);
	histogram->setDataColumn(column);
	p->addChild(histogram);

	// remove the last row and check the ranges, the x-range should become [1,2]
	sheet->setRowCount(2);
	const auto& rangeX = p->range(Dimension::X);
	const auto& rangeY = p->range(Dimension::Y);
	QCOMPARE(rangeX.start(), 1);
	QCOMPARE(rangeX.end(), 2);
	QCOMPARE(rangeY.start(), 0);
	QCOMPARE(rangeY.end(), 1);

	// undo the row removal and check again, the x-range should become [1,3] again
	project.undoStack()->undo();
	QCOMPARE(rangeX.start(), 1);
	QCOMPARE(rangeX.end(), 3);
	QCOMPARE(rangeY.start(), 0);
	QCOMPARE(rangeY.end(), 1);

	// add more (empty) rows in the spreadsheet, the ranges should be unchanged
	sheet->setRowCount(5);
	QCOMPARE(rangeX.start(), 1);
	QCOMPARE(rangeX.end(), 3);
	QCOMPARE(rangeY.start(), 0);
	QCOMPARE(rangeY.end(), 1);
}

void StatisticalPlotsTest::testHistogramColumnRemoved() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* histogram = new Histogram(QStringLiteral("histogram"));
	p->addChild(histogram);

	auto* c = new Column(QStringLiteral("TestColumn"));
	project.addChild(c);

	histogram->setDataColumn(c);
	c->setName(QStringLiteral("NewName"));
	QCOMPARE(histogram->dataColumnPath(), QStringLiteral("Project/NewName"));

	c->remove();

	QCOMPARE(histogram->dataColumn(), nullptr);
	QCOMPARE(histogram->dataColumnPath(), QStringLiteral("Project/NewName"));

	c->setName(QStringLiteral("Another new name")); // Shall not lead to a crash

	QCOMPARE(histogram->dataColumn(), nullptr);
	QCOMPARE(histogram->dataColumnPath(), QStringLiteral("Project/NewName"));
}

// ##############################################################################
// ############################## KDE Plot ######################################
// ##############################################################################

/*!
 * \brief create and add a new KDEPlot, undo and redo this step
 */
void StatisticalPlotsTest::testKDEPlotInit() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* kdePlot = new KDEPlot(QStringLiteral("kdeplot"));
	p->addChild(kdePlot);

	auto children = p->children<KDEPlot>();

	QCOMPARE(children.size(), 1);

	project.undoStack()->undo();
	children = p->children<KDEPlot>();
	QCOMPARE(children.size(), 0);

	// TODO: crash!!!
	// project.undoStack()->redo();
	// children = p->children<KDEPlot>();
	// QCOMPARE(children.size(), 1);
}

/*!
 * \brief create and add a new KDEPlot, duplicate it and check the number of children
 */
void StatisticalPlotsTest::testKDEPlotDuplicate() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* kdePlot = new KDEPlot(QStringLiteral("kdeplot"));
	p->addChild(kdePlot);

	kdePlot->duplicate();

	auto children = p->children<KDEPlot>();
	QCOMPARE(children.size(), 2);
}

/*!
 * \brief create a KDE plot for 3 values check the plot ranges.
 */
void StatisticalPlotsTest::testKDEPlotRange() {
	// prepare the data
	Spreadsheet sheet(QStringLiteral("test"), false);
	sheet.setColumnCount(1);
	sheet.setRowCount(100);
	auto* column = sheet.column(0);
	column->setValueAt(0, 2.);
	column->setValueAt(1, 4.);
	column->setValueAt(2, 6.);

	// prepare the worksheet + plot
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* kdePlot = new KDEPlot(QStringLiteral("kdeplot"));
	kdePlot->setKernelType(nsl_kernel_gauss);
	kdePlot->setBandwidthType(nsl_kde_bandwidth_custom);
	kdePlot->setBandwidth(0.3);
	p->addChild(kdePlot);
	kdePlot->setDataColumn(column);

	// validate with R via:
	// data <- c(2,4,6);
	// kd <- density(data,kernel="gaussian", bw=0.3)
	// plot(kd, col='blue', lwd=2)

	// check the x-range of the plot which should be [1, 7] (subtract/add 3 sigmas from/to min and max, respectively).
	const auto& rangeX = p->range(Dimension::X);
	QCOMPARE(rangeX.start(), 1);
	QCOMPARE(rangeX.end(), 7);

	// check the y-range of the plot which should be [0, 0.45]
	const auto& rangeY = p->range(Dimension::Y);
	QCOMPARE(rangeY.start(), 0.);
	QCOMPARE(rangeY.end(), 0.45);
}

// ##############################################################################
// ############################## Q-Q Plot ######################################
// ##############################################################################

/*!
 * \brief create and add a new QQPlot, undo and redo this step
 */
void StatisticalPlotsTest::testQQPlotInit() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* qqPlot = new QQPlot(QStringLiteral("qqplot"));
	p->addChild(qqPlot);

	auto children = p->children<QQPlot>();

	QCOMPARE(children.size(), 1);

	project.undoStack()->undo();
	children = p->children<QQPlot>();
	QCOMPARE(children.size(), 0);

	// TODO: crash!!!
	// project.undoStack()->redo();
	// children = p->children<QQPlot>();
	// QCOMPARE(children.size(), 1);
}

/*!
 * \brief create and add a new QQPlot, duplicate it and check the number of children
 */
void StatisticalPlotsTest::testQQPlotDuplicate() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* qqPlot = new QQPlot(QStringLiteral("qqplot"));
	p->addChild(qqPlot);

	qqPlot->duplicate();

	auto children = p->children<QQPlot>();
	QCOMPARE(children.size(), 2);
}

/*!
 * \brief create QQPlot for 100 normaly distributed values and check the plot ranges.
 */
void StatisticalPlotsTest::testQQPlotRange() {
	// prepare the data
	Spreadsheet sheet(QStringLiteral("test"), false);
	sheet.setColumnCount(1);
	sheet.setRowCount(100);
	auto* column = sheet.column(0);

	// create a generator chosen by the environment variable GSL_RNG_TYPE
	gsl_rng_env_setup();
	const gsl_rng_type* T = gsl_rng_default;
	gsl_rng* r = gsl_rng_alloc(T);
	const double sigma = 1.;

	for (int i = 0; i < 100; ++i)
		column->setValueAt(i, gsl_ran_gaussian(r, sigma));

	// prepare the worksheet + plot
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* qqPlot = new QQPlot(QStringLiteral("qqplot"));
	p->addChild(qqPlot);
	qqPlot->setDataColumn(column);

	// check the x-range of the plot which should be [-2.5, 2.5] for the theoretical quantiles
	const auto& range = p->range(Dimension::X);
	QCOMPARE(range.start(), -2.5);
	QCOMPARE(range.end(), 2.5);
}

// ##############################################################################
// ############################## BAr Plot ######################################
// ##############################################################################

/*!
 * \brief create and add a new BarPlot, undo and redo this step
 */
void StatisticalPlotsTest::testBarPlotInit() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* barPlot = new BarPlot(QStringLiteral("barplot"));
	p->addChild(barPlot);

	auto children = p->children<BarPlot>();

	QCOMPARE(children.size(), 1);

	project.undoStack()->undo();
	children = p->children<BarPlot>();
	QCOMPARE(children.size(), 0);

	project.undoStack()->redo();
	children = p->children<BarPlot>();
	QCOMPARE(children.size(), 1);
}

/*!
 * \brief create and add a new BarPlot, duplicate it and check the number of children
 */
void StatisticalPlotsTest::testBarPlotDuplicate() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* barPlot = new BarPlot(QStringLiteral("barplot"));
	p->addChild(barPlot);

	barPlot->duplicate();

	auto children = p->children<BarPlot>();
	QCOMPARE(children.size(), 2);
}

/*!
 * \brief create BarPlot for the given data and check the plot ranges.
 */
void StatisticalPlotsTest::testBarPlotRange() {
	Project project;

	// prepare the data
	auto* sheet = new Spreadsheet(QStringLiteral("test"), false);
	project.addChild(sheet);
	sheet->setColumnCount(2);
	sheet->setRowCount(2);
	auto* column1 = sheet->column(0);
	auto* column2 = sheet->column(1);

	// create a generator chosen by the environment variable GSL_RNG_TYPE
	column1->setValueAt(0, 10);
	column1->setValueAt(1, 1);
	column2->setValueAt(0, 20);
	column2->setValueAt(1, 2);

	// prepare the worksheet + plot
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* barPlot = new BarPlot(QStringLiteral("barplot"));
	p->addChild(barPlot);
	barPlot->setDataColumns({column1, column2});

	// check the ranges which should be [0, 2] for x and [0, 20] for y
	const auto& rangeX = p->range(Dimension::X);
	QCOMPARE(rangeX.start(), 0);
	QCOMPARE(rangeX.end(), 2);

	const auto& rangeY = p->range(Dimension::Y);
	QCOMPARE(rangeY.start(), 0);
	QCOMPARE(rangeY.end(), 20);

	// remove the first row in the spreadsheet and check the ranges which should be [0, 1] for x and [0, 2] for y
	sheet->removeRows(0, 1);

	QCOMPARE(rangeX.start(), 0);
	QCOMPARE(rangeX.end(), 1);

	QCOMPARE(rangeY.start(), 0);
	QCOMPARE(rangeY.end(), 2);

	// undo the removal and check again
	project.undoStack()->undo();
	QCOMPARE(rangeX.start(), 0);
	QCOMPARE(rangeX.end(), 2);

	QCOMPARE(rangeY.start(), 0);
	QCOMPARE(rangeY.end(), 20);

	// mask the first row in the spreadsheet and check the ranges which should be [0, 1] for x and [0, 2] for y
	project.undoStack()->beginMacro(QStringLiteral("mask"));
	column1->setMasked(0);
	column2->setMasked(0);
	project.undoStack()->endMacro();

	QCOMPARE(rangeX.start(), 0);
	QCOMPARE(rangeX.end(), 1);

	QCOMPARE(rangeY.start(), 0);
	QCOMPARE(rangeY.end(), 2);

	// undo the masking and check again
	project.undoStack()->undo();
	QCOMPARE(rangeX.start(), 0);
	QCOMPARE(rangeX.end(), 2);

	QCOMPARE(rangeY.start(), 0);
	QCOMPARE(rangeY.end(), 20);
}

// ##############################################################################
// ################### Process Behavior Chart ###################################
// ##############################################################################
/*!
 * \brief create and add a new process behavior chart, undo and redo this step
 */
void StatisticalPlotsTest::testPBChartInit() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* pbc = new ProcessBehaviorChart(QStringLiteral("pbc"));
	p->addChild(pbc);

	auto children = p->children<ProcessBehaviorChart>();

	QCOMPARE(children.size(), 1);

	project.undoStack()->undo();
	children = p->children<ProcessBehaviorChart>();
	QCOMPARE(children.size(), 0);

	// TODO: crash!!!
	// project.undoStack()->redo();
	// children = p->children<ProcessBehaviorChart>();
	// QCOMPARE(children.size(), 1);
}

/*!
 * \brief create and add a new process behavior chart, duplicate it and check the number of children
 */
void StatisticalPlotsTest::testPBChartDuplicate() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* pbc = new ProcessBehaviorChart(QStringLiteral("pbc"));
	p->addChild(pbc);

	pbc->duplicate();

	auto children = p->children<ProcessBehaviorChart>();
	QCOMPARE(children.size(), 2);
}

/*!
 * test the X (XmR) chart using Average for the limits, the example is taken from Wheeler "Making Sense of Data", chapter seven.
 */
void StatisticalPlotsTest::testPBChartXmRAverage() {
	// prepare the data
	auto* column = new Column(QLatin1String("data"), AbstractColumn::ColumnMode::Integer);
	column->setIntegers({11, 4, 6, 4, 5, 7, 5, 4, 7, 12, 4, 2, 4, 5, 6, 4, 2, 2, 5, 9, 5, 6, 5, 9});

	// prepare the worksheet + plot
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* pbc = new ProcessBehaviorChart(QStringLiteral("pbc"));
	pbc->setDataColumn(column);
	pbc->setType(ProcessBehaviorChart::Type::XmR);
	pbc->setLimitsMetric(ProcessBehaviorChart::LimitsMetric::Average);
	p->addChild(pbc);

	// check the limits, two digit comparison with the values from the book
	QCOMPARE(std::round(pbc->center() * 100) / 100, 5.54);
	QCOMPARE(std::round(pbc->upperLimit() * 100) / 100, 12.48);
	QCOMPARE(pbc->lowerLimit(), 0);

	// check the plotted data ("statistics") - the original data is plotted
	const int rowCount = column->rowCount();
	auto* yColumn = pbc->dataCurve()->yColumn();
	QCOMPARE(yColumn, column);
	QCOMPARE(yColumn->rowCount(), rowCount);

	// index from 1 to 24 is used for x
	auto* xColumn = pbc->dataCurve()->xColumn();
	QCOMPARE(xColumn->rowCount(), rowCount);
	for (int i = 0; i < rowCount; ++i)
		QCOMPARE(xColumn->valueAt(i), i + 1);
}

/*!
 * test the mR (XmR) chart using Average for the limits, the example is taken from Wheeler "Making Sense of Data", chapter seven.
 */
void StatisticalPlotsTest::testPBChartmRAverage() {
	// prepare the data
	auto* column = new Column(QLatin1String("data"), AbstractColumn::ColumnMode::Integer);
	column->setIntegers({11, 4, 6, 4, 5, 7, 5, 4, 7, 12, 4, 2, 4, 5, 6, 4, 2, 2, 5, 9, 5, 6, 5, 9});

	// prepare the worksheet + plot
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* pbc = new ProcessBehaviorChart(QStringLiteral("pbc"));
	pbc->setDataColumn(column);
	pbc->setType(ProcessBehaviorChart::Type::mR);
	pbc->setLimitsMetric(ProcessBehaviorChart::LimitsMetric::Average);
	p->addChild(pbc);

	// check the limits, two digit comparison with the values from the book
	QCOMPARE(std::round(pbc->center() * 100) / 100, 2.61);
	QCOMPARE(std::round(pbc->upperLimit() * 100) / 100, 8.52); // in the book 3.27*2.61 \ approx 8.52 is used which is less precise than 3.26653*2.6087 \approx 8.52
	QCOMPARE(pbc->lowerLimit(), 0);

	// check the plotted data ("statistics") - 23 moving ranges are plotted
	const int rowCount = 24; // total count 24, first value not available/used/plotted
	auto* yColumn = pbc->dataCurve()->yColumn();
	QCOMPARE(yColumn->rowCount(), rowCount);
	const QVector<double> ref = {7, 2, 2, 1, 2, 2, 1, 3, 5, 8, 2, 2, 1, 1, 2, 2, 0, 3, 4, 4, 1, 1, 4};
	for (int i = 0; i < rowCount - 1; ++i)
		QCOMPARE(yColumn->valueAt(i + 1), ref.at(i));

	// index from 1 to 24 is used for x
	auto* xColumn = pbc->dataCurve()->xColumn();
	QCOMPARE(xColumn->rowCount(), rowCount);
	for (int i = 0; i < rowCount; ++i)
		QCOMPARE(xColumn->valueAt(i), i +  1);
}

/*!
 * test the X (XmR) chart using Median for the limits, the example is taken from Wheeler "Making Sense of Data", chapter ten.
 */
void StatisticalPlotsTest::testPBChartXmRMedian() {
	// prepare the data
	auto* column = new Column(QLatin1String("data"), AbstractColumn::ColumnMode::Integer);
	column->setIntegers({260, 130, 189, 1080, 175, 200, 193, 120, 33, 293, 195, 571, 55698, 209, 1825, 239, 290, 254, 93, 278, 185, 123, 9434, 408, 570, 118, 238, 207, 153, 209, 243, 110, 306, 343, 244});

	// prepare the worksheet + plot
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* pbc = new ProcessBehaviorChart(QStringLiteral("pbc"));
	pbc->setDataColumn(column);
	pbc->setType(ProcessBehaviorChart::Type::XmR);
	pbc->setLimitsMetric(ProcessBehaviorChart::LimitsMetric::Median);
	p->addChild(pbc);

	// check the limits, two digit comparison with the values from the book
	QCOMPARE(std::round(pbc->center() * 100) / 100, 238);
	QCOMPARE(std::round(pbc->upperLimit() * 100) / 100, 631.13); // in the book 630 is shown for 238 + 3.14 * 125 = 630.5, the more precise value is 238 + 3.14507 * 125 \approx 631.13
	QCOMPARE(pbc->lowerLimit(), 0);

	// check the plotted data ("statistics") - the original data is plotted
	const int rowCount = column->rowCount();
	auto* yColumn = pbc->dataCurve()->yColumn();
	QCOMPARE(yColumn, column);
	QCOMPARE(yColumn->rowCount(), rowCount);

	// index from 1 to 35 is used for x
	auto* xColumn = pbc->dataCurve()->xColumn();
	QCOMPARE(xColumn->rowCount(), rowCount);
	for (int i = 0; i < rowCount; ++i)
		QCOMPARE(xColumn->valueAt(i), i + 1);
}

/*!
 * test the mR (XmR) chart using Median for the limits, the example is taken from Wheeler "Making Sense of Data", chapter ten.
 */
void StatisticalPlotsTest::testPBChartmRMedian() {
	// prepare the data
	auto* column = new Column(QLatin1String("data"), AbstractColumn::ColumnMode::Integer);
	column->setIntegers({260, 130, 189, 1080, 175, 200, 193, 120, 33, 293, 195, 571, 55698, 209, 1825, 239, 290, 254, 93, 278, 185, 123, 9434, 408, 570, 118, 238, 207, 153, 209, 243, 110, 306, 343, 244});

	// prepare the worksheet + plot
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* pbc = new ProcessBehaviorChart(QStringLiteral("pbc"));
	pbc->setDataColumn(column);
	pbc->setType(ProcessBehaviorChart::Type::mR);
	pbc->setLimitsMetric(ProcessBehaviorChart::LimitsMetric::Median);
	p->addChild(pbc);

	// check the limits, two digit comparison with the values from the book
	QCOMPARE(std::round(pbc->center() * 100) / 100, 125);
	QCOMPARE(std::round(pbc->upperLimit() * 100) / 100, 482.95); // in the book 482 is shown for 3.86 * 125 = 482.5, the more precise value is 3.86361*125 \ approx 482.95
	QCOMPARE(pbc->lowerLimit(), 0);

	// check the plotted data ("statistics") - 34 moving ranges are plotted
	const int rowCount = 35; // total count 35, first value not available/used/plotted
	auto* yColumn = pbc->dataCurve()->yColumn();
	QCOMPARE(yColumn->rowCount(), rowCount);
	const QVector<double> ref = {130, 59, 891, 905, 25, 7, 73, 87, 260, 98, 376, 55127, 55489, 1616, 1586, 51, 36, 161, 185, 93, 62, 9311, 9026, 162, 452, 120, 31, 54, 56, 34, 133, 196, 37, 99};
	for (int i = 0; i < rowCount - 1; ++i)
		QCOMPARE(yColumn->valueAt(i + 1), ref.at(i));

	// index from 1 to 24 is used for x
	auto* xColumn = pbc->dataCurve()->xColumn();
	QCOMPARE(xColumn->rowCount(), rowCount);
	for (int i = 0; i < rowCount; ++i)
		QCOMPARE(xColumn->valueAt(i), i +  1);
}

void StatisticalPlotsTest::testPBChartXBarRAverage() {

}

void StatisticalPlotsTest::testPBChartXBarRMedian() {

}

/*!
 * test the XBar (XBarS) chart, the example is taken from Montgomery "Statistical Quality Control", chapter 6.3.
 */
void StatisticalPlotsTest::testPBChartXBarS() {
	// prepare the data
	auto* column = new Column(QLatin1String("data"), AbstractColumn::ColumnMode::Double);
	column->setValues({74.03, 74.002, 74.019, 73.992, 74.008, 73.995, 73.992, 74.001, 74.011, 74.004, 73.988, 74.024, 74.021, 74.005, 74.002, 74.002, 73.996, 73.993, 74.015, 74.009, 73.992, 74.007, 74.015,
		73.989, 74.014, 74.009, 73.994, 73.997, 73.985, 73.993, 73.995, 74.006, 73.994, 74, 74.005, 73.985, 74.003, 73.993, 74.015, 73.988, 74.008, 73.995, 74.009, 74.005, 74.004, 73.998, 74, 73.99, 74.007,
		73.995, 73.994, 73.998, 73.994, 73.995, 73.99, 74.004, 74, 74.007, 74, 73.996, 73.983, 74.002, 73.998, 73.997, 74.012, 74.006, 73.967, 73.994, 74, 73.984, 74.012, 74.014, 73.998, 73.999, 74.007,
		74, 73.984, 74.005, 73.998, 73.996, 73.994, 74.012, 73.986, 74.005, 74.007, 74.006, 74.01, 74.018, 74.003, 74, 73.984, 74.002, 74.003, 74.005, 73.997, 74, 74.01, 74.013, 74.02, 74.003, 73.982, 74.001,
		74.015, 74.005, 73.996, 74.004, 73.999, 73.99, 74.006, 74.009, 74.01, 73.989,  73.99, 74.009, 74.014, 74.015, 74.008, 73.993, 74, 74.01, 73.982, 73.984, 73.995, 74.017, 74.013});

	// prepare the worksheet + plot
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* pbc = new ProcessBehaviorChart(QStringLiteral("pbc"));
	pbc->setDataColumn(column);
	pbc->setType(ProcessBehaviorChart::Type::XbarS);
	p->addChild(pbc);

	// check the limits, three digit comparison with the values from the book
	QCOMPARE(std::round(pbc->center() * 1000) / 1000, 74.001);
	QCOMPARE(std::round(pbc->upperLimit() * 1000) / 1000, 74.015); // in the book 74.001 + 1.427*0.0094 = 74.014 is used, the more precise rounded value is 74.0012 + 1.4273*0.00939948 = 74.0146 \ approx = 74.015
	QCOMPARE(std::round(pbc->lowerLimit() * 1000) / 1000, 73.988);

	// check the plotted data ("statistics") - mean values for every subgroup/sample are plotted
	const int rowCount = 25; // 25 samples
	auto* yColumn = pbc->dataCurve()->yColumn();
	QCOMPARE(yColumn->rowCount(), rowCount);
	const QVector<double> ref = {74.010, 74.001, 74.008, 74.003, 74.003, 73.996, 74., 73.997, 74.004, 73.998, 73.994, 74.001, 73.998, 73.990, 74.006, 73.997, 74.001, 74.007, 73.998, 74.009, 74., 74.002, 74.002, 74.005, 73.998};
	for (int i = 0; i < rowCount - 1; ++i)
		QCOMPARE(std::round(yColumn->valueAt(i) * 1000) / 1000, ref.at(i)); // compare three digits

	// index from 1 to 25 is used for x
	auto* xColumn = pbc->dataCurve()->xColumn();
	QCOMPARE(xColumn->rowCount(), rowCount);
	for (int i = 0; i < rowCount; ++i)
		QCOMPARE(xColumn->valueAt(i), i +  1);
}

/*!
 * test the S chart, the example is taken from Montgomery "Statistical Quality Control", chapter 6.3.
 */
void StatisticalPlotsTest::testPBChartS() {
	// prepare the data
	auto* column = new Column(QLatin1String("data"), AbstractColumn::ColumnMode::Double);
	column->setValues({74.03, 74.002, 74.019, 73.992, 74.008, 73.995, 73.992, 74.001, 74.011, 74.004, 73.988, 74.024, 74.021, 74.005, 74.002, 74.002, 73.996, 73.993, 74.015, 74.009, 73.992, 74.007, 74.015,
		73.989, 74.014, 74.009, 73.994, 73.997, 73.985, 73.993, 73.995, 74.006, 73.994, 74, 74.005, 73.985, 74.003, 73.993, 74.015, 73.988, 74.008, 73.995, 74.009, 74.005, 74.004, 73.998, 74, 73.99, 74.007,
		73.995, 73.994, 73.998, 73.994, 73.995, 73.99, 74.004, 74, 74.007, 74, 73.996, 73.983, 74.002, 73.998, 73.997, 74.012, 74.006, 73.967, 73.994, 74, 73.984, 74.012, 74.014, 73.998, 73.999, 74.007,
		74, 73.984, 74.005, 73.998, 73.996, 73.994, 74.012, 73.986, 74.005, 74.007, 74.006, 74.01, 74.018, 74.003, 74, 73.984, 74.002, 74.003, 74.005, 73.997, 74, 74.01, 74.013, 74.02, 74.003, 73.982, 74.001,
		74.015, 74.005, 73.996, 74.004, 73.999, 73.99, 74.006, 74.009, 74.01, 73.989,  73.99, 74.009, 74.014, 74.015, 74.008, 73.993, 74, 74.01, 73.982, 73.984, 73.995, 74.017, 74.013});

	// prepare the worksheet + plot
	auto* ws = new Worksheet(QStringLiteral("worksheet"));
	auto* p = new CartesianPlot(QStringLiteral("plot"));
	ws->addChild(p);

	auto* pbc = new ProcessBehaviorChart(QStringLiteral("pbc"));
	pbc->setDataColumn(column);
	pbc->setType(ProcessBehaviorChart::Type::S);
	p->addChild(pbc);

	// check the limits, four digit comparison with the values from the book
	QCOMPARE(std::round(pbc->center() * 10000) / 10000, 0.0094);
	QCOMPARE(std::round(pbc->upperLimit() * 10000) / 10000, 0.0196);
	QCOMPARE(pbc->lowerLimit(), 0.);

	// check the plotted data ("statistics") - standard deviations for every subgroup/sample are plotted
	const int rowCount = 25; // 25 samples
	auto* yColumn = pbc->dataCurve()->yColumn();
	QCOMPARE(yColumn->rowCount(), rowCount);
	const QVector<double> ref = {0.0148, 0.0075, 0.0147, 0.0091, 0.0122, 0.0087, 0.0055, 0.0123, 0.0055, 0.0063, 0.0029, 0.0042, 0.0105, 0.0153, 0.0073, 0.0078, 0.0106, 0.0070, 0.0085, 0.0080, 0.0122, 0.0074, 0.0119, 0.0087, 0.0162};
	for (int i = 0; i < rowCount - 1; ++i)
		QCOMPARE(std::round(yColumn->valueAt(i) * 10000) / 10000, ref.at(i)); // compare four digits

	// index from 1 to 25 is used for x
	auto* xColumn = pbc->dataCurve()->xColumn();
	QCOMPARE(xColumn->rowCount(), rowCount);
	for (int i = 0; i < rowCount; ++i)
		QCOMPARE(xColumn->valueAt(i), i +  1);
}

QTEST_MAIN(StatisticalPlotsTest)
