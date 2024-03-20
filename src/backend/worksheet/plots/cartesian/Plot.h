/*
	File                 : Plot.h
	Project              : LabPlot
	Description          : Base class for all plots like scatter plot, box plot, etc.
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2020-2023 Alexander Semke <alexander.semke@web.de>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PLOT_H
#define PLOT_H

#include "backend/lib/macros.h"
#include "backend/worksheet/WorksheetElement.h"
#include "backend/worksheet/plots/cartesian/CartesianCoordinateSystem.h"

class AbstractColumn;
class Column;
class PlotPrivate;
class QPointF;

class Plot : public WorksheetElement {
	Q_OBJECT

public:
	virtual ~Plot();

	BASIC_D_ACCESSOR_DECL(bool, legendVisible, LegendVisible)
	virtual bool minMax(const CartesianCoordinateSystem::Dimension dim, const Range<int>& indexRange, Range<double>& r, bool includeErrorBars = true) const;
	virtual double minimum(CartesianCoordinateSystem::Dimension dim) const = 0;
	virtual double maximum(CartesianCoordinateSystem::Dimension dim) const = 0;
	virtual bool hasData() const = 0;
	bool activatePlot(QPointF mouseScenePos, double maxDist = -1);
	virtual QColor color() const = 0; // Color of the plot. If the plot consists multiple colors, return the main Color (This is used in the cursor dock as
									  // background color for example)

	/*!
	 * returns \c true if the column is used internally in the plot for the visualisation, returns \c false otherwise.
	 */
	virtual bool usingColumn(const Column*) const = 0;

	/*!
	 * recalculates the internal structures (additional data containers, drawing primitives, etc.) on data changes in the source data colums.
	 * these structures are used in the plot during the actual drawing of the plot on geometry changes.
	 */
	virtual void recalc() = 0;

	/*!
	 * This function is called when a column in the project was renamed or a new column was added
	 * with the name/path that was potentially used earlier in the plot.
	 * The implementation in the derived classes should handle these two cases and update the visualisation accordingly:
	 * 1. the column is the same and was just renamed -> update the column path internally
	 * 2. another column was added or renamed and fits to the path that was used before -> set and connect to the new column and update the visualisation
	 */
	virtual void updateColumnDependencies(const AbstractColumn*) = 0;

	typedef PlotPrivate Private;

private:
	Q_DECLARE_PRIVATE(Plot)

protected:
	Plot(const QString&, PlotPrivate* dd, AspectType);
	PlotPrivate* const d_ptr;

Q_SIGNALS:
	void dataChanged(); // emitted when the data to be plotted was changed to re-adjust the parent plot area
	void appearanceChanged();
	void legendVisibleChanged(bool);
};

#endif
