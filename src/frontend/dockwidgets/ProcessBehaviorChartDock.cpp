/*
	File                 : ProcessBehaviorChartDock.cpp
	Project              : LabPlot
	Description          : widget for properties of the process behavior chart
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2024 Alexander Semke <alexander.semke@web.de>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ProcessBehaviorChartDock.h"
#include "backend/core/column/Column.h"
#include "backend/worksheet/plots/cartesian/ProcessBehaviorChart.h"
#include "backend/worksheet/plots/cartesian/Symbol.h"
#include "frontend/TemplateHandler.h"
#include "frontend/widgets/LineWidget.h"
#include "frontend/widgets/SymbolWidget.h"
#include "frontend/widgets/TreeViewComboBox.h"

#include <QFrame>

#include <KConfig>
#include <KLocalizedString>
#include <KMessageWidget>

/*!
  \class ProcessBehaviorChartDock
  \brief  Provides a widget for editing the properties of process behavior charts.

  \ingroup frontend
*/
ProcessBehaviorChartDock::ProcessBehaviorChartDock(QWidget* parent)
	: BaseDock(parent)
	, cbDataColumn(new TreeViewComboBox)
	, cbData2Column(new TreeViewComboBox) {
	ui.setupUi(this);
	setPlotRangeCombobox(ui.cbPlotRanges);
	setBaseWidgets(ui.leName, ui.teComment);
	setVisibilityWidgets(ui.chkVisible, ui.chkLegendVisible);

	// Tab "General"
	auto* gridLayout = qobject_cast<QGridLayout*>(ui.tabGeneral->layout());
	gridLayout->addWidget(cbDataColumn, 4, 2, 1, 1);
	gridLayout->addWidget(cbData2Column, 5, 2, 1, 1);

	// Tab "Data Line"
	auto* hBoxLayout = static_cast<QHBoxLayout*>(ui.tabDataLine->layout());
	dataLineWidget = new LineWidget(ui.tabDataLine);
	hBoxLayout->insertWidget(1, dataLineWidget);

	dataSymbolWidget = new SymbolWidget(ui.tabDataLine);
	hBoxLayout->insertWidget(3, dataSymbolWidget);

	// Tab "Contol Limit Lines"
	hBoxLayout = static_cast<QHBoxLayout*>(ui.tabControlLimitLines->layout());
	centerLineWidget = new LineWidget(ui.tabControlLimitLines);
	hBoxLayout->insertWidget(1, centerLineWidget);
	upperLimitLineWidget = new LineWidget(ui.tabControlLimitLines);
	hBoxLayout->insertWidget(5, upperLimitLineWidget);
	lowerLimitLineWidget = new LineWidget(ui.tabControlLimitLines);
	hBoxLayout->insertWidget(9, lowerLimitLineWidget);

	// adjust layouts in the tabs
	for (int i = 0; i < ui.tabWidget->count(); ++i) {
		auto* layout = dynamic_cast<QGridLayout*>(ui.tabWidget->widget(i)->layout());
		if (!layout)
			continue;

		layout->setContentsMargins(2, 2, 2, 2);
		layout->setHorizontalSpacing(2);
		layout->setVerticalSpacing(2);
	}

	// Slots
	// General
	connect(cbDataColumn, &TreeViewComboBox::currentModelIndexChanged, this, &ProcessBehaviorChartDock::dataColumnChanged);
	connect(cbData2Column, &TreeViewComboBox::currentModelIndexChanged, this, &ProcessBehaviorChartDock::data2ColumnChanged);
	connect(ui.cbType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProcessBehaviorChartDock::typeChanged);
	connect(ui.cbLimitsMetric, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProcessBehaviorChartDock::limitsMetricChanged);
	connect(ui.sbSampleSize, &QSpinBox::valueChanged, this, &ProcessBehaviorChartDock::sampleSizeChanged);
	connect(ui.chbNegativeLowerLimit, &QCheckBox::clicked, this, &ProcessBehaviorChartDock::negativeLowerLimitEnabledChanged);
	connect(ui.chbExactLimits, &QCheckBox::clicked, this, &ProcessBehaviorChartDock::exactLimitsEnabledChanged);

	// template handler
	auto* frame = new QFrame(this);
	auto* layout = new QHBoxLayout(frame);
	layout->setContentsMargins(0, 11, 0, 11);

	auto* templateHandler = new TemplateHandler(this, QLatin1String("ProcessBehaviorChart"));
	layout->addWidget(templateHandler);
	connect(templateHandler, &TemplateHandler::loadConfigRequested, this, &ProcessBehaviorChartDock::loadConfigFromTemplate);
	connect(templateHandler, &TemplateHandler::saveConfigRequested, this, &ProcessBehaviorChartDock::saveConfigAsTemplate);
	connect(templateHandler, &TemplateHandler::info, this, &ProcessBehaviorChartDock::info);

	ui.verticalLayout->addWidget(frame);

	updateLocale();
	retranslateUi();
}

