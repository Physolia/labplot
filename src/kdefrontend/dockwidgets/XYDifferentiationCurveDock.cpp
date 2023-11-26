/*
	File             : XYDifferentiationCurveDock.cpp
	Project          : LabPlot
	Description      : widget for editing properties of differentiation curves
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2016-2021 Stefan Gerlach <stefan.gerlach@uni.kn>
	SPDX-FileCopyrightText: 2017 Alexander Semke <alexander.semke@web.de>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "XYDifferentiationCurveDock.h"
#include "backend/worksheet/plots/cartesian/CartesianCoordinateSystem.h"
#include "backend/worksheet/plots/cartesian/XYDifferentiationCurve.h"
#include "commonfrontend/widgets/TreeViewComboBox.h"

#include <QStandardItemModel>

extern "C" {
#include "backend/nsl/nsl_diff.h"
}

/*!
  \class XYDifferentiationCurveDock
 \brief  Provides a widget for editing the properties of the XYDifferentiationCurves
		(2D-curves defined by a differentiation) currently selected in
		the project explorer.

  If more than one curves are set, the properties of the first column are shown.
  The changes of the properties are applied to all curves.
  The exclusions are the name, the comment and the datasets (columns) of
  the curves  - these properties can only be changed if there is only one single curve.

  \ingroup kdefrontend
*/

XYDifferentiationCurveDock::XYDifferentiationCurveDock(QWidget* parent)
	: XYAnalysisCurveDock(parent) {
}

/*!
 * 	// Tab "General"
 */
void XYDifferentiationCurveDock::setupGeneral() {
	auto* generalTab = new QWidget(ui.tabGeneral);
	uiGeneralTab.setupUi(generalTab);
	setPlotRangeCombobox(uiGeneralTab.cbPlotRanges);
	setBaseWidgets(uiGeneralTab.leName, uiGeneralTab.teComment, 1.2);

	auto* gridLayout = static_cast<QGridLayout*>(generalTab->layout());
	gridLayout->setContentsMargins(2, 2, 2, 2);
	gridLayout->setHorizontalSpacing(2);
	gridLayout->setVerticalSpacing(2);

	uiGeneralTab.cbDataSourceType->addItem(i18n("Spreadsheet"));
	uiGeneralTab.cbDataSourceType->addItem(i18n("XY-Curve"));

	cbDataSourceCurve = new TreeViewComboBox(generalTab);
	gridLayout->addWidget(cbDataSourceCurve, 5, 2, 1, 3);
	cbXDataColumn = new TreeViewComboBox(generalTab);
	gridLayout->addWidget(cbXDataColumn, 6, 2, 1, 3);
	cbYDataColumn = new TreeViewComboBox(generalTab);
	gridLayout->addWidget(cbYDataColumn, 7, 2, 1, 3);

	for (int i = 0; i < NSL_DIFF_DERIV_ORDER_COUNT; ++i)
		uiGeneralTab.cbDerivOrder->addItem(i18n(nsl_diff_deriv_order_name[i]));

	uiGeneralTab.leMin->setValidator(new QDoubleValidator(uiGeneralTab.leMin));
	uiGeneralTab.leMax->setValidator(new QDoubleValidator(uiGeneralTab.leMax));

	uiGeneralTab.pbRecalculate->setIcon(QIcon::fromTheme(QStringLiteral("run-build")));

	auto* layout = new QHBoxLayout(ui.tabGeneral);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(generalTab);

	// Slots
	connect(uiGeneralTab.chkVisible, &QCheckBox::clicked, this, &XYDifferentiationCurveDock::visibilityChanged);
	connect(uiGeneralTab.cbDataSourceType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XYDifferentiationCurveDock::dataSourceTypeChanged);
	connect(uiGeneralTab.cbAutoRange, &QCheckBox::clicked, this, &XYDifferentiationCurveDock::autoRangeChanged);
	connect(uiGeneralTab.leMin, &QLineEdit::textChanged, this, &XYDifferentiationCurveDock::xRangeMinChanged);
	connect(uiGeneralTab.leMax, &QLineEdit::textChanged, this, &XYDifferentiationCurveDock::xRangeMaxChanged);
	connect(uiGeneralTab.dateTimeEditMin, &UTCDateTimeEdit::mSecsSinceEpochUTCChanged, this, &XYDifferentiationCurveDock::xRangeMinDateTimeChanged);
	connect(uiGeneralTab.dateTimeEditMax, &UTCDateTimeEdit::mSecsSinceEpochUTCChanged, this, &XYDifferentiationCurveDock::xRangeMaxDateTimeChanged);
	connect(uiGeneralTab.cbDerivOrder, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XYDifferentiationCurveDock::derivOrderChanged);
	connect(uiGeneralTab.sbAccOrder, QOverload<int>::of(&QSpinBox::valueChanged), this, &XYDifferentiationCurveDock::accOrderChanged);
	connect(uiGeneralTab.cbPlotRanges, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &XYDifferentiationCurveDock::plotRangeChanged);
	connect(uiGeneralTab.pbRecalculate, &QPushButton::clicked, this, &XYDifferentiationCurveDock::recalculateClicked);

	connect(cbDataSourceCurve, &TreeViewComboBox::currentModelIndexChanged, this, &XYDifferentiationCurveDock::dataSourceCurveChanged);
	connect(cbXDataColumn, &TreeViewComboBox::currentModelIndexChanged, this, &XYDifferentiationCurveDock::xDataColumnChanged);
	connect(cbYDataColumn, &TreeViewComboBox::currentModelIndexChanged, this, &XYDifferentiationCurveDock::yDataColumnChanged);
}

