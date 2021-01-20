/***************************************************************************
    File                 : CartesianPlot.cpp
    Project              : LabPlot
    Description          : Cartesian plot
    --------------------------------------------------------------------
    Copyright            : (C) 2011-2021 by Alexander Semke (alexander.semke@web.de)
    Copyright            : (C) 2016-2021 by Stefan Gerlach (stefan.gerlach@uni.kn)
    Copyright            : (C) 2017-2018 by Garvit Khatri (garvitdelhi@gmail.com)

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

#include "CartesianPlot.h"
#include "CartesianPlotPrivate.h"
#include "CartesianPlotLegend.h"
#include "Axis.h"
#include "XYCurve.h"
#include "Histogram.h"
#include "CustomPoint.h"
#include "ReferenceLine.h"
#include "XYEquationCurve.h"
#include "XYDataReductionCurve.h"
#include "XYDifferentiationCurve.h"
#include "XYIntegrationCurve.h"
#include "XYInterpolationCurve.h"
#include "XYSmoothCurve.h"
#include "XYFitCurve.h"
#include "XYFourierFilterCurve.h"
#include "XYFourierTransformCurve.h"
#include "XYConvolutionCurve.h"
#include "XYCorrelationCurve.h"
#include "../PlotArea.h"
#include "../AbstractPlotPrivate.h"
#include "backend/core/Project.h"
#include "backend/core/datatypes/DateTime2StringFilter.h"
#include "backend/spreadsheet/Spreadsheet.h"
#include "backend/worksheet/plots/cartesian/BoxPlot.h"
#include "backend/worksheet/plots/cartesian/CartesianPlotLegend.h"
#include "backend/worksheet/plots/cartesian/CustomPoint.h"
#include "backend/worksheet/plots/cartesian/ReferenceLine.h"
#include "backend/worksheet/plots/PlotArea.h"
#include "backend/worksheet/plots/AbstractPlotPrivate.h"
#include "backend/worksheet/Worksheet.h"
#include "backend/worksheet/plots/cartesian/Axis.h"
#include "backend/worksheet/Image.h"
#include "backend/worksheet/TextLabel.h"
#include "backend/worksheet/InfoElement.h"
#include "backend/lib/XmlStreamReader.h"
#include "backend/lib/commandtemplates.h"
#include "backend/lib/macros.h"
#include "backend/lib/trace.h"
#include "kdefrontend/spreadsheet/PlotDataDialog.h" //for PlotDataDialog::AnalysisAction. TODO: find a better place for this enum.
#include "kdefrontend/ThemeHandler.h"
#include "kdefrontend/widgets/ThemesWidget.h"

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>

#include <QDir>
#include <QDropEvent>
#include <QIcon>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QWidgetAction>

#include <array>
#include <cmath>

/**
 * \class CartesianPlot
 * \brief A xy-plot.
 *
 *
 */
CartesianPlot::CartesianPlot(const QString &name)
	: AbstractPlot(name, new CartesianPlotPrivate(this), AspectType::CartesianPlot) {

	init();
}

CartesianPlot::CartesianPlot(const QString &name, CartesianPlotPrivate *dd)
	: AbstractPlot(name, dd, AspectType::CartesianPlot) {

	init();
}

CartesianPlot::~CartesianPlot() {
	if (m_menusInitialized) {
		delete addNewMenu;
		delete zoomMenu;
		delete themeMenu;
	}

	while (!m_coordinateSystems.isEmpty())
		delete m_coordinateSystems.takeFirst();

	//no need to delete objects added with addChild()

	//no need to delete the d-pointer here - it inherits from QGraphicsItem
	//and is deleted during the cleanup in QGraphicsScene
}

/*!
	initializes all member variables of \c CartesianPlot
*/
void CartesianPlot::init() {
	Q_D(CartesianPlot);

	m_coordinateSystems.append( new CartesianCoordinateSystem(this) );
	// TEST: second cSystem for testing
	//d->coordinateSystems.append( new CartesianCoordinateSystem(this) );
	//m_coordinateSystems.append( d->coordinateSystems.at(1) );
	// TEST: set x range to second x range
	//d->coordinateSystems[1]->setXIndex(1);
	// TEST: second x/y range
	//d->xRanges.append(Range<double>(0, 2));
	//d->yRanges.append(Range<double>(0, 2));

	m_plotArea = new PlotArea(name() + " plot area", this);
	addChildFast(m_plotArea);

	//Plot title
	m_title = new TextLabel(this->name() + QLatin1String("- ") + i18n("Title"), TextLabel::Type::PlotTitle);
	addChild(m_title);
	m_title->setHidden(true);
	m_title->setParentGraphicsItem(m_plotArea->graphicsItem());

	//offset between the plot area and the area defining the coordinate system, in scene units.
	d->horizontalPadding = Worksheet::convertToSceneUnits(1.5, Worksheet::Unit::Centimeter);
	d->verticalPadding = Worksheet::convertToSceneUnits(1.5, Worksheet::Unit::Centimeter);
	d->rightPadding = Worksheet::convertToSceneUnits(1.5, Worksheet::Unit::Centimeter);
	d->bottomPadding = Worksheet::convertToSceneUnits(1.5, Worksheet::Unit::Centimeter);
	d->symmetricPadding = true;

	connect(this, &AbstractAspect::aspectAdded, this, &CartesianPlot::childAdded);
	connect(this, &AbstractAspect::aspectRemoved, this, &CartesianPlot::childRemoved);

	graphicsItem()->setFlag(QGraphicsItem::ItemIsMovable, true);
	graphicsItem()->setFlag(QGraphicsItem::ItemClipsChildrenToShape, true);
	graphicsItem()->setFlag(QGraphicsItem::ItemIsSelectable, true);
	graphicsItem()->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
	graphicsItem()->setFlag(QGraphicsItem::ItemIsFocusable, true);

	//theme is not set at this point, initialize the color palette with default colors
	this->setColorPalette(KConfig());
}

/*!
	initializes all children of \c CartesianPlot and
	setups a default plot of type \c type with a plot title.
*/
void CartesianPlot::setType(Type type) {
	Q_D(CartesianPlot);

	d->type = type;

	switch (type) {
	case Type::FourAxes: {
			//Axes
			Axis* axis = new Axis("x axis 1", Axis::Orientation::Horizontal);
			axis->setDefault(true);
			axis->setSuppressRetransform(true);
			addChild(axis);
			axis->setPosition(Axis::Position::Bottom);
			axis->setRange(0., 1.);
			axis->setMajorTicksDirection(Axis::ticksIn);
			axis->setMajorTicksNumber(6);
			axis->setMinorTicksDirection(Axis::ticksIn);
			axis->setMinorTicksNumber(1);
			axis->setSuppressRetransform(false);

			axis = new Axis("x axis 2", Axis::Orientation::Horizontal);
			//TEST: use second cSystem of plot
			//axis->setCoordinateSystemIndex(1);
			axis->setDefault(true);
			axis->setSuppressRetransform(true);

			addChild(axis);
			axis->setPosition(Axis::Position::Top);
			axis->setRange(0., 1.);
			axis->setMajorTicksDirection(Axis::ticksIn);
			axis->setMajorTicksNumber(6);
			axis->setMinorTicksDirection(Axis::ticksIn);
			axis->setMinorTicksNumber(1);
			QPen pen = axis->minorGridPen();
			pen.setStyle(Qt::NoPen);
			axis->setMajorGridPen(pen);
			pen = axis->minorGridPen();
			pen.setStyle(Qt::NoPen);
			axis->setMinorGridPen(pen);
			axis->setLabelsPosition(Axis::LabelsPosition::NoLabels);
			axis->title()->setText(QString());
			axis->setSuppressRetransform(false);

			axis = new Axis("y axis 1", Axis::Orientation::Vertical);
			axis->setDefault(true);
			axis->setSuppressRetransform(true);
			addChild(axis);
			axis->setPosition(Axis::Position::Left);
			axis->setRange(0., 1.);
			axis->setMajorTicksDirection(Axis::ticksIn);
			axis->setMajorTicksNumber(6);
			axis->setMinorTicksDirection(Axis::ticksIn);
			axis->setMinorTicksNumber(1);
			axis->setSuppressRetransform(false);

			axis = new Axis("y axis 2", Axis::Orientation::Vertical);
			axis->setDefault(true);
			axis->setSuppressRetransform(true);
			addChild(axis);
			axis->setPosition(Axis::Position::Right);
			axis->setRange(0., 1.);
			axis->setOffset(1);
			axis->setMajorTicksDirection(Axis::ticksIn);
			axis->setMajorTicksNumber(6);
			axis->setMinorTicksDirection(Axis::ticksIn);
			axis->setMinorTicksNumber(1);
			pen = axis->minorGridPen();
			pen.setStyle(Qt::NoPen);
			axis->setMajorGridPen(pen);
			pen = axis->minorGridPen();
			pen.setStyle(Qt::NoPen);
			axis->setMinorGridPen(pen);
			axis->setLabelsPosition(Axis::LabelsPosition::NoLabels);
			axis->title()->setText(QString());
			axis->setSuppressRetransform(false);

			break;
		}
	case Type::TwoAxes: {
			Axis* axis = new Axis("x axis 1", Axis::Orientation::Horizontal);
			axis->setDefault(true);
			axis->setSuppressRetransform(true);
			addChild(axis);
			axis->setPosition(Axis::Position::Bottom);
			axis->setRange(0., 1.);
			axis->setMajorTicksDirection(Axis::ticksBoth);
			axis->setMajorTicksNumber(6);
			axis->setMinorTicksDirection(Axis::ticksBoth);
			axis->setMinorTicksNumber(1);
			axis->setArrowType(Axis::ArrowType::FilledSmall);
			axis->setSuppressRetransform(false);

			axis = new Axis("y axis 1", Axis::Orientation::Vertical);
			axis->setDefault(true);
			axis->setSuppressRetransform(true);
			addChild(axis);
			axis->setPosition(Axis::Position::Left);
			axis->setRange(0., 1.);
			axis->setMajorTicksDirection(Axis::ticksBoth);
			axis->setMajorTicksNumber(6);
			axis->setMinorTicksDirection(Axis::ticksBoth);
			axis->setMinorTicksNumber(1);
			axis->setArrowType(Axis::ArrowType::FilledSmall);
			axis->setSuppressRetransform(false);

			break;
		}
	case Type::TwoAxesCentered: {
			d->xRanges[0].setRange(-0.5, 0.5);
			d->yRanges[0].setRange(-0.5, 0.5);

			d->horizontalPadding = Worksheet::convertToSceneUnits(1.0, Worksheet::Unit::Centimeter);
			d->verticalPadding = Worksheet::convertToSceneUnits(1.0, Worksheet::Unit::Centimeter);

			QPen pen = m_plotArea->borderPen();
			pen.setStyle(Qt::NoPen);
			m_plotArea->setBorderPen(pen);

			Axis* axis = new Axis("x axis 1", Axis::Orientation::Horizontal);
			axis->setDefault(true);
			axis->setSuppressRetransform(true);
			addChild(axis);
			axis->setPosition(Axis::Position::Centered);
			axis->setRange(-0.5, 0.5);
			axis->setMajorTicksDirection(Axis::ticksBoth);
			axis->setMajorTicksNumber(6);
			axis->setMinorTicksDirection(Axis::ticksBoth);
			axis->setMinorTicksNumber(1);
			axis->setArrowType(Axis::ArrowType::FilledSmall);
			axis->title()->setText(QString());
			axis->setSuppressRetransform(false);

			axis = new Axis("y axis 1", Axis::Orientation::Vertical);
			axis->setDefault(true);
			axis->setSuppressRetransform(true);
			addChild(axis);
			axis->setPosition(Axis::Position::Centered);
			axis->setRange(-0.5, 0.5);
			axis->setMajorTicksDirection(Axis::ticksBoth);
			axis->setMajorTicksNumber(6);
			axis->setMinorTicksDirection(Axis::ticksBoth);
			axis->setMinorTicksNumber(1);
			axis->setArrowType(Axis::ArrowType::FilledSmall);
			axis->title()->setText(QString());
			axis->setSuppressRetransform(false);

			break;
		}
	case Type::TwoAxesCenteredZero: {
			d->xRanges[0].setRange(-0.5, 0.5);
			d->yRanges[0].setRange(-0.5, 0.5);

			d->horizontalPadding = Worksheet::convertToSceneUnits(1.0, Worksheet::Unit::Centimeter);
			d->verticalPadding = Worksheet::convertToSceneUnits(1.0, Worksheet::Unit::Centimeter);

			QPen pen = m_plotArea->borderPen();
			pen.setStyle(Qt::NoPen);
			m_plotArea->setBorderPen(pen);

			Axis* axis = new Axis("x axis 1", Axis::Orientation::Horizontal);
			axis->setDefault(true);
			axis->setSuppressRetransform(true);
			addChild(axis);
			axis->setPosition(Axis::Position::Custom);
			axis->setOffset(0);
			axis->setRange(-0.5, 0.5);
			axis->setMajorTicksDirection(Axis::ticksBoth);
			axis->setMajorTicksNumber(6);
			axis->setMinorTicksDirection(Axis::ticksBoth);
			axis->setMinorTicksNumber(1);
			axis->setArrowType(Axis::ArrowType::FilledSmall);
			axis->title()->setText(QString());
			axis->setSuppressRetransform(false);

			axis = new Axis("y axis 1", Axis::Orientation::Vertical);
			axis->setDefault(true);
			axis->setSuppressRetransform(true);
			addChild(axis);
			axis->setPosition(Axis::Position::Custom);
			axis->setOffset(0);
			axis->setRange(-0.5, 0.5);
			axis->setMajorTicksDirection(Axis::ticksBoth);
			axis->setMajorTicksNumber(6);
			axis->setMinorTicksDirection(Axis::ticksBoth);
			axis->setMinorTicksNumber(1);
			axis->setArrowType(Axis::ArrowType::FilledSmall);
			axis->title()->setText(QString());
			axis->setSuppressRetransform(false);

			break;
		}
	}

	d->xPrevRange = xRange();
	d->yPrevRange = yRange();

	//Geometry, specify the plot rect in scene coordinates.
	//TODO: Use default settings for left, top, width, height and for min/max for the coordinate system
	double x = Worksheet::convertToSceneUnits(2, Worksheet::Unit::Centimeter);
	double y = Worksheet::convertToSceneUnits(2, Worksheet::Unit::Centimeter);
	double w = Worksheet::convertToSceneUnits(10, Worksheet::Unit::Centimeter);
	double h = Worksheet::convertToSceneUnits(10, Worksheet::Unit::Centimeter);

	//all plot children are initialized -> set the geometry of the plot in scene coordinates.
	d->rect = QRectF(x, y, w, h);
	d->retransform();
}

CartesianPlot::Type CartesianPlot::type() const {
	Q_D(const CartesianPlot);
	return d->type;
}

void CartesianPlot::initActions() {
	//"add new" actions
	addCurveAction = new QAction(QIcon::fromTheme("labplot-xy-curve"), i18n("xy-curve"), this);
	addHistogramAction = new QAction(QIcon::fromTheme("view-object-histogram-linear"), i18n("Histogram"), this);
	addBoxPlotAction = new QAction(QIcon::fromTheme("view-object-histogram-linear"), i18n("Box Plot"), this);
	addEquationCurveAction = new QAction(QIcon::fromTheme("labplot-xy-equation-curve"), i18n("xy-curve from a mathematical Equation"), this);
// no icons yet
	addDataReductionCurveAction = new QAction(QIcon::fromTheme("labplot-xy-curve"), i18n("Data Reduction"), this);
	addDifferentiationCurveAction = new QAction(QIcon::fromTheme("labplot-xy-curve"), i18n("Differentiation"), this);
	addIntegrationCurveAction = new QAction(QIcon::fromTheme("labplot-xy-curve"), i18n("Integration"), this);
	addInterpolationCurveAction = new QAction(QIcon::fromTheme("labplot-xy-interpolation-curve"), i18n("Interpolation"), this);
	addSmoothCurveAction = new QAction(QIcon::fromTheme("labplot-xy-smoothing-curve"), i18n("Smooth"), this);
	addFitCurveAction = new QAction(QIcon::fromTheme("labplot-xy-fit-curve"), i18n("Fit"), this);
	addFourierFilterCurveAction = new QAction(QIcon::fromTheme("labplot-xy-fourier-filter-curve"), i18n("Fourier Filter"), this);
	addFourierTransformCurveAction = new QAction(QIcon::fromTheme("labplot-xy-fourier-transform-curve"), i18n("Fourier Transform"), this);
	addConvolutionCurveAction = new QAction(QIcon::fromTheme("labplot-xy-curve"),i18n("(De-)Convolution"), this);
	addCorrelationCurveAction = new QAction(QIcon::fromTheme("labplot-xy-curve"),i18n("Auto-/Cross-Correlation"), this);

	addLegendAction = new QAction(QIcon::fromTheme("text-field"), i18n("Legend"), this);
	if (children<CartesianPlotLegend>().size() > 0)
		addLegendAction->setEnabled(false);	//only one legend is allowed -> disable the action

	addHorizontalAxisAction = new QAction(QIcon::fromTheme("labplot-axis-horizontal"), i18n("Horizontal Axis"), this);
	addVerticalAxisAction = new QAction(QIcon::fromTheme("labplot-axis-vertical"), i18n("Vertical Axis"), this);
	addTextLabelAction = new QAction(QIcon::fromTheme("draw-text"), i18n("Text Label"), this);
	addImageAction = new QAction(QIcon::fromTheme("viewimage"), i18n("Image"), this);
	addInfoElementAction = new QAction(QIcon::fromTheme("draw-text"), i18n("Info Element"), this);
	addCustomPointAction = new QAction(QIcon::fromTheme("draw-cross"), i18n("Custom Point"), this);
	addReferenceLineAction = new QAction(QIcon::fromTheme("draw-line"), i18n("Reference Line"), this);

	connect(addCurveAction, &QAction::triggered, this, &CartesianPlot::addCurve);
	connect(addHistogramAction,&QAction::triggered, this, &CartesianPlot::addHistogram);
	connect(addBoxPlotAction,&QAction::triggered, this, &CartesianPlot::addBoxPlot);
	connect(addEquationCurveAction, &QAction::triggered, this, &CartesianPlot::addEquationCurve);
	connect(addDataReductionCurveAction, &QAction::triggered, this, &CartesianPlot::addDataReductionCurve);
	connect(addDifferentiationCurveAction, &QAction::triggered, this, &CartesianPlot::addDifferentiationCurve);
	connect(addIntegrationCurveAction, &QAction::triggered, this, &CartesianPlot::addIntegrationCurve);
	connect(addInterpolationCurveAction, &QAction::triggered, this, &CartesianPlot::addInterpolationCurve);
	connect(addSmoothCurveAction, &QAction::triggered, this, &CartesianPlot::addSmoothCurve);
	connect(addFitCurveAction, &QAction::triggered, this, &CartesianPlot::addFitCurve);
	connect(addFourierFilterCurveAction, &QAction::triggered, this, &CartesianPlot::addFourierFilterCurve);
	connect(addFourierTransformCurveAction, &QAction::triggered, this, &CartesianPlot::addFourierTransformCurve);
	connect(addConvolutionCurveAction, &QAction::triggered, this, &CartesianPlot::addConvolutionCurve);
	connect(addCorrelationCurveAction, &QAction::triggered, this, &CartesianPlot::addCorrelationCurve);

	connect(addLegendAction, &QAction::triggered, this, static_cast<void (CartesianPlot::*)()>(&CartesianPlot::addLegend));
	connect(addHorizontalAxisAction, &QAction::triggered, this, &CartesianPlot::addHorizontalAxis);
	connect(addVerticalAxisAction, &QAction::triggered, this, &CartesianPlot::addVerticalAxis);
	connect(addTextLabelAction, &QAction::triggered, this, &CartesianPlot::addTextLabel);
	connect(addImageAction, &QAction::triggered, this, &CartesianPlot::addImage);
	connect(addInfoElementAction, &QAction::triggered, this, &CartesianPlot::addInfoElement);
	connect(addCustomPointAction, &QAction::triggered, this, &CartesianPlot::addCustomPoint);
	connect(addReferenceLineAction, &QAction::triggered, this, &CartesianPlot::addReferenceLine);

	//Analysis menu actions
// 	addDataOperationAction = new QAction(i18n("Data Operation"), this);
	addDataReductionAction = new QAction(QIcon::fromTheme("labplot-xy-curve"), i18n("Data Reduction"), this);
	addDifferentiationAction = new QAction(QIcon::fromTheme("labplot-xy-curve"), i18n("Differentiate"), this);
	addIntegrationAction = new QAction(QIcon::fromTheme("labplot-xy-curve"), i18n("Integrate"), this);
	addInterpolationAction = new QAction(QIcon::fromTheme("labplot-xy-interpolation-curve"), i18n("Interpolate"), this);
	addSmoothAction = new QAction(QIcon::fromTheme("labplot-xy-smoothing-curve"), i18n("Smooth"), this);
	addConvolutionAction = new QAction(QIcon::fromTheme("labplot-xy-curve"), i18n("Convolute/Deconvolute"), this);
	addCorrelationAction = new QAction(QIcon::fromTheme("labplot-xy-curve"), i18n("Auto-/Cross-Correlation"), this);

	QAction* fitAction = new QAction(i18n("Linear"), this);
	fitAction->setData(static_cast<int>(PlotDataDialog::AnalysisAction::FitLinear));
	addFitActions.append(fitAction);

	fitAction = new QAction(i18n("Power"), this);
	fitAction->setData(static_cast<int>(PlotDataDialog::AnalysisAction::FitPower));
	addFitActions.append(fitAction);

	fitAction = new QAction(i18n("Exponential (degree 1)"), this);
	fitAction->setData(static_cast<int>(PlotDataDialog::AnalysisAction::FitExp1));
	addFitActions.append(fitAction);

	fitAction = new QAction(i18n("Exponential (degree 2)"), this);
	fitAction->setData(static_cast<int>(PlotDataDialog::AnalysisAction::FitExp2));
	addFitActions.append(fitAction);

	fitAction = new QAction(i18n("Inverse exponential"), this);
	fitAction->setData(static_cast<int>(PlotDataDialog::AnalysisAction::FitInvExp));
	addFitActions.append(fitAction);

	fitAction = new QAction(i18n("Gauss"), this);
	fitAction->setData(static_cast<int>(PlotDataDialog::AnalysisAction::FitGauss));
	addFitActions.append(fitAction);

	fitAction = new QAction(i18n("Cauchy-Lorentz"), this);
	fitAction->setData(static_cast<int>(PlotDataDialog::AnalysisAction::FitCauchyLorentz));
	addFitActions.append(fitAction);

	fitAction = new QAction(i18n("Arc Tangent"), this);
	fitAction->setData(static_cast<int>(PlotDataDialog::AnalysisAction::FitTan));
	addFitActions.append(fitAction);

	fitAction = new QAction(i18n("Hyperbolic Tangent"), this);
	fitAction->setData(static_cast<int>(PlotDataDialog::AnalysisAction::FitTanh));
	addFitActions.append(fitAction);

	fitAction = new QAction(i18n("Error Function"), this);
	fitAction->setData(static_cast<int>(PlotDataDialog::AnalysisAction::FitErrFunc));
	addFitActions.append(fitAction);

	fitAction = new QAction(i18n("Custom"), this);
	fitAction->setData(static_cast<int>(PlotDataDialog::AnalysisAction::FitCustom));
	addFitActions.append(fitAction);

	addFourierFilterAction = new QAction(QIcon::fromTheme("labplot-xy-fourier-filter-curve"), i18n("Fourier Filter"), this);
	addFourierTransformAction = new QAction(QIcon::fromTheme("labplot-xy-fourier-transform-curve"), i18n("Fourier Transform"), this);

	connect(addDataReductionAction, &QAction::triggered, this, &CartesianPlot::addDataReductionCurve);
	connect(addDifferentiationAction, &QAction::triggered, this, &CartesianPlot::addDifferentiationCurve);
	connect(addIntegrationAction, &QAction::triggered, this, &CartesianPlot::addIntegrationCurve);
	connect(addInterpolationAction, &QAction::triggered, this, &CartesianPlot::addInterpolationCurve);
	connect(addSmoothAction, &QAction::triggered, this, &CartesianPlot::addSmoothCurve);
	connect(addConvolutionAction, &QAction::triggered, this, &CartesianPlot::addConvolutionCurve);
	connect(addCorrelationAction, &QAction::triggered, this, &CartesianPlot::addCorrelationCurve);
	for (const auto& action : addFitActions)
		connect(action, &QAction::triggered, this, &CartesianPlot::addFitCurve);
	connect(addFourierFilterAction, &QAction::triggered, this, &CartesianPlot::addFourierFilterCurve);
	connect(addFourierTransformAction, &QAction::triggered, this, &CartesianPlot::addFourierTransformCurve);

	//zoom/navigate actions
	scaleAutoAction = new QAction(QIcon::fromTheme("labplot-auto-scale-all"), i18n("Auto Scale"), this);
	scaleAutoXAction = new QAction(QIcon::fromTheme("labplot-auto-scale-x"), i18n("Auto Scale X"), this);
	scaleAutoYAction = new QAction(QIcon::fromTheme("labplot-auto-scale-y"), i18n("Auto Scale Y"), this);
	zoomInAction = new QAction(QIcon::fromTheme("zoom-in"), i18n("Zoom In"), this);
	zoomOutAction = new QAction(QIcon::fromTheme("zoom-out"), i18n("Zoom Out"), this);
	zoomInXAction = new QAction(QIcon::fromTheme("labplot-zoom-in-x"), i18n("Zoom In X"), this);
	zoomOutXAction = new QAction(QIcon::fromTheme("labplot-zoom-out-x"), i18n("Zoom Out X"), this);
	zoomInYAction = new QAction(QIcon::fromTheme("labplot-zoom-in-y"), i18n("Zoom In Y"), this);
	zoomOutYAction = new QAction(QIcon::fromTheme("labplot-zoom-out-y"), i18n("Zoom Out Y"), this);
	shiftLeftXAction = new QAction(QIcon::fromTheme("labplot-shift-left-x"), i18n("Shift Left X"), this);
	shiftRightXAction = new QAction(QIcon::fromTheme("labplot-shift-right-x"), i18n("Shift Right X"), this);
	shiftUpYAction = new QAction(QIcon::fromTheme("labplot-shift-up-y"), i18n("Shift Up Y"), this);
	shiftDownYAction = new QAction(QIcon::fromTheme("labplot-shift-down-y"), i18n("Shift Down Y"), this);

	connect(scaleAutoAction, &QAction::triggered, this, &CartesianPlot::scaleAutoTriggered);
	connect(scaleAutoXAction, &QAction::triggered, this, &CartesianPlot::scaleAutoTriggered);
	connect(scaleAutoYAction, &QAction::triggered, this, &CartesianPlot::scaleAutoTriggered);
	connect(zoomInAction, &QAction::triggered, this, &CartesianPlot::zoomIn);
	connect(zoomOutAction, &QAction::triggered, this, &CartesianPlot::zoomOut);
	connect(zoomInXAction, &QAction::triggered, this, &CartesianPlot::zoomInX);
	connect(zoomOutXAction, &QAction::triggered, this, &CartesianPlot::zoomOutX);
	connect(zoomInYAction, &QAction::triggered, this, &CartesianPlot::zoomInY);
	connect(zoomOutYAction, &QAction::triggered, this, &CartesianPlot::zoomOutY);
	connect(shiftLeftXAction, &QAction::triggered, this, &CartesianPlot::shiftLeftX);
	connect(shiftRightXAction, &QAction::triggered, this, &CartesianPlot::shiftRightX);
	connect(shiftUpYAction, &QAction::triggered, this, &CartesianPlot::shiftUpY);
	connect(shiftDownYAction, &QAction::triggered, this, &CartesianPlot::shiftDownY);

	//visibility action
	visibilityAction = new QAction(QIcon::fromTheme("view-visible"), i18n("Visible"), this);
	visibilityAction->setCheckable(true);
	connect(visibilityAction, &QAction::triggered, this, &CartesianPlot::visibilityChanged);
}