ProcessBehaviorChartDock::~ProcessBehaviorChartDock() = default;

void ProcessBehaviorChartDock::setModel() {
	auto* model = aspectModel();
	model->enablePlottableColumnsOnly(true);
	model->enableShowPlotDesignation(true);
	model->setSelectableAspects({AspectType::Column});
	cbDataColumn->setTopLevelClasses(TreeViewComboBox::plotColumnTopLevelClasses());
	cbDataColumn->setModel(model);
	cbData2Column->setTopLevelClasses(TreeViewComboBox::plotColumnTopLevelClasses());
	cbData2Column->setModel(model);
}

void ProcessBehaviorChartDock::setPlots(QList<ProcessBehaviorChart*> list) {
	Lock lock(m_initializing);
	m_plots = list;
	m_plot = list.first();
	setAspects(list);
	Q_ASSERT(m_plot);
	setModel();

	// initialize widgets for common properties
	QList<Line*> dataLines;
	QList<Symbol*> dataSymbols;
	QList<Line*> centerLines;
	QList<Line*> upperLimitLines;
	QList<Line*> lowerLimitLines;
	for (auto* plot : m_plots) {
		dataLines << plot->dataLine();
		dataSymbols << plot->dataSymbol();
		centerLines << plot->centerLine();
		upperLimitLines << plot->upperLimitLine();
		if (m_plot->lowerLimitAvailable())
			lowerLimitLines << plot->lowerLimitLine();
	}
	dataLineWidget->setLines(dataLines);
	dataSymbolWidget->setSymbols(dataSymbols);
	centerLineWidget->setLines(centerLines);
	upperLimitLineWidget->setLines(upperLimitLines);
	if (m_plot->lowerLimitAvailable())
		lowerLimitLineWidget->setLines(lowerLimitLines);

	// if there are more then one curve in the list, disable the content in the tab "general"
	if (m_plots.size() == 1) {
		cbDataColumn->setEnabled(true);
		cbDataColumn->setAspect(m_plot->dataColumn(), m_plot->dataColumnPath());
		cbData2Column->setEnabled(true);
		cbData2Column->setAspect(m_plot->data2Column(), m_plot->data2ColumnPath());
	} else {
		cbDataColumn->setEnabled(false);
		cbDataColumn->setCurrentModelIndex(QModelIndex());
		cbData2Column->setEnabled(false);
		cbData2Column->setCurrentModelIndex(QModelIndex());
	}

	ui.chkLegendVisible->setChecked(m_plot->legendVisible());
	ui.chkVisible->setChecked(m_plot->isVisible());

	// hide the properties for the lower limit line if the lower limit is not available for the current plot
	bool visible = m_plot->lowerLimitAvailable();
	ui.lLowerLimit->setVisible(visible);
	lowerLimitLineWidget->setVisible(visible);

	// load the remaining properties
	load();

	updatePlotRangeList();

	// Slots
	// General-tab
	connect(m_plot, &ProcessBehaviorChart::dataColumnChanged, this, &ProcessBehaviorChartDock::plotDataColumnChanged);
	connect(m_plot, &ProcessBehaviorChart::data2ColumnChanged, this, &ProcessBehaviorChartDock::plotData2ColumnChanged);
	connect(m_plot, &ProcessBehaviorChart::typeChanged, this, &ProcessBehaviorChartDock::plotTypeChanged);
	connect(m_plot, &ProcessBehaviorChart::limitsMetricChanged, this, &ProcessBehaviorChartDock::plotLimitsMetricChanged);
	connect(m_plot, &ProcessBehaviorChart::sampleSizeChanged, this, &ProcessBehaviorChartDock::plotSampleSizeChanged);
	connect(m_plot, &ProcessBehaviorChart::negativeLowerLimitEnabledChanged, this, &ProcessBehaviorChartDock::plotNegativeLowerLimitEnabledChanged);
	connect(m_plot, &ProcessBehaviorChart::exactLimitsEnabledChanged, this, &ProcessBehaviorChartDock::plotExactLimitsEnabledChanged);
	connect(m_plot, &ProcessBehaviorChart::statusInfo, this, &ProcessBehaviorChartDock::showStatusInfo);
}

