/***************************************************************************
    File                 : XYFourierFilterCurve.cpp
    Project              : LabPlot
    Description          : A xy-curve defined by a Fourier filter
    --------------------------------------------------------------------
    Copyright            : (C) 2016 Stefan Gerlach (stefan.gerlach@uni.kn)

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

/*!
  \class XYFourierFilterCurve
  \brief A xy-curve defined by a Fourier filter

  \ingroup worksheet
*/

#include "XYFourierFilterCurve.h"
#include "XYFourierFilterCurvePrivate.h"
#include "backend/core/AbstractColumn.h"
#include "backend/core/column/Column.h"
#include "backend/lib/commandtemplates.h"
#include <cmath>	// isnan
/*#include "backend/gsl/ExpressionParser.h"
#include "backend/gsl/parser_extern.h"
*/
//#include <gsl/gsl_version.h>
#include <gsl/gsl_fft_real.h>
#include <gsl/gsl_fft_halfcomplex.h>

#include <KIcon>
#include <KLocale>
#include <QElapsedTimer>
#include <QDebug>

XYFourierFilterCurve::XYFourierFilterCurve(const QString& name)
		: XYCurve(name, new XYFourierFilterCurvePrivate(this)) {
	init();
}

XYFourierFilterCurve::XYFourierFilterCurve(const QString& name, XYFourierFilterCurvePrivate* dd)
		: XYCurve(name, dd) {
	init();
}


XYFourierFilterCurve::~XYFourierFilterCurve() {
	//no need to delete the d-pointer here - it inherits from QGraphicsItem
	//and is deleted during the cleanup in QGraphicsScene
}

void XYFourierFilterCurve::init() {
	Q_D(XYFourierFilterCurve);

	//TODO: read from the saved settings for XYFourierFilterCurve?
	d->lineType = XYCurve::Line;
	d->symbolsStyle = Symbol::NoSymbols;
}

void XYFourierFilterCurve::recalculate() {
	Q_D(XYFourierFilterCurve);
	d->recalculate();
}

/*!
	Returns an icon to be used in the project explorer.
*/
QIcon XYFourierFilterCurve::icon() const {
	return KIcon("labplot-xy-fourier_filter-curve");
}

//##############################################################################
//##########################  getter methods  ##################################
//##############################################################################
BASIC_SHARED_D_READER_IMPL(XYFourierFilterCurve, const AbstractColumn*, xDataColumn, xDataColumn)
BASIC_SHARED_D_READER_IMPL(XYFourierFilterCurve, const AbstractColumn*, yDataColumn, yDataColumn)
const QString& XYFourierFilterCurve::xDataColumnPath() const { Q_D(const XYFourierFilterCurve); return d->xDataColumnPath; }
const QString& XYFourierFilterCurve::yDataColumnPath() const { Q_D(const XYFourierFilterCurve); return d->yDataColumnPath; }

BASIC_SHARED_D_READER_IMPL(XYFourierFilterCurve, XYFourierFilterCurve::FilterData, filterData, filterData)

const XYFourierFilterCurve::FilterResult& XYFourierFilterCurve::filterResult() const {
	Q_D(const XYFourierFilterCurve);
	return d->filterResult;
}

bool XYFourierFilterCurve::isSourceDataChangedSinceLastFilter() const {
	Q_D(const XYFourierFilterCurve);
	return d->sourceDataChangedSinceLastFilter;
}

//##############################################################################
//#################  setter methods and undo commands ##########################
//##############################################################################
STD_SETTER_CMD_IMPL_S(XYFourierFilterCurve, SetXDataColumn, const AbstractColumn*, xDataColumn)
void XYFourierFilterCurve::setXDataColumn(const AbstractColumn* column) {
	qDebug()<<"XYFourierFilterCurve::setXDataColumn()";
	Q_D(XYFourierFilterCurve);
	if (column != d->xDataColumn) {
		exec(new XYFourierFilterCurveSetXDataColumnCmd(d, column, i18n("%1: assign x-data")));
		emit sourceDataChangedSinceLastFilter();
		if (column) {
			connect(column, SIGNAL(dataChanged(const AbstractColumn*)), this, SLOT(handleSourceDataChanged()));
			//TODO disconnect on undo
		}
	}
}

