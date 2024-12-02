/*
 File                 : CommonAnalysisTest.cpp
 Project              : LabPlot
 Description          : Tests for common analysis tasks
 --------------------------------------------------------------------
 SPDX-FileCopyrightText: 2024 Martin Marmsoler <martin.marmsoler@gmail.com>
 SPDX-FileCopyrightText: 2024 Alexander Semke <alexander.semke@web.de>

 SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "CommonAnalysisTest.h"
#include "backend/core/Project.h"
#include "backend/core/column/Column.h"
#include "backend/datasources/filters/AsciiFilter.h"
#include "backend/spreadsheet/Spreadsheet.h"
#include "backend/worksheet/Worksheet.h"
#include "backend/worksheet/plots/cartesian/Histogram.h"
#include "backend/worksheet/plots/cartesian/XYIntegrationCurve.h"
#include "backend/worksheet/plots/cartesian/XYFitCurve.h"

/*!
 * test save and restore of the columns used as the data source in the analysis curve.
 * the logic is centrally handled in Project and in XYAnalysisCurve, it's enough to test one analysis curve type only.
 */
void CommonAnalysisTest::saveRestoreSourceColumns() {
	QString savePath;

	// save
	{
		Project project;
		auto* ws = new Worksheet(QStringLiteral("Worksheet"));
		project.addChild(ws);

		auto* plot = new CartesianPlot(QStringLiteral("plot"));
		ws->addChild(plot);

		auto* sheet = new Spreadsheet(QStringLiteral("sheet"));
		project.addChild(sheet);
		sheet->setColumnCount(3);
		sheet->column(0)->setName(QStringLiteral("x"));
		sheet->column(1)->setName(QStringLiteral("y"));
		sheet->column(2)->setName(QStringLiteral("y2"));

		auto* fitCurve = new XYFitCurve(QStringLiteral("fit"));
		plot->addChild(fitCurve);
		fitCurve->setDataSourceType(XYAnalysisCurve::DataSourceType::Spreadsheet);
		fitCurve->setXDataColumn(sheet->column(0));
		fitCurve->setYDataColumn(sheet->column(1));
		fitCurve->setY2DataColumn(sheet->column(2));

		SAVE_PROJECT("saveRestoreSourceColumns");
	}

	// restore
	{
		Project project;
		QCOMPARE(project.load(savePath), true);

		const auto* ws = project.child<Worksheet>(0);
		QVERIFY(ws);
		const auto* plot = ws->child<CartesianPlot>(0);
		QVERIFY(plot);
		const auto* fitCurve = plot->child<XYFitCurve>(0);
		QVERIFY(fitCurve);

		const auto* xColumn = fitCurve->xDataColumn();
		const auto* yColumn = fitCurve->yDataColumn();
		const auto* y2Column = fitCurve->y2DataColumn();
		QVERIFY(xColumn);
		QCOMPARE(xColumn->name(), QStringLiteral("x"));
		QVERIFY(yColumn);
		QCOMPARE(yColumn->name(), QStringLiteral("y"));
		QVERIFY(y2Column);
		QCOMPARE(y2Column->name(), QStringLiteral("y2"));
	}
}

/*!
 * test save and restore of the XYCRuve used as the data source in the analysis curve.
 * the logic is centrally handled in Project and in XYAnalysisCurve, it's enough to test one analysis curve type only.
 */
void CommonAnalysisTest::saveRestoreSourceCurve() {
	QString savePath;

	// save
	{
		Project project;
		auto* ws = new Worksheet(QStringLiteral("Worksheet"));
		project.addChild(ws);

		auto* plot = new CartesianPlot(QStringLiteral("plot"));
		ws->addChild(plot);

		auto* curve = new XYCurve(QStringLiteral("curve"));
		plot->addChild(curve);

		auto* fitCurve = new XYFitCurve(QStringLiteral("fit"));
		plot->addChild(fitCurve);
		fitCurve->setDataSourceType(XYAnalysisCurve::DataSourceType::Curve);
		fitCurve->setDataSourceCurve(curve);

		SAVE_PROJECT("saveRestoreSourceColumns");
	}

	// restore
	{
		Project project;
		QCOMPARE(project.load(savePath), true);

		const auto* ws = project.child<Worksheet>(0);
		QVERIFY(ws);
		const auto* plot = ws->child<CartesianPlot>(0);
		QVERIFY(plot);
		const auto* curve = plot->child<XYCurve>(0);
		QVERIFY(curve);
		const auto* fitCurve = plot->child<XYFitCurve>(0);
		QVERIFY(fitCurve);
		QCOMPARE(fitCurve->dataSourceCurve(), curve);
	}
}

/*!
 * test save and restore of the histogram used as the data source in the fit curve.
 */