void ProcessBehaviorChartDock::retranslateUi() {
	ui.cbLimitsMetric->clear();
	ui.cbLimitsMetric->addItem(i18n("Average"), static_cast<int>(ProcessBehaviorChart::LimitsMetric::Average));
	ui.cbLimitsMetric->addItem(i18n("Median"), static_cast<int>(ProcessBehaviorChart::LimitsMetric::Median));

	ui.cbType->clear();
	ui.cbType->addItem(QStringLiteral("X (XmR)"), static_cast<int>(ProcessBehaviorChart::Type::XmR));
	ui.cbType->addItem(QStringLiteral("mR"), static_cast<int>(ProcessBehaviorChart::Type::mR));
	ui.cbType->addItem(QStringLiteral("X̅  (X̅R)"), static_cast<int>(ProcessBehaviorChart::Type::XbarR));
	ui.cbType->addItem(QStringLiteral("R"), static_cast<int>(ProcessBehaviorChart::Type::R));
	ui.cbType->addItem(QStringLiteral("X̅  (X̅S)"), static_cast<int>(ProcessBehaviorChart::Type::XbarS));
	ui.cbType->addItem(QStringLiteral("S"), static_cast<int>(ProcessBehaviorChart::Type::S));
	ui.cbType->addItem(QStringLiteral("P"), static_cast<int>(ProcessBehaviorChart::Type::P));
	ui.cbType->addItem(QStringLiteral("NP"), static_cast<int>(ProcessBehaviorChart::Type::NP));
	ui.cbType->addItem(QStringLiteral("C"), static_cast<int>(ProcessBehaviorChart::Type::C));
	ui.cbType->addItem(QStringLiteral("U"), static_cast<int>(ProcessBehaviorChart::Type::U));

	// tooltips
	QString info = i18n(
		"The supported chart types are grouped according to the plotted statistics and to the metric defining the limits.<br><br>"
		"Individual Values and Moving Ranges, Limits Based on the Average or Median Moving Range:"
		"<ul>"
		"<li>X (XmR) - plot the <b>individual values</b>.</li>"
		"<li>mR - plot the <b>moving ranges</b>.</li>"
		"</ul>"
		"Averages and Ranges, Limits based on the Average or Median Range:"
		"<ul>"
		"<li>X̅  (X̅R) - plot the <b>averages for each sample</b> .</li>"
		"<li>R (X̅R) - plot the <b>ranges for each sample</b>.</li>"
		"</ul>"
		"Averages and Standard Deviations, Limits Based on the Standard Deviations:"
		"<ul>"
		"<li>X̅  (X̅S) - plot the <b>averages for each sample</b>.</li>"
		"<li>S (X̅S) - plot the <b>standard deviations for each sample</b>.</li>"
		"</ul>"
		"Attributes:"
		"<ul>"
		"<li>P - plot <b>binomial proportions</b>.</li>"
		"<li>NP - plot <b>binomial counts</b>.</li>"
		"<li>C - plot <b>Poisson counts</b>.</li>"
		"<li>U - plot <b>Poisson rates</b>.</li>"
		"</ul>");
	ui.lType->setToolTip(info);
	ui.cbType->setToolTip(info);

	info = i18n("Allow negative values for the lower limit.");
	ui.lNegativeLowerLimit->setToolTip(info);
	ui.chbNegativeLowerLimit->setToolTip(info);
}