void XYDifferentiationCurveDock::initGeneralTab() {
	// if there are more than one curve in the list, disable the tab "general"
	if (m_curvesList.size() == 1) {
		uiGeneralTab.lName->setEnabled(true);
		uiGeneralTab.leName->setEnabled(true);
		uiGeneralTab.lComment->setEnabled(true);
		uiGeneralTab.teComment->setEnabled(true);

		uiGeneralTab.leName->setText(m_curve->name());
		uiGeneralTab.teComment->setText(m_curve->comment());
	} else {
		uiGeneralTab.lName->setEnabled(false);
		uiGeneralTab.leName->setEnabled(false);
		uiGeneralTab.lComment->setEnabled(false);
		uiGeneralTab.teComment->setEnabled(false);

		uiGeneralTab.leName->setText(QString());
		uiGeneralTab.teComment->setText(QString());
	}

	// show the properties of the first curve
	// data source
	uiGeneralTab.cbDataSourceType->setCurrentIndex(static_cast<int>(m_differentiationCurve->dataSourceType()));
	this->dataSourceTypeChanged(uiGeneralTab.cbDataSourceType->currentIndex());
	cbDataSourceCurve->setAspect(m_differentiationCurve->dataSourceCurve());
	cbXDataColumn->setColumn(m_differentiationCurve->xDataColumn(), m_differentiationCurve->xDataColumnPath());
	cbYDataColumn->setColumn(m_differentiationCurve->yDataColumn(), m_differentiationCurve->yDataColumnPath());

	// range widgets
	const auto* plot = static_cast<const CartesianPlot*>(m_differentiationCurve->parentAspect());
	const int xIndex = plot->coordinateSystem(m_curve->coordinateSystemIndex())->index(CartesianCoordinateSystem::Dimension::X);
	m_dateTimeRange = (plot->xRangeFormat(xIndex) != RangeT::Format::Numeric);
	if (!m_dateTimeRange) {
		const auto numberLocale = QLocale();
		uiGeneralTab.leMin->setText(numberLocale.toString(m_differentiationData.xRange.first()));
		uiGeneralTab.leMax->setText(numberLocale.toString(m_differentiationData.xRange.last()));
	} else {
		uiGeneralTab.dateTimeEditMin->setMSecsSinceEpochUTC(m_differentiationData.xRange.first());
		uiGeneralTab.dateTimeEditMax->setMSecsSinceEpochUTC(m_differentiationData.xRange.last());
	}

	uiGeneralTab.lMin->setVisible(!m_dateTimeRange);
	uiGeneralTab.leMin->setVisible(!m_dateTimeRange);
	uiGeneralTab.lMax->setVisible(!m_dateTimeRange);
	uiGeneralTab.leMax->setVisible(!m_dateTimeRange);
	uiGeneralTab.lMinDateTime->setVisible(m_dateTimeRange);
	uiGeneralTab.dateTimeEditMin->setVisible(m_dateTimeRange);
	uiGeneralTab.lMaxDateTime->setVisible(m_dateTimeRange);
	uiGeneralTab.dateTimeEditMax->setVisible(m_dateTimeRange);

	// auto range
	uiGeneralTab.cbAutoRange->setChecked(m_differentiationData.autoRange);
	this->autoRangeChanged();

	// update list of selectable types
	xDataColumnChanged(cbXDataColumn->currentModelIndex());

	uiGeneralTab.cbDerivOrder->setCurrentIndex(m_differentiationData.derivOrder);
	this->derivOrderChanged(m_differentiationData.derivOrder);

	uiGeneralTab.sbAccOrder->setValue(m_differentiationData.accOrder);
	this->accOrderChanged(m_differentiationData.accOrder);

	this->showDifferentiationResult();

	uiGeneralTab.chkVisible->setChecked(m_curve->isVisible());

	// Slots
	connect(m_differentiationCurve, &XYDifferentiationCurve::dataSourceTypeChanged, this, &XYDifferentiationCurveDock::curveDataSourceTypeChanged);
	connect(m_differentiationCurve, &XYDifferentiationCurve::dataSourceCurveChanged, this, &XYDifferentiationCurveDock::curveDataSourceCurveChanged);
	connect(m_differentiationCurve, &XYDifferentiationCurve::xDataColumnChanged, this, &XYDifferentiationCurveDock::curveXDataColumnChanged);
	connect(m_differentiationCurve, &XYDifferentiationCurve::yDataColumnChanged, this, &XYDifferentiationCurveDock::curveYDataColumnChanged);
	connect(m_differentiationCurve, &XYDifferentiationCurve::differentiationDataChanged, this, &XYDifferentiationCurveDock::curveDifferentiationDataChanged);
	connect(m_differentiationCurve, &XYDifferentiationCurve::sourceDataChanged, this, &XYDifferentiationCurveDock::enableRecalculate);
	connect(m_differentiationCurve, &WorksheetElement::plotRangeListChanged, this, &XYDifferentiationCurveDock::updatePlotRanges);
	connect(m_differentiationCurve, &XYCurve::visibleChanged, this, &XYDifferentiationCurveDock::curveVisibilityChanged);
}