void CartesianPlot::initMenus() {
	initActions();

	addNewMenu = new QMenu(i18n("Add New"));
	addNewMenu->setIcon(QIcon::fromTheme("list-add"));
	addNewMenu->addAction(addCurveAction);
	addNewMenu->addAction(addHistogramAction);
	addNewMenu->addAction(addBoxPlotAction);
	addNewMenu->addAction(addEquationCurveAction);
	addNewMenu->addSeparator();

	addNewAnalysisMenu = new QMenu(i18n("Analysis Curve"));
	addNewAnalysisMenu->addAction(addFitCurveAction);
	addNewAnalysisMenu->addSeparator();
	addNewAnalysisMenu->addAction(addDifferentiationCurveAction);
	addNewAnalysisMenu->addAction(addIntegrationCurveAction);
	addNewAnalysisMenu->addSeparator();
	addNewAnalysisMenu->addAction(addInterpolationCurveAction);
	addNewAnalysisMenu->addAction(addSmoothCurveAction);
	addNewAnalysisMenu->addSeparator();
	addNewAnalysisMenu->addAction(addFourierFilterCurveAction);
	addNewAnalysisMenu->addAction(addFourierTransformCurveAction);
	addNewAnalysisMenu->addSeparator();
	addNewAnalysisMenu->addAction(addConvolutionCurveAction);
	addNewAnalysisMenu->addAction(addCorrelationCurveAction);
	addNewAnalysisMenu->addSeparator();
	addNewAnalysisMenu->addAction(addDataReductionCurveAction);
	addNewMenu->addMenu(addNewAnalysisMenu);

	addNewMenu->addSeparator();
	addNewMenu->addAction(addLegendAction);
	addNewMenu->addSeparator();
	addNewMenu->addAction(addHorizontalAxisAction);
	addNewMenu->addAction(addVerticalAxisAction);
	addNewMenu->addSeparator();
	addNewMenu->addAction(addTextLabelAction);
	addNewMenu->addAction(addImageAction);
	addNewMenu->addAction(addInfoElementAction);
	addNewMenu->addSeparator();
	addNewMenu->addAction(addCustomPointAction);
	addNewMenu->addAction(addReferenceLineAction);

	zoomMenu = new QMenu(i18n("Zoom/Navigate"));
	zoomMenu->setIcon(QIcon::fromTheme("zoom-draw"));
	zoomMenu->addAction(scaleAutoAction);
	zoomMenu->addAction(scaleAutoXAction);
	zoomMenu->addAction(scaleAutoYAction);
	zoomMenu->addSeparator();
	zoomMenu->addAction(zoomInAction);
	zoomMenu->addAction(zoomOutAction);
	zoomMenu->addSeparator();
	zoomMenu->addAction(zoomInXAction);
	zoomMenu->addAction(zoomOutXAction);
	zoomMenu->addSeparator();
	zoomMenu->addAction(zoomInYAction);
	zoomMenu->addAction(zoomOutYAction);
	zoomMenu->addSeparator();
	zoomMenu->addAction(shiftLeftXAction);
	zoomMenu->addAction(shiftRightXAction);
	zoomMenu->addSeparator();
	zoomMenu->addAction(shiftUpYAction);
	zoomMenu->addAction(shiftDownYAction);

	// Data manipulation menu
// 	QMenu* dataManipulationMenu = new QMenu(i18n("Data Manipulation"));
// 	dataManipulationMenu->setIcon(QIcon::fromTheme("zoom-draw"));
// 	dataManipulationMenu->addAction(addDataOperationAction);
// 	dataManipulationMenu->addAction(addDataReductionAction);

	// Data fit menu
	QMenu* dataFitMenu = new QMenu(i18n("Fit"));
	dataFitMenu->setIcon(QIcon::fromTheme("labplot-xy-fit-curve"));
	dataFitMenu->addAction(addFitActions.at(0));
	dataFitMenu->addAction(addFitActions.at(1));
	dataFitMenu->addAction(addFitActions.at(2));
	dataFitMenu->addAction(addFitActions.at(3));
	dataFitMenu->addAction(addFitActions.at(4));
	dataFitMenu->addSeparator();
	dataFitMenu->addAction(addFitActions.at(5));
	dataFitMenu->addAction(addFitActions.at(6));
	dataFitMenu->addSeparator();
	dataFitMenu->addAction(addFitActions.at(7));
	dataFitMenu->addAction(addFitActions.at(8));
	dataFitMenu->addAction(addFitActions.at(9));
	dataFitMenu->addSeparator();
	dataFitMenu->addAction(addFitActions.at(10));

	//analysis menu
	dataAnalysisMenu = new QMenu(i18n("Analysis"));
	dataAnalysisMenu->addMenu(dataFitMenu);
	dataAnalysisMenu->addSeparator();
	dataAnalysisMenu->addAction(addDifferentiationAction);
	dataAnalysisMenu->addAction(addIntegrationAction);
	dataAnalysisMenu->addSeparator();
	dataAnalysisMenu->addAction(addInterpolationAction);
	dataAnalysisMenu->addAction(addSmoothAction);
	dataAnalysisMenu->addSeparator();
	dataAnalysisMenu->addAction(addFourierFilterAction);
	dataAnalysisMenu->addAction(addFourierTransformAction);
	dataAnalysisMenu->addSeparator();
	dataAnalysisMenu->addAction(addConvolutionAction);
	dataAnalysisMenu->addAction(addCorrelationAction);
	dataAnalysisMenu->addSeparator();
// 	dataAnalysisMenu->insertMenu(nullptr, dataManipulationMenu);
	dataAnalysisMenu->addAction(addDataReductionAction);

	//theme menu
	themeMenu = new QMenu(i18n("Apply Theme"));
	themeMenu->setIcon(QIcon::fromTheme("color-management"));
	auto* themeWidget = new ThemesWidget(nullptr);
	themeWidget->setFixedMode();
	connect(themeWidget, &ThemesWidget::themeSelected, this, &CartesianPlot::loadTheme);
	connect(themeWidget, &ThemesWidget::themeSelected, themeMenu, &QMenu::close);

	auto* widgetAction = new QWidgetAction(this);
	widgetAction->setDefaultWidget(themeWidget);
	themeMenu->addAction(widgetAction);

	m_menusInitialized = true;
}

QMenu* CartesianPlot::createContextMenu() {
	if (!m_menusInitialized)
		initMenus();

	QMenu* menu = WorksheetElement::createContextMenu();
	// seems to be a bug, because the tooltips are not shown
	menu->setToolTipsVisible(true);
	QAction* firstAction = menu->actions().at(1);

	menu->insertMenu(firstAction, addNewMenu);
	menu->insertSeparator(firstAction);
	menu->insertMenu(firstAction, zoomMenu);
	menu->insertSeparator(firstAction);
	menu->insertMenu(firstAction, themeMenu);
	menu->insertSeparator(firstAction);

	visibilityAction->setChecked(isVisible());
	menu->insertAction(firstAction, visibilityAction);
	menu->insertSeparator(firstAction);

	if (children<XYCurve>().isEmpty()) {
		addInfoElementAction->setEnabled(false);
		addInfoElementAction->setToolTip("No curve inside plot.");
	} else {
		addInfoElementAction->setEnabled(true);
		addInfoElementAction->setToolTip("");
	}

	return menu;
}

QMenu* CartesianPlot::analysisMenu() {
	if (!m_menusInitialized)
		initMenus();

	return dataAnalysisMenu;
}

/*!
	Returns an icon to be used in the project explorer.
*/
QIcon CartesianPlot::icon() const {
	return QIcon::fromTheme("office-chart-line");
}

QVector<AbstractAspect*> CartesianPlot::dependsOn() const {
	//aspects which the plotted data in the worksheet depends on (spreadsheets and later matrices)
	QVector<AbstractAspect*> aspects;

	for (const auto* curve : children<XYCurve>()) {
		if (curve->xColumn() && dynamic_cast<Spreadsheet*>(curve->xColumn()->parentAspect()) )
			aspects << curve->xColumn()->parentAspect();

		if (curve->yColumn() && dynamic_cast<Spreadsheet*>(curve->yColumn()->parentAspect()) )
			aspects << curve->yColumn()->parentAspect();
	}

	return aspects;
}

QVector<AspectType> CartesianPlot::pasteTypes() const {
	QVector<AspectType> types{
		AspectType::XYCurve, AspectType::Histogram,
		AspectType::Axis, AspectType::XYEquationCurve,
		AspectType::XYConvolutionCurve, AspectType::XYCorrelationCurve,
		AspectType::XYDataReductionCurve, AspectType::XYDifferentiationCurve,
		AspectType::XYFitCurve, AspectType::XYFourierFilterCurve,
		AspectType::XYFourierTransformCurve, AspectType::XYIntegrationCurve,
		AspectType::XYInterpolationCurve, AspectType::XYSmoothCurve,
		AspectType::TextLabel, AspectType::Image,
		AspectType::InfoElement, AspectType::CustomPoint,
		AspectType::ReferenceLine
	};

	//only allow to paste a legend if there is no legend available yet in the plot
	if (!m_legend)
		types << AspectType::CartesianPlotLegend;

	return types;
}

void CartesianPlot::navigate(NavigationOperation op) {
	Q_D(CartesianPlot);
	if (op == NavigationOperation::ScaleAuto) {
		if (d->curvesXMinMaxIsDirty || d->curvesYMinMaxIsDirty || !autoScaleX() || !autoScaleY()) {
			d->curvesXMinMaxIsDirty = true;
			d->curvesYMinMaxIsDirty = true;
		}
		scaleAuto();
	} else if (op == NavigationOperation::ScaleAutoX) setAutoScaleX(true);
	else if (op == NavigationOperation::ScaleAutoY) setAutoScaleY(true);
	else if (op == NavigationOperation::ZoomIn) zoomIn();
	else if (op == NavigationOperation::ZoomOut) zoomOut();
	else if (op == NavigationOperation::ZoomInX) zoomInX();
	else if (op == NavigationOperation::ZoomOutX) zoomOutX();
	else if (op == NavigationOperation::ZoomInY) zoomInY();
	else if (op == NavigationOperation::ZoomOutY) zoomOutY();
	else if (op == NavigationOperation::ShiftLeftX) shiftLeftX();
	else if (op == NavigationOperation::ShiftRightX) shiftRightX();
	else if (op == NavigationOperation::ShiftUpY) shiftUpY();
	else if (op == NavigationOperation::ShiftDownY) shiftDownY();
}

void CartesianPlot::setSuppressDataChangedSignal(bool value) {
	Q_D(CartesianPlot);
	d->suppressRetransform = value;
}

void CartesianPlot::processDropEvent(const QVector<quintptr>& vec) {
	PERFTRACE("CartesianPlot::processDropEvent");

	QVector<AbstractColumn*> columns;
	for (auto a : vec) {
		auto* aspect = (AbstractAspect*)a;
		auto* column = dynamic_cast<AbstractColumn*>(aspect);
		if (column)
			columns << column;
	}

	//return if there are no columns being dropped.
	//TODO: extend this later when we allow to drag&drop plots, etc.
	if (columns.isEmpty())
		return;

	//determine the first column with "x plot designation" as the x-data column for all curves to be created
	const AbstractColumn* xColumn = nullptr;
	for (const auto* column : columns) {
		if (column->plotDesignation() == AbstractColumn::PlotDesignation::X) {
			xColumn = column;
			break;
		}
	}

	//if no column with "x plot designation" is available, use the x-data column of the first curve in the plot,
	if (xColumn == nullptr) {
		QVector<XYCurve*> curves = children<XYCurve>();
		if (!curves.isEmpty())
			xColumn = curves.at(0)->xColumn();
	}

	//use the first dropped column if no column with "x plot designation" nor curves are available
	if (xColumn == nullptr)
		xColumn = columns.at(0);

	//create curves
	bool curvesAdded = false;
	for (const auto* column : columns) {
		if (column == xColumn)
			continue;

		XYCurve* curve = new XYCurve(column->name());
		curve->suppressRetransform(true); //suppress retransform, all curved will be recalculated at the end
		curve->setXColumn(xColumn);
		curve->setYColumn(column);
		addChild(curve);
		curve->suppressRetransform(false);
		curvesAdded = true;
	}

	if (curvesAdded)
		dataChanged();
}

bool CartesianPlot::isPanningActive() const {
	Q_D(const CartesianPlot);
	return d->panningStarted;
}

bool CartesianPlot::isHovered() const {
	Q_D(const CartesianPlot);
	return d->m_hovered;
}
bool CartesianPlot::isPrinted() const {
	Q_D(const CartesianPlot);
	return d->m_printing;
}

bool CartesianPlot::isSelected() const {
	Q_D(const CartesianPlot);
	return d->isSelected();
}

//##############################################################################
//################################  getter methods  ############################
//##############################################################################
BASIC_SHARED_D_READER_IMPL(CartesianPlot, CartesianPlot::RangeType, rangeType, rangeType)
BASIC_SHARED_D_READER_IMPL(CartesianPlot, int, rangeLastValues, rangeLastValues)
BASIC_SHARED_D_READER_IMPL(CartesianPlot, int, rangeFirstValues, rangeFirstValues)

//TODO
//BASIC_SHARED_D_READER_IMPL(CartesianPlot, bool, autoScaleX, autoScaleX)
//BASIC_SHARED_D_READER_IMPL(CartesianPlot, bool, autoScaleY, autoScaleY)

BASIC_SHARED_D_READER_IMPL(CartesianPlot, bool, xRangeBreakingEnabled, xRangeBreakingEnabled)
CLASS_SHARED_D_READER_IMPL(CartesianPlot, CartesianPlot::RangeBreaks, xRangeBreaks, xRangeBreaks)
BASIC_SHARED_D_READER_IMPL(CartesianPlot, bool, yRangeBreakingEnabled, yRangeBreakingEnabled)
CLASS_SHARED_D_READER_IMPL(CartesianPlot, CartesianPlot::RangeBreaks, yRangeBreaks, yRangeBreaks)

CLASS_SHARED_D_READER_IMPL(CartesianPlot, QPen, cursorPen, cursorPen);
CLASS_SHARED_D_READER_IMPL(CartesianPlot, bool, cursor0Enable, cursor0Enable);
CLASS_SHARED_D_READER_IMPL(CartesianPlot, bool, cursor1Enable, cursor1Enable);
CLASS_SHARED_D_READER_IMPL(CartesianPlot, QString, theme, theme)

/*!
	returns the actual bounding rectangular of the plot area showing data (plot's rectangular minus padding)
	in plot's coordinates
 */
QRectF CartesianPlot::dataRect() const {
	Q_D(const CartesianPlot);
	return d->dataRect;
}

CartesianPlot::MouseMode CartesianPlot::mouseMode() const {
	Q_D(const CartesianPlot);
	return d->mouseMode;
}

const QString CartesianPlot::xRangeDateTimeFormat() const {
	const int index{ defaultCoordinateSystem()->xIndex() };
	return xRangeDateTimeFormat(index);
}
const QString CartesianPlot::yRangeDateTimeFormat() const {
	const int index{ defaultCoordinateSystem()->yIndex() };
	return yRangeDateTimeFormat(index);
}
const QString CartesianPlot::xRangeDateTimeFormat(const int index) const {
	Q_D(const CartesianPlot);
	return d->xRanges.at(index).dateTimeFormat();
}
const QString CartesianPlot::yRangeDateTimeFormat(const int index) const {
	Q_D(const CartesianPlot);
	return d->yRanges.at(index).dateTimeFormat();
}

//##############################################################################
//######################  setter methods and undo commands  ####################
//##############################################################################
/*!
	set the rectangular, defined in scene coordinates
 */
class CartesianPlotSetRectCmd : public QUndoCommand {
public:
	CartesianPlotSetRectCmd(CartesianPlotPrivate* private_obj, QRectF rect) : m_private(private_obj), m_rect(rect) {
		setText(i18n("%1: change geometry rect", m_private->name()));
	}

	void redo() override {
// 		const double horizontalRatio = m_rect.width() / m_private->rect.width();
// 		const double verticalRatio = m_rect.height() / m_private->rect.height();

		qSwap(m_private->rect, m_rect);

// 		m_private->q->handleResize(horizontalRatio, verticalRatio, false);
		m_private->retransform();
		emit m_private->q->rectChanged(m_private->rect);
	}

	void undo() override {
		redo();
	}

private:
	CartesianPlotPrivate* m_private;
	QRectF m_rect;
};

void CartesianPlot::setRect(const QRectF& rect) {
	Q_D(CartesianPlot);
	if (rect != d->rect)
		exec(new CartesianPlotSetRectCmd(d, rect));
}

STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetRangeType, CartesianPlot::RangeType, rangeType, rangeChanged);
void CartesianPlot::setRangeType(RangeType type) {
	Q_D(CartesianPlot);
	if (type != d->rangeType)
		exec(new CartesianPlotSetRangeTypeCmd(d, type, ki18n("%1: set range type")));
}

STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetRangeLastValues, int, rangeLastValues, rangeChanged);
void CartesianPlot::setRangeLastValues(int values) {
	Q_D(CartesianPlot);
	if (values != d->rangeLastValues)
		exec(new CartesianPlotSetRangeLastValuesCmd(d, values, ki18n("%1: set range")));
}

STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetRangeFirstValues, int, rangeFirstValues, rangeChanged);
void CartesianPlot::setRangeFirstValues(int values) {
	Q_D(CartesianPlot);
	if (values != d->rangeFirstValues)
		exec(new CartesianPlotSetRangeFirstValuesCmd(d, values, ki18n("%1: set range")));
}

// x/y ranges

RangeT::Format CartesianPlot::xRangeFormat() const {
	return xRangeFormat(defaultCoordinateSystem()->xIndex());
}
RangeT::Format CartesianPlot::yRangeFormat() const {
	return yRangeFormat(defaultCoordinateSystem()->yIndex());
}
RangeT::Format CartesianPlot::xRangeFormat(const int index) const {
	Q_D(const CartesianPlot);
	if (index < 0 || index > xRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return RangeT::Format::Numeric;
	}
	return d->xRanges.at(index).format();
}
RangeT::Format CartesianPlot::yRangeFormat(const int index) const {
	Q_D(const CartesianPlot);
	if (index < 0 || index > yRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return RangeT::Format::Numeric;
	}
	return d->yRanges.at(index).format();
}
void CartesianPlot::setXRangeFormat(const RangeT::Format format) {
	setXRangeFormat(defaultCoordinateSystem()->xIndex(), format);
}
void CartesianPlot::setYRangeFormat(const RangeT::Format format) {
	setYRangeFormat(defaultCoordinateSystem()->yIndex(), format);
}
//TODO: CartesianPlotSetXRangeFormatIndexCmd
/*STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetYRangeFormat, CartesianPlot::RangeFormat, yRangeFormat, yRangeFormatChanged);
void CartesianPlot::setYRangeFormat(RangeFormat format) {
	Q_D(CartesianPlot);
	if (format != d->yRangeFormat)
		exec(new CartesianPlotSetYRangeFormatCmd(d, format, ki18n("%1: set y-range format")));
}*/
void CartesianPlot::setXRangeFormat(const int index, const RangeT::Format format) {
	Q_D(CartesianPlot);
	if (index < 0 || index > xRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return;
	}
	if (format != xRangeFormat(index)) {
		d->xRanges[index].setFormat(format);
		emit d->xRangeFormatChanged();
	}
}
//TODO: CartesianPlotSetYRangeFormatIndexCmd
void CartesianPlot::setYRangeFormat(const int index, const RangeT::Format format) {
	Q_D(CartesianPlot);
	if (index < 0 || index > yRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return;
	}
	if (format != yRangeFormat(index)) {
		d->yRanges[index].setFormat(format);
		emit d->yRangeFormatChanged();
	}
}

