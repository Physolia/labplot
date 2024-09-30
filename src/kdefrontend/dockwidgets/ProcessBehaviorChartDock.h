/*
	File                 : ProcessBehaviorChartDock.h
	Project              : LabPlot
	Description          : widget for properties of the process behavior chart
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2024 Alexander Semke <alexander.semke@web.de>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROCESSBEHAVIORCHARTDOCK_H
#define PROCESSBEHAVIORCHARTDOCK_H

#include "backend/worksheet/plots/cartesian/ProcessBehaviorChart.h"
#include "kdefrontend/dockwidgets/BaseDock.h"
#include "ui_processbehaviorchartdock.h"

class LineWidget;
class ProcessBehaviorChart;
class SymbolWidget;
class TreeViewComboBox;

class ProcessBehaviorChartDock : public BaseDock {
	Q_OBJECT

public:
	explicit ProcessBehaviorChartDock(QWidget*);
	~ProcessBehaviorChartDock() override;

	void setPlots(QList<ProcessBehaviorChart*>);
	void updateLocale() override;

private:
	TreeViewComboBox* cbDataColumn;

	void load();
	void loadConfig(KConfig&);

protected:
	Ui::ProcessBehaviorChartDock ui;
	LineWidget* dataLineWidget{nullptr};
	SymbolWidget* dataSymbolWidget{nullptr};
	LineWidget* centerLineWidget{nullptr};
	LineWidget* upperLimitLineWidget{nullptr};
	LineWidget* lowerLimitLineWidget{nullptr};

	QList<ProcessBehaviorChart*> m_plots;
	ProcessBehaviorChart* m_plot{nullptr};

	virtual void setModel();

private Q_SLOTS:
	void retranslateUi();

	// SLOTs for changes triggered in ProcessBehaviorChartDock
	// General-Tab
	void dataColumnChanged(const QModelIndex&);
	void typeChanged(int);
	void limitsMetricChanged(int);
	void subgroupSizeChanged(int);
	void negativeLowerLimitEnabledChanged(bool);

	// SLOTs for changes triggered in ProcessBehaviorChart
	// General-Tab
	void plotDataColumnChanged(const AbstractColumn*);
	void plotTypeChanged(ProcessBehaviorChart::Type);
	void plotLimitsMetricChanged(ProcessBehaviorChart::LimitsMetric);
	void plotSubgroupSizeChanged(int);
	void plotNegativeLowerLimitEnabledChanged(bool);

	// load and save
	void loadConfigFromTemplate(KConfig&);
	void saveConfigAsTemplate(KConfig&);

Q_SIGNALS:
	void info(const QString&);
};

#endif
