/***************************************************************************
    File                 : Column.h
    Project              : LabPlot
    Description          : Aspect that manages a column
    --------------------------------------------------------------------
    Copyright            : (C) 2007-2009 Tilman Benkert (thzs@gmx.net)
    Copyright            : (C) 2013-2017 Alexander Semke (alexander.semke@web.de)
    Copyright            : (C) 2017 Stefan Gerlach (stefan.gerlach@uni.kn)

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

#ifndef COLUMN_H
#define COLUMN_H

#include "backend/core/AbstractSimpleFilter.h"
#include "backend/lib/XmlStreamReader.h"
#include "backend/core/column/ColumnPrivate.h"

class ColumnStringIO;
class QActionGroup;

class Column : public AbstractColumn {
	Q_OBJECT

public:
	explicit Column(const QString& name, AbstractColumn::ColumnMode = AbstractColumn::Numeric);
	// template constructor for all supported data types (AbstractColumn::ColumnMode) must be defined in header
	template <typename T>
	explicit Column(const QString& name, QVector<T> data, AbstractColumn::ColumnMode mode = AbstractColumn::Numeric)
		: AbstractColumn(name), d(new ColumnPrivate(this, mode, new QVector<T>(data))) {
		init();
	};
	void init();
	~Column();

	virtual QIcon icon() const override;
	virtual QMenu* createContextMenu() override;

	AbstractColumn::ColumnMode columnMode() const override;
	void setColumnMode(AbstractColumn::ColumnMode) override;

	bool copy(const AbstractColumn*) override;
	bool copy(const AbstractColumn* source, int source_start, int dest_start, int num_rows) override;

	AbstractColumn::PlotDesignation plotDesignation() const override;
	void setPlotDesignation(AbstractColumn::PlotDesignation) override;

	bool isReadOnly() const override;
	int rowCount() const override;
	int width() const;
	void setWidth(const int);
	void clear() override;
	AbstractSimpleFilter* outputFilter() const;
	ColumnStringIO* asStringColumn() const;

	void setFormula(const QString& formula, const QStringList& variableNames, const QStringList& variableColumnPathes);
	QString formula() const;
	const QStringList& formulaVariableNames() const;
	const QStringList& formulaVariableColumnPathes() const;
	QString formula(const int) const  override;
	QList< Interval<int> > formulaIntervals() const override;
	void setFormula(Interval<int>, QString) override;
	void setFormula(int, QString) override;
	void clearFormulas() override;

	const AbstractColumn::ColumnStatistics& statistics();
	void* data() const;

	QString textAt(const int) const override;
	void setTextAt(const int, const QString&) override;
	void replaceTexts(const int, const QVector<QString>&) override;
	QDate dateAt(const int) const override;
	void setDateAt(const int, const QDate&) override;
	QTime timeAt(const int) const override;
	void setTimeAt(const int, const QTime&) override;
	QDateTime dateTimeAt(const int) const override;
	void setDateTimeAt(const int, const QDateTime&) override;
	void replaceDateTimes(const int, const QVector<QDateTime>&) override;
	double valueAt(const int) const override;
	void setValueAt(const int, const double) override;
	virtual void replaceValues(const int, const QVector<double>&) override;
	int integerAt(const int) const override;
	void setIntegerAt(const int, const int) override;
	virtual void replaceInteger(const int, const QVector<int>&) override;

	virtual double maximum(int count = 0) const override;
	virtual double minimum(int count = 0) const override;

	void setChanged();
	void setSuppressDataChangedSignal(const bool);

	void save(QXmlStreamWriter*) const override;
	bool load(XmlStreamReader*, bool preview) override;

private:
	bool XmlReadInputFilter(XmlStreamReader*);
	bool XmlReadOutputFilter(XmlStreamReader*);
	bool XmlReadFormula(XmlStreamReader*);
	bool XmlReadRow(XmlStreamReader*);

	void handleRowInsertion(int before, int count) override;
	void handleRowRemoval(int first, int count) override;

	void calculateStatistics();
	void setStatisticsAvailable(bool);
	bool statisticsAvailable() const;

	bool m_suppressDataChangedSignal;
	QActionGroup* m_usedInActionGroup;

	ColumnPrivate* d;
	ColumnStringIO* m_string_io;

signals:
	void requestProjectContextMenu(QMenu*);

private slots:
	void navigateTo(QAction*);
	void handleFormatChange();

	friend class ColumnPrivate;
	friend class ColumnStringIO;
};


#endif