class CartesianPlotSetAutoScaleXCmd : public QUndoCommand {
public:
	CartesianPlotSetAutoScaleXCmd(CartesianPlotPrivate* private_obj, bool autoScale) :
		m_private(private_obj), m_autoScale(autoScale), m_autoScaleOld(false), m_oldRange(0.0, 0.0) {
		setText(i18n("%1: change x-range auto scaling", m_private->name()));
	}

	void redo() override {
		m_autoScaleOld = m_private->autoScaleX();
		if (m_autoScale) {
			m_oldRange = m_private->xRange();
			m_private->q->scaleAutoX();
		}
		m_private->setAutoScaleX(m_autoScale);
		emit m_private->q->xAutoScaleChanged(m_autoScale);
	}

	void undo() override {
		if (!m_autoScaleOld) {
			m_private->xRange() = m_oldRange;
			m_private->retransformScales();
		}
		m_private->setAutoScaleX(m_autoScaleOld);
		emit m_private->q->xAutoScaleChanged(m_autoScaleOld);
	}

private:
	CartesianPlotPrivate* m_private;
	bool m_autoScale;
	bool m_autoScaleOld;
	Range<double> m_oldRange;
};
class CartesianPlotSetAutoScaleYCmd : public QUndoCommand {
public:
	CartesianPlotSetAutoScaleYCmd(CartesianPlotPrivate* private_obj, bool autoScale) :
		m_private(private_obj), m_autoScale(autoScale), m_autoScaleOld(false), m_oldRange(0.0, 0.0) {
		setText(i18n("%1: change y-range auto scaling", m_private->name()));
	}

	void redo() override {
		m_autoScaleOld = m_private->autoScaleY();
		if (m_autoScale) {
			m_oldRange = m_private->yRange();
			m_private->q->scaleAutoY();
		}
		m_private->setAutoScaleY(m_autoScale);
		emit m_private->q->yAutoScaleChanged(m_autoScale);
	}

	void undo() override {
		if (!m_autoScaleOld) {
			m_private->yRange() = m_oldRange;
			m_private->retransformScales();
		}
		m_private->setAutoScaleY(m_autoScaleOld);
		emit m_private->q->yAutoScaleChanged(m_autoScaleOld);
	}

private:
	CartesianPlotPrivate* m_private;
	bool m_autoScale;
	bool m_autoScaleOld;
	Range<double> m_oldRange;
};

// auto scales x range of default plot range
void CartesianPlot::setAutoScaleX(bool autoScaleX) {
	Q_D(CartesianPlot);
	if (autoScaleX != d->autoScaleX())
		exec(new CartesianPlotSetAutoScaleXCmd(d, autoScaleX));
}
// auto scale y range of default plot range
void CartesianPlot::setAutoScaleY(bool autoScaleY) {
	Q_D(CartesianPlot);
	if (autoScaleY != d->autoScaleY())
		exec(new CartesianPlotSetAutoScaleYCmd(d, autoScaleY));
}
// auto scales x range index
//TODO: undo aware CartesianPlotSetAutoScaleXIndexCmd
void CartesianPlot::setAutoScaleX(const int index, bool autoScaleX) {
	Q_D(CartesianPlot);
	if (autoScaleX != xRange(index).autoScale())
		d->xRanges[index].setAutoScale(autoScaleX);
}
//TODO: undo aware CartesianPlotSetAutoScaleYIndexCmd
void CartesianPlot::setAutoScaleY(const int index, bool autoScaleY) {
	Q_D(CartesianPlot);
	if (autoScaleY != yRange(index).autoScale())
		d->yRanges[index].setAutoScale(autoScaleY);
}

// set x/y range command with index
class CartesianPlotSetXRangeIndexCmd: public StandardQVectorSetterCmd<CartesianPlot::Private, Range<double>> {
	public:
		CartesianPlotSetXRangeIndexCmd(CartesianPlot::Private *target, Range<double> newValue, int index, const KLocalizedString &description)
			: StandardQVectorSetterCmd<CartesianPlot::Private, Range<double>>(target, &CartesianPlot::Private::xRanges, index, newValue, description) {}
		//TODO: check emit
		virtual void finalize() override { m_target->retransformScales(); emit m_target->q->xRangeChanged((m_target->*m_field).at(m_index)); }
};
class CartesianPlotSetYRangeIndexCmd: public StandardQVectorSetterCmd<CartesianPlot::Private, Range<double>> {
	public:
		CartesianPlotSetYRangeIndexCmd(CartesianPlot::Private *target, Range<double> newValue, int index, const KLocalizedString &description)
			: StandardQVectorSetterCmd<CartesianPlot::Private, Range<double>>(target, &CartesianPlot::Private::yRanges, index, newValue, description) {}
		//TODO: check emit
		virtual void finalize() override { m_target->retransformScales(); emit m_target->q->yRangeChanged((m_target->*m_field).at(m_index)); }
};

bool CartesianPlot::autoScaleX() {
	Q_D(const CartesianPlot);
	return d->xRanges.at(defaultCoordinateSystem()->xIndex()).autoScale();
}
bool CartesianPlot::autoScaleY() {
	Q_D(const CartesianPlot);
	return d->yRanges.at(defaultCoordinateSystem()->yIndex()).autoScale();
}

int CartesianPlot::xRangeCount() const {
	Q_D(const CartesianPlot);
	return ( d ? d->xRanges.size() : 0 );
}
int CartesianPlot::yRangeCount() const {
	Q_D(const CartesianPlot);
	return ( d ? d->yRanges.size() : 0 );
}
const Range<double>& CartesianPlot::xRange() const {
	Q_D(const CartesianPlot);
	return d->xRanges.at( defaultCoordinateSystem()->xIndex() );
}
const Range<double>& CartesianPlot::yRange() const {
	Q_D(const CartesianPlot);
	return d->yRanges.at( defaultCoordinateSystem()->yIndex() );
}
Range<double> CartesianPlot::xRange(const int index) const {
	Q_D(const CartesianPlot);
	if (index < 0 || index > xRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return Range<double>();
	}
	return d->xRanges.at(index);
}
Range<double> CartesianPlot::yRange(const int index) const {
	Q_D(const CartesianPlot);
	if (index < 0 || index > yRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return Range<double>();
	}
	return d->yRanges.at(index);
}
// sets x range of default plot range
void CartesianPlot::setXRange(Range<double> range) {
	DEBUG(Q_FUNC_INFO << ", set x range to " << range.toStdString())
	Q_D(CartesianPlot);
	const int xIndex{ defaultCoordinateSystem()->xIndex() };
	if (range.finite() && range != xRange()) {
		d->curvesYMinMaxIsDirty = true;
		exec(new CartesianPlotSetXRangeIndexCmd(d, range, xIndex, ki18n("%1: set x range")));
		//d->xRanges[xIndex] = range;
		if (autoScaleY())
			scaleAutoY();
	}
}
// sets y range of default plot range
void CartesianPlot::setYRange(Range<double> range) {
	DEBUG(Q_FUNC_INFO << ", set y range to " << range.toStdString())
	Q_D(CartesianPlot);
	const int yIndex{ defaultCoordinateSystem()->yIndex() };
	if (range.finite() && range != yRange()) {
		d->curvesXMinMaxIsDirty = true;
		exec(new CartesianPlotSetYRangeIndexCmd(d, range, yIndex, ki18n("%1: set y range")));
		//d->yRanges[yIndex] = range;
		if (autoScaleX())
			scaleAutoX();
	}
}
void CartesianPlot::addXRange() {
	Q_D(CartesianPlot);
	d->xRanges.append(Range<double>());
}
void CartesianPlot::addYRange() {
	Q_D(CartesianPlot);
	d->yRanges.append(Range<double>());
}
void CartesianPlot::addXRange(const Range<double> range) {
	Q_D(CartesianPlot);
	d->xRanges.append(range);
}
void CartesianPlot::addYRange(const Range<double> range) {
	Q_D(CartesianPlot);
	d->yRanges.append(range);
}

void CartesianPlot::removeXRange(int index) {
	Q_D(CartesianPlot);
	if (index < 0 || index > xRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return;
	}
	d->xRanges.remove(index);
}
void CartesianPlot::removeYRange(int index) {
	Q_D(CartesianPlot);
	if (index < 0 || index > yRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return;
	}
	d->yRanges.remove(index);
}
void CartesianPlot::setXRange(const int index, const Range<double> range) {
	DEBUG(Q_FUNC_INFO)
	Q_D(CartesianPlot);
	if (range.finite() && range != xRange(index)) {
		d->curvesYMinMaxIsDirty = true;
		exec(new CartesianPlotSetXRangeIndexCmd(d, range, index, ki18n("%1: set x range")));
		if (autoScaleY())
			scaleAutoY();
	}
}
void CartesianPlot::setYRange(const int index, const Range<double> range) {
	DEBUG(Q_FUNC_INFO)
	Q_D(CartesianPlot);
	if (range.finite() && range != yRange(index)) {
		d->curvesXMinMaxIsDirty = true;
		exec(new CartesianPlotSetYRangeIndexCmd(d, range, index, ki18n("%1: set y range")));
		if (autoScaleX())
			scaleAutoX();
	}
}
void CartesianPlot::setXMin(const int index, const double value) {
	DEBUG(Q_FUNC_INFO)
	Range<double> range{ xRange(index) };
	range.setStart(value);

	setXRange(index, range);
}
void CartesianPlot::setXMax(const int index, const double value) {
	DEBUG(Q_FUNC_INFO)
	Range<double> range{ xRange(index) };
	range.setEnd(value);

	setXRange(index, range);
}
void CartesianPlot::setYMin(const int index, const double value) {
	DEBUG(Q_FUNC_INFO)
	Range<double> range{ yRange(index) };
	range.setStart(value);

	setYRange(index, range);
}
void CartesianPlot::setYMax(const int index, const double value) {
	DEBUG(Q_FUNC_INFO)
	Range<double> range{ yRange(index) };
	range.setEnd(value);

	setYRange(index, range);
}

//TODO: undo aware?
//STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetXScale, CartesianPlot::Scale, xScale, retransformScales)
/*void CartesianPlot::setXScale(Scale scale) {
	Q_D(CartesianPlot);
	if (scale != d->xScale)
		exec(new CartesianPlotSetXScaleCmd(d, scale, ki18n("%1: set x scale")));
}*/
RangeT::Scale CartesianPlot::xRangeScale() const {
	return xRangeScale(defaultCoordinateSystem()->xIndex());
}
RangeT::Scale CartesianPlot::yRangeScale() const {
	return yRangeScale(defaultCoordinateSystem()->yIndex());
}
RangeT::Scale CartesianPlot::xRangeScale(const int index) const {
	if (index < 0 || index > xRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return RangeT::Scale::Linear;
	}
	return xRange(index).scale();
}
RangeT::Scale CartesianPlot::yRangeScale(const int index) const {
	if (index < 0 || index > yRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return RangeT::Scale::Linear;
	}
	return yRange(index).scale();
}
void CartesianPlot::setXRangeScale(const RangeT::Scale scale) {
	setXRangeScale(defaultCoordinateSystem()->xIndex(), scale);
}
void CartesianPlot::setYRangeScale(const RangeT::Scale scale) {
	setYRangeScale(defaultCoordinateSystem()->yIndex(), scale);
}
void CartesianPlot::setXRangeScale(const int index, const RangeT::Scale scale) {
	Q_D(CartesianPlot);
	if (index < 0 || index > xRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return;
	}
	d->xRanges[index].setScale(scale);
	d->retransformScales();
}
void CartesianPlot::setYRangeScale(const int index, const RangeT::Scale scale) {
	Q_D(CartesianPlot);
	if (index < 0 || index > yRangeCount()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return;
	}
	d->yRanges[index].setScale(scale);
	d->retransformScales();
}

// coordinate systems

int CartesianPlot::coordinateSystemCount() const {
	return m_coordinateSystems.size();
}

CartesianCoordinateSystem* CartesianPlot::coordinateSystem(int index) const {
	if (index > m_coordinateSystems.size())
		return nullptr;

	return dynamic_cast<CartesianCoordinateSystem*>(m_coordinateSystems.at(index));
}

void CartesianPlot::addCoordinateSystem() {
	DEBUG(Q_FUNC_INFO)
	auto* cSystem{ new CartesianCoordinateSystem(this) };
	m_coordinateSystems.append( cSystem );
}
void CartesianPlot::addCoordinateSystem(CartesianCoordinateSystem* cSystem) {
	DEBUG(Q_FUNC_INFO)
	m_coordinateSystems.append( cSystem );
}
void CartesianPlot::removeCoordinateSystem(int index) {
	DEBUG(Q_FUNC_INFO)
	if (index < 0 || index > m_coordinateSystems.size()) {
		DEBUG(Q_FUNC_INFO << ", index " << index << " out of range")
		return;
	}

	m_coordinateSystems.remove(index);
}

STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetDefaultCoordinateSystemIndex, int, defaultCoordinateSystemIndex, retransformScales)
int CartesianPlot::defaultCoordinateSystemIndex() const {
	Q_D(const CartesianPlot);
	return d->defaultCoordinateSystemIndex;
}
void CartesianPlot::setDefaultCoordinateSystemIndex(int index) {
        Q_D(CartesianPlot);
	if (index != d->defaultCoordinateSystemIndex)
		exec(new CartesianPlotSetDefaultCoordinateSystemIndexCmd(d, index, ki18n("%1: set default plot range")));
}
CartesianCoordinateSystem* CartesianPlot::defaultCoordinateSystem() const {
	Q_D(const CartesianPlot);
	return d->defaultCoordinateSystem();
}

// range breaks

STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetXRangeBreakingEnabled, bool, xRangeBreakingEnabled, retransformScales)
void CartesianPlot::setXRangeBreakingEnabled(bool enabled) {
	Q_D(CartesianPlot);
	if (enabled != d->xRangeBreakingEnabled)
		exec(new CartesianPlotSetXRangeBreakingEnabledCmd(d, enabled, ki18n("%1: x-range breaking enabled")));
}

STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetXRangeBreaks, CartesianPlot::RangeBreaks, xRangeBreaks, retransformScales)
void CartesianPlot::setXRangeBreaks(const RangeBreaks& breakings) {
	Q_D(CartesianPlot);
	exec(new CartesianPlotSetXRangeBreaksCmd(d, breakings, ki18n("%1: x-range breaks changed")));
}

STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetYRangeBreakingEnabled, bool, yRangeBreakingEnabled, retransformScales)
void CartesianPlot::setYRangeBreakingEnabled(bool enabled) {
	Q_D(CartesianPlot);
	if (enabled != d->yRangeBreakingEnabled)
		exec(new CartesianPlotSetYRangeBreakingEnabledCmd(d, enabled, ki18n("%1: y-range breaking enabled")));
}

STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetYRangeBreaks, CartesianPlot::RangeBreaks, yRangeBreaks, retransformScales)
void CartesianPlot::setYRangeBreaks(const RangeBreaks& breaks) {
	Q_D(CartesianPlot);
	exec(new CartesianPlotSetYRangeBreaksCmd(d, breaks, ki18n("%1: y-range breaks changed")));
}

// cursor

STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetCursorPen, QPen, cursorPen, update)
void CartesianPlot::setCursorPen(const QPen &pen) {
	Q_D(CartesianPlot);
	if (pen != d->cursorPen)
		exec(new CartesianPlotSetCursorPenCmd(d, pen, ki18n("%1: y-range breaks changed")));
}

STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetCursor0Enable, bool, cursor0Enable, updateCursor)
void CartesianPlot::setCursor0Enable(const bool &enable) {
	Q_D(CartesianPlot);
	if (enable != d->cursor0Enable) {
		if (std::isnan(d->cursor0Pos.x())) { // if never set, set initial position
			d->cursor0Pos.setX(defaultCoordinateSystem()->mapSceneToLogical(QPointF(0, 0)).x());
			mousePressCursorModeSignal(0, d->cursor0Pos); // simulate mousePress to update values in the cursor dock
		}
		exec(new CartesianPlotSetCursor0EnableCmd(d, enable, ki18n("%1: Cursor0 enable")));
	}
}

STD_SETTER_CMD_IMPL_F_S(CartesianPlot, SetCursor1Enable, bool, cursor1Enable, updateCursor)
void CartesianPlot::setCursor1Enable(const bool &enable) {
	Q_D(CartesianPlot);
	if (enable != d->cursor1Enable) {
		if (std::isnan(d->cursor1Pos.x())) { // if never set, set initial position
			d->cursor1Pos.setX(defaultCoordinateSystem()->mapSceneToLogical(QPointF(0, 0)).x());
			mousePressCursorModeSignal(1, d->cursor1Pos); // simulate mousePress to update values in the cursor dock
		}
		exec(new CartesianPlotSetCursor1EnableCmd(d, enable, ki18n("%1: Cursor1 enable")));
	}
}

// theme

STD_SETTER_CMD_IMPL_S(CartesianPlot, SetTheme, QString, theme)
void CartesianPlot::setTheme(const QString& theme) {
	Q_D(CartesianPlot);
	if (theme != d->theme) {
		QString info;
		if (!theme.isEmpty())
			info = i18n("%1: load theme %2", name(), theme);
		else
			info = i18n("%1: load default theme", name());
		beginMacro(info);
		exec(new CartesianPlotSetThemeCmd(d, theme, ki18n("%1: set theme")));
		loadTheme(theme);
		endMacro();
	}
}

//################################################################
//########################## Slots ###############################
//################################################################
void CartesianPlot::addHorizontalAxis() {
	DEBUG(Q_FUNC_INFO)
	Axis* axis = new Axis("x-axis", Axis::Orientation::Horizontal);
	if (axis->autoScale()) {
		axis->setUndoAware(false);
		// use x range of default plot range
		axis->setRange( xRange() );
		axis->setUndoAware(true);
	}
	axis->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	axis->setSuppressRetransform(true);	// retransformTicks() needs plot
	addChild(axis);
	axis->setSuppressRetransform(false);
	axis->retransform();
}

void CartesianPlot::addVerticalAxis() {
	Axis* axis = new Axis("y-axis", Axis::Orientation::Vertical);
	if (axis->autoScale()) {
		axis->setUndoAware(false);
		// use y range of default plot range
		axis->setRange( yRange() );
		axis->setUndoAware(true);
	}
	axis->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	axis->setSuppressRetransform(true);	// retransformTicks() needs plot
	addChild(axis);
	axis->setSuppressRetransform(false);
	axis->retransform();
}

void CartesianPlot::addCurve() {
	DEBUG(Q_FUNC_INFO)
	auto* curve{ new XYCurve("xy-curve") };
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	addChild(curve);
}

void CartesianPlot::addEquationCurve() {
	DEBUG(Q_FUNC_INFO << ", to default coordinate system " << defaultCoordinateSystemIndex())
	auto* curve{ new XYEquationCurve("f(x)") };
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	addChild(curve);
}

void CartesianPlot::addHistogram() {
	DEBUG(Q_FUNC_INFO << ", TODO: to default coordinate system " << defaultCoordinateSystemIndex())
	auto* hist{ new Histogram("Histogram") };
	//hist->setCoordinateSystemIndex(defaultCoordinateSystem());
	DEBUG(Q_FUNC_INFO << ", TODO")
	addChild(hist);
}

void CartesianPlot::addBoxPlot() {
	addChild(new BoxPlot("Box Plot"));
}

/*!
 * returns the first selected XYCurve in the plot
 */
const XYCurve* CartesianPlot::currentCurve() const {
	for (const auto* curve : this->children<const XYCurve>()) {
		if (curve->graphicsItem()->isSelected())
			return curve;
	}

	return nullptr;
}

void CartesianPlot::addDataReductionCurve() {
	auto* curve = new XYDataReductionCurve("Data reduction");
	const XYCurve* curCurve = currentCurve();
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	if (curCurve) {
		beginMacro( i18n("%1: reduce '%2'", name(), curCurve->name()) );
		curve->setName( i18n("Reduction of '%1'", curCurve->name()) );
		curve->setDataSourceType(XYAnalysisCurve::DataSourceType::Curve);
		curve->setDataSourceCurve(curCurve);
		this->addChild(curve);
		curve->recalculate();
		emit curve->dataReductionDataChanged(curve->dataReductionData());
	} else {
		beginMacro(i18n("%1: add data reduction curve", name()));
		this->addChild(curve);
	}

	endMacro();
}

void CartesianPlot::addDifferentiationCurve() {
	auto* curve = new XYDifferentiationCurve("Differentiation");
	const XYCurve* curCurve = currentCurve();
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	if (curCurve) {
		beginMacro( i18n("%1: differentiate '%2'", name(), curCurve->name()) );
		curve->setName( i18n("Derivative of '%1'", curCurve->name()) );
		curve->setDataSourceType(XYAnalysisCurve::DataSourceType::Curve);
		curve->setDataSourceCurve(curCurve);
		this->addChild(curve);
		curve->recalculate();
		emit curve->differentiationDataChanged(curve->differentiationData());
	} else {
		beginMacro(i18n("%1: add differentiation curve", name()));
		this->addChild(curve);
	}

	endMacro();
}

void CartesianPlot::addIntegrationCurve() {
	auto* curve = new XYIntegrationCurve("Integration");
	const XYCurve* curCurve = currentCurve();
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	if (curCurve) {
		beginMacro( i18n("%1: integrate '%2'", name(), curCurve->name()) );
		curve->setName( i18n("Integral of '%1'", curCurve->name()) );
		curve->setDataSourceType(XYAnalysisCurve::DataSourceType::Curve);
		curve->setDataSourceCurve(curCurve);
		this->addChild(curve);
		curve->recalculate();
		emit curve->integrationDataChanged(curve->integrationData());
	} else {
		beginMacro(i18n("%1: add integration curve", name()));
		this->addChild(curve);
	}

	endMacro();
}

void CartesianPlot::addInterpolationCurve() {
	auto* curve = new XYInterpolationCurve("Interpolation");
	const XYCurve* curCurve = currentCurve();
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	if (curCurve) {
		beginMacro( i18n("%1: interpolate '%2'", name(), curCurve->name()) );
		curve->setName( i18n("Interpolation of '%1'", curCurve->name()) );
		curve->setDataSourceType(XYAnalysisCurve::DataSourceType::Curve);
		curve->setDataSourceCurve(curCurve);
		curve->recalculate();
		this->addChild(curve);
		emit curve->interpolationDataChanged(curve->interpolationData());
	} else {
		beginMacro(i18n("%1: add interpolation curve", name()));
		this->addChild(curve);
	}

	endMacro();
}

