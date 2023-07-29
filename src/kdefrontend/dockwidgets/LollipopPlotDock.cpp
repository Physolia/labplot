/*
	File                 : LollipopPlotDock.cpp
	Project              : LabPlot
	Description          : Dock widget for the lolliplot plot
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2023 Alexander Semke <alexander.semke@web.de>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "LollipopPlotDock.h"
#include "backend/core/AbstractColumn.h"
#include "backend/core/AspectTreeModel.h"
#include "backend/core/Project.h"
#include "backend/lib/macros.h"
#include "commonfrontend/widgets/TreeViewComboBox.h"
#include "kdefrontend/GuiTools.h"
#include "kdefrontend/TemplateHandler.h"
#include "kdefrontend/widgets/LineWidget.h"
#include "kdefrontend/widgets/SymbolWidget.h"
#include "kdefrontend/widgets/ValueWidget.h"

#include <QPushButton>

#include <KConfig>
#include <KLocalizedString>

LollipopPlotDock::LollipopPlotDock(QWidget* parent)
	: BaseDock(parent)
	, cbXColumn(new TreeViewComboBox) {
	ui.setupUi(this);
	m_leName = ui.leName;
	m_teComment = ui.teComment;
	m_teComment->setFixedHeight(m_leName->height());

	// Tab "General"

	// x-data
	QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	cbXColumn->setSizePolicy(sizePolicy);
	static_cast<QVBoxLayout*>(ui.frameXColumn->layout())->insertWidget(0, cbXColumn);
	ui.bRemoveXColumn->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear")));

	// y-data
	m_buttonNew = new QPushButton();
	m_buttonNew->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));

	m_gridLayout = new QGridLayout(ui.frameDataColumns);
	m_gridLayout->setContentsMargins(0, 0, 0, 0);
	m_gridLayout->setHorizontalSpacing(2);
	m_gridLayout->setVerticalSpacing(2);
	ui.frameDataColumns->setLayout(m_gridLayout);

	ui.cbOrientation->addItem(i18n("Horizontal"));
	ui.cbOrientation->addItem(i18n("Vertical"));

	// Tab "Line"
	QString msg = i18n("Select the data column for which the properties should be shown and edited");
	ui.lNumberLine->setToolTip(msg);
	ui.cbNumberLine->setToolTip(msg);
	lineWidget = new LineWidget(ui.tabLine);
	auto* gridLayout = qobject_cast<QGridLayout*>(ui.tabLine->layout());
	gridLayout->addWidget(lineWidget, 2, 0, 1, 3);

	// Tab "Symbol"
	ui.lNumberSymbol->setToolTip(msg);
	ui.cbNumberSymbol->setToolTip(msg);
	symbolWidget = new SymbolWidget(ui.tabSymbol);
	gridLayout = qobject_cast<QGridLayout*>(ui.tabSymbol->layout());
	gridLayout->addWidget(symbolWidget, 2, 0, 1, 3);

	// Tab "Values"
	auto* hboxLayout = new QHBoxLayout(ui.tabValues);
	valueWidget = new ValueWidget(ui.tabValues);
	hboxLayout->addWidget(valueWidget);
	hboxLayout->setContentsMargins(2, 2, 2, 2);
	hboxLayout->setSpacing(2);

	// adjust layouts in the tabs
	for (int i = 0; i < ui.tabWidget->count(); ++i) {
		auto* layout = dynamic_cast<QGridLayout*>(ui.tabWidget->widget(i)->layout());
		if (!layout)
			continue;

		layout->setContentsMargins(2, 2, 2, 2);
		layout->setHorizontalSpacing(2);
		layout->setVerticalSpacing(2);
	}

	// SLOTS
	// Tab "General"
	connect(ui.leName, &QLineEdit::textChanged, this, &LollipopPlotDock::nameChanged);
	connect(ui.teComment, &QTextEdit::textChanged, this, &LollipopPlotDock::commentChanged);
	connect(cbXColumn, &TreeViewComboBox::currentModelIndexChanged, this, &LollipopPlotDock::xColumnChanged);
	connect(ui.bRemoveXColumn, &QPushButton::clicked, this, &LollipopPlotDock::removeXColumn);
	connect(m_buttonNew, &QPushButton::clicked, this, &LollipopPlotDock::addDataColumn);
	connect(ui.cbOrientation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LollipopPlotDock::orientationChanged);
	connect(ui.chkVisible, &QCheckBox::toggled, this, &LollipopPlotDock::visibilityChanged);
	connect(ui.cbPlotRanges, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LollipopPlotDock::plotRangeChanged);

	// Tab "Line"
	connect(ui.cbNumberLine, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LollipopPlotDock::currentBarLineChanged);

	// Tab "Symbol"
	connect(ui.cbNumberSymbol, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LollipopPlotDock::currentBarSymbolChanged);

	// template handler
	auto* frame = new QFrame(this);
	auto* layout = new QHBoxLayout(frame);
	layout->setContentsMargins(0, 11, 0, 11);

	auto* templateHandler = new TemplateHandler(this, TemplateHandler::ClassName::Worksheet);
	layout->addWidget(templateHandler);
	connect(templateHandler, &TemplateHandler::loadConfigRequested, this, &LollipopPlotDock::loadConfigFromTemplate);
	connect(templateHandler, &TemplateHandler::saveConfigRequested, this, &LollipopPlotDock::saveConfigAsTemplate);
	connect(templateHandler, &TemplateHandler::info, this, &LollipopPlotDock::info);

	ui.verticalLayout->addWidget(frame);
}

void LollipopPlotDock::setPlots(QList<LollipopPlot*> list) {
	CONDITIONAL_LOCK_RETURN;
	m_plots = list;
	m_plot = list.first();
	setAspects(list);
	Q_ASSERT(m_plot);
	m_aspectTreeModel = new AspectTreeModel(m_plot->project());
	setModel();

	// if there is more than one point in the list, disable the comment and name widgets in "general"
	if (list.size() == 1) {
		ui.lName->setEnabled(true);
		ui.leName->setEnabled(true);
		ui.lComment->setEnabled(true);
		ui.teComment->setEnabled(true);
		ui.leName->setText(m_plot->name());
		ui.teComment->setText(m_plot->comment());

		ui.lDataColumn->setEnabled(true);
	} else {
		ui.lName->setEnabled(false);
		ui.leName->setEnabled(false);
		ui.lComment->setEnabled(false);
		ui.teComment->setEnabled(false);
		ui.leName->setText(QString());
		ui.teComment->setText(QString());

		ui.lDataColumn->setEnabled(false);
	}
	ui.leName->setStyleSheet(QString());
	ui.leName->setToolTip(QString());

	// backgrounds
	QList<Line*> lines;
	QList<Symbol*> symbols;
	QList<Value*> values;
	for (auto* plot : m_plots) {
		lines << plot->lineAt(0);
		symbols << plot->symbolAt(0);
		values << plot->value();
	}

	lineWidget->setLines(lines);
	symbolWidget->setSymbols(symbols);
	valueWidget->setValues(values);

	// show the properties of the first box plot
	ui.chkVisible->setChecked(m_plot->isVisible());
	load();
	cbXColumn->setColumn(m_plot->xColumn(), m_plot->xColumnPath());
	loadDataColumns();

	updatePlotRanges();

	// set the current locale
	updateLocale();

	// SIGNALs/SLOTs
	// general
	connect(m_plot, &AbstractAspect::aspectDescriptionChanged, this, &LollipopPlotDock::aspectDescriptionChanged);
	connect(m_plot, &WorksheetElement::plotRangeListChanged, this, &LollipopPlotDock::updatePlotRanges);
	connect(m_plot, &LollipopPlot::visibleChanged, this, &LollipopPlotDock::plotVisibilityChanged);
	connect(m_plot, &LollipopPlot::xColumnChanged, this, &LollipopPlotDock::plotXColumnChanged);
	connect(m_plot, &LollipopPlot::dataColumnsChanged, this, &LollipopPlotDock::plotDataColumnsChanged);
}

void LollipopPlotDock::setModel() {
	m_aspectTreeModel->enablePlottableColumnsOnly(true);
	m_aspectTreeModel->enableShowPlotDesignation(true);

	QList<AspectType> list{AspectType::Column};
	m_aspectTreeModel->setSelectableAspects(list);

	list = {AspectType::Folder,
			AspectType::Workbook,
			AspectType::Datapicker,
			AspectType::DatapickerCurve,
			AspectType::Spreadsheet,
			AspectType::LiveDataSource,
			AspectType::Column,
			AspectType::Worksheet,
			AspectType::CartesianPlot,
			AspectType::XYFitCurve,
			AspectType::XYSmoothCurve,
			AspectType::CantorWorksheet};

	cbXColumn->setTopLevelClasses(list);
	cbXColumn->setModel(m_aspectTreeModel);
}

/*
 * updates the locale in the widgets. called when the application settins are changed.
 */