/*
 * updates the locale in the widgets. called when the application settins are changed.
 */
void ProcessBehaviorChartDock::updateLocale() {
	dataLineWidget->updateLocale();
	dataSymbolWidget->updateLocale();
	centerLineWidget->updateLocale();
	upperLimitLineWidget->updateLocale();
	lowerLimitLineWidget->updateLocale();
}

//*************************************************************
//** SLOTs for changes triggered in ProcessBehaviorChartDock **
//*************************************************************

// "General"-tab
void ProcessBehaviorChartDock::dataColumnChanged(const QModelIndex& index) {
	if (m_initializing)
		return;

	auto aspect = static_cast<AbstractAspect*>(index.internalPointer());
	AbstractColumn* column(nullptr);
	if (aspect) {
		column = dynamic_cast<AbstractColumn*>(aspect);
		Q_ASSERT(column);
	}

	for (auto* plot : m_plots)
		plot->setDataColumn(column);
}

void ProcessBehaviorChartDock::data2ColumnChanged(const QModelIndex& index) {
	if (m_initializing)
		return;

	auto aspect = static_cast<AbstractAspect*>(index.internalPointer());
	AbstractColumn* column(nullptr);
	if (aspect) {
		column = dynamic_cast<AbstractColumn*>(aspect);
		Q_ASSERT(column);
	}

	for (auto* plot : m_plots)
		plot->setData2Column(column);
}

void ProcessBehaviorChartDock::typeChanged(int index) {
	const auto type = static_cast<ProcessBehaviorChart::Type>(ui.cbType->itemData(index).toInt());

	// depending on the current type, show/hide the settings for the sample type
	bool visible = (type == ProcessBehaviorChart::Type::XbarR || type == ProcessBehaviorChart::Type::R || type == ProcessBehaviorChart::Type::XbarS
					|| type == ProcessBehaviorChart::Type::S || type == ProcessBehaviorChart::Type::NP);
	ui.lSampleSize->setVisible(visible);
	ui.sbSampleSize->setVisible(visible);

	// depending on the current type, show/hide the settings for the metric used to define the limits
	visible = (type == ProcessBehaviorChart::Type::XmR || type == ProcessBehaviorChart::Type::mR || type == ProcessBehaviorChart::Type::XbarR
				|| type == ProcessBehaviorChart::Type::R);
	ui.lLimitsMetric->setVisible(visible);
	ui.cbLimitsMetric->setVisible(visible);

	// allow negative value
	visible = (type == ProcessBehaviorChart::Type::XmR || type == ProcessBehaviorChart::Type::XbarR || type == ProcessBehaviorChart::Type::XbarS);
	ui.lNegativeLowerLimit->setVisible(visible);
	ui.chbNegativeLowerLimit->setVisible(visible);

	// second data column
	visible = (type == ProcessBehaviorChart::Type::P || type == ProcessBehaviorChart::Type::U);
	ui.lData2Column->setVisible(visible);
	cbData2Column->setVisible(visible);

	CONDITIONAL_LOCK_RETURN;
	for (auto* plot : m_plots)
		plot->setType(type);
}