void CartesianPlot::addSmoothCurve() {
	auto* curve = new XYSmoothCurve("Smooth");
	const XYCurve* curCurve = currentCurve();
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	if (curCurve) {
		beginMacro( i18n("%1: smooth '%2'", name(), curCurve->name()) );
		curve->setName( i18n("Smoothing of '%1'", curCurve->name()) );
		curve->setDataSourceType(XYAnalysisCurve::DataSourceType::Curve);
		curve->setDataSourceCurve(curCurve);
		this->addChild(curve);
		curve->recalculate();
		emit curve->smoothDataChanged(curve->smoothData());
	} else {
		beginMacro(i18n("%1: add smoothing curve", name()));
		this->addChild(curve);
	}

	endMacro();
}

void CartesianPlot::addFitCurve() {
	auto* curve = new XYFitCurve("fit");
	const XYCurve* curCurve = currentCurve();
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	if (curCurve) {
		beginMacro( i18n("%1: fit to '%2'", name(), curCurve->name()) );
		curve->setName( i18n("Fit to '%1'", curCurve->name()) );
		curve->setDataSourceType(XYAnalysisCurve::DataSourceType::Curve);
		curve->setDataSourceCurve(curCurve);


		//set the fit model category and type
		const auto* action = qobject_cast<const QAction*>(QObject::sender());
		PlotDataDialog::AnalysisAction type = (PlotDataDialog::AnalysisAction)action->data().toInt();
		curve->initFitData(type);
		curve->initStartValues(curCurve);

		//fit with weights for y if the curve has error bars for y
		if (curCurve->yErrorType() == XYCurve::ErrorType::Symmetric && curCurve->yErrorPlusColumn()) {
			XYFitCurve::FitData fitData = curve->fitData();
			fitData.yWeightsType = nsl_fit_weight_instrumental;
			curve->setFitData(fitData);
			curve->setYErrorColumn(curCurve->yErrorPlusColumn());
		}

		curve->recalculate();

		//add the child after the fit was calculated so the dock widgets gets the fit results
		//and call retransform() after this to calculate and to paint the data points of the fit-curve
		this->addChild(curve);
		curve->retransform();
	} else {
		beginMacro(i18n("%1: add fit curve", name()));
		this->addChild(curve);
	}

	endMacro();
}

void CartesianPlot::addFourierFilterCurve() {
	auto* curve = new XYFourierFilterCurve("Fourier filter");
	const XYCurve* curCurve = currentCurve();
	if (curCurve) {
		beginMacro( i18n("%1: Fourier filtering of '%2'", name(), curCurve->name()) );
		curve->setName( i18n("Fourier filtering of '%1'", curCurve->name()) );
		curve->setDataSourceType(XYAnalysisCurve::DataSourceType::Curve);
		curve->setDataSourceCurve(curCurve);
	} else {
		beginMacro(i18n("%1: add Fourier filter curve", name()));
	}
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	this->addChild(curve);

	endMacro();
}

void CartesianPlot::addFourierTransformCurve() {
	auto* curve = new XYFourierTransformCurve("Fourier transform");
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	this->addChild(curve);
}

void CartesianPlot::addConvolutionCurve() {
	auto* curve = new XYConvolutionCurve("Convolution");
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	this->addChild(curve);
}

void CartesianPlot::addCorrelationCurve() {
	auto* curve = new XYCorrelationCurve("Auto-/Cross-Correlation");
	curve->setCoordinateSystemIndex(defaultCoordinateSystemIndex());
	this->addChild(curve);
}

/*!
 * public helper function to set a legend object created outside of CartesianPlot, e.g. in \c OriginProjectParser.
 */
void CartesianPlot::addLegend(CartesianPlotLegend* legend) {
	m_legend = legend;
	this->addChild(legend);
}

void CartesianPlot::addLegend() {
	//don't do anything if there's already a legend
	if (m_legend)
		return;

	m_legend = new CartesianPlotLegend("legend");
	this->addChild(m_legend);
	m_legend->retransform();

	//only one legend is allowed -> disable the action
	if (m_menusInitialized)
		addLegendAction->setEnabled(false);
}

void CartesianPlot::addInfoElement() {
	XYCurve* curve = nullptr;
	auto curves = children<XYCurve>();
	if (curves.count())
		curve = curves.first();

	const double pos = xRange().center();

	InfoElement* element = new InfoElement("Info Element", this, curve, pos);
	this->addChild(element);
	element->setParentGraphicsItem(graphicsItem());
	element->retransform(); // must be done, because the custompoint must be retransformed (see https://invent.kde.org/marmsoler/labplot/issues/9)
}

void CartesianPlot::addTextLabel() {
	auto* label = new TextLabel("text label", this);
	this->addChild(label);
	label->setParentGraphicsItem(graphicsItem());
}

void CartesianPlot::addImage() {
	auto* image = new Image("image");
	this->addChild(image);
}

void CartesianPlot::addCustomPoint() {
	auto* point = new CustomPoint(this, "custom point");
	this->addChild(point);
	point->retransform();
}

void CartesianPlot::addReferenceLine() {
	auto* line = new ReferenceLine(this, "reference line");
	this->addChild(line);
	line->retransform();
}

int CartesianPlot::curveCount(){
	return children<XYCurve>().length();
}

const XYCurve* CartesianPlot::getCurve(int index){
	return children<XYCurve>().at(index);
}

double CartesianPlot::cursorPos(int cursorNumber) {
	Q_D(CartesianPlot);
	return ( cursorNumber == 0 ? d->cursor0Pos.x() : d->cursor1Pos.x() );
}

void CartesianPlot::childAdded(const AbstractAspect* child) {
	Q_D(CartesianPlot);
	const auto* curve = qobject_cast<const XYCurve*>(child);
	if (curve) {
		connect(curve, &XYCurve::dataChanged, this, &CartesianPlot::dataChanged);
		connect(curve, &XYCurve::xDataChanged, this, &CartesianPlot::xDataChanged);
		connect(curve, &XYCurve::xErrorTypeChanged, this, &CartesianPlot::dataChanged);
		connect(curve, &XYCurve::xErrorPlusColumnChanged, this, &CartesianPlot::dataChanged);
		connect(curve, &XYCurve::xErrorMinusColumnChanged, this, &CartesianPlot::dataChanged);
		connect(curve, &XYCurve::yDataChanged, this, &CartesianPlot::yDataChanged);
		connect(curve, &XYCurve::yErrorTypeChanged, this, &CartesianPlot::dataChanged);
		connect(curve, &XYCurve::yErrorPlusColumnChanged, this, &CartesianPlot::dataChanged);
		connect(curve, &XYCurve::yErrorMinusColumnChanged, this, &CartesianPlot::dataChanged);
		connect(curve, QOverload<bool>::of(&XYCurve::visibilityChanged),
				this, &CartesianPlot::curveVisibilityChanged);

		//update the legend on changes of the name, line and symbol styles
		connect(curve, &XYCurve::aspectDescriptionChanged, this, &CartesianPlot::updateLegend);
		connect(curve, &XYCurve::aspectDescriptionChanged, this, &CartesianPlot::curveNameChanged);
		connect(curve, &XYCurve::lineTypeChanged, this, &CartesianPlot::updateLegend);
		connect(curve, &XYCurve::linePenChanged, this, &CartesianPlot::updateLegend);
		connect(curve, &XYCurve::linePenChanged, this, static_cast<void (CartesianPlot::*)(QPen)>(&CartesianPlot::curveLinePenChanged));
		connect(curve, &XYCurve::lineOpacityChanged, this, &CartesianPlot::updateLegend);
		connect(curve, &XYCurve::symbolsStyleChanged, this, &CartesianPlot::updateLegend);
		connect(curve, &XYCurve::symbolsSizeChanged, this, &CartesianPlot::updateLegend);
		connect(curve, &XYCurve::symbolsRotationAngleChanged, this, &CartesianPlot::updateLegend);
		connect(curve, &XYCurve::symbolsOpacityChanged, this, &CartesianPlot::updateLegend);
		connect(curve, &XYCurve::symbolsBrushChanged, this, &CartesianPlot::updateLegend);
		connect(curve, &XYCurve::symbolsPenChanged, this, &CartesianPlot::updateLegend);
		connect(curve, &XYCurve::linePenChanged, this, QOverload<QPen>::of(&CartesianPlot::curveLinePenChanged)); // forward to Worksheet to update CursorDock

		updateLegend();
		d->curvesXMinMaxIsDirty = true;
		d->curvesYMinMaxIsDirty = true;

		//in case the first curve is added, check whether we start plotting datetime data
		if (children<XYCurve>().size() == 1) {
			const auto* col = dynamic_cast<const Column*>(curve->xColumn());
			if (col) {
				if (col->columnMode() == AbstractColumn::ColumnMode::DateTime) {
					setUndoAware(false);
					// see format of curves x range
					setXRangeFormat(coordinateSystem(curve->coordinateSystemIndex())->xIndex(), RangeT::Format::DateTime);
					setUndoAware(true);

					//set column's datetime format for all horizontal axis
					//TODO: do we really want this?
					/*for (auto* axis : children<Axis>()) {
						if (axis->orientation() == Axis::Orientation::Horizontal) {
							auto* filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
							xRange(X).setDateTimeFormat(filter->format());
							axis->setUndoAware(false);
							axis->setLabelsDateTimeFormat(xRange(X).dateTimeFormat());
							axis->setUndoAware(true);
						}
					}*/
				}
			}

			col = dynamic_cast<const Column*>(curve->yColumn());
			if (col) {
				if (col->columnMode() == AbstractColumn::ColumnMode::DateTime) {
					setUndoAware(false);
					setYRangeFormat(coordinateSystem(curve->coordinateSystemIndex())->yIndex(), RangeT::Format::DateTime);
					setUndoAware(true);

					//set column's datetime format for all vertical axis
					// TODO: do we really want this?
					/*for (auto* axis : children<Axis>()) {
						if (axis->orientation() == Axis::Orientation::Vertical) {
							auto* filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
							yRange(X).setDateTimeFormat(filter->format());
							axis->setUndoAware(false);
							axis->setLabelsDateTimeFormat(yRange(X).dateTimeFormat());
							axis->setUndoAware(true);
						}
					}*/
				}
			}
		}
		emit curveAdded(curve);

	} else {
		const auto* hist = qobject_cast<const Histogram*>(child);
		if (hist) {
			connect(hist, &Histogram::dataChanged, this, &CartesianPlot::dataChanged);
			connect(hist, &Histogram::visibilityChanged, this, &CartesianPlot::curveVisibilityChanged);

			updateLegend();
		}

		const auto* boxPlot = qobject_cast<const BoxPlot*>(child);
		if (boxPlot) {
			connect(boxPlot, &BoxPlot::dataChanged, this, &CartesianPlot::dataChanged);
// 			connect(boxPlot, &BoxPlot::visibilityChanged, this, &CartesianPlot::curveVisibilityChanged);

			updateLegend();
		}

		const auto* infoElement= qobject_cast<const InfoElement*>(child);
		if (infoElement)
			connect(this, &CartesianPlot::curveRemoved, infoElement, &InfoElement::removeCurve);

		// if an element is hovered, the curves which are handled manually in this class
		// must be unhovered
		const auto* element = static_cast<const WorksheetElement*>(child);
		connect(element, &WorksheetElement::hovered, this, &CartesianPlot::childHovered);
	}

	if (!isLoading()) {
		//if a theme was selected, apply the theme settings for newly added children,
		//load default theme settings otherwise.
		const auto* elem = dynamic_cast<const WorksheetElement*>(child);
		if (elem) {
			if (!d->theme.isEmpty()) {
				KConfig config(ThemeHandler::themeFilePath(d->theme), KConfig::SimpleConfig);
				const_cast<WorksheetElement*>(elem)->loadThemeConfig(config);
			} else {
				KConfig config;
				const_cast<WorksheetElement*>(elem)->loadThemeConfig(config);
			}
		}
	}
}

void CartesianPlot::childRemoved(const AbstractAspect* parent, const AbstractAspect* before, const AbstractAspect* child) {
	Q_UNUSED(parent);
	Q_UNUSED(before);
	if (m_legend == child) {
		if (m_menusInitialized)
			addLegendAction->setEnabled(true);
		m_legend = nullptr;
	} else {
		const auto* curve = qobject_cast<const XYCurve*>(child);
		if (curve) {
			updateLegend();
			emit curveRemoved(curve);
		}
	}
}

/*!
 * \brief CartesianPlot::childHovered
 * Unhover all curves, when another child is hovered. The hover handling for the curves is done in their parent (CartesianPlot),
 * because the hover should set when the curve is hovered and not just the bounding rect (for more see hoverMoveEvent)
 */
void CartesianPlot::childHovered() {
	Q_D(CartesianPlot);
	bool curveSender = dynamic_cast<XYCurve*>(QObject::sender()) != nullptr;
	if (!d->isSelected()) {
		if (d->m_hovered)
			d->m_hovered = false;
		d->update();
	}
	if (!curveSender) {
		for (auto curve: children<XYCurve>())
			curve->setHover(false);
	}
}

void CartesianPlot::updateLegend() {
	if (m_legend)
		m_legend->retransform();
}

bool CartesianPlot::autoScale(bool fullRange) {
	bool updated{ false };

	if (autoScaleX() && autoScaleY())
		updated = scaleAuto();
	else if (autoScaleX())
		updated = scaleAutoX(fullRange);
	else if (autoScaleY())
		updated = scaleAutoY(fullRange);

	return updated;
}

/*!
	called when in one of the curves the data was changed.
	Autoscales the coordinate system and the x-axes, when "auto-scale" is active.
*/
void CartesianPlot::dataChanged() {
	if (project() && project()->isLoading())
		return;

	Q_D(CartesianPlot);
	d->curvesXMinMaxIsDirty = true;
	d->curvesYMinMaxIsDirty = true;
	const bool updated{ autoScale(true) };

	if (!updated || !QObject::sender()) {
		//even if the plot ranges were not changed, either no auto scale active or the new data
		//is within the current ranges and no change of the ranges is required,
		//retransform the curve in order to show the changes
		auto* curve = dynamic_cast<XYCurve*>(QObject::sender());
		if (curve)
			curve->retransform();
		else {
			auto* hist = dynamic_cast<Histogram*>(QObject::sender());
			if (hist)
				hist->retransform();
			else {
				//no sender available, the function was called directly in the file filter (live data source got new data)
				//or in Project::load() -> retransform all available curves since we don't know which curves are affected.
				//TODO: this logic can be very expensive
				for (auto* c : children<XYCurve>()) {
					c->recalcLogicalPoints();
					c->retransform();
				}
			}
		}
	}
}

/*!
	called when in one of the curves the x-data was changed.
	Autoscales the coordinate system and the x-axes, when "auto-scale" is active.
*/
void CartesianPlot::xDataChanged() {
	DEBUG(Q_FUNC_INFO)
	if (project() && project()->isLoading())
		return;

	Q_D(CartesianPlot);
	if (d->suppressRetransform)
		return;

	d->curvesXMinMaxIsDirty = true;
	bool updated = false;
	if (autoScaleX())
		updated = this->scaleAutoX();

	if (!updated) {
		//even if the plot ranges were not changed, either no auto scale active or the new data
		//is within the current ranges and no change of the ranges is required,
		//retransform the curve in order to show the changes
		auto* curve = dynamic_cast<XYCurve*>(QObject::sender());
		if (curve)
			curve->retransform();
		else {
			auto* hist = dynamic_cast<Histogram*>(QObject::sender());
			if (hist)
				hist->retransform();
		}
	}

	//in case there is only one curve and its column mode was changed, check whether we start plotting datetime data
	if (children<XYCurve>().size() == 1) {
		auto* curve = dynamic_cast<XYCurve*>(QObject::sender());
		if (curve) {
			const AbstractColumn* col = curve->xColumn();
			const auto xRangeFormat{ xRange().format() };
			if (col->columnMode() == AbstractColumn::ColumnMode::DateTime && xRangeFormat != RangeT::Format::DateTime) {
				setUndoAware(false);
				setXRangeFormat(RangeT::Format::DateTime);
				setUndoAware(true);
			}
		}
	}
	emit curveDataChanged(dynamic_cast<XYCurve*>(QObject::sender()));
}

/*!
	called when in one of the curves the x-data was changed.
	Autoscales the coordinate system and the x-axes, when "auto-scale" is active.
*/
void CartesianPlot::yDataChanged() {
	if (project() && project()->isLoading())
		return;

	Q_D(CartesianPlot);
	if (d->suppressRetransform)
		return;

	d->curvesYMinMaxIsDirty = true;
	bool updated = false;
	if (autoScaleY())
		updated = this->scaleAutoY();

	if (!updated) {
		//even if the plot ranges were not changed, either no auto scale active or the new data
		//is within the current ranges and no change of the ranges is required,
		//retransform the curve in order to show the changes
		auto* curve = dynamic_cast<XYCurve*>(QObject::sender());
		if (curve)
			curve->retransform();
		else {
			auto* hist = dynamic_cast<Histogram*>(QObject::sender());
			if (hist)
				hist->retransform();
		}
	}

	//in case there is only one curve and its column mode was changed, check whether we start plotting datetime data
	if (children<XYCurve>().size() == 1) {
		auto* curve = dynamic_cast<XYCurve*>(QObject::sender());
		if (curve) {
			const AbstractColumn* col = curve->yColumn();
			const auto yRangeFormat{ yRange().format() };
			if (col && col->columnMode() == AbstractColumn::ColumnMode::DateTime && yRangeFormat != RangeT::Format::DateTime) {
				setUndoAware(false);
				setYRangeFormat(RangeT::Format::DateTime);
				setUndoAware(true);
			}
		}
	}
	emit curveDataChanged(dynamic_cast<XYCurve*>(QObject::sender()));
}

void CartesianPlot::curveVisibilityChanged() {
	Q_D(CartesianPlot);
	d->curvesXMinMaxIsDirty = true;
	d->curvesYMinMaxIsDirty = true;
	updateLegend();
	if (autoScaleX() && autoScaleY())
		this->scaleAuto();
	else if (autoScaleX())
		this->scaleAutoX();
	else if (autoScaleY())
		this->scaleAutoY();

	emit curveVisibilityChangedSignal();
}

void CartesianPlot::curveLinePenChanged(QPen pen) {
	const auto* curve = qobject_cast<const XYCurve*>(QObject::sender());
	emit curveLinePenChanged(pen, curve->name());
}

void CartesianPlot::setMouseMode(MouseMode mouseMode) {
	Q_D(CartesianPlot);

	d->mouseMode = mouseMode;
	d->setHandlesChildEvents(mouseMode != MouseMode::Selection);

	QList<QGraphicsItem*> items = d->childItems();
	if (mouseMode == MouseMode::Selection) {
		d->setZoomSelectionBandShow(false);
		d->setCursor(Qt::ArrowCursor);
		for (auto* item : items)
			item->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
	} else {
		if (mouseMode == MouseMode::ZoomSelection || mouseMode == MouseMode::Crosshair)
			d->setCursor(Qt::CrossCursor);
		else if (mouseMode == MouseMode::ZoomXSelection)
			d->setCursor(Qt::SizeHorCursor);
		else if (mouseMode == MouseMode::ZoomYSelection)
			d->setCursor(Qt::SizeVerCursor);

		for (auto* item : items)
			item->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
	}

	//when doing zoom selection, prevent the graphics item from being movable
	//if it's currently movable (no worksheet layout available)
	const auto* worksheet = dynamic_cast<const Worksheet*>(parentAspect());
	if (worksheet) {
		if (mouseMode == MouseMode::Selection) {
			if (worksheet->layout() != Worksheet::Layout::NoLayout)
				graphicsItem()->setFlag(QGraphicsItem::ItemIsMovable, false);
			else
				graphicsItem()->setFlag(QGraphicsItem::ItemIsMovable, true);
		} else   //zoom m_selection
			graphicsItem()->setFlag(QGraphicsItem::ItemIsMovable, false);
	}

	emit mouseModeChanged(mouseMode);
}

void CartesianPlot::setLocked(bool locked) {
	Q_D(CartesianPlot);
	d->locked = locked;
}
bool CartesianPlot::isLocked() const {
	Q_D(const CartesianPlot);
	return d->locked;
}

// auto scale

void CartesianPlot::scaleAutoTriggered() {
	QAction* action = dynamic_cast<QAction*>(QObject::sender());
	if (!action)
		return;

	if (action == scaleAutoAction)
		scaleAuto();
	else if (action == scaleAutoXAction)
		setAutoScaleX(true);
	else if (action == scaleAutoYAction)
		setAutoScaleY(true);
}

bool CartesianPlot::scaleAutoX(bool fullRange) {
	DEBUG(Q_FUNC_INFO << ", full range = " << fullRange)
	Q_D(CartesianPlot);
	if (d->curvesXMinMaxIsDirty) {
		calculateCurvesXMinMax(fullRange);

		/* TODO:
		//take the size of the error bar cap into account if error bars with caps are plotted
		double errorBarsCapSize = -1;
		for (auto* curve : this->children<const XYCurve>()) {
			if (curve->yErrorType() == XYCurve::ErrorType::NoError)
				continue;

			if (curve->errorBarsType() != XYCurve::ErrorBarsType::WithEnds)
				continue;

			if ( (curve->yErrorType() == XYCurve::ErrorType::Symmetric && curve->yErrorPlusColumn())
				|| (curve->yErrorType() == XYCurve::ErrorType::Asymmetric && (curve->yErrorPlusColumn() && curve->yErrorMinusColumn())) )
				errorBarsCapSize = qMax(errorBarsCapSize, curve->errorBarsCapSize());
		}

		if (errorBarsCapSize > 0) {
			// must be done, because retransformScales uses xMin/xMax
			if (d->curvesXMin != d->xMin && d->curvesXMin != qInf())
				d->xMin = d->curvesXMin;

			if (d->curvesXMax != d->xMax && d->curvesXMax != -qInf())
				d->xMax = d->curvesXMax;
			// When the previous scale is completely different. The mapTo functions scale with wrong values. To prevent
			// this a rescale must be done.
			// The errorBarsCapSize is in Scene coordinates. So this value must be transformed into a logical value. Due
			// to nonlinear scalings it cannot only be multiplied with a scaling factor and depends on the position of the
			// column value
			// dirty hack: call setIsLoading(true) to suppress the call of retransform() in retransformScales() since a
			// retransform is already done at the end of this function
			setIsLoading(true);
			d->retransformScales();
			setIsLoading(false);
			QPointF point = coordinateSystem()->mapLogicalToScene(QPointF(d->curvesXMin, 0), AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			point.setX(point.x() - errorBarsCapSize/2.);
			point = coordinateSystem()->mapSceneToLogical(point, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			// Problem is, when the scaling is not linear (for example log(x)) and the minimum is 0. In this
			// case mapLogicalToScene returns (0,0) which is smaller than the curves minimum
			if (point.x() < d->curvesXMin)
				d->curvesXMin = point.x();

			point = coordinateSystem()->mapLogicalToScene(QPointF(d->curvesXMax, 0), AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			point.setX(point.x() + errorBarsCapSize/2.);
			point = coordinateSystem()->mapSceneToLogical(point, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			if (point.x() > d->curvesXMax)
				d->curvesXMax = point.x();
		}
		*/
		d->curvesYMinMaxIsDirty = true;
		d->curvesXMinMaxIsDirty = false;
	}

	// if no curve: do not reset to [0, 1] but don't change

	//TODO: sets default x range
	DEBUG(Q_FUNC_INFO << ", x range = " << xRange().toStdString() << "., curves x range = " << d->curvesXRange.toStdString())
	bool update = false;
	if (!qFuzzyCompare(d->curvesXRange.start(), xRange().start()) && !qIsInf(d->curvesXRange.start())) {
		d->xRange().start() = d->curvesXRange.start();
		update = true;
	}

	if (!qFuzzyCompare(d->curvesXRange.end(), xRange().end()) && !qIsInf(d->curvesXRange.end()) ) {
		d->xRange().end() = d->curvesXRange.end();
		update = true;
	}

	if (update) {
		DEBUG(Q_FUNC_INFO << ", set new x range = " << xRange().toStdString())
		//in case min and max are equal (e.g. if we plot a single point), subtract/add 10% of the value
		if (xRange().isZero()) {
			const double value{ xRange().start() };
			if (!qFuzzyIsNull(value))
				d->xRange().setRange(value * 0.9, value * 1.1);
			else
				d->xRange().setRange(-0.1, 0.1);
		} else {
			const double offset{ xRange().size() * d->autoScaleOffsetFactor };
			d->xRange().extend(offset);
		}
		d->retransformScales();
	}

	return update;
}