void LollipopPlotDock::updateLocale() {
	lineWidget->updateLocale();
}

void LollipopPlotDock::updatePlotRanges() {
	updatePlotRangeList(ui.cbPlotRanges);
}

void LollipopPlotDock::loadDataColumns() {
	// add the combobox for the first column, is always present
	if (m_dataComboBoxes.count() == 0)
		addDataColumn();

	int count = m_plot->dataColumns().count();
	ui.cbNumberLine->clear();
	ui.cbNumberSymbol->clear();

	if (count != 0) {
		// box plot has already data columns, make sure we have the proper number of comboboxes
		int diff = count - m_dataComboBoxes.count();
		if (diff > 0) {
			for (int i = 0; i < diff; ++i)
				addDataColumn();
		} else if (diff < 0) {
			for (int i = diff; i != 0; ++i)
				removeDataColumn();
		}

		// show the columns in the comboboxes
		for (int i = 0; i < count; ++i)
			m_dataComboBoxes.at(i)->setAspect(m_plot->dataColumns().at(i));

		// show columns names in the combobox for the selection of the bar to be modified
		for (int i = 0; i < count; ++i)
			if (m_plot->dataColumns().at(i)) {
				ui.cbNumberLine->addItem(m_plot->dataColumns().at(i)->name());
				ui.cbNumberSymbol->addItem(m_plot->dataColumns().at(i)->name());
			}
	} else {
		// no data columns set in the box plot yet, we show the first combo box only
		m_dataComboBoxes.first()->setAspect(nullptr);
		for (int i = 0; i < m_dataComboBoxes.count(); ++i)
			removeDataColumn();
	}

	// disable data column widgets if we're modifying more than one box plot at the same time
	bool enabled = (m_plots.count() == 1);
	m_buttonNew->setVisible(enabled);
	for (auto* cb : m_dataComboBoxes)
		cb->setEnabled(enabled);
	for (auto* b : m_removeButtons)
		b->setVisible(enabled);

	// select the first column after all of them were added to the combobox
	ui.cbNumberLine->setCurrentIndex(0);
	ui.cbNumberSymbol->setCurrentIndex(0);
}

