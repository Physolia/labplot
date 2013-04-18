/***************************************************************************
    File                 : Double2StringFilter.h
    Project              : AbstractColumn
    --------------------------------------------------------------------
    Copyright            : (C) 2007 by Knut Franke, Tilman Benkert
    Email (use @ for *)  : knut.franke*gmx.de, thzs@gmx.net
    Description          : Locale-aware conversion filter double -> QString.
                           
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
#ifndef DOUBLE2STRING_FILTER_H
#define DOUBLE2STRING_FILTER_H

#include "../AbstractSimpleFilter.h"
#include <QLocale>
#include <QChar>
#include <math.h>

//! Locale-aware conversion filter double -> QString.
class Double2StringFilter : public AbstractSimpleFilter
{
	Q_OBJECT

	public:
		//! Standard constructor.
		explicit Double2StringFilter(char format='e', int digits=6) : m_format(format), m_digits(digits) {}
		//! Set format character as in QString::number
		void setNumericFormat(char format);
		//! Set number of displayed digits
		void setNumDigits(int digits);
		//! Get format character as in QString::number
		char numericFormat() const { return m_format; }
		//! Get number of displayed digits
		int numDigits() const { return m_digits; }

		//! Return the data type of the column
		virtual AbstractColumn::ColumnMode columnMode() const { return AbstractColumn::Text; }

	signals:
		void formatChanged();

	private:
		friend class Double2StringFilterSetFormatCmd;
		friend class Double2StringFilterSetDigitsCmd;
		//! Format character as in QString::number 
		char m_format;
		//! Display digits or precision as in QString::number  
		int m_digits;


		//! \name XML related functions
		//@{
		virtual void writeExtraAttributes(QXmlStreamWriter * writer) const;
		virtual bool load(XmlStreamReader * reader);
		//@}

	public:
		virtual QString textAt(int row) const {
			if (!m_inputs.value(0)) return QString();
			if (m_inputs.value(0)->rowCount() <= row) return QString();
			double inputValue = m_inputs.value(0)->valueAt(row);
			if (isnan(inputValue)) return QString();
			return QLocale().toString(inputValue, m_format, m_digits);
		}

	protected:
		//! Using typed ports: only double inputs are accepted.
		virtual bool inputAcceptable(int, const AbstractColumn *source) {
			return source->columnMode() == AbstractColumn::Numeric;
		}
};

#endif // ifndef DOUBLE2STRING_FILTER_H