void CommonAnalysisTest::saveRestoreSourceHistogram() {
	QString savePath;

	// save
	{
		Project project;
		auto* ws = new Worksheet(QStringLiteral("Worksheet"));
		project.addChild(ws);

		auto* plot = new CartesianPlot(QStringLiteral("plot"));
		ws->addChild(plot);

		auto* hist = new Histogram(QStringLiteral("hist"));
		plot->addChild(hist);

		auto* fitCurve = new XYFitCurve(QStringLiteral("fit"));
		plot->addChild(fitCurve);
		fitCurve->setDataSourceType(XYAnalysisCurve::DataSourceType::Histogram);
		fitCurve->setDataSourceHistogram(hist);

		SAVE_PROJECT("saveRestoreSourceColumns");
	}

	// restore
	{
		Project project;
		QCOMPARE(project.load(savePath), true);

		const auto* ws = project.child<Worksheet>(0);
		QVERIFY(ws);
		const auto* plot = ws->child<CartesianPlot>(0);
		QVERIFY(plot);
		const auto* hist = plot->child<Histogram>(0);
		QVERIFY(hist);
		const auto* fitCurve = plot->child<XYFitCurve>(0);
		QVERIFY(fitCurve);
		QCOMPARE(fitCurve->dataSourceHistogram(), hist);
	}
}

void CommonAnalysisTest::dataImportRecalculationAnalysisCurveColumnDependency() {
	Project project;
	auto* ws = new Worksheet(QStringLiteral("Worksheet"));
	QVERIFY(ws != nullptr);
	project.addChild(ws);

	auto* p = new CartesianPlot(QStringLiteral("plot"));
	p->setType(CartesianPlot::Type::TwoAxes); // Otherwise no axis are created
	QVERIFY(p != nullptr);
	ws->addChild(p);

	// Generate data and
	Spreadsheet* sheet = new Spreadsheet(QStringLiteral("Spreadsheet"), false);
	project.addChild(sheet);
	sheet->setColumnCount(2);
	sheet->setRowCount(11);
	sheet->column(0)->setColumnMode(AbstractColumn::ColumnMode::Double);
	sheet->column(1)->setColumnMode(AbstractColumn::ColumnMode::Double);

	{
		QVector<double> xData = {1, 2, 3, 4};
		QVector<double> yData = {1., 2., 3., 4.};
		sheet->column(0)->replaceValues(0, xData);
		sheet->column(1)->replaceValues(0, yData);
	}

	QCOMPARE(sheet->column(0)->name(), QStringLiteral("1"));
	QCOMPARE(sheet->column(1)->name(), QStringLiteral("2"));

	p->addChild(new XYIntegrationCurve(QLatin1String("eq2")));
	auto integrationCurves = p->children(AspectType::XYIntegrationCurve);
	QCOMPARE(integrationCurves.count(), 1);
	auto* integrationCurve = static_cast<XYIntegrationCurve*>(integrationCurves.at(0));

	integrationCurve->setXDataColumn(sheet->column(0));
	integrationCurve->setYDataColumn(sheet->column(1));

	// prepare the integration
	XYIntegrationCurve::IntegrationData integrationData = integrationCurve->integrationData();
	integrationCurve->setIntegrationData(integrationData);

	// perform the integration
	integrationCurve->recalculate();
	const XYIntegrationCurve::IntegrationResult& integrationResult = integrationCurve->integrationResult();

	// check the results
	QCOMPARE(integrationResult.available, true);
	QCOMPARE(integrationResult.valid, true);

	const AbstractColumn* resultXDataColumn = integrationCurve->xColumn();
	const AbstractColumn* resultYDataColumn = integrationCurve->yColumn();

	const int np = resultXDataColumn->rowCount();
	QCOMPARE(np, 4);

	for (int i = 0; i < np; i++)
		QCOMPARE(resultXDataColumn->valueAt(i), (double)i + 1);

	VALUES_EQUAL(resultYDataColumn->valueAt(0), 0.);
	VALUES_EQUAL(resultYDataColumn->valueAt(1), 1.5);
	VALUES_EQUAL(resultYDataColumn->valueAt(2), 4.);
	VALUES_EQUAL(resultYDataColumn->valueAt(3), 7.5);

	QStringList fileContent = {
		QStringLiteral("5,8"),
		QStringLiteral("6,10"),
		QStringLiteral("7,12"),
		QStringLiteral("8,14"),
		QStringLiteral("9,15"),
	};
	QString savePath;
	SAVE_FILE("dataImportRecalculationAnalysisCurveColumnDependency", fileContent);

	AsciiFilter filter;
	auto properties = filter.properties();
	properties.headerEnabled = false;
	filter.setProperties(properties);
	filter.readDataFromFile(savePath, sheet, AbstractFileFilter::ImportMode::Replace);

	{
		const auto* xColumn = integrationCurve->xColumn();
		const auto* yColumn = integrationCurve->yColumn();
		QCOMPARE(xColumn->rowCount(), 5);
		QCOMPARE(yColumn->rowCount(), 5);
		VALUES_EQUAL(xColumn->valueAt(0), 5.);
		VALUES_EQUAL(xColumn->valueAt(1), 6.);
		VALUES_EQUAL(xColumn->valueAt(2), 7.);
		VALUES_EQUAL(xColumn->valueAt(3), 8.);
		VALUES_EQUAL(xColumn->valueAt(4), 9.);

		VALUES_EQUAL(yColumn->valueAt(0), 0.);
		VALUES_EQUAL(yColumn->valueAt(1), 9.);
		VALUES_EQUAL(yColumn->valueAt(2), yColumn->valueAt(1) + 11);
		VALUES_EQUAL(yColumn->valueAt(3), yColumn->valueAt(2) + 13.);
		VALUES_EQUAL(yColumn->valueAt(4), yColumn->valueAt(3) + 14.5);
	}
}

QTEST_MAIN(CommonAnalysisTest)