STD_SETTER_CMD_IMPL_S(XYFourierFilterCurve, SetYDataColumn, const AbstractColumn*, yDataColumn)
void XYFourierFilterCurve::setYDataColumn(const AbstractColumn* column) {
	Q_D(XYFourierFilterCurve);
	if (column != d->yDataColumn) {
		exec(new XYFourierFilterCurveSetYDataColumnCmd(d, column, i18n("%1: assign y-data")));
		emit sourceDataChangedSinceLastFilter();
		if (column) {
			connect(column, SIGNAL(dataChanged(const AbstractColumn*)), this, SLOT(handleSourceDataChanged()));
			//TODO disconnect on undo
		}
	}
}

STD_SETTER_CMD_IMPL_F_S(XYFourierFilterCurve, SetFilterData, XYFourierFilterCurve::FilterData, filterData, recalculate);
void XYFourierFilterCurve::setFilterData(const XYFourierFilterCurve::FilterData& filterData) {
	Q_D(XYFourierFilterCurve);
	exec(new XYFourierFilterCurveSetFilterDataCmd(d, filterData, i18n("%1: set filter options and perform the Fourier filter")));
}

//##############################################################################
//################################## SLOTS ####################################
//##############################################################################
void XYFourierFilterCurve::handleSourceDataChanged() {
	Q_D(XYFourierFilterCurve);
	d->sourceDataChangedSinceLastFilter = true;
	emit sourceDataChangedSinceLastFilter();
}
//##############################################################################
//######################### Private implementation #############################
//##############################################################################
XYFourierFilterCurvePrivate::XYFourierFilterCurvePrivate(XYFourierFilterCurve* owner) : XYCurvePrivate(owner),
	xDataColumn(0), yDataColumn(0), 
	xColumn(0), yColumn(0), 
	xVector(0), yVector(0), 
	sourceDataChangedSinceLastFilter(true),
	q(owner)  {

}

XYFourierFilterCurvePrivate::~XYFourierFilterCurvePrivate() {
	//no need to delete xColumn and yColumn, they are deleted
	//when the parent aspect is removed
}

// ...
// see XYFitCurvePrivate