// TODO: copy paste code?
bool CartesianPlot::scaleAutoY(bool fullRange) {
	DEBUG(Q_FUNC_INFO << ", full range = " << fullRange)
	Q_D(CartesianPlot);

	if (d->curvesYMinMaxIsDirty) {
		calculateCurvesYMinMax(fullRange);

		/* TODO:
		//take the size of the error bar cap into account if error bars with caps are plotted
		double errorBarsCapSize = -1;
		for (auto* curve : this->children<const XYCurve>()) {
			if (curve->xErrorType() == XYCurve::ErrorType::NoError)
				continue;

			if (curve->errorBarsType() != XYCurve::ErrorBarsType::WithEnds)
				continue;

			if ( (curve->xErrorType() == XYCurve::ErrorType::Symmetric && curve->xErrorPlusColumn())
				|| (curve->xErrorType() == XYCurve::ErrorType::Asymmetric && (curve->xErrorPlusColumn() && curve->xErrorMinusColumn())) )
				errorBarsCapSize = qMax(errorBarsCapSize, curve->errorBarsCapSize());
		}

		if (errorBarsCapSize > 0) {
			if (d->curvesYMin != d->yMin && d->curvesYMin != qInf())
				d->yMin = d->curvesYMin;

			if (d->curvesYMax != d->yMax && d->curvesYMax != -qInf())
				d->yMax = d->curvesYMax;
			setIsLoading(true);
			d->retransformScales();
			setIsLoading(false);
			QPointF point = coordinateSystem()->mapLogicalToScene(QPointF(0, d->curvesYMin), AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			point.setY(point.y() + errorBarsCapSize);
			point = coordinateSystem()->mapSceneToLogical(point, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			if (point.y() < d->curvesYMin)
				d->curvesYMin = point.y();

			point = coordinateSystem()->mapLogicalToScene(QPointF(0, d->curvesYMax), AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			point.setY(point.y() - errorBarsCapSize);
			point = coordinateSystem()->mapSceneToLogical(point, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			if (point.y() > d->curvesYMax)
				d->curvesYMax = point.y();
		}
		*/

		d->curvesXMinMaxIsDirty = true;
		d->curvesYMinMaxIsDirty = false;
	}

	bool update = false;
	//TODO: sets default y range
	DEBUG(Q_FUNC_INFO << ", y range = " << yRange().toStdString() << "., curves y range = " << d->curvesYRange.toStdString())
	if (!qFuzzyCompare(d->curvesYRange.start(), yRange().start()) && !qIsInf(d->curvesYRange.start()) ) {
		d->yRange().start() = d->curvesYRange.start();
		update = true;
	}

	if (!qFuzzyCompare(d->curvesYRange.end(), yRange().end()) && !qIsInf(d->curvesYRange.end()) ) {
		d->yRange().end() = d->curvesYRange.end();
		update = true;
	}
	if (update) {
		DEBUG(Q_FUNC_INFO << ", set new y range = " << yRange().toStdString())
		//in case min and max are equal (e.g. if we plot a single point), subtract/add 10% of the value
		if (yRange().isZero()) {
			const double value{ yRange().start() };
			if (!qFuzzyIsNull(value))
				d->yRange().setRange(value * 0.9, value * 1.1);
			else
				d->yRange().setRange(-0.1, 0.1);
		} else {
			d->yRange().extend( yRange().size() * d->autoScaleOffsetFactor );
		}
		d->retransformScales();
	}

	return update;
}


// TODO: copy paste code?
bool CartesianPlot::scaleAuto(const bool fullRange) {
	DEBUG(Q_FUNC_INFO)
	Q_D(CartesianPlot);

	if (d->curvesXMinMaxIsDirty) {
		calculateCurvesXMinMax(fullRange);

		/* TODO
		//take the size of the error bar cap into account if error bars with caps are plotted
		double errorBarsCapSize = -1;
		for (auto* curve : this->children<const XYCurve>()) {
			if (curve->yErrorType() == XYCurve::ErrorType::NoError)
				continue;

			if (curve->errorBarsType() != XYCurve::ErrorBarsType::WithEnds)
				continue;

			if ( (curve->yErrorType() == XYCurve::ErrorType::Symmetric && curve->yErrorPlusColumn())
				|| (curve->yErrorType() == XYCurve::ErrorType::Asymmetric && (curve->yErrorPlusColumn() && curve->yErrorMinusColumn())) )
				errorBarsCapSize = qMax(errorBarsCapSize, curve->errorBarsCapSize());
		}

		if (errorBarsCapSize > 0) {
			if (d->curvesXMin != d->xMin && d->curvesXMin != qInf())
				d->xMin = d->curvesXMin;

			if (d->curvesXMax != d->xMax && d->curvesXMax != -qInf())
				d->xMax = d->curvesXMax;
			setIsLoading(true);
			d->retransformScales();
			setIsLoading(false);
			QPointF point = coordinateSystem()->mapLogicalToScene(QPointF(d->curvesXMin, 0), AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			point.setX(point.x() - errorBarsCapSize);
			point = coordinateSystem()->mapSceneToLogical(point, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			if (point.x() < d->curvesXMin)
				d->curvesXMin = point.x();

			point = coordinateSystem()->mapLogicalToScene(QPointF(d->curvesXMax, 0), AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			point.setX(point.x() + errorBarsCapSize);
			point = coordinateSystem()->mapSceneToLogical(point, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			if (point.x() > d->curvesXMax)
				d->curvesXMax = point.x();
		}
		*/
		d->curvesXMinMaxIsDirty = false;
	}

	if (d->curvesYMinMaxIsDirty) {
		calculateCurvesYMinMax();

		/*
		//take the size of the error bar cap into account if error bars with caps are plotted
		double errorBarsCapSize = -1;
		for (auto* curve : this->children<const XYCurve>()) {
			if (curve->xErrorType() == XYCurve::ErrorType::NoError)
				continue;

			if (curve->errorBarsType() != XYCurve::ErrorBarsType::WithEnds)
				continue;

			if ( (curve->xErrorType() == XYCurve::ErrorType::Symmetric && curve->xErrorPlusColumn())
				|| (curve->xErrorType() == XYCurve::ErrorType::Asymmetric && (curve->xErrorPlusColumn() && curve->xErrorMinusColumn())) )
				errorBarsCapSize = qMax(errorBarsCapSize, curve->errorBarsCapSize());
		}

		if (errorBarsCapSize > 0) {
			if (d->curvesYMin != d->yMin && d->curvesYMin != qInf())
				d->yMin = d->curvesYMin;

			if (d->curvesYMax != d->yMax && d->curvesYMax != -qInf())
				d->yMax = d->curvesYMax;
			setIsLoading(true);
			d->retransformScales();
			setIsLoading(false);
			QPointF point = coordinateSystem()->mapLogicalToScene(QPointF(0, d->curvesYMin), AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			point.setY(point.y() + errorBarsCapSize);
			point = coordinateSystem()->mapSceneToLogical(point, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			if (point.y() < d->curvesYMin)
				d->curvesYMin = point.y();

			point = coordinateSystem()->mapLogicalToScene(QPointF(0, d->curvesYMax), AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			point.setY(point.y() - errorBarsCapSize);
			point = coordinateSystem()->mapSceneToLogical(point, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
			if (point.y() > d->curvesYMax)
				d->curvesYMax = point.y();
		}
		*/
		d->curvesYMinMaxIsDirty = false;
	}

	bool updateX = false;
	bool updateY = false;

	auto& xRange{ d->xRange() };
	auto& yRange{ d->yRange() };

	if (!qFuzzyCompare(d->curvesXRange.start(), xRange.start()) && !qIsInf(d->curvesXRange.start()) ) {
		xRange.start() = d->curvesXRange.start();
		updateX = true;
	}
	if (!qFuzzyCompare(d->curvesXRange.end(), xRange.end()) && !qIsInf(d->curvesXRange.end()) ) {
		xRange.end() = d->curvesXRange.end();
		updateX = true;
	}
	if (!qFuzzyCompare(d->curvesYRange.start(), yRange.start()) && !qIsInf(d->curvesYRange.start()) ) {
		yRange.start() = d->curvesYRange.start();
		updateY = true;
	}
	if (!qFuzzyCompare(d->curvesYRange.end(), yRange.end()) && !qIsInf(d->curvesYRange.end()) ) {
		yRange.end() = d->curvesYRange.end();
		updateY = true;
	}
	DEBUG( Q_FUNC_INFO << ", xrange = " << xRange.toStdString() << ", yrange = " << yRange.toStdString() );

	//TODO: copy/paste code (see also scaleAutoX(), scaleAutoY())
	if (updateX || updateY) {
		if (updateX) {
			//in case min and max are equal (e.g. if we plot a single point), subtract/add 10% of the value
			if (xRange.isZero()) {
				const double value{ xRange.start() };
				if (!qFuzzyIsNull(value))
					xRange.setRange(value * 0.9, value * 1.1);
				else
					xRange.setRange(-0.1, 0.1);
			} else {
				xRange.extend( xRange.size() * d->autoScaleOffsetFactor );
			}
			setAutoScaleX(true);
		}
		if (updateY) {
			//in case min and max are equal (e.g. if we plot a single point), subtract/add 10% of the value
			if (yRange.isZero()) {
				const double value{ yRange.start() };
				if (!qFuzzyIsNull(value))
					yRange.setRange(value * 0.9, value * 1.1);
				else
					yRange.setRange(-0.1, 0.1);
			} else {
				yRange.extend( yRange.size()*d->autoScaleOffsetFactor );
			}
			setAutoScaleY(true);
		}
		d->retransformScales();
	}

	return (updateX || updateY);
}

/*!
 * Calculates and sets curves y min and max. This function does not respect the range
 * of the y axis
 */
void CartesianPlot::calculateCurvesXMinMax(bool completeRange) {
	DEBUG(Q_FUNC_INFO << ", complete range = " << completeRange)
	Q_D(CartesianPlot);

	d->curvesXRange.setRange(qInf(), -qInf());

	//loop over all xy-curves and determine the maximum and minimum x-values
	for (const auto* curve : this->children<const XYCurve>()) {
		if (!curve->isVisible())
			continue;

		auto* xColumn = curve->xColumn();
		if (!xColumn)
			continue;

		Range<int> indexRange{0, 0};
		if (d->rangeType == RangeType::Free && curve->yColumn()
				&& !completeRange) {
			DEBUG(Q_FUNC_INFO << ", free incomplete range with y column. y range = " << yRange().toStdString())
			//TODO: Range
			curve->yColumn()->indicesMinMax(yRange().start(), yRange().end(), indexRange.start(), indexRange.end());
			//if (indexRange.end() < curve->yColumn()->rowCount())	// NO
			//	indexRange.end()++;
		} else {
			DEBUG(Q_FUNC_INFO << ", else. range type = " << (int)d->rangeType)
			switch (d->rangeType) {
			case RangeType::Free:
				indexRange.setRange(0, xColumn->rowCount());
				break;
			case RangeType::Last:
				indexRange.setRange(xColumn->rowCount() - d->rangeLastValues, xColumn->rowCount());
				break;
			case RangeType::First:
				indexRange.setRange(0, d->rangeFirstValues);
				break;
			}
		}
		DEBUG(Q_FUNC_INFO << ", index range = " << indexRange.toStdString())

		auto range{d->curvesXRange};	// curves range
		curve->minMaxX(indexRange, range, true);

		if (range.start() < d->curvesXRange.start())
			d->curvesXRange.start() = range.start();

		if (range.end() > d->curvesXRange.end())
			d->curvesXRange.end() = range.end();
		DEBUG(Q_FUNC_INFO << ", curves x range i = " << d->curvesXRange.toStdString())
	}
	DEBUG(Q_FUNC_INFO << ", curves x range = " << d->curvesXRange.toStdString())

	//loop over all histograms and determine the maximum and minimum x-value
	for (const auto* curve : this->children<const Histogram>()) {
		if (!curve->isVisible())
			continue;
		if (!curve->dataColumn())
			continue;

		const double min = curve->xMinimum();
		if (d->curvesXRange.start() > min)
			d->curvesXRange.start() = min;

		const double max = curve->xMaximum();
		if (max > d->curvesXRange.end())
			d->curvesXRange.end() = max;
	}

	//loop over all box plots and determine the maximum and minimum x-values
	for (const auto* curve : this->children<const BoxPlot>()) {
		if (!curve->isVisible())
			continue;
		if (!curve->dataColumn())
			continue;

		const double min = curve->xMinimum();
		if (d->curvesXRange.start() > min)
			d->curvesXRange.start() = min;

		const double max = curve->xMaximum();
		if (max > d->curvesXRange.end())
			d->curvesXRange.end() = max;
	}
}

/*!
 * Calculates and sets curves y min and max. This function does not respect the range
 * of the x axis
 */
void CartesianPlot::calculateCurvesYMinMax(bool completeRange) {
	Q_D(CartesianPlot);

	d->curvesYRange.setRange(qInf(), -qInf());
	Range<double> range{d->curvesYRange};

	//loop over all xy-curves and determine the maximum and minimum y-values
	for (const auto* curve : this->children<const XYCurve>()) {
		if (!curve->isVisible())
			continue;

		auto* yColumn = curve->yColumn();
		if (!yColumn)
			continue;

		Range<int> indexRange{0, 0};
		if (d->rangeType == RangeType::Free && curve->xColumn() && !completeRange) {
			curve->xColumn()->indicesMinMax(xRange().start(), xRange().end(), indexRange.start(), indexRange.end());
			//if (indexRange.end() < curve->xColumn()->rowCount())	// No
			//	indexRange.end()++; // because minMaxY excludes indexMax
		} else {
			switch (d->rangeType) {
				case RangeType::Free:
					indexRange.setRange(0, yColumn->rowCount());
					break;
				case RangeType::Last:
					indexRange.setRange(yColumn->rowCount() - d->rangeLastValues, yColumn->rowCount());
					break;
				case RangeType::First:
					indexRange.setRange(0, d->rangeFirstValues);
					break;
			}
		}

		curve->minMaxY(indexRange, range, true);

		if (range.start() < d->curvesYRange.start())
			d->curvesYRange.start() = range.start();

		if (range.end() > d->curvesYRange.end())
			d->curvesYRange.end() = range.end();
	}

	//loop over all histograms and determine the maximum y-value
	for (const auto* curve : this->children<const Histogram>()) {
		if (!curve->isVisible())
			continue;

		const double min = curve->yMinimum();
		if (d->curvesYRange.start() > min)
			d->curvesYRange.start() = min;

		const double max = curve->yMaximum();
		if (max > d->curvesYRange.end())
			d->curvesYRange.end() = max;
	}

	//loop over all box plots and determine the maximum y-value
	for (const auto* curve : this->children<const BoxPlot>()) {
		if (!curve->isVisible())
			continue;

		const double min = curve->yMinimum();
		if (d->curvesYRange.start() > min)
			d->curvesYRange.start() = min;

		const double max = curve->yMaximum();
		if (max > d->curvesYRange.end())
			d->curvesYRange.end() = max;
	}
}

// zoom

void CartesianPlot::zoomIn() {
	Q_D(CartesianPlot);

	setUndoAware(false);
	setAutoScaleX(false);
	setAutoScaleY(false);
	setUndoAware(true);
	d->curvesXMinMaxIsDirty = true;
	d->curvesYMinMaxIsDirty = true;
	zoom(true, true); //zoom in x
	zoom(false, true); //zoom in y
	d->retransformScales();
}

void CartesianPlot::zoomOut() {
	Q_D(CartesianPlot);

	setUndoAware(false);
	setAutoScaleX(false);
	setAutoScaleY(false);
	setUndoAware(true);
	d->curvesXMinMaxIsDirty = true;
	d->curvesYMinMaxIsDirty = true;
	zoom(true, false); //zoom out x
	zoom(false, false); //zoom out y
	d->retransformScales();
}

void CartesianPlot::zoomInX() {
	Q_D(CartesianPlot);

	setUndoAware(false);
	setAutoScaleX(false);
	setUndoAware(true);
	d->curvesYMinMaxIsDirty = true;
	zoom(true, true); //zoom in x
	if (autoScaleY())
		return;

	d->retransformScales();
}

void CartesianPlot::zoomOutX() {
	Q_D(CartesianPlot);

	setUndoAware(false);
	setAutoScaleX(false);
	setUndoAware(true);
	d->curvesYMinMaxIsDirty = true;
	zoom(true, false); //zoom out x

	if (autoScaleY())
		return;

	d->retransformScales();
}

void CartesianPlot::zoomInY() {
	Q_D(CartesianPlot);

	setUndoAware(false);
	setAutoScaleY(false);
	setUndoAware(true);
	d->curvesYMinMaxIsDirty = true;
	zoom(false, true); //zoom in y

	if (autoScaleX())
		return;

	d->retransformScales();
}

void CartesianPlot::zoomOutY() {
	Q_D(CartesianPlot);

	setUndoAware(false);
	setAutoScaleY(false);
	setUndoAware(true);
	d->curvesYMinMaxIsDirty = true;
	zoom(false, false); //zoom out y

	if (autoScaleX())
		return;

	d->retransformScales();
}

/*!
 * helper function called in other zoom*() functions
 * and doing the actual change of the data ranges.
 * @param x if set to \true the x-range is modified, the y-range for \c false
 * @param in the "zoom in" is performed if set to \c \true, "zoom out" for \c false
 */
void CartesianPlot::zoom(bool x, bool in) {
	Q_D(CartesianPlot);

	Range<double> range{ x ? xRange() : yRange() };

	double factor = m_zoomFactor;
	if (in)
		factor = 1/factor;

	switch (range.scale()) {
	case RangeT::Scale::Linear: {
		double oldRange = range.size();
		double newRange = range.size() * factor;
		range.extend((newRange - oldRange) / 2);
		break;
	}
	case RangeT::Scale::Log10:
	case RangeT::Scale::Log10Abs: {
		double oldRange = log10(range.end()) - log10(range.start());
		double newRange = oldRange * factor;
		range.end() *= pow(10, (newRange - oldRange) / 2.);
		range.start() /= pow(10, (newRange - oldRange) / 2.);
		break;
	}
	case RangeT::Scale::Log2:
	case RangeT::Scale::Log2Abs: {
		double oldRange = log2(range.end()) - log2(range.start());
		double newRange = oldRange * factor;
		range.end() *= pow(2, (newRange - oldRange) / 2.);
		range.start() /= pow(2, (newRange - oldRange) / 2.);
		break;
	}
	case RangeT::Scale::Ln:
	case RangeT::Scale::LnAbs: {
		double oldRange = log(range.end()) - log(range.start());
		double newRange = oldRange * factor;
		range.end() *= exp((newRange - oldRange) / 2.);
		range.start() /= exp((newRange - oldRange) / 2.);
		break;
	}
	case RangeT::Scale::Sqrt:
	case RangeT::Scale::X2:
		break;
	}

	if (range.finite())
		x ? d->xRange() = range : d->yRange() = range;
}

/*!
 * helper function called in other shift*() functions
 * and doing the actual change of the data ranges.
 * @param x if set to \true the x-range is modified, the y-range for \c false
 * @param leftOrDown the "shift left" for x or "shift dows" for y is performed if set to \c \true,
 * "shift right" or "shift up" for \c false
 */
void CartesianPlot::shift(bool x, bool leftOrDown) {
	Q_D(CartesianPlot);

	Range<double> range{ x ? xRange() : yRange() };
	double offset = 0.0, factor = 0.1;

	if (leftOrDown)
		factor *= -1.;

	switch (range.scale()) {
	case RangeT::Scale::Linear: {
		offset = range.size() * factor;
		range += offset;
		break;
	}
	case RangeT::Scale::Log10:
	case RangeT::Scale::Log10Abs: {
		offset = (log10(range.end()) - log10(range.start())) * factor;
		range *= pow(10, offset);
		break;
	}
	case RangeT::Scale::Log2:
	case RangeT::Scale::Log2Abs: {
		offset = (log2(range.end()) - log2(range.start())) * factor;
		range *= pow(2, offset);
		break;
	}
	case RangeT::Scale::Ln:
	case RangeT::Scale::LnAbs: {
		offset = (log10(range.end()) - log10(range.start())) * factor;
		range *= exp(offset);
		break;
	}
	case RangeT::Scale::Sqrt:
	case RangeT::Scale::X2:
		break;
	}

	if (range.finite())
		x ? d->xRange() = range : d->yRange() = range;
}

void CartesianPlot::shiftLeftX() {
	Q_D(CartesianPlot);

	setUndoAware(false);
	setAutoScaleX(false);
	setUndoAware(true);
	d->curvesYMinMaxIsDirty = true;
	shift(true, true);

	if (autoScaleY() && scaleAutoY())
		return;

	d->retransformScales();
}

void CartesianPlot::shiftRightX() {
	Q_D(CartesianPlot);

	setUndoAware(false);
	setAutoScaleX(false);
	setUndoAware(true);
	d->curvesYMinMaxIsDirty = true;
	shift(true, false);

	if (autoScaleY() && scaleAutoY())
		return;

	d->retransformScales();
}

void CartesianPlot::shiftUpY() {
	Q_D(CartesianPlot);

	setUndoAware(false);
	setAutoScaleY(false);
	setUndoAware(true);
	d->curvesXMinMaxIsDirty = true;
	shift(false, false);

	if (autoScaleX() && scaleAutoX())
		return;

	d->retransformScales();
}

void CartesianPlot::shiftDownY() {
	Q_D(CartesianPlot);

	setUndoAware(false);
	setAutoScaleY(false);
	setUndoAware(true);
	d->curvesXMinMaxIsDirty = true;
	shift(false, true);

	if (autoScaleX() && scaleAutoX())
		return;

	d->retransformScales();
}

void CartesianPlot::cursor() {
	Q_D(CartesianPlot);
	d->retransformScales();
}