void ProcessBehaviorChartDock::limitsMetricChanged(int index) {
	CONDITIONAL_LOCK_RETURN;
	const auto limitsMetric = static_cast<ProcessBehaviorChart::LimitsMetric>(ui.cbLimitsMetric->itemData(index).toInt());
	for (auto* plot : m_plots)
		plot->setLimitsMetric(limitsMetric);
}

void ProcessBehaviorChartDock::sampleSizeChanged(int value) {
	CONDITIONAL_LOCK_RETURN;
	for (auto* plot : m_plots)
		plot->setSampleSize(value);
}

void ProcessBehaviorChartDock::negativeLowerLimitEnabledChanged(bool enabled) {
	CONDITIONAL_LOCK_RETURN;
	for (auto* plot : m_plots)
		plot->setNegativeLowerLimitEnabled(enabled);
}

void ProcessBehaviorChartDock::exactLimitsEnabledChanged(bool enabled) {
	CONDITIONAL_LOCK_RETURN;
	for (auto* plot : m_plots)
		plot->setExactLimitsEnabled(enabled);
}

//*************************************************************
//**** SLOTs for changes triggered in ProcessBehaviorChart ****
//*************************************************************
// General-Tab
void ProcessBehaviorChartDock::plotDataColumnChanged(const AbstractColumn* column) {
	CONDITIONAL_LOCK_RETURN;
	cbDataColumn->setAspect(column, m_plot->dataColumnPath());
}

void ProcessBehaviorChartDock::plotData2ColumnChanged(const AbstractColumn* column) {
	CONDITIONAL_LOCK_RETURN;
	cbData2Column->setAspect(column, m_plot->data2ColumnPath());
}

void ProcessBehaviorChartDock::plotTypeChanged(ProcessBehaviorChart::Type type) {
	CONDITIONAL_LOCK_RETURN;
	const int index = ui.cbType->findData(static_cast<int>(type));
	ui.cbType->setCurrentIndex(index);
}

void ProcessBehaviorChartDock::plotLimitsMetricChanged(ProcessBehaviorChart::LimitsMetric limitsMetric) {
	CONDITIONAL_LOCK_RETURN;
	const int index = ui.cbLimitsMetric->findData(static_cast<int>(limitsMetric));
	ui.cbLimitsMetric->setCurrentIndex(index);
}

void ProcessBehaviorChartDock::plotSampleSizeChanged(int value) {
	CONDITIONAL_LOCK_RETURN;
	ui.sbSampleSize->setValue(value);
}

void ProcessBehaviorChartDock::plotNegativeLowerLimitEnabledChanged(bool enabled) {
	CONDITIONAL_LOCK_RETURN;
	ui.chbNegativeLowerLimit->setChecked(enabled);
}

void ProcessBehaviorChartDock::plotExactLimitsEnabledChanged(bool enabled) {
	CONDITIONAL_LOCK_RETURN;
	ui.chbExactLimits->setChecked(enabled);
}

void ProcessBehaviorChartDock::showStatusInfo(const QString& info) {
	if (info.isEmpty()) {
		if (m_messageWidget && m_messageWidget->isVisible())
			m_messageWidget->close();
	} else {
		if (!m_messageWidget) {
			m_messageWidget = new KMessageWidget(this);
			m_messageWidget->setMessageType(KMessageWidget::Warning);
			static_cast<QGridLayout*>(ui.tabGeneral->layout())->addWidget(m_messageWidget, 13, 0, 1, 3);
		}
		m_messageWidget->setText(info);
		m_messageWidget->animatedShow();
		QDEBUG(info);
	}
}