void XYDifferentiationCurveDock::setModel() {
	auto list = defaultColumnTopLevelClasses();
	list.append(AspectType::XYFitCurve);

	XYAnalysisCurveDock::setModel(list);
}

/*!
  sets the curves. The properties of the curves in the list \c list can be edited in this widget.
*/
void XYDifferentiationCurveDock::setCurves(QList<XYCurve*> list) {
	m_initializing = true;
	m_curvesList = list;
	m_curve = list.first();
	setAspects(list);
	m_differentiationCurve = static_cast<XYDifferentiationCurve*>(m_curve);
	this->setModel();
	m_differentiationData = m_differentiationCurve->differentiationData();

	initGeneralTab();
	initTabs();
	setSymbols(list);
	m_initializing = false;

	updatePlotRanges();

	// hide the "skip gaps" option after the curves were set
	ui.lLineSkipGaps->hide();
	ui.chkLineSkipGaps->hide();
}

void XYDifferentiationCurveDock::updatePlotRanges() {
	updatePlotRangeList();
}

//*************************************************************
//**** SLOTs for changes triggered in XYFitCurveDock *****
//*************************************************************
void XYDifferentiationCurveDock::dataSourceTypeChanged(int index) {
	const auto type = (XYAnalysisCurve::DataSourceType)index;
	if (type == XYAnalysisCurve::DataSourceType::Spreadsheet) {
		uiGeneralTab.lDataSourceCurve->hide();
		cbDataSourceCurve->hide();
		uiGeneralTab.lXColumn->show();
		cbXDataColumn->show();
		uiGeneralTab.lYColumn->show();
		cbYDataColumn->show();
	} else {
		uiGeneralTab.lDataSourceCurve->show();
		cbDataSourceCurve->show();
		uiGeneralTab.lXColumn->hide();
		cbXDataColumn->hide();
		uiGeneralTab.lYColumn->hide();
		cbYDataColumn->hide();
	}

	CONDITIONAL_LOCK_RETURN;

	for (auto* curve : m_curvesList)
		static_cast<XYDifferentiationCurve*>(curve)->setDataSourceType(type);
}