void LollipopPlotDock::setDataColumns() const {
	int newCount = m_dataComboBoxes.count();
	int oldCount = m_plot->dataColumns().count();

	if (newCount > oldCount) {
		ui.cbNumberLine->addItem(QString::number(newCount));
		ui.cbNumberSymbol->addItem(QString::number(newCount));
	} else {
		if (newCount != 0) {
			ui.cbNumberLine->removeItem(ui.cbNumberLine->count() - 1);
			ui.cbNumberSymbol->removeItem(ui.cbNumberSymbol->count() - 1);
		}
	}

	QVector<const AbstractColumn*> columns;

	for (auto* cb : m_dataComboBoxes) {
		auto* aspect = cb->currentAspect();
		if (aspect && aspect->type() == AspectType::Column)
			columns << static_cast<AbstractColumn*>(aspect);
	}

	m_plot->setDataColumns(columns);
}

//**********************************************************
//******* SLOTs for changes triggered in LollipopPlotDock *******
//**********************************************************
//"General"-tab
void LollipopPlotDock::xColumnChanged(const QModelIndex& index) {
	auto aspect = static_cast<AbstractAspect*>(index.internalPointer());
	AbstractColumn* column(nullptr);
	if (aspect) {
		column = dynamic_cast<AbstractColumn*>(aspect);
		Q_ASSERT(column);
	}

	ui.bRemoveXColumn->setEnabled(column != nullptr);

	CONDITIONAL_LOCK_RETURN;

	for (auto* plot : m_plots)
		plot->setXColumn(column);
}