//*************************************************************
//************************* Settings **************************
//*************************************************************
void ProcessBehaviorChartDock::load() {
	// type
	int index = ui.cbType->findData(static_cast<int>(m_plot->type()));
	ui.cbType->setCurrentIndex(index);

	// limits metric
	index = ui.cbLimitsMetric->findData(static_cast<int>(m_plot->limitsMetric()));
	ui.cbLimitsMetric->setCurrentIndex(index);

	// sample size
	ui.sbSampleSize->setValue(static_cast<int>(m_plot->sampleSize()));

	// allow negative values for the lower limit
	ui.chbNegativeLowerLimit->setChecked(m_plot->negativeLowerLimitEnabled());

	// user exact/individual limits, relevant for P and U charts only
	ui.chbExactLimits->setChecked(m_plot->exactLimitsEnabled());
}

void ProcessBehaviorChartDock::loadConfig(KConfig& config) {
	KConfigGroup group = config.group(QStringLiteral("ProcessBehaviorChart"));

	// type
	const auto type = group.readEntry(QStringLiteral("Type"), static_cast<int>(m_plot->type()));
	int index = ui.cbType->findData(static_cast<int>(type));
	ui.cbType->setCurrentIndex(index);

	// limits metric
	const auto limitsMetric = group.readEntry(QStringLiteral("LimitsMetric"), static_cast<int>(m_plot->limitsMetric()));
	index = ui.cbLimitsMetric->findData(static_cast<int>(limitsMetric));
	ui.cbLimitsMetric->setCurrentIndex(index);

	// sample size
	const int size = group.readEntry(QStringLiteral("SampleSize"), static_cast<int>(m_plot->sampleSize()));
	ui.sbSampleSize->setValue(size);

	// allow negative values for the lower limit
	ui.chbNegativeLowerLimit->setChecked(group.readEntry(QStringLiteral("NegativeLowerLimitEnabled"), false));

	// user exact/individual limits, relevant for P and U charts only
	ui.chbExactLimits->setChecked(group.readEntry(QStringLiteral("ExactLimitsEnabled"), false));

	// properties of the data and limit curves
	dataLineWidget->loadConfig(group);
	dataSymbolWidget->loadConfig(group);
	centerLineWidget->loadConfig(group);
	upperLimitLineWidget->loadConfig(group);
	if (m_plot->lowerLimitAvailable())
		lowerLimitLineWidget->loadConfig(group);
}

void ProcessBehaviorChartDock::loadConfigFromTemplate(KConfig& config) {
	auto name = TemplateHandler::templateName(config);
	int size = m_plots.size();
	if (size > 1)
		m_plot->beginMacro(i18n("%1 xy-curves: template \"%2\" loaded", size, name));
	else
		m_plot->beginMacro(i18n("%1: template \"%2\" loaded", m_plot->name(), name));

	this->loadConfig(config);

	m_plot->endMacro();
}

void ProcessBehaviorChartDock::saveConfigAsTemplate(KConfig& config) {
	KConfigGroup group = config.group(QStringLiteral("ProcessBehaviorChart"));

	// general
	group.writeEntry(QStringLiteral("Type"), static_cast<int>(m_plot->type()));
	group.writeEntry(QStringLiteral("LimitsMetric"), static_cast<int>(m_plot->limitsMetric()));
	group.writeEntry(QStringLiteral("SampleSize"), m_plot->sampleSize());
	group.writeEntry(QStringLiteral("NegativeLowerLimitEnabled"), m_plot->negativeLowerLimitEnabled());
	group.writeEntry(QStringLiteral("ExactLimitsEnabled"), m_plot->exactLimitsEnabled());

	// properties of the data and limit curves
	dataLineWidget->saveConfig(group);
	dataSymbolWidget->saveConfig(group);
	centerLineWidget->saveConfig(group);
	upperLimitLineWidget->saveConfig(group);
	if (m_plot->lowerLimitAvailable())
		lowerLimitLineWidget->saveConfig(group);
	config.sync();
}