void XYDifferentiationCurveDock::dataSourceCurveChanged(const QModelIndex& index) {
	auto* dataSourceCurve = static_cast<XYCurve*>(index.internalPointer());

	// disable deriv orders and accuracies that need more data points
	this->updateSettings(dataSourceCurve->xColumn());

	CONDITIONAL_LOCK_RETURN;

	for (auto* curve : m_curvesList)
		static_cast<XYDifferentiationCurve*>(curve)->setDataSourceCurve(dataSourceCurve);
}

void XYDifferentiationCurveDock::xDataColumnChanged(const QModelIndex& index) {
	CONDITIONAL_LOCK_RETURN;

	auto* column = static_cast<AbstractColumn*>(index.internalPointer());

	// disable deriv orders and accuracies that need more data points
	this->updateSettings(column);

	for (auto* curve : m_curvesList)
		static_cast<XYDifferentiationCurve*>(curve)->setXDataColumn(column);

	cbXDataColumn->useCurrentIndexText(true);
	cbXDataColumn->setInvalid(false);
}

void XYDifferentiationCurveDock::yDataColumnChanged(const QModelIndex& index) {
	CONDITIONAL_LOCK_RETURN;

	auto* column = static_cast<AbstractColumn*>(index.internalPointer());

	for (auto* curve : m_curvesList)
		static_cast<XYDifferentiationCurve*>(curve)->setYDataColumn(column);

	cbYDataColumn->useCurrentIndexText(true);
	cbYDataColumn->setInvalid(false);
}

/*!
 * disable deriv orders and accuracies that need more data points
 */
void XYDifferentiationCurveDock::updateSettings(const AbstractColumn* column) {
	if (!column)
		return;

	if (uiGeneralTab.cbAutoRange->isChecked()) {
		const auto numberLocale = QLocale();
		uiGeneralTab.leMin->setText(numberLocale.toString(column->minimum()));
		uiGeneralTab.leMax->setText(numberLocale.toString(column->maximum()));
	}

	size_t n = 0;
	for (int row = 0; row < column->rowCount(); ++row)
		if (!std::isnan(column->valueAt(row)) && !column->isMasked(row))
			n++;

	const auto* model = qobject_cast<const QStandardItemModel*>(uiGeneralTab.cbDerivOrder->model());
	auto* item = model->item(nsl_diff_deriv_order_first);
	if (n < 3)
		item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
	else {
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		if (n < 5)
			uiGeneralTab.sbAccOrder->setMinimum(2);
	}

	item = model->item(nsl_diff_deriv_order_second);
	if (n < 3) {
		item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		if (uiGeneralTab.cbDerivOrder->currentIndex() == nsl_diff_deriv_order_second)
			uiGeneralTab.cbDerivOrder->setCurrentIndex(nsl_diff_deriv_order_first);
	} else {
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		if (n < 4)
			uiGeneralTab.sbAccOrder->setMinimum(1);
		else if (n < 5)
			uiGeneralTab.sbAccOrder->setMinimum(2);
	}

	item = model->item(nsl_diff_deriv_order_third);
	if (n < 5) {
		item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		if (uiGeneralTab.cbDerivOrder->currentIndex() == nsl_diff_deriv_order_third)
			uiGeneralTab.cbDerivOrder->setCurrentIndex(nsl_diff_deriv_order_first);
	} else
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

	item = model->item(nsl_diff_deriv_order_fourth);
	if (n < 5) {
		item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		if (uiGeneralTab.cbDerivOrder->currentIndex() == nsl_diff_deriv_order_fourth)
			uiGeneralTab.cbDerivOrder->setCurrentIndex(nsl_diff_deriv_order_first);
	} else {
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		if (n < 7)
			uiGeneralTab.sbAccOrder->setMinimum(1);
	}

	item = model->item(nsl_diff_deriv_order_fifth);
	if (n < 7) {
		item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		if (uiGeneralTab.cbDerivOrder->currentIndex() == nsl_diff_deriv_order_fifth)
			uiGeneralTab.cbDerivOrder->setCurrentIndex(nsl_diff_deriv_order_first);
	} else
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

	item = model->item(nsl_diff_deriv_order_sixth);
	if (n < 7) {
		item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
		if (uiGeneralTab.cbDerivOrder->currentIndex() == nsl_diff_deriv_order_sixth)
			uiGeneralTab.cbDerivOrder->setCurrentIndex(nsl_diff_deriv_order_first);
	} else
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}