void CartesianPlot::mousePressZoomSelectionMode(QPointF logicPos) {
	Q_D(CartesianPlot);
	d->mousePressZoomSelectionMode(logicPos);
}
void CartesianPlot::mousePressCursorMode(int cursorNumber, QPointF logicPos) {
	Q_D(CartesianPlot);
	d->mousePressCursorMode(cursorNumber, logicPos);
}
void CartesianPlot::mouseMoveZoomSelectionMode(QPointF logicPos) {
	Q_D(CartesianPlot);
	d->mouseMoveZoomSelectionMode(logicPos);
}
void CartesianPlot::mouseMoveCursorMode(int cursorNumber, QPointF logicPos) {
	Q_D(CartesianPlot);
	d->mouseMoveCursorMode(cursorNumber, logicPos);
}

void CartesianPlot::mouseReleaseZoomSelectionMode() {
	Q_D(CartesianPlot);
	d->mouseReleaseZoomSelectionMode();
}

void CartesianPlot::mouseHoverZoomSelectionMode(QPointF logicPos) {
	Q_D(CartesianPlot);
	d->mouseHoverZoomSelectionMode(logicPos);
}

void CartesianPlot::mouseHoverOutsideDataRect() {
	Q_D(CartesianPlot);
	d->mouseHoverOutsideDataRect();
}

//##############################################################################
//######  SLOTs for changes triggered via QActions in the context menu  ########
//##############################################################################
void CartesianPlot::visibilityChanged() {
	Q_D(CartesianPlot);
	this->setVisible(!d->isVisible());
}

//#####################################################################
//################### Private implementation ##########################
//#####################################################################
CartesianPlotPrivate::CartesianPlotPrivate(CartesianPlot* plot) : AbstractPlotPrivate(plot), q(plot) {
	setData(0, static_cast<int>(WorksheetElement::WorksheetElementName::NameCartesianPlot));
	m_cursor0Text.prepare();
	m_cursor1Text.prepare();
}

/*!
	updates the position of plot rectangular in scene coordinates to \c r and recalculates the scales.
	The size of the plot corresponds to the size of the plot area, the area which is filled with the background color etc.
	and which can pose the parent item for several sub-items (like TextLabel).
	Note, the size of the area used to define the coordinate system doesn't need to be equal to this plot area.
	Also, the size (=bounding box) of CartesianPlot can be greater than the size of the plot area.
 */
void CartesianPlotPrivate::retransform() {
	DEBUG(Q_FUNC_INFO)
	if (suppressRetransform)
		return;

	PERFTRACE("CartesianPlotPrivate::retransform()");
	prepareGeometryChange();
	setPos(rect.x() + rect.width()/2, rect.y() + rect.height()/2);

	updateDataRect();
	retransformScales();

	//plotArea position is always (0, 0) in parent's coordinates, don't need to update here
	q->plotArea()->setRect(rect);

	//call retransform() for the title and the legend (if available)
	//when a predefined position relative to the (Left, Centered etc.) is used,
	//the actual position needs to be updated on plot's geometry changes.
	if (q->title())
		q->title()->retransform();
	if (q->m_legend)
		q->m_legend->retransform();

	WorksheetElementContainerPrivate::recalcShapeAndBoundingRect();
}

/*
 * calculate x and y scales from scence range and logical range (x/y range) for all coordinate systems
 */
void CartesianPlotPrivate::retransformScales() {
	int i{1}; // debugging
	for (auto& range : xRanges)
		DEBUG( Q_FUNC_INFO << ", x range " << i++ << " = " << range.toStdString() << ", scale = " << (int)range.scale() );
	i = 1;	// debugging
	for (auto& range : yRanges)
		DEBUG( Q_FUNC_INFO << ", y range " << i++ << " = " << range.toStdString() << ", scale = " << (int)range.scale() );
	PERFTRACE(Q_FUNC_INFO);

	QVector<CartesianScale*> scales;
	static const int breakGap = 20;
	Range<double> sceneRange, logicalRange;
	Range<double> plotSceneRange{dataRect.x(), dataRect.x() + dataRect.width()};

	//////////// Create X-scales ////////////////
	// loop over all cSystems and use the correct x/yRanges to set scales
	DEBUG(Q_FUNC_INFO << ", number of coordinate systems = " << q->m_coordinateSystems.size())
	i = 0; // debugging
	for (const auto& cSystem : q->m_coordinateSystems) {
		const int xRangeIndex{ dynamic_cast<CartesianCoordinateSystem*>(cSystem)->xIndex() };	// use x range of current cSystem
		const auto xRange{ xRanges.at(xRangeIndex) };
		DEBUG(Q_FUNC_INFO << ", coordinate system " << i++ <<  ", x range index = " << xRangeIndex)
		//check ranges for nonlinear scales
		if (xRange.scale() != RangeT::Scale::Linear)
			checkXRange();

		//check whether we have x-range breaks - the first break, if available, should be valid
		bool hasValidBreak = (xRangeBreakingEnabled && !xRangeBreaks.list.isEmpty() && xRangeBreaks.list.first().isValid());
		if (!hasValidBreak) {	//no breaks available -> range goes from the start to the end of the plot
			sceneRange = plotSceneRange;
			logicalRange = xRange;

			//TODO: how should we handle the case sceneRange.length() == 0?
			//(to reproduce, create plots and adjust the spacing/pading to get zero size for the plots)
			if (sceneRange.length() > 0)
				scales << this->createScale(xRange.scale(), sceneRange, logicalRange);
		} else {
			double sceneEndLast = plotSceneRange.start();
			double logicalEndLast = xRange.start();
			for (const auto& rb : xRangeBreaks.list) {
				if (!rb.isValid())
					break;

				//current range goes from the end of the previous one (or from the plot beginning) to curBreak.start
				sceneRange.start() = sceneEndLast;
				if (&rb == &xRangeBreaks.list.first()) sceneRange.start() += breakGap;
				sceneRange.end() = plotSceneRange.start() + plotSceneRange.size() * rb.position;
				logicalRange = Range<double>(logicalEndLast, rb.range.start());

				if (sceneRange.length() > 0)
					scales << this->createScale(xRange.scale(), sceneRange, logicalRange);

				sceneEndLast = sceneRange.end();
				logicalEndLast = rb.range.end();
			}

			//add the remaining range going from the last available range break to the end of the plot (=end of the x-data range)
			sceneRange.setRange(sceneEndLast + breakGap, plotSceneRange.end());
			logicalRange.setRange(logicalEndLast, xRange.end());

			if (sceneRange.length() > 0)
				scales << this->createScale(xRange.scale(), sceneRange, logicalRange);
		}

		//set x scales of cSystem
		dynamic_cast<CartesianCoordinateSystem*>(cSystem)->setXScales(scales);
		scales.clear();
	}

	//////// Create Y-scales /////////////

	plotSceneRange.setRange(dataRect.y() + dataRect.height(), dataRect.y());

	// loop over all cSystems
	i = 0; // debugging
	for (auto cSystem : q->m_coordinateSystems) {
		const int yRangeIndex{ dynamic_cast<CartesianCoordinateSystem*>(cSystem)->yIndex() };	// use y range of current cSystem
		const auto yRange{ yRanges.at(yRangeIndex) };
		DEBUG(Q_FUNC_INFO << ", coordinate system " << i++ <<  ", y range index = " << yRangeIndex)
		DEBUG(Q_FUNC_INFO << ", yrange = " << yRange.toStdString())
		//check ranges for nonlinear scales
		if (yRange.scale() != RangeT::Scale::Linear)
			checkYRange();

		//check whether we have y-range breaks - the first break, if available, should be valid
		bool hasValidBreak = (yRangeBreakingEnabled && !yRangeBreaks.list.isEmpty() && yRangeBreaks.list.first().isValid());
		if (!hasValidBreak) {	//no breaks available -> range goes from the start to the end of the plot
			sceneRange = plotSceneRange;
			logicalRange = yRange;

			if (sceneRange.length() > 0)
				scales << this->createScale(yRange.scale(), sceneRange, logicalRange);
		} else {
			double sceneEndLast = plotSceneRange.start();
			double logicalEndLast = yRange.start();
			for (const auto& rb : yRangeBreaks.list) {
				if (!rb.isValid())
					break;

				//current range goes from the end of the previous one (or from the plot beginning) to curBreak.start
				sceneRange.start() = sceneEndLast;
				if (&rb == &yRangeBreaks.list.first()) sceneRange.start() -= breakGap;
				sceneRange.end() = plotSceneRange.start() + plotSceneRange.size() * rb.position;
				logicalRange = Range<double>(logicalEndLast, rb.range.start());

				if (sceneRange.length() > 0)
					scales << this->createScale(yRange.scale(), sceneRange, logicalRange);

				sceneEndLast = sceneRange.end();
				logicalEndLast = rb.range.end();
			}

			//add the remaining range going from the last available range break to the end of the plot (=end of the y-data range)
			sceneRange.setRange(sceneEndLast - breakGap, plotSceneRange.end());
			logicalRange.setRange(logicalEndLast, yRange.end());

			if (sceneRange.length() > 0)
				scales << this->createScale(yRange.scale(), sceneRange, logicalRange);
		}

		//set y scales of cSystem
		dynamic_cast<CartesianCoordinateSystem*>(cSystem)->setYScales(scales);
		scales.clear();
	}

	//calculate the changes in x and y and save the current values for xMin, xMax, yMin, yMax
	double deltaXMin = xRange().start() - xPrevRange.start();
	double deltaXMax = xRange().end() - xPrevRange.end();
	double deltaYMin = yRange().start() - yPrevRange.start();
	double deltaYMax = yRange().end() - yPrevRange.end();

	if (!qFuzzyIsNull(deltaXMin))
		emit q->xMinChanged(xRange().start());
	if (!qFuzzyIsNull(deltaXMax))
		emit q->xMaxChanged(xRange().end());
	if (!qFuzzyIsNull(deltaYMin))
		emit q->yMinChanged(yRange().start());
	if (!qFuzzyIsNull(deltaYMax))
		emit q->yMaxChanged(yRange().end());

	xPrevRange = xRange();
	yPrevRange = yRange();

	//adjust all auto-scale axes
	for (auto* axis : q->children<Axis>()) {
		if (!axis->autoScale())
			continue;

		if (axis->orientation() == Axis::Orientation::Horizontal) {	// x
			if (!qFuzzyIsNull(deltaXMax)) {
				axis->setUndoAware(false);
				axis->setSuppressRetransform(true);
				axis->setEnd(xRange().end());
				axis->setUndoAware(true);
				axis->setSuppressRetransform(false);
			}
			if (!qFuzzyIsNull(deltaXMin)) {
				axis->setUndoAware(false);
				axis->setSuppressRetransform(true);
				axis->setStart(xRange().start());
				axis->setUndoAware(true);
				axis->setSuppressRetransform(false);
			}
			//TODO;
// 			if (axis->position() == Axis::Position::Custom && deltaYMin != 0) {
// 				axis->setOffset(axis->offset() + deltaYMin, false);
// 			}
		} else {	// y
			if (!qFuzzyIsNull(deltaYMax)) {
				axis->setUndoAware(false);
				axis->setSuppressRetransform(true);
				axis->setEnd(yRange().end());
				axis->setUndoAware(true);
				axis->setSuppressRetransform(false);
			}
			if (!qFuzzyIsNull(deltaYMin)) {
				axis->setUndoAware(false);
				axis->setSuppressRetransform(true);
				axis->setStart(yRange().start());
				axis->setUndoAware(true);
				axis->setSuppressRetransform(false);
			}

			//TODO
// 			if (axis->position() == Axis::Position::Custom && deltaXMin != 0) {
// 				axis->setOffset(axis->offset() + deltaXMin, false);
// 			}
		}
	}
	// call retransform() on the parent to trigger the update of all axes and curves.
	//no need to do this on load since all plots are retransformed again after the project is loaded.
	if (!q->isLoading())
		q->retransform();
}

/*
 * calculates the rectangular of the are showing the actual data (plot's rect minus padding),
 * in plot's coordinates.
 */
void CartesianPlotPrivate::updateDataRect() {
	dataRect = mapRectFromScene(rect);

	double paddingLeft = horizontalPadding;
	double paddingRight = rightPadding;
	double paddingTop = verticalPadding;
	double paddingBottom = bottomPadding;
	if (symmetricPadding) {
		paddingRight = horizontalPadding;
		paddingBottom = verticalPadding;
	}

	dataRect.setX(dataRect.x() + paddingLeft);
	dataRect.setY(dataRect.y() + paddingTop);

	double newHeight = dataRect.height() - paddingBottom;
	if (newHeight < 0)
		newHeight = 0;
	dataRect.setHeight(newHeight);

	double newWidth = dataRect.width() - paddingRight;
	if (newWidth < 0)
		newWidth = 0;
	dataRect.setWidth(newWidth);
}

void CartesianPlotPrivate::rangeChanged() {
	DEBUG(Q_FUNC_INFO)
	curvesXMinMaxIsDirty = true;
	curvesYMinMaxIsDirty = true;
	if (autoScaleX() && autoScaleY())
		q->scaleAuto();
	else if (autoScaleX())
		q->scaleAutoX();
	else if (autoScaleY())
		q->scaleAutoY();
}

void CartesianPlotPrivate::xRangeFormatChanged() {
	DEBUG(Q_FUNC_INFO)
	for (auto* axis : q->children<Axis>()) {
		//TODO: only if x range of axis's plot range is changed
		if (axis->orientation() == Axis::Orientation::Horizontal)
			axis->retransformTickLabelStrings();
	}
}

void CartesianPlotPrivate::yRangeFormatChanged() {
	DEBUG(Q_FUNC_INFO)
	for (auto* axis : q->children<Axis>()) {
		//TODO: only if x range of axis's plot range is changed
		if (axis->orientation() == Axis::Orientation::Vertical)
			axis->retransformTickLabelStrings();
	}
}

/*!
 * don't allow any negative values for the x range when log or sqrt scalings are used
 */
void CartesianPlotPrivate::checkXRange() {
	double min = 0.01;

	//TODO: refactor
	if (xRange().start() <= 0.0) {
		(min < xRange().end() * min) ? xRange().start() = min : xRange().start() = xRange().end() * min;
		emit q->xMinChanged(xRange().start());
	} else if (xRange().end() <= 0.0) {
		(-min > xRange().start() * min) ? xRange().end() = -min : xRange().end() = xRange().start() * min;
		emit q->xMaxChanged(xRange().end());
	}
}

/*!
 * don't allow any negative values for the y range when log or sqrt scalings are used
 */
void CartesianPlotPrivate::checkYRange() {
	double min = 0.01;

	//TODO: refactor
	if (yRange().start() <= 0.0) {
		(min < yRange().end()*min) ? yRange().start() = min : yRange().start() = yRange().end()*min;
		emit q->yMinChanged(yRange().start());
	} else if (yRange().end() <= 0.0) {
		(-min > yRange().start()*min) ? yRange().end() = -min : yRange().end() = yRange().start()*min;
		emit q->yMaxChanged(yRange().end());
	}
}

CartesianScale* CartesianPlotPrivate::createScale(RangeT::Scale scale, const Range<double> &sceneRange, const Range<double> &logicalRange) {
	DEBUG( Q_FUNC_INFO << ", scene start/end = " << sceneRange.toStdString() << ", logical start/end = " << logicalRange.toStdString() );
// 	Interval<double> interval (logicalStart-0.01, logicalEnd+0.01); //TODO: move this to CartesianScale
	//TODO
	Range<double> range(std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max());
// 	Interval<double> interval (logicalStart, logicalEnd);
	if (scale == RangeT::Scale::Linear)
		return CartesianScale::createLinearScale(range, sceneRange, logicalRange);
	else
		return CartesianScale::createLogScale(range, sceneRange, logicalRange, scale);
}

/*!
 * Reimplemented from QGraphicsItem.
 */
QVariant CartesianPlotPrivate::itemChange(GraphicsItemChange change, const QVariant &value) {
	if (change == QGraphicsItem::ItemPositionChange) {
		const QPointF& itemPos = value.toPointF();//item's center point in parent's coordinates;
		const qreal x = itemPos.x();
		const qreal y = itemPos.y();

		//calculate the new rect and forward the changes to the frontend
		QRectF newRect;
		const qreal w = rect.width();
		const qreal h = rect.height();
		newRect.setX(x - w/2);
		newRect.setY(y - h/2);
		newRect.setWidth(w);
		newRect.setHeight(h);
		emit q->rectChanged(newRect);
	}
	return QGraphicsItem::itemChange(change, value);
}

//##############################################################################
//##################################  Events  ##################################
//##############################################################################

/*!
 * \brief CartesianPlotPrivate::mousePressEvent
 * In this function only basic stuff is done. The mousePressEvent is forwarded to the Worksheet, which
 * has access to all cartesian plots and can apply the changes to all plots if the option "applyToAll"
 * is set. The worksheet calls then the corresponding mousepressZoomMode/CursorMode function in this class
 * This is done for mousePress, mouseMove and mouseRelease event
 * This function sends a signal with the logical position, because this is the only value which is the same
 * in all plots. Using the scene coordinates is not possible
 * \param event
 */
void CartesianPlotPrivate::mousePressEvent(QGraphicsSceneMouseEvent* event) {
	const auto* cSystem{ defaultCoordinateSystem() };
	if (mouseMode == CartesianPlot::MouseMode::Selection) {
		if (!locked && dataRect.contains(event->pos())) {
			panningStarted = true;
			m_panningStart = event->pos();
			setCursor(Qt::ClosedHandCursor);
		}
	} else if (mouseMode == CartesianPlot::MouseMode::ZoomSelection
		|| mouseMode == CartesianPlot::MouseMode::ZoomXSelection
		|| mouseMode == CartesianPlot::MouseMode::ZoomYSelection) {
		const QPointF logicalPos = cSystem->mapSceneToLogical(event->pos(), AbstractCoordinateSystem::MappingFlag::Limit);
		emit q->mousePressZoomSelectionModeSignal(logicalPos);
	} else if (mouseMode == CartesianPlot::MouseMode::Cursor) {
		setCursor(Qt::SizeHorCursor);
		const QPointF logicalPos = cSystem->mapSceneToLogical(event->pos(), AbstractCoordinateSystem::MappingFlag::Limit);
		double cursorPenWidth2 = cursorPen.width()/2.;
		if (cursorPenWidth2 < 10.)
			cursorPenWidth2 = 10.;
		if (cursor0Enable && qAbs(event->pos().x() - cSystem->mapLogicalToScene(QPointF(cursor0Pos.x(), yRange().start())).x()) < cursorPenWidth2) {
			selectedCursor = 0;
		} else if (cursor1Enable && qAbs(event->pos().x() - cSystem->mapLogicalToScene(QPointF(cursor1Pos.x(), yRange().start())).x()) < cursorPenWidth2) {
			selectedCursor = 1;
		} else if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
			cursor1Enable = true;
			selectedCursor = 1;
			emit q->cursor1EnableChanged(cursor1Enable);
		} else {
			cursor0Enable = true;
			selectedCursor = 0;
			emit q->cursor0EnableChanged(cursor0Enable);
		}
		emit q->mousePressCursorModeSignal(selectedCursor, logicalPos);
	}

	QGraphicsItem::mousePressEvent(event);
}

void CartesianPlotPrivate::mousePressZoomSelectionMode(QPointF logicalPos) {
	const auto* cSystem{ defaultCoordinateSystem() };
	const QPointF scenePos = cSystem->mapLogicalToScene(logicalPos, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
	if (mouseMode == CartesianPlot::MouseMode::ZoomSelection) {
		if (logicalPos.x() < xRange().start())
			logicalPos.setX(xRange().start());

		if (logicalPos.x() > xRange().end())
			logicalPos.setX(xRange().end());

		if (logicalPos.y() < yRange().start())
			logicalPos.setY(yRange().start());

		if (logicalPos.y() > yRange().end())
			logicalPos.setY(yRange().end());

		m_selectionStart = scenePos;

	} else if (mouseMode == CartesianPlot::MouseMode::ZoomXSelection) {
		logicalPos.setY(yRange().start()); // must be done, because the other plots can have other ranges, value must be in the scenes
		m_selectionStart.setX(scenePos.x());
		m_selectionStart.setY(dataRect.y());
	} else if (mouseMode == CartesianPlot::MouseMode::ZoomYSelection) {
		logicalPos.setX(xRange().start()); // must be done, because the other plots can have other ranges, value must be in the scenes
		m_selectionStart.setX(dataRect.x());
		m_selectionStart.setY(scenePos.y());
	}
	m_selectionEnd = m_selectionStart;
	m_selectionBandIsShown = true;
}

void CartesianPlotPrivate::mousePressCursorMode(int cursorNumber, QPointF logicalPos) {

	cursorNumber == 0 ? cursor0Enable = true : cursor1Enable = true;

	QPointF p1(logicalPos.x(), yRange().start());
	QPointF p2(logicalPos.x(), yRange().end());

	if (cursorNumber == 0) {
		cursor0Pos.setX(logicalPos.x());
		cursor0Pos.setY(0);
	} else {
		cursor1Pos.setX(logicalPos.x());
		cursor1Pos.setY(0);
	}
	update();
}

void CartesianPlotPrivate::updateCursor() {
	update();
}

void CartesianPlotPrivate::setZoomSelectionBandShow(bool show) {
	m_selectionBandIsShown = show;
}

void CartesianPlotPrivate::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
	const auto* cSystem{ defaultCoordinateSystem() };
	if (mouseMode == CartesianPlot::MouseMode::Selection) {
		if (panningStarted && dataRect.contains(event->pos()) ) {
			//don't retransform on small mouse movement deltas
			const int deltaXScene = (m_panningStart.x() - event->pos().x());
			const int deltaYScene = (m_panningStart.y() - event->pos().y());
			if (qAbs(deltaXScene) < 5 && qAbs(deltaYScene) < 5)
				return;

			const QPointF logicalEnd = cSystem->mapSceneToLogical(event->pos());
			const QPointF logicalStart = cSystem->mapSceneToLogical(m_panningStart);

			//handle the change in x
			switch (xRange().scale()) {
			case RangeT::Scale::Linear: {
				const double deltaX = (logicalStart.x() - logicalEnd.x());
				xRange().translate(deltaX);
				break;
			}
			case RangeT::Scale::Log10:
			case RangeT::Scale::Log10Abs: {
				const double deltaX = log10(logicalStart.x()) - log10(logicalEnd.x());
				xRange() *= pow(10, deltaX);
				break;
			}
			case RangeT::Scale::Log2:
			case RangeT::Scale::Log2Abs: {
				const double deltaX = log2(logicalStart.x()) - log2(logicalEnd.x());
				xRange() *= pow(2, deltaX);
				break;
			}
			case RangeT::Scale::Ln:
			case RangeT::Scale::LnAbs: {
				const double deltaX = log(logicalStart.x()) - log(logicalEnd.x());
				xRange() *= exp(deltaX);
				break;
			}
			case RangeT::Scale::Sqrt:
			case RangeT::Scale::X2:
				break;
			}

			//handle the change in y
			switch (yRange().scale()) {
			case RangeT::Scale::Linear: {
				const double deltaY = (logicalStart.y() - logicalEnd.y());
				yRange().translate(deltaY);
				break;
			}
			case RangeT::Scale::Log10:
			case RangeT::Scale::Log10Abs: {
				const double deltaY = log10(logicalStart.y()) - log10(logicalEnd.y());
				yRange() *= pow(10, deltaY);
				break;
			}
			case RangeT::Scale::Log2:
			case RangeT::Scale::Log2Abs: {
				const double deltaY = log2(logicalStart.y()) - log2(logicalEnd.y());
				yRange() *= pow(2, deltaY);
				break;
			}
			case RangeT::Scale::Ln:
			case RangeT::Scale::LnAbs: {
				const double deltaY = log(logicalStart.y()) - log(logicalEnd.y());
				yRange() *= exp(deltaY);
				break;
			}
			case RangeT::Scale::Sqrt:
			case RangeT::Scale::X2:
				break;
			}

			q->setUndoAware(false);
			q->setAutoScaleX(false);
			q->setAutoScaleY(false);
			q->setUndoAware(true);

			retransformScales();
			m_panningStart = event->pos();
		} else
			QGraphicsItem::mouseMoveEvent(event);
	} else if (mouseMode == CartesianPlot::MouseMode::ZoomSelection
		|| mouseMode == CartesianPlot::MouseMode::ZoomXSelection
		|| mouseMode == CartesianPlot::MouseMode::ZoomYSelection) {
		QGraphicsItem::mouseMoveEvent(event);
		if ( !boundingRect().contains(event->pos()) ) {
			q->info(QString());
			return;
		}
		const QPointF logicalPos = cSystem->mapSceneToLogical(event->pos(), AbstractCoordinateSystem::MappingFlag::Limit);
		emit q->mouseMoveZoomSelectionModeSignal(logicalPos);

	} else if (mouseMode == CartesianPlot::MouseMode::Cursor) {
		QGraphicsItem::mouseMoveEvent(event);
		if (!boundingRect().contains(event->pos())) {
			q->info(i18n("Not inside of the bounding rect"));
			return;
		}

		// updating treeview data and cursor position
		// updating cursor position is done in Worksheet, because
		// multiple plots must be updated
		const QPointF logicalPos = cSystem->mapSceneToLogical(event->pos(), AbstractCoordinateSystem::MappingFlag::Limit);
		emit q->mouseMoveCursorModeSignal(selectedCursor, logicalPos);
	}
}