void LollipopPlotDock::removeXColumn() {
	cbXColumn->setAspect(nullptr);
	ui.bRemoveXColumn->setEnabled(false);
	for (auto* plot : m_plots)
		plot->setXColumn(nullptr);
}

void LollipopPlotDock::addDataColumn() {
	auto* cb = new TreeViewComboBox;

	static const QList<AspectType> list{AspectType::Folder,
										AspectType::Workbook,
										AspectType::Datapicker,
										AspectType::DatapickerCurve,
										AspectType::Spreadsheet,
										AspectType::LiveDataSource,
										AspectType::Column,
										AspectType::Worksheet,
										AspectType::CartesianPlot,
										AspectType::XYFitCurve,
										AspectType::XYSmoothCurve,
										AspectType::CantorWorksheet};
	cb->setTopLevelClasses(list);
	cb->setModel(m_aspectTreeModel);
	connect(cb, &TreeViewComboBox::currentModelIndexChanged, this, &LollipopPlotDock::dataColumnChanged);

	int index = m_dataComboBoxes.size();

	if (index == 0) {
		QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Preferred);
		sizePolicy1.setHorizontalStretch(0);
		sizePolicy1.setVerticalStretch(0);
		sizePolicy1.setHeightForWidth(cb->sizePolicy().hasHeightForWidth());
		cb->setSizePolicy(sizePolicy1);
	} else {
		auto* button = new QPushButton();
		button->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
		connect(button, &QPushButton::clicked, this, &LollipopPlotDock::removeDataColumn);
		m_gridLayout->addWidget(button, index, 1, 1, 1);
		m_removeButtons << button;
	}

	m_gridLayout->addWidget(cb, index, 0, 1, 1);
	m_gridLayout->addWidget(m_buttonNew, index + 1, 1, 1, 1);

	m_dataComboBoxes << cb;
	ui.lDataColumn->setText(i18n("Columns:"));
}

void LollipopPlotDock::removeDataColumn() {
	auto* sender = static_cast<QPushButton*>(QObject::sender());
	if (sender) {
		// remove button was clicked, determin which one and
		// delete it together with the corresponding combobox
		for (int i = 0; i < m_removeButtons.count(); ++i) {
			if (sender == m_removeButtons.at(i)) {
				delete m_dataComboBoxes.takeAt(i + 1);
				delete m_removeButtons.takeAt(i);
			}
		}
	} else {
		// no sender is available, the function is being called directly in loadDataColumns().
		// delete the last remove button together with the corresponding combobox
		int index = m_removeButtons.count() - 1;
		if (index >= 0) {
			delete m_dataComboBoxes.takeAt(index + 1);
			delete m_removeButtons.takeAt(index);
		}
	}

	// TODO
	if (!m_removeButtons.isEmpty()) {
		ui.lDataColumn->setText(i18n("Columns:"));
	} else {
		ui.lDataColumn->setText(i18n("Column:"));
	}

	if (!m_initializing)
		setDataColumns();
}

void LollipopPlotDock::dataColumnChanged(const QModelIndex&) {
	CONDITIONAL_LOCK_RETURN;

	setDataColumns();
}