void XYDifferentiationCurveDock::autoRangeChanged() {
	bool autoRange = uiGeneralTab.cbAutoRange->isChecked();
	m_differentiationData.autoRange = autoRange;

	uiGeneralTab.lMin->setEnabled(!autoRange);
	uiGeneralTab.leMin->setEnabled(!autoRange);
	uiGeneralTab.lMax->setEnabled(!autoRange);
	uiGeneralTab.leMax->setEnabled(!autoRange);
	uiGeneralTab.lMinDateTime->setEnabled(!autoRange);
	uiGeneralTab.dateTimeEditMin->setEnabled(!autoRange);
	uiGeneralTab.lMaxDateTime->setEnabled(!autoRange);
	uiGeneralTab.dateTimeEditMax->setEnabled(!autoRange);

	if (autoRange) {
		const AbstractColumn* xDataColumn = nullptr;
		if (m_differentiationCurve->dataSourceType() == XYAnalysisCurve::DataSourceType::Spreadsheet)
			xDataColumn = m_differentiationCurve->xDataColumn();
		else {
			if (m_differentiationCurve->dataSourceCurve())
				xDataColumn = m_differentiationCurve->dataSourceCurve()->xColumn();
		}

		if (xDataColumn) {
			if (!m_dateTimeRange) {
				const auto numberLocale = QLocale();
				uiGeneralTab.leMin->setText(numberLocale.toString(xDataColumn->minimum()));
				uiGeneralTab.leMax->setText(numberLocale.toString(xDataColumn->maximum()));
			} else {
				uiGeneralTab.dateTimeEditMin->setMSecsSinceEpochUTC(xDataColumn->minimum());
				uiGeneralTab.dateTimeEditMax->setMSecsSinceEpochUTC(xDataColumn->maximum());
			}
		}
	}
}

void XYDifferentiationCurveDock::xRangeMinChanged() {
	SET_DOUBLE_FROM_LE_REC(m_differentiationData.xRange.first(), uiGeneralTab.leMin);
}

void XYDifferentiationCurveDock::xRangeMaxChanged() {
	SET_DOUBLE_FROM_LE_REC(m_differentiationData.xRange.last(), uiGeneralTab.leMax);
}

void XYDifferentiationCurveDock::xRangeMinDateTimeChanged(qint64 value) {
	CONDITIONAL_LOCK_RETURN;

	m_differentiationData.xRange.first() = value;
	uiGeneralTab.pbRecalculate->setEnabled(true);
}

void XYDifferentiationCurveDock::xRangeMaxDateTimeChanged(qint64 value) {
	CONDITIONAL_LOCK_RETURN;

	m_differentiationData.xRange.last() = value;
	uiGeneralTab.pbRecalculate->setEnabled(true);
}

void XYDifferentiationCurveDock::derivOrderChanged(int index) {
	const auto derivOrder = (nsl_diff_deriv_order_type)index;
	m_differentiationData.derivOrder = derivOrder;

	// update avail. accuracies
	switch (derivOrder) {
	case nsl_diff_deriv_order_first:
		uiGeneralTab.sbAccOrder->setMinimum(2);
		uiGeneralTab.sbAccOrder->setMaximum(4);
		uiGeneralTab.sbAccOrder->setSingleStep(2);
		uiGeneralTab.sbAccOrder->setValue(4);
		break;
	case nsl_diff_deriv_order_second:
		uiGeneralTab.sbAccOrder->setMinimum(1);
		uiGeneralTab.sbAccOrder->setMaximum(3);
		uiGeneralTab.sbAccOrder->setSingleStep(1);
		uiGeneralTab.sbAccOrder->setValue(3);
		break;
	case nsl_diff_deriv_order_third:
		uiGeneralTab.sbAccOrder->setMinimum(2);
		uiGeneralTab.sbAccOrder->setMaximum(2);
		break;
	case nsl_diff_deriv_order_fourth:
		uiGeneralTab.sbAccOrder->setMinimum(1);
		uiGeneralTab.sbAccOrder->setMaximum(3);
		uiGeneralTab.sbAccOrder->setSingleStep(2);
		uiGeneralTab.sbAccOrder->setValue(3);
		break;
	case nsl_diff_deriv_order_fifth:
		uiGeneralTab.sbAccOrder->setMinimum(2);
		uiGeneralTab.sbAccOrder->setMaximum(2);
		break;
	case nsl_diff_deriv_order_sixth:
		uiGeneralTab.sbAccOrder->setMinimum(1);
		uiGeneralTab.sbAccOrder->setMaximum(1);
		break;
	}

	uiGeneralTab.pbRecalculate->setEnabled(true);
}