void XYFourierFilterCurvePrivate::recalculate() {
	qDebug()<<"XYFourierFilterCurvePrivate::recalculate()";

	QElapsedTimer timer;
	timer.start();

	//create filter result columns if not available yet, clear them otherwise
	if (!xColumn) {
		xColumn = new Column("x", AbstractColumn::Numeric);
		yColumn = new Column("y", AbstractColumn::Numeric);
		xVector = static_cast<QVector<double>* >(xColumn->data());
		yVector = static_cast<QVector<double>* >(yColumn->data());

		xColumn->setHidden(true);
		q->addChild(xColumn);
		yColumn->setHidden(true);
		q->addChild(yColumn);

		q->setUndoAware(false);
		q->setXColumn(xColumn);
		q->setYColumn(yColumn);
		q->setUndoAware(true);
	} else {
		xVector->clear();
		yVector->clear();
	}

	// clear the previous result
	filterResult = XYFourierFilterCurve::FilterResult();

	if (!xDataColumn || !yDataColumn) {
		emit (q->dataChanged());
		sourceDataChangedSinceLastFilter = false;
		return;
	}

	//check column sizes
	if (xDataColumn->rowCount()!=yDataColumn->rowCount()) {
		filterResult.available = true;
		filterResult.valid = false;
		filterResult.status = i18n("Number of x and y data points must be equal.");
		emit (q->dataChanged());
		sourceDataChangedSinceLastFilter = false;
		return;
	}

	//copy all valid data point for the fit to temporary vectors
	QVector<double> xdataVector;
	QVector<double> ydataVector;
	for (int row=0; row<xDataColumn->rowCount(); ++row) {
		//only copy those data where _all_ values (for x and y, if given) are valid
		if (!isnan(xDataColumn->valueAt(row)) && !isnan(yDataColumn->valueAt(row))
			&& !xDataColumn->isMasked(row) && !yDataColumn->isMasked(row)) {

			xdataVector.append(xDataColumn->valueAt(row));
			ydataVector.append(yDataColumn->valueAt(row));
		}
	}

	//number of data points to fit
	unsigned int n = ydataVector.size();
	if (n == 0) {
		filterResult.available = true;
		filterResult.valid = false;
		filterResult.status = i18n("No data points available.");
		emit (q->dataChanged());
		sourceDataChangedSinceLastFilter = false;
		return;
	}

	//double* xdata = xdataVector.data();
	double* ydata = ydataVector.data();

        double min = xDataColumn->minimum();
        double max = xDataColumn->maximum();

	// filter settings
	XYFourierFilterCurve::FilterType type = filterData.type;
	double cutoff = filterData.cutoff, cutoff2 = filterData.cutoff2;
	XYFourierFilterCurve::CutoffUnit unit = filterData.unit, unit2 = filterData.unit2;
#ifdef QT_DEBUG
	qDebug()<<"cutoffs ="<<cutoff<<cutoff2;
	qDebug()<<"unit ="<<unit<<unit2;
#endif
///////////////////////////////////////////////////////////
	//see http://www.originlab.com/doc/Origin-Help/2DFFT-Filter-Algorithm
	// http://www.imagemet.com/WebHelp6/Default.htm#FourierAnalysis/Band_Filtering.htm

// Fourier transform
	gsl_fft_real_workspace *work = gsl_fft_real_workspace_alloc (n);
	gsl_fft_real_wavetable *real = gsl_fft_real_wavetable_alloc (n);

        gsl_fft_real_transform (ydata, 1, n, real, work);
        gsl_fft_real_wavetable_free (real);

	double cutindex=0, cutindex2=0;
	switch(unit) {
	case XYFourierFilterCurve::Frequency:
		cutindex = cutoff*(max-min);
		break;
	case XYFourierFilterCurve::Fraction:
		cutindex = cutoff*n;
		break;
	case XYFourierFilterCurve::Index:
		cutindex = cutoff;
	}


// calculate filter
	switch(filterData.type) {
	case XYFourierFilterCurve::LowPass:
		for (unsigned int i = cutindex; i < n; i++)
			ydata[i] = 0;
		break;
	case XYFourierFilterCurve::HighPass:
		for (unsigned int i = 0; i < cutindex; i++)
			ydata[i] = 0;
		break;
//TODO: other types
	default:
		qDebug()<<"Filter type not supported!";
	}

// back transform
	gsl_fft_halfcomplex_wavetable *hc = gsl_fft_halfcomplex_wavetable_alloc (n);
	gsl_fft_halfcomplex_inverse (ydata, 1, n, hc, work);
	gsl_fft_halfcomplex_wavetable_free (hc);
	gsl_fft_real_workspace_free (work);

	xVector->resize(n);
	yVector->resize(n);
	for (unsigned int i = 0; i < n; i++) {
		(*xVector)[i] = xdataVector[i];
		(*yVector)[i] = ydata[i];
	}
///////////////////////////////////////////////////////////

//	for (unsigned int i = 0; i < n; i++) {
//		qDebug()<<xdata[i]<<ydata[i];
//	}
	
	//write the result
	filterResult.available = true;
	filterResult.valid = true;
	filterResult.elapsedTime = timer.elapsed();

	//redraw the curve
	emit (q->dataChanged());
	sourceDataChangedSinceLastFilter = false;
}

//##############################################################################
//##################  Serialization/Deserialization  ###########################
//##############################################################################
//! Save as XML
void XYFourierFilterCurve::save(QXmlStreamWriter* writer) const{
	Q_D(const XYFourierFilterCurve);

	writer->writeStartElement("xyFourierFilterCurve");

	//write xy-curve information
	XYCurve::save(writer);

	//write xy-fourier_filter-curve specific information
	//filter data
	writer->writeStartElement("filterData");
	WRITE_COLUMN(d->xDataColumn, xDataColumn);
	WRITE_COLUMN(d->yDataColumn, yDataColumn);
	writer->writeAttribute( "type", QString::number(d->filterData.type) );
	writer->writeAttribute( "form", QString::number(d->filterData.form) );
	writer->writeAttribute( "cutoff", QString::number(d->filterData.cutoff) );
	writer->writeAttribute( "unit", QString::number(d->filterData.unit) );
	writer->writeAttribute( "cutoff2", QString::number(d->filterData.cutoff2) );
	writer->writeAttribute( "unit2", QString::number(d->filterData.unit2) );
	writer->writeEndElement();// filterData

	//filter results (generated columns)
	writer->writeStartElement("filterResult");
	writer->writeAttribute( "available", QString::number(d->filterResult.available) );
	writer->writeAttribute( "valid", QString::number(d->filterResult.valid) );
	writer->writeAttribute( "status", d->filterResult.status );
	writer->writeAttribute( "time", QString::number(d->filterResult.elapsedTime) );

	//save calculated columns if available
	if (d->xColumn) {
		d->xColumn->save(writer);
		d->yColumn->save(writer);
	}
	writer->writeEndElement(); //"filterResult"

	writer->writeEndElement(); //"xyFourierFilterCurve"
}