void LollipopPlotDock::orientationChanged(int index) {
	CONDITIONAL_LOCK_RETURN;

	auto orientation = LollipopPlot::Orientation(index);
	for (auto* plot : m_plots)
		plot->setOrientation(orientation);
}

void LollipopPlotDock::visibilityChanged(bool state) {
	CONDITIONAL_LOCK_RETURN;

	for (auto* plot : m_plots)
		plot->setVisible(state);
}

//"Line"-tab
/*!
 * called when the current bar number was changed, shows the line properties for the selected bar.
 */
void LollipopPlotDock::currentBarLineChanged(int index) {
	if (index == -1)
		return;

	CONDITIONAL_LOCK_RETURN;

	QList<Line*> lines;
	for (auto* plot : m_plots) {
		auto* line = plot->lineAt(index);
		if (line)
			lines << line;
	}

	lineWidget->setLines(lines);
}

//"Symbol"-tab
/*!
 * called when the current bar number was changed, shows the symbol properties for the selected bar.
 */
void LollipopPlotDock::currentBarSymbolChanged(int index) {
	if (index == -1)
		return;

	CONDITIONAL_LOCK_RETURN;

	QList<Symbol*> symbols;
	for (auto* plot : m_plots) {
		auto* symbol = plot->symbolAt(index);
		if (symbol)
			symbols << symbol;
	}

	symbolWidget->setSymbols(symbols);
}

//*************************************************************
//******* SLOTs for changes triggered in Lollipop ********
//*************************************************************
// general
void LollipopPlotDock::plotXColumnChanged(const AbstractColumn* column) {
	CONDITIONAL_LOCK_RETURN;
	cbXColumn->setColumn(column, m_plot->xColumnPath());
}
void LollipopPlotDock::plotDataColumnsChanged(const QVector<const AbstractColumn*>&) {
	CONDITIONAL_LOCK_RETURN;
	loadDataColumns();
}
void LollipopPlotDock::plotOrientationChanged(LollipopPlot::Orientation orientation) {
	CONDITIONAL_LOCK_RETURN;
	ui.cbOrientation->setCurrentIndex((int)orientation);
}
void LollipopPlotDock::plotVisibilityChanged(bool on) {
	CONDITIONAL_LOCK_RETURN;
	ui.chkVisible->setChecked(on);
}

//**********************************************************
//******************** SETTINGS ****************************
//**********************************************************
void LollipopPlotDock::load() {
	// general
	ui.cbOrientation->setCurrentIndex((int)m_plot->orientation());
}

void LollipopPlotDock::loadConfig(KConfig& config) {
	KConfigGroup group = config.group(QLatin1String("Lollipop"));

	// general
	ui.cbOrientation->setCurrentIndex(group.readEntry("Orientation", (int)m_plot->orientation()));

	lineWidget->loadConfig(group);
	symbolWidget->loadConfig(group);
	valueWidget->loadConfig(group);
}

void LollipopPlotDock::loadConfigFromTemplate(KConfig& config) {
	// extract the name of the template from the file name
	QString name;
	int index = config.name().lastIndexOf(QLatin1String("/"));
	if (index != -1)
		name = config.name().right(config.name().size() - index - 1);
	else
		name = config.name();

	int size = m_plots.size();
	if (size > 1)
		m_plot->beginMacro(i18n("%1 lollipop plots: template \"%2\" loaded", size, name));
	else
		m_plot->beginMacro(i18n("%1: template \"%2\" loaded", m_plot->name(), name));

	this->loadConfig(config);

	m_plot->endMacro();
}

void LollipopPlotDock::saveConfigAsTemplate(KConfig& config) {
	KConfigGroup group = config.group("Lollipop");

	// general
	group.writeEntry("Orientation", ui.cbOrientation->currentIndex());

	lineWidget->saveConfig(group);
	symbolWidget->saveConfig(group);
	valueWidget->saveConfig(group);

	config.sync();
}