void CartesianPlotPrivate::mouseMoveZoomSelectionMode(QPointF logicalPos) {
	QString info;
	const auto* cSystem{ defaultCoordinateSystem() };
	const auto xRangeFormat{ xRange().format() };
	const auto yRangeFormat{ yRange().format() };
	const auto xRangeDateTimeFormat{ xRange().dateTimeFormat() };
	const auto yRangeDateTimeFormat{ yRange().dateTimeFormat() };
	const QPointF logicalStart = cSystem->mapSceneToLogical(m_selectionStart, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);

	if (mouseMode == CartesianPlot::MouseMode::ZoomSelection) {
		m_selectionEnd = cSystem->mapLogicalToScene(logicalPos, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
		QPointF logicalEnd = logicalPos;
	if (xRangeFormat == RangeT::Format::Numeric)
			info = QString::fromUtf8("Δx=") + QString::number(logicalEnd.x()-logicalStart.x());
		else
			info = i18n("from x=%1 to x=%2", QDateTime::fromMSecsSinceEpoch(logicalStart.x()).toString(xRangeDateTimeFormat),
						QDateTime::fromMSecsSinceEpoch(logicalEnd.x()).toString(xRangeDateTimeFormat));

		info += QLatin1String(", ");
		if (yRangeFormat == RangeT::Format::Numeric)
			info += QString::fromUtf8("Δy=") + QString::number(logicalEnd.y()-logicalStart.y());
		else
			info += i18n("from y=%1 to y=%2", QDateTime::fromMSecsSinceEpoch(logicalStart.y()).toString(xRangeDateTimeFormat),
						 QDateTime::fromMSecsSinceEpoch(logicalEnd.y()).toString(xRangeDateTimeFormat));
	} else if (mouseMode == CartesianPlot::MouseMode::ZoomXSelection) {
		logicalPos.setY(yRange().start()); // must be done, because the other plots can have other ranges, value must be in the scenes
		m_selectionEnd.setX(cSystem->mapLogicalToScene(logicalPos, CartesianCoordinateSystem::MappingFlag::SuppressPageClipping).x());//event->pos().x());
		m_selectionEnd.setY(dataRect.bottom());
		QPointF logicalEnd = logicalPos;
	if (xRangeFormat == RangeT::Format::Numeric)
			info = QString::fromUtf8("Δx=") + QString::number(logicalEnd.x()-logicalStart.x());
		else
			info = i18n("from x=%1 to x=%2", QDateTime::fromMSecsSinceEpoch(logicalStart.x()).toString(xRangeDateTimeFormat),
						QDateTime::fromMSecsSinceEpoch(logicalEnd.x()).toString(xRangeDateTimeFormat));
	} else if (mouseMode == CartesianPlot::MouseMode::ZoomYSelection) {
		m_selectionEnd.setX(dataRect.right());
		logicalPos.setX(xRange().start()); // must be done, because the other plots can have other ranges, value must be in the scenes
		m_selectionEnd.setY(cSystem->mapLogicalToScene(logicalPos, CartesianCoordinateSystem::MappingFlag::SuppressPageClipping).y());//event->pos().y());
		QPointF logicalEnd = logicalPos;
		if (yRangeFormat == RangeT::Format::Numeric)
			info = QString::fromUtf8("Δy=") + QString::number(logicalEnd.y()-logicalStart.y());
		else
			info = i18n("from y=%1 to y=%2", QDateTime::fromMSecsSinceEpoch(logicalStart.y()).toString(xRangeDateTimeFormat),
						QDateTime::fromMSecsSinceEpoch(logicalEnd.y()).toString(xRangeDateTimeFormat));
	}
	q->info(info);
	update();
}

void CartesianPlotPrivate::mouseMoveCursorMode(int cursorNumber, QPointF logicalPos) {
	const auto xRangeFormat{ xRange().format() };
	const auto xRangeDateTimeFormat{ xRange().dateTimeFormat() };

	QPointF p1(logicalPos.x(), 0);
	cursorNumber == 0 ? cursor0Pos = p1 : cursor1Pos = p1;

	QString info;
	if (xRangeFormat == RangeT::Format::Numeric)
		info = QString::fromUtf8("x=") + QString::number(logicalPos.x());
	else
		info = i18n("x=%1", QDateTime::fromMSecsSinceEpoch(logicalPos.x()).toString(xRangeDateTimeFormat));
	q->info(info);
	update();
}

void CartesianPlotPrivate::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
	if (mouseMode == CartesianPlot::MouseMode::Selection) {
		setCursor(Qt::ArrowCursor);
		panningStarted = false;

		//TODO: why do we do this all the time?!?!
		const QPointF& itemPos = pos();//item's center point in parent's coordinates;
		const qreal x = itemPos.x();
		const qreal y = itemPos.y();

		//calculate the new rect and set it
		QRectF newRect;
		const qreal w = rect.width();
		const qreal h = rect.height();
		newRect.setX(x - w/2);
		newRect.setY(y - h/2);
		newRect.setWidth(w);
		newRect.setHeight(h);

		suppressRetransform = true;
		q->setRect(newRect);
		suppressRetransform = false;

		QGraphicsItem::mouseReleaseEvent(event);
	} else if (mouseMode == CartesianPlot::MouseMode::ZoomSelection || mouseMode == CartesianPlot::MouseMode::ZoomXSelection || mouseMode == CartesianPlot::MouseMode::ZoomYSelection) {
		emit q->mouseReleaseZoomSelectionModeSignal();
	}
}

void CartesianPlotPrivate::mouseReleaseZoomSelectionMode() {
	//don't zoom if very small region was selected, avoid occasional/unwanted zooming
	if ( qAbs(m_selectionEnd.x()-m_selectionStart.x()) < 20 || qAbs(m_selectionEnd.y()-m_selectionStart.y()) < 20 ) {
		m_selectionBandIsShown = false;
		return;
	}
	bool retransformPlot = true;

	//determine the new plot ranges
	const auto* cSystem{ defaultCoordinateSystem() };
	QPointF logicalZoomStart = cSystem->mapSceneToLogical(m_selectionStart, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
	QPointF logicalZoomEnd = cSystem->mapSceneToLogical(m_selectionEnd, AbstractCoordinateSystem::MappingFlag::SuppressPageClipping);
	if (m_selectionEnd.x() > m_selectionStart.x())
		xRange().setRange(logicalZoomStart.x(), logicalZoomEnd.x());
	else
		xRange().setRange(logicalZoomEnd.x(), logicalZoomStart.x());

	if (m_selectionEnd.y() > m_selectionStart.y())
		yRange().setRange(logicalZoomEnd.y(), logicalZoomStart.y());
	else
		yRange().setRange(logicalZoomStart.y(), logicalZoomEnd.y());

	if (mouseMode == CartesianPlot::MouseMode::ZoomSelection) {
		curvesXMinMaxIsDirty = true;
		curvesYMinMaxIsDirty = true;
		q->setAutoScaleX(false);
		q->setAutoScaleY(false);
	} else if (mouseMode == CartesianPlot::MouseMode::ZoomXSelection) {
		curvesYMinMaxIsDirty = true;
		q->setAutoScaleX(false);
		if (q->autoScaleY() && q->scaleAutoY())
			retransformPlot = false;
	} else if (mouseMode == CartesianPlot::MouseMode::ZoomYSelection) {
		curvesXMinMaxIsDirty = true;
		q->setAutoScaleY(false);
		if (q->autoScaleX() && q->scaleAutoX())
			retransformPlot = false;
	}

	if (retransformPlot)
		retransformScales();

	m_selectionBandIsShown = false;
}

void CartesianPlotPrivate::wheelEvent(QGraphicsSceneWheelEvent* event) {
	if (locked)
		return;

	//determine first, which axes are selected and zoom only in the corresponding direction.
	//zoom the entire plot if no axes selected.
	bool zoomX = false;
	bool zoomY = false;
	for (auto* axis : q->children<Axis>()) {
		if (!axis->graphicsItem()->isSelected() && !axis->isHovered())
			continue;

		if (axis->orientation() == Axis::Orientation::Horizontal)
			zoomX  = true;
		else
			zoomY = true;
	}

	if (event->delta() > 0) {
		if (!zoomX && !zoomY) {
			//no special axis selected -> zoom in everything
			q->zoomIn();
		} else {
			if (zoomX) q->zoomInX();
			if (zoomY) q->zoomInY();
		}
	} else {
		if (!zoomX && !zoomY) {
			//no special axis selected -> zoom in everything
			q->zoomOut();
		} else {
			if (zoomX) q->zoomOutX();
			if (zoomY) q->zoomOutY();
		}
	}
}

void CartesianPlotPrivate::keyPressEvent(QKeyEvent* event) {
	if (event->key() == Qt::Key_Escape) {
		q->setMouseMode(CartesianPlot::MouseMode::Selection);
		m_selectionBandIsShown = false;
	} else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right
		|| event->key() == Qt::Key_Up ||event->key() == Qt::Key_Down) {

		const auto* worksheet = static_cast<const Worksheet*>(q->parentAspect());
		if (worksheet->layout() == Worksheet::Layout::NoLayout) {
			const int delta = 5;
			QRectF rect = q->rect();

			if (event->key() == Qt::Key_Left) {
				rect.setX(rect.x() - delta);
				rect.setWidth(rect.width() - delta);
			} else if (event->key() == Qt::Key_Right) {
				rect.setX(rect.x() + delta);
				rect.setWidth(rect.width() + delta);
			} else if (event->key() == Qt::Key_Up) {
				rect.setY(rect.y() - delta);
				rect.setHeight(rect.height() - delta);
			} else if (event->key() == Qt::Key_Down) {
				rect.setY(rect.y() + delta);
				rect.setHeight(rect.height() + delta);
			}

			q->setRect(rect);
		}
	}

	QGraphicsItem::keyPressEvent(event);
}

void CartesianPlotPrivate::hoverMoveEvent(QGraphicsSceneHoverEvent* event) {
	QPointF point = event->pos();
	QString info;
	const auto* cSystem{ defaultCoordinateSystem() };
	const auto xRangeFormat{ xRange().format() };
	const auto yRangeFormat{ yRange().format() };
	const auto xRangeDateTimeFormat{ xRange().dateTimeFormat() };
	const auto yRangeDateTimeFormat{ yRange().dateTimeFormat() };
	if (dataRect.contains(point)) {
		QPointF logicalPoint = cSystem->mapSceneToLogical(point);

		if ((mouseMode == CartesianPlot::MouseMode::ZoomSelection)
			|| mouseMode == CartesianPlot::MouseMode::Selection
			|| mouseMode == CartesianPlot::MouseMode::Crosshair) {
			info = "x=";
			if (xRangeFormat == RangeT::Format::Numeric)
				 info += QString::number(logicalPoint.x());
			else
				info += QDateTime::fromMSecsSinceEpoch(logicalPoint.x()).toString(xRangeDateTimeFormat);

			info += ", y=";
			if (yRangeFormat == RangeT::Format::Numeric)
				info += QString::number(logicalPoint.y());
			else
				info += QDateTime::fromMSecsSinceEpoch(logicalPoint.y()).toString(yRangeDateTimeFormat);
		}

		if (mouseMode == CartesianPlot::MouseMode::ZoomSelection && !m_selectionBandIsShown) {
			emit q->mouseHoverZoomSelectionModeSignal(logicalPoint);
		} else if (mouseMode == CartesianPlot::MouseMode::ZoomXSelection && !m_selectionBandIsShown) {
			info = "x=";
			if (xRangeFormat == RangeT::Format::Numeric)
				 info += QString::number(logicalPoint.x());
			else
				info += QDateTime::fromMSecsSinceEpoch(logicalPoint.x()).toString(xRangeDateTimeFormat);
			emit q->mouseHoverZoomSelectionModeSignal(logicalPoint);
		} else if (mouseMode == CartesianPlot::MouseMode::ZoomYSelection && !m_selectionBandIsShown) {
			info = "y=";
			if (yRangeFormat == RangeT::Format::Numeric)
				info += QString::number(logicalPoint.y());
			else
				info += QDateTime::fromMSecsSinceEpoch(logicalPoint.y()).toString(yRangeDateTimeFormat);
			emit q->mouseHoverZoomSelectionModeSignal(logicalPoint);
		} else if (mouseMode == CartesianPlot::MouseMode::Selection) {
			// hover the nearest curve to the mousepointer
			// hovering curves is implemented in the parent, because no ignoreEvent() exists
			// for it. Checking all curves and hover the first
			bool curve_hovered = false;
			const auto& curves = q->children<Curve>();
			for (int i = curves.count() - 1; i >= 0; i--) { // because the last curve is above the other curves
				if (curve_hovered){ // if a curve is already hovered, disable hover for the rest
					curves[i]->setHover(false);
					continue;
				}
				if (curves[i]->activateCurve(event->pos())) {
					curves[i]->setHover(true);
					curve_hovered = true;
					continue;
				}
				curves[i]->setHover(false);
			}
		} else if (mouseMode == CartesianPlot::MouseMode::Crosshair) {
			m_crosshairPos = event->pos();
			update();
		} else if (mouseMode == CartesianPlot::MouseMode::Cursor) {
			info = "x=";
			if (yRangeFormat == RangeT::Format::Numeric)
				info += QString::number(logicalPoint.x());
			else
				info += QDateTime::fromMSecsSinceEpoch(logicalPoint.x()).toString(xRangeDateTimeFormat);

			double cursorPenWidth2 = cursorPen.width()/2.;
			if (cursorPenWidth2 < 10.)
				cursorPenWidth2 = 10.;
			if ((cursor0Enable && qAbs(point.x() - defaultCoordinateSystem()->mapLogicalToScene(QPointF(cursor0Pos.x(), yRange().start())).x()) < cursorPenWidth2) ||
					(cursor1Enable && qAbs(point.x() - defaultCoordinateSystem()->mapLogicalToScene(QPointF(cursor1Pos.x(), yRange().start())).x()) < cursorPenWidth2))
				setCursor(Qt::SizeHorCursor);
			else
				setCursor(Qt::ArrowCursor);

			update();
		}
	} else
		emit q->mouseHoverOutsideDataRectSignal();

	q->info(info);

	QGraphicsItem::hoverMoveEvent(event);
}

void CartesianPlotPrivate::mouseHoverOutsideDataRect() {
	m_insideDataRect = false;
	update();
}

void CartesianPlotPrivate::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
	QVector<XYCurve*> curves = q->children<XYCurve>();
	for (auto* curve : curves)
		curve->setHover(false);

	m_hovered = false;
	QGraphicsItem::hoverLeaveEvent(event);
}

void CartesianPlotPrivate::mouseHoverZoomSelectionMode(QPointF logicPos) {
	m_insideDataRect = true;

	const auto* cSystem{ defaultCoordinateSystem() };
	if (mouseMode == CartesianPlot::MouseMode::ZoomSelection && !m_selectionBandIsShown) {

	} else if (mouseMode == CartesianPlot::MouseMode::ZoomXSelection && !m_selectionBandIsShown) {
		QPointF p1(logicPos.x(), yRange().start());
		QPointF p2(logicPos.x(), yRange().end());
		m_selectionStartLine.setP1(cSystem->mapLogicalToScene(p1, CartesianCoordinateSystem::MappingFlag::Limit));
		m_selectionStartLine.setP2(cSystem->mapLogicalToScene(p2, CartesianCoordinateSystem::MappingFlag::Limit));
	} else if (mouseMode == CartesianPlot::MouseMode::ZoomYSelection && !m_selectionBandIsShown) {
		QPointF p1(xRange().start(), logicPos.y());
		QPointF p2(xRange().end(), logicPos.y());
		m_selectionStartLine.setP1(cSystem->mapLogicalToScene(p1, CartesianCoordinateSystem::MappingFlag::Limit));
		m_selectionStartLine.setP2(cSystem->mapLogicalToScene(p2, CartesianCoordinateSystem::MappingFlag::Limit));
	}

	update(); // because if previous another selection mode was selected, the lines must be deleted
}

void CartesianPlotPrivate::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) {
	Q_UNUSED(option)
	Q_UNUSED(widget)

	if (!isVisible())
		return;

	if (m_printing)
		return;

	if ((mouseMode == CartesianPlot::MouseMode::ZoomXSelection
		|| mouseMode == CartesianPlot::MouseMode::ZoomYSelection)
		&& (!m_selectionBandIsShown) && m_insideDataRect) {
		painter->setPen(zoomSelectPen);
		painter->drawLine(m_selectionStartLine);
	} else if (m_selectionBandIsShown) {
		QPointF selectionStart = m_selectionStart;
		if (m_selectionStart.x() > dataRect.right())
			selectionStart.setX(dataRect.right());
		if (m_selectionStart.x() < dataRect.left())
			selectionStart.setX(dataRect.left());
		if (m_selectionStart.y() > dataRect.bottom())
			selectionStart.setY(dataRect.bottom());
		if (m_selectionStart.y() < dataRect.top())
			selectionStart.setY(dataRect.top());

		QPointF selectionEnd = m_selectionEnd;
		if (m_selectionEnd.x() > dataRect.right())
			selectionEnd.setX(dataRect.right());
		if (m_selectionEnd.x() < dataRect.left())
			selectionEnd.setX(dataRect.left());
		if (m_selectionEnd.y() > dataRect.bottom())
			selectionEnd.setY(dataRect.bottom());
		if (m_selectionEnd.y() < dataRect.top())
			selectionEnd.setY(dataRect.top());
		painter->save();
		painter->setPen(zoomSelectPen);
		painter->drawRect(QRectF(selectionStart, selectionEnd));
		painter->setBrush(Qt::blue);
		painter->setOpacity(0.2);
		painter->drawRect(QRectF(selectionStart, selectionEnd));
		painter->restore();
	} else if (mouseMode == CartesianPlot::MouseMode::Crosshair) {
		painter->setPen(crossHairPen);

		//horizontal line
		double x1 = dataRect.left();
		double y1 = m_crosshairPos.y();
		double x2 = dataRect.right();
		double y2 = y1;
		painter->drawLine(x1, y1, x2, y2);

		//vertical line
		x1 = m_crosshairPos.x();
		y1 = dataRect.bottom();
		x2 = x1;
		y2 = dataRect.top();
		painter->drawLine(x1, y1, x2, y2);
	}

	//draw cursor lines if available
	if (cursor0Enable || cursor1Enable) {
		painter->save();
		painter->setPen(cursorPen);
		QFont font = painter->font();
		font.setPointSize(font.pointSize() * 4);
		painter->setFont(font);

		QPointF p1 = defaultCoordinateSystem()->mapLogicalToScene(QPointF(cursor0Pos.x(), yRange().start()));
		if (cursor0Enable && p1 != QPointF(0, 0)) {
			QPointF p2 = defaultCoordinateSystem()->mapLogicalToScene(QPointF(cursor0Pos.x(), yRange().end()));
			painter->drawLine(p1, p2);
			QPointF textPos = p2;
			textPos.setX(p2.x() - m_cursor0Text.size().width()/2);
			textPos.setY(p2.y() - m_cursor0Text.size().height());
			if (textPos.y() < boundingRect().y())
				textPos.setY(boundingRect().y());
			painter->drawStaticText(textPos, m_cursor0Text);
		}

		p1 = defaultCoordinateSystem()->mapLogicalToScene(QPointF(cursor1Pos.x(), yRange().start()));
		if (cursor1Enable && p1 != QPointF(0, 0)) {
			QPointF p2 = defaultCoordinateSystem()->mapLogicalToScene(QPointF(cursor1Pos.x(), yRange().end()));
			painter->drawLine(p1, p2);
			QPointF textPos = p2;
			// TODO: Moving this stuff into other function to not calculate it every time
			textPos.setX(p2.x() - m_cursor1Text.size().width()/2);
			textPos.setY(p2.y() - m_cursor1Text.size().height());
			if (textPos.y() < boundingRect().y())
				textPos.setY(boundingRect().y());
			painter->drawStaticText(textPos, m_cursor1Text);
		}

		painter->restore();
	}

	const bool hovered = (m_hovered && !isSelected());
	const bool selected = isSelected();
	if ((hovered || selected)  && !m_printing) {
		static double penWidth = 20.;
		const QRectF& br = q->m_plotArea->graphicsItem()->boundingRect();
		const qreal width = br.width();
		const qreal height = br.height();
		const QRectF rect = QRectF(-width/2 + penWidth/2, -height/2 + penWidth/2,
					  width - penWidth, height - penWidth);

		if (m_hovered)
			painter->setPen(QPen(QApplication::palette().color(QPalette::Shadow), penWidth));
		else
			painter->setPen(QPen(QApplication::palette().color(QPalette::Highlight), penWidth));

		painter->drawRect(rect);
	}
}

//##############################################################################
//##################  Serialization/Deserialization  ###########################
//##############################################################################