//! Load from XML
bool XYFourierFilterCurve::load(XmlStreamReader* reader){
	Q_D(XYFourierFilterCurve);

	if(!reader->isStartElement() || reader->name() != "xyFourierFilterCurve"){
		reader->raiseError(i18n("no xy Fourier filter curve element found"));
		return false;
	}

	QString attributeWarning = i18n("Attribute '%1' missing or empty, default value is used");
	QXmlStreamAttributes attribs;
	QString str;

	while (!reader->atEnd()) {
		reader->readNext();
		if (reader->isEndElement() && reader->name() == "xyFourierFilterCurve")
			break;

		if (!reader->isStartElement())
			continue;

		if (reader->name() == "xyCurve") {
			if ( !XYCurve::load(reader) )
				return false;
		} else if (reader->name() == "filterData") {
			attribs = reader->attributes();

			READ_COLUMN(xDataColumn);
			READ_COLUMN(yDataColumn);
			qDebug()<<"	x column"<<xDataColumnPath();

			str = attribs.value("type").toString();
			if(str.isEmpty())
				reader->raiseWarning(attributeWarning.arg("'type'"));
			else
				d->filterData.type = (XYFourierFilterCurve::FilterType)str.toInt();

			str = attribs.value("form").toString();
			if(str.isEmpty())
				reader->raiseWarning(attributeWarning.arg("'form'"));
			else
				d->filterData.form = (XYFourierFilterCurve::FilterForm)str.toInt();

			str = attribs.value("cutoff").toString();
			if(str.isEmpty())
				reader->raiseWarning(attributeWarning.arg("'cutoff'"));
			else
				d->filterData.cutoff = str.toDouble();

			str = attribs.value("unit").toString();
			if(str.isEmpty())
				reader->raiseWarning(attributeWarning.arg("'unit'"));
			else
				d->filterData.unit = (XYFourierFilterCurve::CutoffUnit)str.toInt();

			str = attribs.value("cutoff2").toString();
			if(str.isEmpty())
				reader->raiseWarning(attributeWarning.arg("'cutoff2'"));
			else
				d->filterData.cutoff2 = str.toDouble();

			str = attribs.value("unit2").toString();
			if(str.isEmpty())
				reader->raiseWarning(attributeWarning.arg("'unit2'"));
			else
				d->filterData.unit2 = (XYFourierFilterCurve::CutoffUnit)str.toInt();
		} else if (reader->name() == "filterResult") {

			attribs = reader->attributes();

			str = attribs.value("available").toString();
			if(str.isEmpty())
				reader->raiseWarning(attributeWarning.arg("'available'"));
			else
				d->filterResult.available = str.toInt();

			str = attribs.value("valid").toString();
			if(str.isEmpty())
				reader->raiseWarning(attributeWarning.arg("'valid'"));
			else
				d->filterResult.valid = str.toInt();
			
			str = attribs.value("status").toString();
			if(str.isEmpty())
				reader->raiseWarning(attributeWarning.arg("'status'"));
			else
				d->filterResult.status = str;

			str = attribs.value("time").toString();
			if(str.isEmpty())
				reader->raiseWarning(attributeWarning.arg("'time'"));
			else
				d->filterResult.elapsedTime = str.toInt();
		} else if(reader->name() == "column") {
			qDebug()<<"	reading filter column";
			Column* column = new Column("", AbstractColumn::Numeric);
			if (!column->load(reader)) {
				delete column;
				return false;
			}
			if (column->name()=="x")
				d->xColumn = column;
			else if (column->name()=="y")
				d->yColumn = column;
		}
	}

	if (d->xColumn) {
		qDebug()<<"	add filter columns";
		d->xColumn->setHidden(true);
		addChild(d->xColumn);

		d->yColumn->setHidden(true);
		addChild(d->yColumn);

		d->xVector = static_cast<QVector<double>* >(d->xColumn->data());
		d->yVector = static_cast<QVector<double>* >(d->yColumn->data());

		setUndoAware(false);
		XYCurve::d_ptr->xColumn = d->xColumn;
		XYCurve::d_ptr->yColumn = d->yColumn;
		setUndoAware(true);
	}

	return true;
}