void XYDifferentiationCurveDock::accOrderChanged(int value) {
	m_differentiationData.accOrder = value;
	uiGeneralTab.pbRecalculate->setEnabled(true);
}

void XYDifferentiationCurveDock::recalculateClicked() {
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	for (auto* curve : m_curvesList)
		static_cast<XYDifferentiationCurve*>(curve)->setDifferentiationData(m_differentiationData);

	uiGeneralTab.pbRecalculate->setEnabled(false);
	Q_EMIT info(i18n("Differentiation status: %1", m_differentiationCurve->differentiationResult().status));
	QApplication::restoreOverrideCursor();
}

void XYDifferentiationCurveDock::enableRecalculate() const {
	CONDITIONAL_RETURN_NO_LOCK;

	// no differentiation possible without the x- and y-data
	bool hasSourceData = false;
	if (m_differentiationCurve->dataSourceType() == XYAnalysisCurve::DataSourceType::Spreadsheet) {
		AbstractAspect* aspectX = static_cast<AbstractAspect*>(cbXDataColumn->currentModelIndex().internalPointer());
		AbstractAspect* aspectY = static_cast<AbstractAspect*>(cbYDataColumn->currentModelIndex().internalPointer());
		hasSourceData = (aspectX && aspectY);
		if (aspectX) {
			cbXDataColumn->useCurrentIndexText(true);
			cbXDataColumn->setInvalid(false);
		}
		if (aspectY) {
			cbYDataColumn->useCurrentIndexText(true);
			cbYDataColumn->setInvalid(false);
		}
	} else {
		hasSourceData = (m_differentiationCurve->dataSourceCurve() != nullptr);
	}

	uiGeneralTab.pbRecalculate->setEnabled(hasSourceData);
}

/*!
 * show the result and details of the differentiation
 */
void XYDifferentiationCurveDock::showDifferentiationResult() {
	showResult(m_differentiationCurve, uiGeneralTab.teResult, uiGeneralTab.pbRecalculate);
}

//*************************************************************
//*** SLOTs for changes triggered in XYDifferentiationCurve ***
//*************************************************************
// General-Tab
void XYDifferentiationCurveDock::curveDataSourceTypeChanged(XYAnalysisCurve::DataSourceType type) {
	CONDITIONAL_LOCK_RETURN;
	uiGeneralTab.cbDataSourceType->setCurrentIndex(static_cast<int>(type));
}

void XYDifferentiationCurveDock::curveDataSourceCurveChanged(const XYCurve* curve) {
	CONDITIONAL_LOCK_RETURN;
	cbDataSourceCurve->setAspect(curve);
}

void XYDifferentiationCurveDock::curveXDataColumnChanged(const AbstractColumn* column) {
	CONDITIONAL_LOCK_RETURN;
	cbXDataColumn->setColumn(column, m_differentiationCurve->xDataColumnPath());
}

void XYDifferentiationCurveDock::curveYDataColumnChanged(const AbstractColumn* column) {
	CONDITIONAL_LOCK_RETURN;
	cbYDataColumn->setColumn(column, m_differentiationCurve->yDataColumnPath());
}

void XYDifferentiationCurveDock::curveDifferentiationDataChanged(const XYDifferentiationCurve::DifferentiationData& differentiationData) {
	CONDITIONAL_LOCK_RETURN;
	m_differentiationData = differentiationData;
	uiGeneralTab.cbDerivOrder->setCurrentIndex(m_differentiationData.derivOrder);
	this->derivOrderChanged(m_differentiationData.derivOrder);
	uiGeneralTab.sbAccOrder->setValue(m_differentiationData.accOrder);
	this->accOrderChanged(m_differentiationData.accOrder);

	this->showDifferentiationResult();
}

void XYDifferentiationCurveDock::dataChanged() {
	this->enableRecalculate();
}

void XYDifferentiationCurveDock::curveVisibilityChanged(bool on) {
	CONDITIONAL_LOCK_RETURN;
	uiGeneralTab.chkVisible->setChecked(on);
}