//! Save as XML
void CartesianPlot::save(QXmlStreamWriter* writer) const {
	Q_D(const CartesianPlot);

	writer->writeStartElement( "cartesianPlot" );
	writeBasicAttributes(writer);
	writeCommentElement(writer);

	//applied theme
	if (!d->theme.isEmpty()) {
		writer->writeStartElement( "theme" );
		writer->writeAttribute("name", d->theme);
		writer->writeEndElement();
	}

	//cursor
	writer->writeStartElement( "cursor" );
	WRITE_QPEN(d->cursorPen);
	writer->writeEndElement();
	//geometry
	writer->writeStartElement( "geometry" );
	writer->writeAttribute( "x", QString::number(d->rect.x()) );
	writer->writeAttribute( "y", QString::number(d->rect.y()) );
	writer->writeAttribute( "width", QString::number(d->rect.width()) );
	writer->writeAttribute( "height", QString::number(d->rect.height()) );
	writer->writeAttribute( "visible", QString::number(d->isVisible()) );
	writer->writeEndElement();

	//coordinate system and padding
	//new style
	writer->writeStartElement("xRanges");
	for (const auto& range : d->xRanges) {
		writer->writeStartElement("xRange");
		writer->writeAttribute( "autoScale", QString::number(range.autoScale()) );
		writer->writeAttribute( "start", QString::number(range.start(), 'g', 16));
		writer->writeAttribute( "end", QString::number(range.end(), 'g', 16) );
		writer->writeAttribute( "scale", QString::number(static_cast<int>(range.scale())) );
		writer->writeAttribute( "format", QString::number(static_cast<int>(range.format())) );
		writer->writeEndElement();
	}
	writer->writeEndElement();
	writer->writeStartElement("yRanges");
	for (const auto& range : d->yRanges) {
		writer->writeStartElement("yRange");
		writer->writeAttribute( "autoScale", QString::number(range.autoScale()) );
		writer->writeAttribute( "start", QString::number(range.start(), 'g', 16));
		writer->writeAttribute( "end", QString::number(range.end(), 'g', 16) );
		writer->writeAttribute( "scale", QString::number(static_cast<int>(range.scale())) );
		writer->writeAttribute( "format", QString::number(static_cast<int>(range.format())) );
		writer->writeEndElement();
	}
	writer->writeEndElement();
	writer->writeStartElement("coordinateSystems");
	writer->writeAttribute( "defaultCoordinateSystem", QString::number(defaultCoordinateSystemIndex()) );
	for (const auto& cSystem : m_coordinateSystems) {
		writer->writeStartElement( "coordinateSystem" );
		writer->writeAttribute( "xIndex", QString::number(dynamic_cast<CartesianCoordinateSystem*>(cSystem)->xIndex()) );
		writer->writeAttribute( "yIndex", QString::number(dynamic_cast<CartesianCoordinateSystem*>(cSystem)->yIndex()) );
		writer->writeEndElement();
	}
	// padding
	writer->writeAttribute( "horizontalPadding", QString::number(d->horizontalPadding) );
	writer->writeAttribute( "verticalPadding", QString::number(d->verticalPadding) );
	writer->writeAttribute( "rightPadding", QString::number(d->rightPadding) );
	writer->writeAttribute( "bottomPadding", QString::number(d->bottomPadding) );
	writer->writeAttribute( "symmetricPadding", QString::number(d->symmetricPadding));
	writer->writeEndElement();
	// OLD style (pre 2.9.0)
//	writer->writeStartElement( "coordinateSystem" );
//	writer->writeAttribute( "autoScaleX", QString::number(d->autoScaleX) );
//	writer->writeAttribute( "autoScaleY", QString::number(d->autoScaleY) );
//	writer->writeAttribute( "xMin", QString::number(xRange(0).start(), 'g', 16));
//	writer->writeAttribute( "xMax", QString::number(xRange(0).end(), 'g', 16) );
//	writer->writeAttribute( "yMin", QString::number(d->yRange.start(), 'g', 16) );
//	writer->writeAttribute( "yMax", QString::number(d->yRange.end(), 'g', 16) );
//	writer->writeAttribute( "xScale", QString::number(static_cast<int>(xRange(0).scale())) );
//	writer->writeAttribute( "yScale", QString::number(static_cast<int>(d->yScale)) );
//	writer->writeAttribute( "xRangeFormat", QString::number(static_cast<int>(xRangeFormat(0))) );
//	writer->writeAttribute( "yRangeFormat", QString::number(static_cast<int>(d->yRangeFormat)) );
//	writer->writeAttribute( "horizontalPadding", QString::number(d->horizontalPadding) );
//	writer->writeAttribute( "verticalPadding", QString::number(d->verticalPadding) );
//	writer->writeAttribute( "rightPadding", QString::number(d->rightPadding) );
//	writer->writeAttribute( "bottomPadding", QString::number(d->bottomPadding) );
//	writer->writeAttribute( "symmetricPadding", QString::number(d->symmetricPadding));

	//x-scale breaks
	if (d->xRangeBreakingEnabled || !d->xRangeBreaks.list.isEmpty()) {
		writer->writeStartElement("xRangeBreaks");
		writer->writeAttribute( "enabled", QString::number(d->xRangeBreakingEnabled) );
		for (const auto& rb : d->xRangeBreaks.list) {
			writer->writeStartElement("xRangeBreak");
			writer->writeAttribute("start", QString::number(rb.range.start()));
			writer->writeAttribute("end", QString::number(rb.range.end()));
			writer->writeAttribute("position", QString::number(rb.position));
			writer->writeAttribute("style", QString::number(static_cast<int>(rb.style)));
			writer->writeEndElement();
		}
		writer->writeEndElement();
	}

	//y-scale breaks
	if (d->yRangeBreakingEnabled || !d->yRangeBreaks.list.isEmpty()) {
		writer->writeStartElement("yRangeBreaks");
		writer->writeAttribute( "enabled", QString::number(d->yRangeBreakingEnabled) );
		for (const auto& rb : d->yRangeBreaks.list) {
			writer->writeStartElement("yRangeBreak");
			writer->writeAttribute("start", QString::number(rb.range.start()));
			writer->writeAttribute("end", QString::number(rb.range.end()));
			writer->writeAttribute("position", QString::number(rb.position));
			writer->writeAttribute("style", QString::number(static_cast<int>(rb.style)));
			writer->writeEndElement();
		}
		writer->writeEndElement();
	}

	//serialize all children (plot area, title text label, axes and curves)
	for (auto* elem : children<WorksheetElement>(ChildIndexFlag::IncludeHidden))
		elem->save(writer);

	writer->writeEndElement(); // cartesianPlot
}

//! Load from XML
bool CartesianPlot::load(XmlStreamReader* reader, bool preview) {
	Q_D(CartesianPlot);

	if (!readBasicAttributes(reader))
		return false;

	KLocalizedString attributeWarning = ki18n("Attribute '%1' missing or empty, default value is used");
	QXmlStreamAttributes attribs;
	QString str;
	bool titleLabelRead = false;

	while (!reader->atEnd()) {
		reader->readNext();
		if (reader->isEndElement() && reader->name() == "cartesianPlot")
			break;

		if (!reader->isStartElement())
			continue;

		if (reader->name() == "comment") {
			if (!readCommentElement(reader))
				return false;
		} else if (!preview && reader->name() == "theme") {
			attribs = reader->attributes();
			d->theme = attribs.value("name").toString();
		} else if (!preview && reader->name() == "cursor") {
			attribs = reader->attributes();
			QPen pen;
			pen.setWidth(attribs.value("width").toInt());
			pen.setStyle(static_cast<Qt::PenStyle>(attribs.value("style").toInt()));
			QColor color;
			color.setRed(attribs.value("color_r").toInt());
			color.setGreen(attribs.value("color_g").toInt());
			color.setBlue(attribs.value("color_b").toInt());
			pen.setColor(color);
			d->cursorPen = pen;
		} else if (!preview && reader->name() == "geometry") {
			attribs = reader->attributes();

			str = attribs.value("x").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("x").toString());
			else
				d->rect.setX( str.toDouble() );

			str = attribs.value("y").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("y").toString());
			else
				d->rect.setY( str.toDouble() );

			str = attribs.value("width").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("width").toString());
			else
				d->rect.setWidth( str.toDouble() );

			str = attribs.value("height").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("height").toString());
			else
				d->rect.setHeight( str.toDouble() );

			str = attribs.value("visible").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("visible").toString());
			else
				d->setVisible(str.toInt());
		} else if (!preview && reader->name() == "xRanges") {
			d->xRanges.clear();
		} else if (!preview && reader->name() == "xRange") {
			attribs = reader->attributes();

			//TODO: Range<double> range = Range::load(reader)
			Range<double> range;
			str = attribs.value("autoScale").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("autoScale").toString());
			else
				range.setAutoScale( str.toInt() );
			str = attribs.value("start").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("start").toString());
			else
				range.setStart( str.toDouble() );
			str = attribs.value("end").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("end").toString());
			else
				range.setEnd( str.toDouble() );
			str = attribs.value("scale").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("scale").toString());
			else
				range.setScale( static_cast<RangeT::Scale>(str.toInt()) );
			str = attribs.value("format").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("format").toString());
			else
				range.setFormat( static_cast<RangeT::Format>(str.toInt()) );

			addXRange(range);
		} else if (!preview && reader->name() == "yRanges") {
			d->yRanges.clear();
		} else if (!preview && reader->name() == "yRange") {
			attribs = reader->attributes();

			//TODO: Range<double> range = Range::load(reader)
			Range<double> range;
			str = attribs.value("autoScale").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("autoScale").toString());
			else
				range.setAutoScale( str.toInt() );
			str = attribs.value("start").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("start").toString());
			else
				range.setStart( str.toDouble() );
			str = attribs.value("end").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("end").toString());
			else
				range.setEnd( str.toDouble() );
			str = attribs.value("scale").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("scale").toString());
			else
				range.setScale( static_cast<RangeT::Scale>(str.toInt()) );
			str = attribs.value("format").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("format").toString());
			else
				range.setFormat( static_cast<RangeT::Format>(str.toInt()) );

			addYRange(range);
		} else if (!preview && reader->name() == "coordinateSystems") {
			attribs = reader->attributes();
			READ_INT_VALUE("defaultCoordinateSystem", defaultCoordinateSystemIndex, int);

			READ_DOUBLE_VALUE("horizontalPadding", horizontalPadding);
			READ_DOUBLE_VALUE("verticalPadding", verticalPadding);
			READ_DOUBLE_VALUE("rightPadding", rightPadding);
			READ_DOUBLE_VALUE("bottomPadding", bottomPadding);
			READ_INT_VALUE("symmetricPadding", symmetricPadding, bool);

//			d->coordinateSystems.clear();
			m_coordinateSystems.clear();
		} else if (!preview && reader->name() == "coordinateSystem") {
			attribs = reader->attributes();
			// new style
			CartesianCoordinateSystem* cSystem{ new CartesianCoordinateSystem(this) };
			str = attribs.value("xIndex").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("xIndex").toString());
			else
				cSystem->setXIndex( str.toInt() );
			str = attribs.value("yIndex").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("yIndex").toString());
			else
				cSystem->setYIndex( str.toInt() );
			addCoordinateSystem(cSystem);

			// old style (pre 2.9.0, to read old projects)
			str = attribs.value("autoScaleX").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("autoScaleX").toString());
			else
				xRange(0).setAutoScale(str.toInt());
			str = attribs.value("autoScaleY").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("autoScaleY").toString());
			else
				yRange(0).setAutoScale(str.toInt());

			str = attribs.value("xMin").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("xMin").toString());
			else {
				xRange(0).start() = str.toDouble();
				d->xPrevRange.start() = xRange(0).start();
			}

			str = attribs.value("xMax").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("xMax").toString());
			else {
				xRange(0).end() = str.toDouble();
				d->xPrevRange.end() = xRange(0).end();
			}

			str = attribs.value("yMin").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("yMin").toString());
			else {
				yRange(0).start() = str.toDouble();
				d->yPrevRange.start() = yRange(0).start();
			}

			str = attribs.value("yMax").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("yMax").toString());
			else {
				yRange(0).end() = str.toDouble();
				d->yPrevRange.end() = yRange(0).end();
			}

			str = attribs.value("xScale").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("xScale").toString());
			else
				xRange(0).scale() = static_cast<RangeT::Scale>(str.toInt());
			str = attribs.value("yScale").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("yScale").toString());
			else
				yRange(0).scale() = static_cast<RangeT::Scale>(str.toInt());

			str = attribs.value("xRangeFormat").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("xRangeFormat").toString());
			else
				xRange(0).format() = static_cast<RangeT::Format>(str.toInt());
			str = attribs.value("yRangeFormat").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("yRangeFormat").toString());
			else
				yRange(0).format() = static_cast<RangeT::Format>(str.toInt());

			READ_DOUBLE_VALUE("horizontalPadding", horizontalPadding);
			READ_DOUBLE_VALUE("verticalPadding", verticalPadding);
			READ_DOUBLE_VALUE("rightPadding", rightPadding);
			READ_DOUBLE_VALUE("bottomPadding", bottomPadding);
			READ_INT_VALUE("symmetricPadding", symmetricPadding, bool);
		} else if (!preview && reader->name() == "xRangeBreaks") {
			//delete default range break
			d->xRangeBreaks.list.clear();

			attribs = reader->attributes();
			READ_INT_VALUE("enabled", xRangeBreakingEnabled, bool);
		} else if (!preview && reader->name() == "xRangeBreak") {
			attribs = reader->attributes();

			RangeBreak b;
			str = attribs.value("start").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("start").toString());
			else
				b.range.start() = str.toDouble();

			str = attribs.value("end").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("end").toString());
			else
				b.range.end() = str.toDouble();

			str = attribs.value("position").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("position").toString());
			else
				b.position = str.toDouble();

			str = attribs.value("style").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("style").toString());
			else
				b.style = CartesianPlot::RangeBreakStyle(str.toInt());

			d->xRangeBreaks.list << b;
		} else if (!preview && reader->name() == "yRangeBreaks") {
			//delete default range break
			d->yRangeBreaks.list.clear();

			attribs = reader->attributes();
			READ_INT_VALUE("enabled", yRangeBreakingEnabled, bool);
		} else if (!preview && reader->name() == "yRangeBreak") {
			attribs = reader->attributes();

			RangeBreak b;
			str = attribs.value("start").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("start").toString());
			else
				b.range.start() = str.toDouble();

			str = attribs.value("end").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("end").toString());
			else
				b.range.end() = str.toDouble();

			str = attribs.value("position").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("position").toString());
			else
				b.position = str.toDouble();

			str = attribs.value("style").toString();
			if (str.isEmpty())
				reader->raiseWarning(attributeWarning.subs("style").toString());
			else
				b.style = CartesianPlot::RangeBreakStyle(str.toInt());

			d->yRangeBreaks.list << b;
		} else if (!preview && reader->name() == "textLabel") {
			if (!titleLabelRead) {
				//the first text label is always the title label
				m_title->load(reader, preview);
				titleLabelRead = true;

				//TODO: the name is read in m_title->load() but we overwrite it here
				//since the old projects don't have this " - Title" appendix yet that we add in init().
				//can be removed in couple of releases
				m_title->setName(name() + QLatin1String(" - ") + i18n("Title"));
			} else {
				TextLabel* label = new TextLabel("text label", this);
				if (label->load(reader, preview)) {
					addChildFast(label);
					label->setParentGraphicsItem(graphicsItem());
				} else {
					delete label;
					return false;
				}
			}
		} else if (!preview && reader->name() == "image") {
			auto* image = new Image(QString());
			if (!image->load(reader, preview)) {
				delete image;
				return false;
			} else
				addChildFast(image);
		} else if (!preview && reader->name() == "infoElement") {
			InfoElement* marker = new InfoElement("Marker", this);
			if (marker->load(reader, preview)) {
				addChildFast(marker);
				marker->setParentGraphicsItem(graphicsItem());
			} else {
				delete marker;
				return false;
			}
		} else if (!preview && reader->name() == "plotArea")
			m_plotArea->load(reader, preview);
		else if (!preview && reader->name() == "axis") {
			auto* axis = new Axis(QString());
			if (axis->load(reader, preview))
				addChildFast(axis);
			else {
				delete axis;
				return false;
			}
		} else if (reader->name() == "xyCurve") {
			auto* curve = new XYCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (reader->name() == "xyEquationCurve") {
			auto* curve = new XYEquationCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (reader->name() == "xyDataReductionCurve") {
			auto* curve = new XYDataReductionCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (reader->name() == "xyDifferentiationCurve") {
			auto* curve = new XYDifferentiationCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (reader->name() == "xyIntegrationCurve") {
			auto* curve = new XYIntegrationCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (reader->name() == "xyInterpolationCurve") {
			auto* curve = new XYInterpolationCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (reader->name() == "xySmoothCurve") {
			auto* curve = new XYSmoothCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (reader->name() == "xyFitCurve") {
			auto* curve = new XYFitCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (reader->name() == "xyFourierFilterCurve") {
			auto* curve = new XYFourierFilterCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (reader->name() == "xyFourierTransformCurve") {
			auto* curve = new XYFourierTransformCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (reader->name() == "xyConvolutionCurve") {
			auto* curve = new XYConvolutionCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (reader->name() == "xyCorrelationCurve") {
			auto* curve = new XYCorrelationCurve(QString());
			if (curve->load(reader, preview))
				addChildFast(curve);
			else {
				removeChild(curve);
				return false;
			}
		} else if (!preview && reader->name() == "cartesianPlotLegend") {
			m_legend = new CartesianPlotLegend(QString());
			if (m_legend->load(reader, preview))
				addChildFast(m_legend);
			else {
				delete m_legend;
				return false;
			}
		} else if (!preview && reader->name() == "customPoint") {
			auto* point = new CustomPoint(this, QString());
			if (point->load(reader, preview))
				addChildFast(point);
			else {
				delete point;
				return false;
			}
		} else if (!preview && reader->name() == "referenceLine") {
			auto* line = new ReferenceLine(this, QString());
			if (line->load(reader, preview))
				addChildFast(line);
			else {
				delete line;
				return false;
			}
		} else if (reader->name() == "boxPlot") {
			auto* boxPlot = new BoxPlot("BoxPlot");
			if (boxPlot->load(reader, preview))
				addChildFast(boxPlot);
			else {
				removeChild(boxPlot);
				return false;
			}
		} else if (reader->name() == "Histogram") {
			auto* hist = new Histogram("Histogram");
			if (hist->load(reader, preview))
				addChildFast(hist);
			else {
				removeChild(hist);
				return false;
			}
		} else { // unknown element
			if (!preview)
				reader->raiseWarning(i18n("unknown cartesianPlot element '%1'", reader->name().toString()));
			if (!reader->skipToEndElement()) return false;
		}
	}

	if (preview)
		return true;

	d->retransform();

	//if a theme was used, initialize the color palette
	if (!d->theme.isEmpty()) {
		//TODO: check whether the theme config really exists
		KConfig config( ThemeHandler::themeFilePath(d->theme), KConfig::SimpleConfig );
		this->setColorPalette(config);
	} else {
		//initialize the color palette with default colors
		this->setColorPalette(KConfig());
	}

	return true;
}

//##############################################################################
//#########################  Theme management ##################################
//##############################################################################
void CartesianPlot::loadTheme(const QString& theme) {
	if (!theme.isEmpty()) {
		KConfig config(ThemeHandler::themeFilePath(theme), KConfig::SimpleConfig);
		loadThemeConfig(config);
	} else {
		KConfig config;
		loadThemeConfig(config);
	}
}

void CartesianPlot::loadThemeConfig(const KConfig& config) {
	Q_D(CartesianPlot);

	QString theme = QString();
	if (config.hasGroup(QLatin1String("Theme"))) {
		theme = config.name();

		// theme path is saved with UNIX dir separator
		theme = theme.right(theme.length() - theme.lastIndexOf(QLatin1Char('/')) - 1);
		DEBUG(Q_FUNC_INFO << ", set theme to " << STDSTRING(theme));
	}

	//loadThemeConfig() can be called from
	//1. CartesianPlot::setTheme() when the user changes the theme for the plot
	//2. Worksheet::setTheme() -> Worksheet::loadTheme() when the user changes the theme for the worksheet
	//In the second case (i.e. when d->theme is not equal to theme yet),
	///we need to put the new theme name on the undo-stack.
	if (theme != d->theme)
		exec(new CartesianPlotSetThemeCmd(d, theme, ki18n("%1: set theme")));

	//load the color palettes for the curves
	this->setColorPalette(config);

	//load the theme for all the children
	for (auto* child : children<WorksheetElement>(ChildIndexFlag::IncludeHidden))
		child->loadThemeConfig(config);

	d->update(this->rect());
}

void CartesianPlot::saveTheme(KConfig &config) {
	const QVector<Axis*>& axisElements = children<Axis>(ChildIndexFlag::IncludeHidden);
	const QVector<PlotArea*>& plotAreaElements = children<PlotArea>(ChildIndexFlag::IncludeHidden);
	const QVector<TextLabel*>& textLabelElements = children<TextLabel>(ChildIndexFlag::IncludeHidden);

	axisElements.at(0)->saveThemeConfig(config);
	plotAreaElements.at(0)->saveThemeConfig(config);
	textLabelElements.at(0)->saveThemeConfig(config);

	for (auto *child : children<XYCurve>(ChildIndexFlag::IncludeHidden))
		child->saveThemeConfig(config);
}

//Generating colors from 5-color theme palette
void CartesianPlot::setColorPalette(const KConfig& config) {
	if (config.hasGroup(QLatin1String("Theme"))) {
		KConfigGroup group = config.group(QLatin1String("Theme"));

		//read the five colors defining the palette
		m_themeColorPalette.clear();
		m_themeColorPalette.append(group.readEntry("ThemePaletteColor1", QColor()));
		m_themeColorPalette.append(group.readEntry("ThemePaletteColor2", QColor()));
		m_themeColorPalette.append(group.readEntry("ThemePaletteColor3", QColor()));
		m_themeColorPalette.append(group.readEntry("ThemePaletteColor4", QColor()));
		m_themeColorPalette.append(group.readEntry("ThemePaletteColor5", QColor()));
	} else {
		//no theme is available, provide 5 "default colors"
		m_themeColorPalette.clear();
		m_themeColorPalette.append(QColor(25, 25, 25));
		m_themeColorPalette.append(QColor(0, 0, 127));
		m_themeColorPalette.append(QColor(127 ,0, 0));
		m_themeColorPalette.append(QColor(0, 127, 0));
		m_themeColorPalette.append(QColor(85, 0, 127));
	}

	//generate 30 additional shades if the color palette contains more than one color
	if (m_themeColorPalette.at(0) != m_themeColorPalette.at(1)) {
		QColor c;

		//3 factors to create shades from theme's palette
		std::array<float, 3> fac = {0.25f, 0.45f, 0.65f};

		//Generate 15 lighter shades
		for (int i = 0; i < 5; i++) {
			for (int j = 1; j < 4; j++) {
				c.setRed( m_themeColorPalette.at(i).red()*(1-fac[j-1]) );
				c.setGreen( m_themeColorPalette.at(i).green()*(1-fac[j-1]) );
				c.setBlue( m_themeColorPalette.at(i).blue()*(1-fac[j-1]) );
				m_themeColorPalette.append(c);
			}
		}

		//Generate 15 darker shades
		for (int i = 0; i < 5; i++) {
			for (int j = 4; j < 7; j++) {
				c.setRed( m_themeColorPalette.at(i).red()+((255-m_themeColorPalette.at(i).red())*fac[j-4]) );
				c.setGreen( m_themeColorPalette.at(i).green()+((255-m_themeColorPalette.at(i).green())*fac[j-4]) );
				c.setBlue( m_themeColorPalette.at(i).blue()+((255-m_themeColorPalette.at(i).blue())*fac[j-4]) );
				m_themeColorPalette.append(c);
			}
		}
	}

	//use the color of the axis lines as the color for the different mouse cursor lines
	Q_D(CartesianPlot);
	const KConfigGroup& group = config.group("Axis");
	const QColor& color = group.readEntry("LineColor", QColor(Qt::black));
	d->zoomSelectPen.setColor(color);
	d->crossHairPen.setColor(color);
}

const QList<QColor>& CartesianPlot::themeColorPalette() const {
	return m_themeColorPalette;
}
