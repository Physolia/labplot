/*
	File                 : ColumnPrivate.cpp
	Project              : AbstractColumn
	Description          : Private data class of Column
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2007-2008 Tilman Benkert <thzs@gmx.net>
	SPDX-FileCopyrightText: 2012-2019 Alexander Semke <alexander.semke@web.de>
	SPDX-FileCopyrightText: 2017-2022 Stefan Gerlach <stefan.gerlach@uni.kn>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ColumnPrivate.h"
#include "Column.h"
#include "ColumnStringIO.h"
#include "backend/core/datatypes/filter.h"
#include "backend/gsl/ExpressionParser.h"
#include "backend/lib/trace.h"
#include "backend/spreadsheet/Spreadsheet.h"

#include <array>
#include <unordered_map>

extern "C" {
#include "backend/nsl/nsl_stats.h"
#include <gsl/gsl_math.h>
#include <gsl/gsl_statistics.h>
}

ColumnPrivate::ColumnPrivate(Column* owner, AbstractColumn::ColumnMode mode)
	: m_columnMode(mode)
	, m_owner(owner) {
	initIOFilters();
}

/**
 * \brief Special ctor (to be called from Column only!)
 */
ColumnPrivate::ColumnPrivate(Column* owner, AbstractColumn::ColumnMode mode, void* data)
	: m_columnMode(mode)
	, m_data(data)
	, m_owner(owner) {
	initIOFilters();
}

void ColumnPrivate::initDataContainer() {
	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double: {
		auto* vec = new QVector<double>();
		vec->resize(m_rowCount);
		vec->fill(std::numeric_limits<double>::quiet_NaN());
		m_data = vec;
		break;
	}
	case AbstractColumn::ColumnMode::Integer: {
		auto* vec = new QVector<int>();
		vec->resize(m_rowCount);
		m_data = vec;
		break;
	}
	case AbstractColumn::ColumnMode::BigInt: {
		auto* vec = new QVector<qint64>();
		vec->resize(m_rowCount);
		m_data = vec;
		break;
	}
	case AbstractColumn::ColumnMode::Text: {
		auto* vec = new QVector<QString>();
		vec->resize(m_rowCount);
		m_data = vec;
		break;
	}
	case AbstractColumn::ColumnMode::DateTime:
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day: {
		auto* vec = new QVector<QDateTime>();
		vec->resize(m_rowCount);
		m_data = vec;
		break;
	}
	}
}

void ColumnPrivate::initIOFilters() {
	SET_NUMBER_LOCALE
	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double:
		m_inputFilter = new String2DoubleFilter();
		m_inputFilter->setNumberLocale(numberLocale);
		m_outputFilter = new Double2StringFilter();
		m_outputFilter->setNumberLocale(numberLocale);
		break;
	case AbstractColumn::ColumnMode::Integer:
		m_inputFilter = new String2IntegerFilter();
		m_inputFilter->setNumberLocale(numberLocale);
		m_outputFilter = new Integer2StringFilter();
		m_outputFilter->setNumberLocale(numberLocale);
		break;
	case AbstractColumn::ColumnMode::BigInt:
		m_inputFilter = new String2BigIntFilter();
		m_inputFilter->setNumberLocale(numberLocale);
		m_outputFilter = new BigInt2StringFilter();
		m_outputFilter->setNumberLocale(numberLocale);
		break;
	case AbstractColumn::ColumnMode::Text:
		m_inputFilter = new SimpleCopyThroughFilter();
		m_outputFilter = new SimpleCopyThroughFilter();
		break;
	case AbstractColumn::ColumnMode::DateTime:
		m_inputFilter = new String2DateTimeFilter();
		m_outputFilter = new DateTime2StringFilter();
		break;
	case AbstractColumn::ColumnMode::Month:
		m_inputFilter = new String2MonthFilter();
		m_outputFilter = new DateTime2StringFilter();
		static_cast<DateTime2StringFilter*>(m_outputFilter)->setFormat(QStringLiteral("MMMM"));
		break;
	case AbstractColumn::ColumnMode::Day:
		m_inputFilter = new String2DayOfWeekFilter();
		m_outputFilter = new DateTime2StringFilter();
		static_cast<DateTime2StringFilter*>(m_outputFilter)->setFormat(QStringLiteral("dddd"));
		break;
	}

	connect(m_outputFilter, &AbstractSimpleFilter::formatChanged, m_owner, &Column::handleFormatChange);
}

ColumnPrivate::~ColumnPrivate() {
	if (!m_data)
		return;

	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double:
		delete static_cast<QVector<double>*>(m_data);
		break;
	case AbstractColumn::ColumnMode::Integer:
		delete static_cast<QVector<int>*>(m_data);
		break;
	case AbstractColumn::ColumnMode::BigInt:
		delete static_cast<QVector<qint64>*>(m_data);
		break;
	case AbstractColumn::ColumnMode::Text:
		delete static_cast<QVector<QString>*>(m_data);
		break;
	case AbstractColumn::ColumnMode::DateTime:
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
		delete static_cast<QVector<QDateTime>*>(m_data);
		break;
	}
}

AbstractColumn::ColumnMode ColumnPrivate::columnMode() const {
	return m_columnMode;
}

/**
 * \brief Set the column mode
 *
 * This sets the column mode and, if
 * necessary, converts it to another datatype.
 * Remark: setting the mode back to undefined (the
 * initial value) is not supported.
 */
void ColumnPrivate::setColumnMode(AbstractColumn::ColumnMode mode) {
	//	DEBUG(Q_FUNC_INFO << ", " << ENUM_TO_STRING(AbstractColumn, ColumnMode, m_column_mode)
	//		<< " -> " << ENUM_TO_STRING(AbstractColumn, ColumnMode, mode))
	if (mode == m_columnMode)
		return;

	void* old_data = m_data;
	// remark: the deletion of the old data will be done in the dtor of a command

	AbstractSimpleFilter *filter{nullptr}, *new_in_filter{nullptr}, *new_out_filter{nullptr};
	bool filter_is_temporary = false; // it can also become outputFilter(), which we may not delete here
	Column* temp_col = nullptr;

	Q_EMIT m_owner->modeAboutToChange(m_owner);

	// determine the conversion filter and allocate the new data vector
	SET_NUMBER_LOCALE
	switch (m_columnMode) { // old mode
	case AbstractColumn::ColumnMode::Double: {
		disconnect(static_cast<Double2StringFilter*>(m_outputFilter), &Double2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		switch (mode) {
		case AbstractColumn::ColumnMode::Double:
			break;
		case AbstractColumn::ColumnMode::Integer:
			filter = new Double2IntegerFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<double>*>(old_data)));
				m_data = new QVector<int>();
			}
			break;
		case AbstractColumn::ColumnMode::BigInt:
			filter = new Double2BigIntFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<double>*>(old_data)));
				m_data = new QVector<qint64>();
			}
			break;
		case AbstractColumn::ColumnMode::Text:
			filter = outputFilter();
			filter_is_temporary = false;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<double>*>(old_data)));
				m_data = new QVector<QString>();
			}
			break;
		case AbstractColumn::ColumnMode::DateTime:
			filter = new Double2DateTimeFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<double>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		case AbstractColumn::ColumnMode::Month:
			filter = new Double2MonthFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<double>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		case AbstractColumn::ColumnMode::Day:
			filter = new Double2DayOfWeekFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<double>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		} // switch(mode)

		break;
	}
	case AbstractColumn::ColumnMode::Integer: {
		disconnect(static_cast<Integer2StringFilter*>(m_outputFilter), &Integer2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		switch (mode) {
		case AbstractColumn::ColumnMode::Integer:
			break;
		case AbstractColumn::ColumnMode::BigInt:
			filter = new Integer2BigIntFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<int>*>(old_data)));
				m_data = new QVector<qint64>();
			}
			break;
		case AbstractColumn::ColumnMode::Double:
			filter = new Integer2DoubleFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<int>*>(old_data)));
				m_data = new QVector<double>();
			}
			break;
		case AbstractColumn::ColumnMode::Text:
			filter = outputFilter();
			filter_is_temporary = false;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<int>*>(old_data)));
				m_data = new QVector<QString>();
			}
			break;
		case AbstractColumn::ColumnMode::DateTime:
			filter = new Integer2DateTimeFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<int>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		case AbstractColumn::ColumnMode::Month:
			filter = new Integer2MonthFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<int>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		case AbstractColumn::ColumnMode::Day:
			filter = new Integer2DayOfWeekFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<int>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		} // switch(mode)

		break;
	}
	case AbstractColumn::ColumnMode::BigInt: {
		disconnect(static_cast<BigInt2StringFilter*>(m_outputFilter), &BigInt2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		switch (mode) {
		case AbstractColumn::ColumnMode::BigInt:
			break;
		case AbstractColumn::ColumnMode::Integer:
			filter = new BigInt2IntegerFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<qint64>*>(old_data)));
				m_data = new QVector<int>();
			}
			break;
		case AbstractColumn::ColumnMode::Double:
			filter = new BigInt2DoubleFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<qint64>*>(old_data)));
				m_data = new QVector<double>();
			}
			break;
		case AbstractColumn::ColumnMode::Text:
			filter = outputFilter();
			filter_is_temporary = false;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<qint64>*>(old_data)));
				m_data = new QVector<QString>();
			}
			break;
		case AbstractColumn::ColumnMode::DateTime:
			filter = new BigInt2DateTimeFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<qint64>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		case AbstractColumn::ColumnMode::Month:
			filter = new BigInt2MonthFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<qint64>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		case AbstractColumn::ColumnMode::Day:
			filter = new BigInt2DayOfWeekFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<qint64>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		} // switch(mode)

		break;
	}
	case AbstractColumn::ColumnMode::Text: {
		switch (mode) {
		case AbstractColumn::ColumnMode::Text:
			break;
		case AbstractColumn::ColumnMode::Double:
			filter = new String2DoubleFilter();
			filter->setNumberLocale(numberLocale);
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<QString>*>(old_data)));
				m_data = new QVector<double>();
			}
			break;
		case AbstractColumn::ColumnMode::Integer:
			filter = new String2IntegerFilter();
			filter->setNumberLocale(numberLocale);
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<QString>*>(old_data)));
				m_data = new QVector<int>();
			}
			break;
		case AbstractColumn::ColumnMode::BigInt:
			filter = new String2BigIntFilter();
			filter->setNumberLocale(numberLocale);
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<QString>*>(old_data)));
				m_data = new QVector<qint64>();
			}
			break;
		case AbstractColumn::ColumnMode::DateTime:
			filter = new String2DateTimeFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<QString>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		case AbstractColumn::ColumnMode::Month:
			filter = new String2MonthFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<QString>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		case AbstractColumn::ColumnMode::Day:
			filter = new String2DayOfWeekFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<QString>*>(old_data)));
				m_data = new QVector<QDateTime>();
			}
			break;
		} // switch(mode)

		break;
	}
	case AbstractColumn::ColumnMode::DateTime:
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day: {
		disconnect(static_cast<DateTime2StringFilter*>(m_outputFilter), &DateTime2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		switch (mode) {
		case AbstractColumn::ColumnMode::DateTime:
		case AbstractColumn::ColumnMode::Month:
		case AbstractColumn::ColumnMode::Day:
			break;
		case AbstractColumn::ColumnMode::Text:
			filter = outputFilter();
			filter_is_temporary = false;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<QDateTime>*>(old_data)), m_columnMode);
				m_data = new QStringList();
			}
			break;
		case AbstractColumn::ColumnMode::Double:
			if (m_columnMode == AbstractColumn::ColumnMode::Month)
				filter = new Month2DoubleFilter();
			else if (m_columnMode == AbstractColumn::ColumnMode::Day)
				filter = new DayOfWeek2DoubleFilter();
			else
				filter = new DateTime2DoubleFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<QDateTime>*>(old_data)), m_columnMode);
				m_data = new QVector<double>();
			}
			break;
		case AbstractColumn::ColumnMode::Integer:
			if (m_columnMode == AbstractColumn::ColumnMode::Month)
				filter = new Month2IntegerFilter();
			else if (m_columnMode == AbstractColumn::ColumnMode::Day)
				filter = new DayOfWeek2IntegerFilter();
			else
				filter = new DateTime2IntegerFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<QDateTime>*>(old_data)), m_columnMode);
				m_data = new QVector<int>();
			}
			break;
		case AbstractColumn::ColumnMode::BigInt:
			if (m_columnMode == AbstractColumn::ColumnMode::Month)
				filter = new Month2BigIntFilter();
			else if (m_columnMode == AbstractColumn::ColumnMode::Day)
				filter = new DayOfWeek2BigIntFilter();
			else
				filter = new DateTime2BigIntFilter();
			filter_is_temporary = true;
			if (m_data) {
				temp_col = new Column(QStringLiteral("temp_col"), *(static_cast<QVector<QDateTime>*>(old_data)), m_columnMode);
				m_data = new QVector<qint64>();
			}
			break;
		} // switch(mode)

		break;
	}
	}

	// determine the new input and output filters
	switch (mode) { // new mode
	case AbstractColumn::ColumnMode::Double:
		new_in_filter = new String2DoubleFilter();
		new_in_filter->setNumberLocale(numberLocale);
		new_out_filter = new Double2StringFilter();
		new_out_filter->setNumberLocale(numberLocale);
		connect(static_cast<Double2StringFilter*>(new_out_filter), &Double2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	case AbstractColumn::ColumnMode::Integer:
		new_in_filter = new String2IntegerFilter();
		new_in_filter->setNumberLocale(numberLocale);
		new_out_filter = new Integer2StringFilter();
		new_out_filter->setNumberLocale(numberLocale);
		connect(static_cast<Integer2StringFilter*>(new_out_filter), &Integer2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	case AbstractColumn::ColumnMode::BigInt:
		new_in_filter = new String2BigIntFilter();
		new_in_filter->setNumberLocale(numberLocale);
		new_out_filter = new BigInt2StringFilter();
		new_out_filter->setNumberLocale(numberLocale);
		connect(static_cast<BigInt2StringFilter*>(new_out_filter), &BigInt2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	case AbstractColumn::ColumnMode::Text:
		new_in_filter = new SimpleCopyThroughFilter();
		new_out_filter = new SimpleCopyThroughFilter();
		break;
	case AbstractColumn::ColumnMode::DateTime:
		new_in_filter = new String2DateTimeFilter();
		new_out_filter = new DateTime2StringFilter();
		connect(static_cast<DateTime2StringFilter*>(new_out_filter), &DateTime2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	case AbstractColumn::ColumnMode::Month:
		new_in_filter = new String2MonthFilter();
		new_out_filter = new DateTime2StringFilter();
		static_cast<DateTime2StringFilter*>(new_out_filter)->setFormat(QStringLiteral("MMMM"));
		// DEBUG("	Month out_filter format: " << STDSTRING(static_cast<DateTime2StringFilter*>(new_out_filter)->format()));
		connect(static_cast<DateTime2StringFilter*>(new_out_filter), &DateTime2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	case AbstractColumn::ColumnMode::Day:
		new_in_filter = new String2DayOfWeekFilter();
		new_out_filter = new DateTime2StringFilter();
		static_cast<DateTime2StringFilter*>(new_out_filter)->setFormat(QStringLiteral("dddd"));
		connect(static_cast<DateTime2StringFilter*>(new_out_filter), &DateTime2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	} // switch(mode)

	m_columnMode = mode;

	m_inputFilter = new_in_filter;
	m_outputFilter = new_out_filter;
	m_inputFilter->input(0, m_owner->m_string_io);
	m_outputFilter->input(0, m_owner);
	m_inputFilter->setHidden(true);
	m_outputFilter->setHidden(true);

	if (temp_col) { // if temp_col == 0, only the input/output filters need to be changed
		// copy the filtered, i.e. converted, column (mode is orig mode)
		//		DEBUG("	temp_col column mode = " << ENUM_TO_STRING(AbstractColumn, ColumnMode, temp_col->columnMode()));
		filter->input(0, temp_col);
		//		DEBUG("	filter->output size = " << filter->output(0)->rowCount());
		copy(filter->output(0));
		delete temp_col;
	}

	if (filter_is_temporary)
		delete filter;

	Q_EMIT m_owner->modeChanged(m_owner);
}

/**
 * \brief Replace all mode related members
 *
 * Replace column mode, data type, data pointer and filters directly
 */
void ColumnPrivate::replaceModeData(AbstractColumn::ColumnMode mode, void* data, AbstractSimpleFilter* in_filter, AbstractSimpleFilter* out_filter) {
	Q_EMIT m_owner->modeAboutToChange(m_owner);
	// disconnect formatChanged()
	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double:
		disconnect(static_cast<Double2StringFilter*>(m_outputFilter), &Double2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	case AbstractColumn::ColumnMode::Integer:
		disconnect(static_cast<Integer2StringFilter*>(m_outputFilter), &Integer2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	case AbstractColumn::ColumnMode::BigInt:
		disconnect(static_cast<BigInt2StringFilter*>(m_outputFilter), &BigInt2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	case AbstractColumn::ColumnMode::Text:
		break;
	case AbstractColumn::ColumnMode::DateTime:
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
		disconnect(static_cast<DateTime2StringFilter*>(m_outputFilter), &DateTime2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	}

	m_columnMode = mode;
	m_data = data;

	m_inputFilter = in_filter;
	m_outputFilter = out_filter;
	m_inputFilter->input(0, m_owner->m_string_io);
	m_outputFilter->input(0, m_owner);

	// connect formatChanged()
	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double:
		connect(static_cast<Double2StringFilter*>(m_outputFilter), &Double2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	case AbstractColumn::ColumnMode::Integer:
		connect(static_cast<Integer2StringFilter*>(m_outputFilter), &Integer2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	case AbstractColumn::ColumnMode::BigInt:
		connect(static_cast<BigInt2StringFilter*>(m_outputFilter), &BigInt2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	case AbstractColumn::ColumnMode::Text:
		break;
	case AbstractColumn::ColumnMode::DateTime:
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
		connect(static_cast<DateTime2StringFilter*>(m_outputFilter), &DateTime2StringFilter::formatChanged, m_owner, &Column::handleFormatChange);
		break;
	}

	Q_EMIT m_owner->modeChanged(m_owner);
}

/**
 * \brief Replace data pointer
 */
void ColumnPrivate::replaceData(void* data) {
	Q_EMIT m_owner->dataAboutToChange(m_owner);

	m_data = data;
	invalidate();
	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
}

/**
 * \brief Copy another column of the same type
 *
 * This function will return false if the data type
 * of 'other' is not the same as the type of 'this'.
 * Use a filter to convert a column to another type.
 */
bool ColumnPrivate::copy(const AbstractColumn* other) {
	if (other->columnMode() != columnMode())
		return false;
	// 	DEBUG(Q_FUNC_INFO << ", mode = " << ENUM_TO_STRING(AbstractColumn, ColumnMode, columnMode()));
	int num_rows = other->rowCount();
	// 	DEBUG(Q_FUNC_INFO << ", rows " << num_rows);

	Q_EMIT m_owner->dataAboutToChange(m_owner);
	resizeTo(num_rows);

	if (!m_data)
		initDataContainer();

	// copy the data
	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double: {
		double* ptr = static_cast<QVector<double>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[i] = other->valueAt(i);
		break;
	}
	case AbstractColumn::ColumnMode::Integer: {
		int* ptr = static_cast<QVector<int>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[i] = other->integerAt(i);
		break;
	}
	case AbstractColumn::ColumnMode::BigInt: {
		qint64* ptr = static_cast<QVector<qint64>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[i] = other->bigIntAt(i);
		break;
	}
	case AbstractColumn::ColumnMode::Text: {
		auto* vec = static_cast<QVector<QString>*>(m_data);
		for (int i = 0; i < num_rows; ++i)
			vec->replace(i, other->textAt(i));
		break;
	}
	case AbstractColumn::ColumnMode::DateTime:
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day: {
		auto* vec = static_cast<QVector<QDateTime>*>(m_data);
		for (int i = 0; i < num_rows; ++i)
			vec->replace(i, other->dateTimeAt(i));
		break;
	}
	}

	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);

	return true;
}

/**
 * \brief Copies a part of another column of the same type
 *
 * This function will return false if the data type
 * of 'other' is not the same as the type of 'this'.
 * \param source pointer to the column to copy
 * \param source_start first row to copy in the column to copy
 * \param dest_start first row to copy in
 * \param num_rows the number of rows to copy
 */
bool ColumnPrivate::copy(const AbstractColumn* source, int source_start, int dest_start, int num_rows) {
	if (source->columnMode() != m_columnMode)
		return false;
	if (num_rows == 0)
		return true;

	Q_EMIT m_owner->dataAboutToChange(m_owner);
	if (dest_start + num_rows > rowCount())
		resizeTo(dest_start + num_rows);

	if (!m_data)
		initDataContainer();

	// copy the data
	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double: {
		double* ptr = static_cast<QVector<double>*>(m_data)->data();
		for (int i = 0; i < num_rows; i++)
			ptr[dest_start + i] = source->valueAt(source_start + i);
		break;
	}
	case AbstractColumn::ColumnMode::Integer: {
		int* ptr = static_cast<QVector<int>*>(m_data)->data();
		for (int i = 0; i < num_rows; i++)
			ptr[dest_start + i] = source->integerAt(source_start + i);
		break;
	}
	case AbstractColumn::ColumnMode::BigInt: {
		qint64* ptr = static_cast<QVector<qint64>*>(m_data)->data();
		for (int i = 0; i < num_rows; i++)
			ptr[dest_start + i] = source->bigIntAt(source_start + i);
		break;
	}
	case AbstractColumn::ColumnMode::Text:
		for (int i = 0; i < num_rows; i++)
			static_cast<QVector<QString>*>(m_data)->replace(dest_start + i, source->textAt(source_start + i));
		break;
	case AbstractColumn::ColumnMode::DateTime:
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
		for (int i = 0; i < num_rows; i++)
			static_cast<QVector<QDateTime>*>(m_data)->replace(dest_start + i, source->dateTimeAt(source_start + i));
		break;
	}

	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
	return true;
}

/**
 * \brief Copy another column of the same type
 *
 * This function will return false if the data type
 * of 'other' is not the same as the type of 'this'.
 * Use a filter to convert a column to another type.
 */
bool ColumnPrivate::copy(const ColumnPrivate* other) {
	if (other->columnMode() != m_columnMode)
		return false;
	int num_rows = other->rowCount();

	Q_EMIT m_owner->dataAboutToChange(m_owner);
	resizeTo(num_rows);

	if (!m_data)
		initDataContainer();

	// copy the data
	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double: {
		double* ptr = static_cast<QVector<double>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[i] = other->valueAt(i);
		break;
	}
	case AbstractColumn::ColumnMode::Integer: {
		int* ptr = static_cast<QVector<int>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[i] = other->integerAt(i);
		break;
	}
	case AbstractColumn::ColumnMode::BigInt: {
		qint64* ptr = static_cast<QVector<qint64>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[i] = other->bigIntAt(i);
		break;
	}
	case AbstractColumn::ColumnMode::Text:
		for (int i = 0; i < num_rows; ++i)
			static_cast<QVector<QString>*>(m_data)->replace(i, other->textAt(i));
		break;
	case AbstractColumn::ColumnMode::DateTime:
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
		for (int i = 0; i < num_rows; ++i)
			static_cast<QVector<QDateTime>*>(m_data)->replace(i, other->dateTimeAt(i));
		break;
	}

	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);

	return true;
}

/**
 * \brief Copies a part of another column of the same type
 *
 * This function will return false if the data type
 * of 'other' is not the same as the type of 'this'.
 * \param source pointer to the column to copy
 * \param source_start first row to copy in the column to copy
 * \param dest_start first row to copy in
 * \param num_rows the number of rows to copy
 */
bool ColumnPrivate::copy(const ColumnPrivate* source, int source_start, int dest_start, int num_rows) {
	if (source->columnMode() != m_columnMode)
		return false;
	if (num_rows == 0)
		return true;

	Q_EMIT m_owner->dataAboutToChange(m_owner);
	if (dest_start + num_rows > rowCount())
		resizeTo(dest_start + num_rows);

	if (!m_data)
		initDataContainer();

	// copy the data
	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double: {
		double* ptr = static_cast<QVector<double>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[dest_start + i] = source->valueAt(source_start + i);
		break;
	}
	case AbstractColumn::ColumnMode::Integer: {
		int* ptr = static_cast<QVector<int>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[dest_start + i] = source->integerAt(source_start + i);
		break;
	}
	case AbstractColumn::ColumnMode::BigInt: {
		qint64* ptr = static_cast<QVector<qint64>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[dest_start + i] = source->bigIntAt(source_start + i);
		break;
	}
	case AbstractColumn::ColumnMode::Text:
		for (int i = 0; i < num_rows; ++i)
			static_cast<QVector<QString>*>(m_data)->replace(dest_start + i, source->textAt(source_start + i));
		break;
	case AbstractColumn::ColumnMode::DateTime:
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
		for (int i = 0; i < num_rows; ++i)
			static_cast<QVector<QDateTime>*>(m_data)->replace(dest_start + i, source->dateTimeAt(source_start + i));
		break;
	}

	invalidate();

	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);

	return true;
}

/**
 * \brief Return the data vector size
 *
 * This returns the size of the column container
 */
int ColumnPrivate::rowCount() const {
	if (!m_data)
		return m_rowCount;

	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double:
		return static_cast<QVector<double>*>(m_data)->size();
	case AbstractColumn::ColumnMode::Integer:
		return static_cast<QVector<int>*>(m_data)->size();
	case AbstractColumn::ColumnMode::BigInt:
		return static_cast<QVector<qint64>*>(m_data)->size();
	case AbstractColumn::ColumnMode::DateTime:
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
		return static_cast<QVector<QDateTime>*>(m_data)->size();
	case AbstractColumn::ColumnMode::Text:
		return static_cast<QVector<QString>*>(m_data)->size();
	}

	return 0;
}

/**
 * \brief Return the number of available rows
 *
 * This returns the number of rows that actually contain data.
 * Rows beyond this can be masked etc. but should be ignored by filters,
 * plots etc.
 */
int ColumnPrivate::availableRowCount(int max) const {
	int count = 0;
	for (int row = 0; row < rowCount(); row++) {
		if (m_owner->isValid(row) && !m_owner->isMasked(row)) {
			count++;
			if (count == max)
				return max;
		}
	}

	return count;
}

/**
 * \brief Resize the vector to the specified number of rows
 *
 * Since selecting and masking rows higher than the
 * real internal number of rows is supported, this
 * does not change the interval attributes. Also
 * no signal is emitted. If the new rows are filled
 * with values AbstractColumn::dataChanged()
 * must be emitted.
 */
void ColumnPrivate::resizeTo(int new_size) {
	int old_size = rowCount();
	if (new_size == old_size)
		return;

	// 	DEBUG("ColumnPrivate::resizeTo() " << old_size << " -> " << new_size);
	const int new_rows = new_size - old_size;

	if (!m_data) {
		m_rowCount += new_rows;
		return;
	}

	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double: {
		auto* data = static_cast<QVector<double>*>(m_data);
		if (new_rows > 0)
			data->insert(data->end(), new_rows, NAN);
		else
			data->remove(old_size - 1 + new_rows, -new_rows);
		break;
	}
	case AbstractColumn::ColumnMode::Integer: {
		auto* data = static_cast<QVector<int>*>(m_data);
		if (new_rows > 0)
			data->insert(data->end(), new_rows, 0);
		else
			data->remove(old_size - 1 + new_rows, -new_rows);
		break;
	}
	case AbstractColumn::ColumnMode::BigInt: {
		auto* data = static_cast<QVector<qint64>*>(m_data);
		if (new_rows > 0)
			data->insert(data->end(), new_rows, 0);
		else
			data->remove(old_size - 1 + new_rows, -new_rows);
		break;
	}
	case AbstractColumn::ColumnMode::Text: {
		auto* data = static_cast<QVector<QString>*>(m_data);
		if (new_rows > 0)
			data->insert(data->end(), new_rows, QString());
		else
			data->remove(old_size - 1 + new_rows, -new_rows);
		break;
	}
	case AbstractColumn::ColumnMode::DateTime:
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day: {
		auto* data = static_cast<QVector<QDateTime>*>(m_data);
		if (new_rows > 0)
			data->insert(data->end(), new_rows, QDateTime());
		else
			data->remove(old_size - 1 + new_rows, -new_rows);
		break;
	}
	}
}

/**
 * \brief Insert some empty (or initialized with zero) rows
 */
void ColumnPrivate::insertRows(int before, int count) {
	if (count == 0)
		return;

	m_formulas.insertRows(before, count);

	if (!m_data) {
		m_rowCount += count;
		return;
	}

	if (before <= rowCount()) {
		switch (m_columnMode) {
		case AbstractColumn::ColumnMode::Double:
			static_cast<QVector<double>*>(m_data)->insert(before, count, NAN);
			break;
		case AbstractColumn::ColumnMode::Integer:
			static_cast<QVector<int>*>(m_data)->insert(before, count, 0);
			break;
		case AbstractColumn::ColumnMode::BigInt:
			static_cast<QVector<qint64>*>(m_data)->insert(before, count, 0);
			break;
		case AbstractColumn::ColumnMode::DateTime:
		case AbstractColumn::ColumnMode::Month:
		case AbstractColumn::ColumnMode::Day:
			for (int i = 0; i < count; ++i)
				static_cast<QVector<QDateTime>*>(m_data)->insert(before, QDateTime());
			break;
		case AbstractColumn::ColumnMode::Text:
			for (int i = 0; i < count; ++i)
				static_cast<QVector<QString>*>(m_data)->insert(before, QString());
			break;
		}
	}
}

/**
 * \brief Remove 'count' rows starting from row 'first'
 */
void ColumnPrivate::removeRows(int first, int count) {
	if (count == 0)
		return;

	m_formulas.removeRows(first, count);

	if (first < rowCount()) {
		int corrected_count = count;
		if (first + count > rowCount())
			corrected_count = rowCount() - first;

		if (!m_data) {
			m_rowCount -= corrected_count;
			return;
		}

		switch (m_columnMode) {
		case AbstractColumn::ColumnMode::Double:
			static_cast<QVector<double>*>(m_data)->remove(first, corrected_count);
			break;
		case AbstractColumn::ColumnMode::Integer:
			static_cast<QVector<int>*>(m_data)->remove(first, corrected_count);
			break;
		case AbstractColumn::ColumnMode::BigInt:
			static_cast<QVector<qint64>*>(m_data)->remove(first, corrected_count);
			break;
		case AbstractColumn::ColumnMode::DateTime:
		case AbstractColumn::ColumnMode::Month:
		case AbstractColumn::ColumnMode::Day:
			for (int i = 0; i < corrected_count; ++i)
				static_cast<QVector<QDateTime>*>(m_data)->removeAt(first);
			break;
		case AbstractColumn::ColumnMode::Text:
			for (int i = 0; i < corrected_count; ++i)
				static_cast<QVector<QString>*>(m_data)->removeAt(first);
			break;
		}
	}
}

//! Return the column name
QString ColumnPrivate::name() const {
	return m_owner->name();
}

/**
 * \brief Return the column plot designation
 */
AbstractColumn::PlotDesignation ColumnPrivate::plotDesignation() const {
	return m_plotDesignation;
}

/**
 * \brief Set the column plot designation
 */
void ColumnPrivate::setPlotDesignation(AbstractColumn::PlotDesignation pd) {
	Q_EMIT m_owner->plotDesignationAboutToChange(m_owner);
	m_plotDesignation = pd;
	Q_EMIT m_owner->plotDesignationChanged(m_owner);
}

/**
 * \brief Get width
 */
int ColumnPrivate::width() const {
	return m_width;
}

/**
 * \brief Set width
 */
void ColumnPrivate::setWidth(int value) {
	m_width = value;
}

/**
 * \brief Return the data pointer
 */
void* ColumnPrivate::data() const {
	if (!m_data)
		const_cast<ColumnPrivate*>(this)->initDataContainer();

	return m_data;
}

/**
 * \brief Return the input filter (for string -> data type conversion)
 */
AbstractSimpleFilter* ColumnPrivate::inputFilter() const {
	return m_inputFilter;
}

/**
 * \brief Return the output filter (for data type -> string  conversion)
 */
AbstractSimpleFilter* ColumnPrivate::outputFilter() const {
	return m_outputFilter;
}

//! \name Labels related functions
//@{
bool ColumnPrivate::hasValueLabels() const {
	return (m_labels != nullptr);
}

void ColumnPrivate::removeValueLabel(const QString& key) {
	if (!hasValueLabels())
		return;

	SET_NUMBER_LOCALE
	bool ok;
	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double: {
		double value = numberLocale.toDouble(key, &ok);
		if (!ok)
			return;
		static_cast<QMap<double, QString>*>(m_labels)->remove(value);
		break;
	}
	case AbstractColumn::ColumnMode::Integer: {
		int value = numberLocale.toInt(key, &ok);
		if (!ok)
			return;
		static_cast<QMap<int, QString>*>(m_labels)->remove(value);
		break;
	}
	case AbstractColumn::ColumnMode::BigInt: {
		qint64 value = numberLocale.toLongLong(key, &ok);
		if (!ok)
			return;
		static_cast<QMap<qint64, QString>*>(m_labels)->remove(value);
		break;
	}
	case AbstractColumn::ColumnMode::Text: {
		static_cast<QMap<QString, QString>*>(m_labels)->remove(key);
		break;
	}
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
	case AbstractColumn::ColumnMode::DateTime: {
		auto* filter = static_cast<DateTime2StringFilter*>(m_outputFilter);
		static_cast<QMap<QDateTime, QString>*>(m_labels)->remove(QDateTime::fromString(key, filter->format()));
		break;
	}
	}
}

void ColumnPrivate::clearValueLabels() {
	if (!hasValueLabels())
		return;

	switch (m_columnMode) {
	case AbstractColumn::ColumnMode::Double: {
		static_cast<QMap<double, QString>*>(m_labels)->clear();
		break;
	}
	case AbstractColumn::ColumnMode::Integer: {
		static_cast<QMap<int, QString>*>(m_labels)->clear();
		break;
	}
	case AbstractColumn::ColumnMode::BigInt: {
		static_cast<QMap<qint64, QString>*>(m_labels)->clear();
		break;
	}
	case AbstractColumn::ColumnMode::Text: {
		static_cast<QMap<QString, QString>*>(m_labels)->clear();
		break;
	}
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
	case AbstractColumn::ColumnMode::DateTime: {
		static_cast<QMap<QDateTime, QString>*>(m_labels)->clear();
		break;
	}
	}
}

const QMap<QString, QString>& ColumnPrivate::textValueLabels() {
	initLabels();
	return *(static_cast<QMap<QString, QString>*>(m_labels));
}

const QMap<QDateTime, QString>& ColumnPrivate::dateTimeValueLabels() {
	initLabels();
	return *(static_cast<QMap<QDateTime, QString>*>(m_labels));
}

const QMap<double, QString>& ColumnPrivate::valueLabels() {
	initLabels();
	return *(static_cast<QMap<double, QString>*>(m_labels));
}

const QMap<int, QString>& ColumnPrivate::intValueLabels() {
	initLabels();
	return *(static_cast<QMap<int, QString>*>(m_labels));
}

const QMap<qint64, QString>& ColumnPrivate::bigIntValueLabels() {
	initLabels();
	return *(static_cast<QMap<qint64, QString>*>(m_labels));
}
//@}

//! \name Formula related functions
//@{
/**
 * \brief Return the formula last used to generate data for the column
 */
QString ColumnPrivate::formula() const {
	return m_formula;
}

const QVector<Column::FormulaData>& ColumnPrivate::formulaData() const {
	return m_formulaData;
}

bool ColumnPrivate::formulaAutoUpdate() const {
	return m_formulaAutoUpdate;
}

/**
 * \brief Sets the formula used to generate column values
 */
void ColumnPrivate::setFormula(const QString& formula, const QVector<Column::FormulaData>& formulaData, bool autoUpdate) {
	m_formula = formula;
	m_formulaData = formulaData; // TODO: disconnecting everything?
	m_formulaAutoUpdate = autoUpdate;

	for (auto connection : m_connectionsUpdateFormula)
		if (static_cast<bool>(connection))
			disconnect(connection);

	for (const auto& formulaData : m_formulaData) {
		const auto* column = formulaData.column();
		assert(column);
		if (autoUpdate)
			connectFormulaColumn(column);
	}

	Q_EMIT m_owner->formulaChanged(m_owner);
}

/*!
 * called after the import of the project was done and all columns were loaded in \sa Project::load()
 * to establish the required slot-signal connections for the formula update
 */
void ColumnPrivate::finalizeLoad() {
	if (m_formulaAutoUpdate) {
		for (const auto& formulaData : m_formulaData) {
			const auto* column = formulaData.column();
			if (column)
				connectFormulaColumn(column);
		}
	}
}

/*!
 * \brief ColumnPrivate::connectFormulaColumn
 * This function is used to connect the columns to the needed slots for updating formulas
 * \param column
 */
void ColumnPrivate::connectFormulaColumn(const AbstractColumn* column) {
	if (!column)
		return;

	// avoid circular dependencies - the current column cannot be part of the variable columns.
	// this should't actually happen because of the checks done when the formula is defined,
	// but in case we have bugs somewhere or somebody manipulated the project xml file we add
	// a sanity check to avoid recursive calls here and crash because of the stack overflow.
	if (column == m_owner)
		return;

	DEBUG(Q_FUNC_INFO)
	m_connectionsUpdateFormula << connect(column, &AbstractColumn::dataChanged, m_owner, &Column::updateFormula);
	connect(column->parentAspect(), &AbstractAspect::aspectAboutToBeRemoved, this, &ColumnPrivate::formulaVariableColumnRemoved);
	connect(column, &AbstractColumn::reset, this, &ColumnPrivate::formulaVariableColumnRemoved);
	connect(column->parentAspect(), &AbstractAspect::aspectAdded, this, &ColumnPrivate::formulaVariableColumnAdded);
}

/*!
 * helper function used in \c Column::load() to set parameters read from the xml file.
 * \param variableColumnPaths is used to restore the pointers to columns from pathes
 * after the project was loaded in Project::load().
 */
void ColumnPrivate::setFormula(const QString& formula, const QStringList& variableNames, const QStringList& variableColumnPaths, bool autoUpdate) {
	m_formula = formula;
	m_formulaData.clear();
	for (int i = 0; i < variableNames.count(); i++)
		m_formulaData.append(Column::FormulaData(variableNames.at(i), variableColumnPaths.at(i)));

	m_formulaAutoUpdate = autoUpdate;
}

void ColumnPrivate::setFormulVariableColumnsPath(int index, const QString& path) {
	if (!m_formulaData[index].setColumnPath(path))
		DEBUG(Q_FUNC_INFO << ": For some reason, there was already a column assigned");
}

void ColumnPrivate::setFormulVariableColumn(int index, Column* column) {
	if (m_formulaData.at(index).column()) // if there exists already a valid column, disconnect it first
		disconnect(m_formulaData.at(index).column(), nullptr, this, nullptr);
	m_formulaData[index].setColumn(column);
	connectFormulaColumn(column);
}

void ColumnPrivate::setFormulVariableColumn(Column* c) {
	for (auto& d : m_formulaData) {
		if (d.columnName() == c->path()) {
			d.setColumn(c);
			break;
		}
	}
}

/*!
 * \sa FunctionValuesDialog::generate()
 */
void ColumnPrivate::updateFormula() {
	DEBUG(Q_FUNC_INFO)
	// determine variable names and the data vectors of the specified columns
	QVector<QVector<double>*> xVectors;
	QString formula = m_formula;

	bool valid = true;
	QStringList formulaVariableNames;
	int maxRowCount = 0;
	for (const auto& formulaData : m_formulaData) {
		auto* column = formulaData.column();
		if (!column) {
			valid = false;
			break;
		}
		auto varName = formulaData.variableName();
		formulaVariableNames << varName;

		// care about special expressions
		// A) replace statistical values
		// list of available statistical methods (see AbstractColumn.h)
		QVector<QPair<QString, double>> methodList = {{QStringLiteral("size"), static_cast<double>(column->statistics().size)},
													  {QStringLiteral("min"), column->minimum()},
													  {QStringLiteral("max"), column->maximum()},
													  {QStringLiteral("mean"), column->statistics().arithmeticMean},
													  {QStringLiteral("median"), column->statistics().median},
													  {QStringLiteral("stdev"), column->statistics().standardDeviation},
													  {QStringLiteral("var"), column->statistics().variance},
													  {QStringLiteral("gm"), column->statistics().geometricMean},
													  {QStringLiteral("hm"), column->statistics().harmonicMean},
													  {QStringLiteral("chm"), column->statistics().contraharmonicMean},
													  {QStringLiteral("mode"), column->statistics().mode},
													  {QStringLiteral("quartile1"), column->statistics().firstQuartile},
													  {QStringLiteral("quartile3"), column->statistics().thirdQuartile},
													  {QStringLiteral("iqr"), column->statistics().iqr},
													  {QStringLiteral("percentile1"), column->statistics().percentile_1},
													  {QStringLiteral("percentile5"), column->statistics().percentile_5},
													  {QStringLiteral("percentile10"), column->statistics().percentile_10},
													  {QStringLiteral("percentile90"), column->statistics().percentile_90},
													  {QStringLiteral("percentile95"), column->statistics().percentile_95},
													  {QStringLiteral("percentile99"), column->statistics().percentile_99},
													  {QStringLiteral("trimean"), column->statistics().trimean},
													  {QStringLiteral("meandev"), column->statistics().meanDeviation},
													  {QStringLiteral("meandevmedian"), column->statistics().meanDeviationAroundMedian},
													  {QStringLiteral("mediandev"), column->statistics().medianDeviation},
													  {QStringLiteral("skew"), column->statistics().skewness},
													  {QStringLiteral("kurt"), column->statistics().kurtosis},
													  {QStringLiteral("entropy"), column->statistics().entropy}};

		SET_NUMBER_LOCALE
		for (auto m : methodList)
			formula.replace(m.first + QStringLiteral("(%1)").arg(varName), numberLocale.toString(m.second));

		// B) methods with options like method(p, x): get option p and calculate value to replace method
		QStringList optionMethodList = {QLatin1String("quantile\\((\\d+[\\.\\,]?\\d+).*%1\\)"), // quantile(p, x)
										QLatin1String("percentile\\((\\d+[\\.\\,]?\\d+).*%1\\)")}; // percentile(p, x)

		for (auto m : optionMethodList) {
			QRegExp rx(m.arg(varName));
			rx.setMinimal(true); // only match one method call at a time

			int pos = 0;
			while ((pos = rx.indexIn(formula, pos)) != -1) { // all method calls
				QDEBUG("method call:" << rx.cap(0))
				double p = numberLocale.toDouble(rx.cap(1)); // option
				DEBUG("p = " << p)

				// scale (quantile: p=0..1, percentile: p=0..100)
				if (m.startsWith(QLatin1String("percentile")))
					p /= 100.;

				double value = 0.0;
				switch (column->columnMode()) { // all types
				case AbstractColumn::ColumnMode::Double: {
					auto data = reinterpret_cast<QVector<double>*>(column->data());
					value = nsl_stats_quantile(data->data(), 1, column->statistics().size, p, nsl_stats_quantile_type7);
					break;
				}
				case AbstractColumn::ColumnMode::Integer: {
					auto* intData = reinterpret_cast<QVector<int>*>(column->data());

					QVector<double> data = QVector<double>(); // copy data to double
					data.reserve(column->rowCount());
					for (auto v : *intData)
						data << static_cast<double>(v);
					value = nsl_stats_quantile(data.data(), 1, column->statistics().size, p, nsl_stats_quantile_type7);
					break;
				}
				case AbstractColumn::ColumnMode::BigInt: {
					auto* bigIntData = reinterpret_cast<QVector<qint64>*>(column->data());

					QVector<double> data = QVector<double>(); // copy data to double
					data.reserve(column->rowCount());
					for (auto v : *bigIntData)
						data << static_cast<double>(v);
					value = nsl_stats_quantile(data.data(), 1, column->statistics().size, p, nsl_stats_quantile_type7);
					break;
				}
				case AbstractColumn::ColumnMode::DateTime: // not supported yet
				case AbstractColumn::ColumnMode::Day:
				case AbstractColumn::ColumnMode::Month:
				case AbstractColumn::ColumnMode::Text:
					break;
				}

				formula.replace(rx.cap(0), numberLocale.toString(value));
			}
		}

		// C) simple replacements
		QVector<QPair<QString, QString>> replaceList = {{QStringLiteral("mr"), QStringLiteral("fabs(cell(i, %1) - cell(i-1, %1))")},
														{QStringLiteral("ma"), QStringLiteral("(cell(i-1, %1) + cell(i, %1))/2.")}};
		for (auto m : replaceList)
			formula.replace(m.first + QLatin1String("(%1)").arg(varName), m.second.arg(varName));

		// D) advanced replacements
		QVector<QPair<QString, QString>> advancedReplaceList = {{QStringLiteral("smr\\((.*),.*%1\\)"), QStringLiteral("smmax(%1, %2) - smmin(%1, %2)")}};
		for (auto m : advancedReplaceList) {
			QRegExp rx(m.first.arg(varName));
			rx.setMinimal(true); // only match one method call at a time

			int pos = 0;
			while ((pos = rx.indexIn(formula, pos)) != -1) { // all method calls
				QDEBUG("method call:" << rx.cap(0))
				const int N = numberLocale.toInt(rx.cap(1));
				DEBUG("N = " << N)

				formula.replace(rx.cap(0), m.second.arg(numberLocale.toString(N)).arg(varName));
			}
		}

		QDEBUG("FORMULA: " << formula);

		if (column->columnMode() == AbstractColumn::ColumnMode::Integer || column->columnMode() == AbstractColumn::ColumnMode::BigInt) {
			// convert integers to doubles first
			auto* xVector = new QVector<double>(column->rowCount());
			for (int i = 0; i < column->rowCount(); ++i)
				(*xVector)[i] = column->valueAt(i);

			xVectors << xVector;
		} else
			xVectors << static_cast<QVector<double>*>(column->data());

		if (column->rowCount() > maxRowCount)
			maxRowCount = column->rowCount();
	}

	if (valid) {
		// resize the spreadsheet if one of the data vectors from
		// other spreadsheet(s) has more elements than the parent spreadsheet
		// TODO: maybe it's better to not extend the spreadsheet (see #31)

		auto* spreadsheet = static_cast<Spreadsheet*>(m_owner->parentAspect());
		if (spreadsheet->rowCount() < maxRowCount)
			spreadsheet->setRowCount(maxRowCount);

		// create new vector for storing the calculated values
		// the vectors with the variable data can be smaller then the result vector. So, not all values in the result vector might get initialized.
		//->"clean" the result vector first
		QVector<double> new_data(rowCount(), NAN);

		// evaluate the expression for f(x_1, x_2, ...) and write the calculated values into a new vector.
		auto* parser = ExpressionParser::getInstance();
		QDEBUG(Q_FUNC_INFO << ", Calling evaluateCartesian(). formula: " << m_formula << ", var names: " << formulaVariableNames)
		parser->evaluateCartesian(formula, formulaVariableNames, xVectors, &new_data);
		DEBUG(Q_FUNC_INFO << ", Calling replaceValues()")
		replaceValues(0, new_data);

		// initialize remaining rows with NAN
		int remainingRows = rowCount() - maxRowCount;
		if (remainingRows > 0) {
			QVector<double> emptyRows(remainingRows, NAN);
			replaceValues(maxRowCount, emptyRows);
		}
	} else { // not valid
		QVector<double> new_data(rowCount(), NAN);
		replaceValues(0, new_data);
	}

	DEBUG(Q_FUNC_INFO << " DONE")
}

void ColumnPrivate::formulaVariableColumnRemoved(const AbstractAspect* aspect) {
	const Column* column = dynamic_cast<const Column*>(aspect);
	disconnect(column, nullptr, this, nullptr);
	int index = -1;
	for (int i = 0; i < formulaData().count(); i++) {
		auto& d = formulaData().at(i);
		if (d.column() == column) {
			index = i;
			break;
		}
	}
	if (index != -1) {
		m_formulaData[index].setColumn(nullptr);
		DEBUG(Q_FUNC_INFO << ", calling updateFormula()")
		updateFormula();
	}
}

void ColumnPrivate::formulaVariableColumnAdded(const AbstractAspect* aspect) {
	PERFTRACE(QLatin1String(Q_FUNC_INFO));
	const auto& path = aspect->path();
	int index = -1;
	for (int i = 0; i < formulaData().count(); i++) {
		if (formulaData().at(i).columnName() == path) {
			index = i;
			break;
		}
	}
	if (index != -1) {
		const Column* column = dynamic_cast<const Column*>(aspect);
		m_formulaData[index].setColumn(const_cast<Column*>(column));
		DEBUG(Q_FUNC_INFO << ", calling updateFormula()")
		updateFormula();
	}
}

/**
 * \brief Return the formula associated with row 'row'
 */
QString ColumnPrivate::formula(int row) const {
	return m_formulas.value(row);
}

/**
 * \brief Return the intervals that have associated formulas
 *
 * This can be used to make a list of formulas with their intervals.
 * Here is some example code:
 *
 * \code
 * QStringList list;
 * QVector< Interval<int> > intervals = my_column.formulaIntervals();
 * foreach(Interval<int> interval, intervals)
 * 	list << QString(interval.toString() + ": " + my_column.formula(interval.start()));
 * \endcode
 */
QVector<Interval<int>> ColumnPrivate::formulaIntervals() const {
	return m_formulas.intervals();
}

/**
 * \brief Set a formula string for an interval of rows
 */
void ColumnPrivate::setFormula(const Interval<int>& i, const QString& formula) {
	m_formulas.setValue(i, formula);
}

/**
 * \brief Overloaded function for convenience
 */
void ColumnPrivate::setFormula(int row, const QString& formula) {
	setFormula(Interval<int>(row, row), formula);
}

/**
 * \brief Clear all formulas
 */
void ColumnPrivate::clearFormulas() {
	m_formulas.clear();
}
//@}

////////////////////////////////////////////////////////////////////////////////
//! \name type specific functions
//@{
////////////////////////////////////////////////////////////////////////////////
void ColumnPrivate::setValueAt(int row, int new_value) {
	if (!m_data)
		initDataContainer();
	setIntegerAt(row, new_value);
}

void ColumnPrivate::setValueAt(int row, qint64 new_value) {
	if (!m_data)
		initDataContainer();
	setBigIntAt(row, new_value);
}

void ColumnPrivate::setValueAt(int row, QDateTime new_value) {
	if (!m_data)
		initDataContainer();
	setDateTimeAt(row, new_value);
}

void ColumnPrivate::setValueAt(int row, QString new_value) {
	if (!m_data)
		initDataContainer();
	setTextAt(row, new_value);
}

void ColumnPrivate::replaceValues(int first, const QVector<int>& new_values) {
	if (!m_data)
		initDataContainer();
	replaceInteger(first, new_values);
}

void ColumnPrivate::replaceValues(int first, const QVector<qint64>& new_values) {
	if (!m_data)
		initDataContainer();
	replaceBigInt(first, new_values);
}

void ColumnPrivate::replaceValues(int first, const QVector<QDateTime>& new_values) {
	if (!m_data)
		initDataContainer();
	replaceDateTimes(first, new_values);
}

void ColumnPrivate::replaceValues(int first, const QVector<QString>& new_values) {
	if (!m_data)
		initDataContainer();
	replaceTexts(first, new_values);
}

/**
 * \brief Return the content of row 'row'.
 *
 * Use this only when columnMode() is Text
 */
QString ColumnPrivate::textAt(int row) const {
	if (!m_data || m_columnMode != AbstractColumn::ColumnMode::Text)
		return {};
	return static_cast<QVector<QString>*>(m_data)->value(row);
}

/**
 * \brief Return the date part of row 'row'
 *
 * Use this only when columnMode() is DateTime, Month or Day
 */
QDate ColumnPrivate::dateAt(int row) const {
	if (!m_data
		|| (m_columnMode != AbstractColumn::ColumnMode::DateTime && m_columnMode != AbstractColumn::ColumnMode::Month
			&& m_columnMode != AbstractColumn::ColumnMode::Day))
		return QDate{};
	return dateTimeAt(row).date();
}

/**
 * \brief Return the time part of row 'row'
 *
 * Use this only when columnMode() is DateTime, Month or Day
 */
QTime ColumnPrivate::timeAt(int row) const {
	if (!m_data
		|| (m_columnMode != AbstractColumn::ColumnMode::DateTime && m_columnMode != AbstractColumn::ColumnMode::Month
			&& m_columnMode != AbstractColumn::ColumnMode::Day))
		return QTime{};
	return dateTimeAt(row).time();
}

/**
 * \brief Return the QDateTime in row 'row'
 *
 * Use this only when columnMode() is DateTime, Month or Day
 */
QDateTime ColumnPrivate::dateTimeAt(int row) const {
	if (!m_data
		|| (m_columnMode != AbstractColumn::ColumnMode::DateTime && m_columnMode != AbstractColumn::ColumnMode::Month
			&& m_columnMode != AbstractColumn::ColumnMode::Day))
		return QDateTime();
	return static_cast<QVector<QDateTime>*>(m_data)->value(row);
}

/**
 * \brief Return the double value at index 'index' for columns with type Numeric, Integer or BigInt.
 * This function has to be used everywhere where the exact type (double, int or qint64) is not relevant for numerical calculations.
 * For cases where the integer value is needed without any implicit conversions, \sa integerAt() has to be used.
 */
double ColumnPrivate::valueAt(int index) const {
	if (!m_data)
		return NAN;

	if (m_columnMode == AbstractColumn::ColumnMode::Double)
		return static_cast<QVector<double>*>(m_data)->value(index, NAN);
	else if (m_columnMode == AbstractColumn::ColumnMode::Integer)
		return static_cast<QVector<int>*>(m_data)->value(index, 0);
	else if (m_columnMode == AbstractColumn::ColumnMode::BigInt)
		return static_cast<QVector<qint64>*>(m_data)->value(index, 0);
	else
		return NAN;
}

/**
 * \brief Return the int value in row 'row'
 */
int ColumnPrivate::integerAt(int row) const {
	if (!m_data || m_columnMode != AbstractColumn::ColumnMode::Integer)
		return 0;
	return static_cast<QVector<int>*>(m_data)->value(row, 0);
}

/**
 * \brief Return the bigint value in row 'row'
 */
qint64 ColumnPrivate::bigIntAt(int row) const {
	if (!m_data || m_columnMode != AbstractColumn::ColumnMode::BigInt)
		return 0;
	return static_cast<QVector<qint64>*>(m_data)->value(row, 0);
}

void ColumnPrivate::invalidate() {
	available.setUnavailable();
}

/**
 * \brief Set the content of row 'row'
 *
 * Use this only when columnMode() is Text
 */
void ColumnPrivate::setTextAt(int row, const QString& new_value) {
	if (m_columnMode != AbstractColumn::ColumnMode::Text)
		return;

	if (!m_data)
		initDataContainer();

	invalidate();

	Q_EMIT m_owner->dataAboutToChange(m_owner);
	if (row >= rowCount())
		resizeTo(row + 1);

	static_cast<QVector<QString>*>(m_data)->replace(row, new_value);
	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
}

/**
 * \brief Replace a range of values
 *
 * Use this only when columnMode() is Text
 */
void ColumnPrivate::replaceTexts(int first, const QVector<QString>& new_values) {
	if (m_columnMode != AbstractColumn::ColumnMode::Text)
		return;

	if (!m_data)
		initDataContainer();

	invalidate();

	Q_EMIT m_owner->dataAboutToChange(m_owner);

	if (first < 0)
		*static_cast<QVector<QString>*>(m_data) = new_values;
	else {
		const int num_rows = new_values.size();
		resizeTo(first + num_rows);

		for (int i = 0; i < num_rows; ++i)
			static_cast<QVector<QString>*>(m_data)->replace(first + i, new_values.at(i));
	}

	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
}

int ColumnPrivate::dictionaryIndex(int row) const {
	if (!available.dictionary)
		const_cast<ColumnPrivate*>(this)->initDictionary();

	const auto& value = textAt(row);
	int index = 0;
	auto it = m_dictionary.constBegin();
	while (it != m_dictionary.constEnd()) {
		if (*it == value)
			break;
		++index;
		++it;
	}

	return index;
}

const QMap<QString, int>& ColumnPrivate::frequencies() const {
	if (!available.dictionary)
		const_cast<ColumnPrivate*>(this)->initDictionary();

	return m_dictionaryFrequencies;
}

void ColumnPrivate::initDictionary() {
	m_dictionary.clear();
	m_dictionaryFrequencies.clear();
	if (!m_data || columnMode() != AbstractColumn::ColumnMode::Text)
		return;

	auto data = static_cast<QVector<QString>*>(m_data);
	for (auto& value : *data) {
		if (value.isEmpty())
			continue;

		if (!m_dictionary.contains(value))
			m_dictionary << value;

		if (m_dictionaryFrequencies.constFind(value) == m_dictionaryFrequencies.constEnd())
			m_dictionaryFrequencies[value] = 1;
		else
			m_dictionaryFrequencies[value]++;
	}

	available.dictionary = true;
}

/**
 * \brief Set the content of row 'row'
 *
 * Use this only when columnMode() is DateTime, Month or Day
 */
void ColumnPrivate::setDateAt(int row, QDate new_value) {
	if (m_columnMode != AbstractColumn::ColumnMode::DateTime && m_columnMode != AbstractColumn::ColumnMode::Month
		&& m_columnMode != AbstractColumn::ColumnMode::Day)
		return;

	if (!m_data)
		initDataContainer();

	setDateTimeAt(row, QDateTime(new_value, timeAt(row)));
}

/**
 * \brief Set the content of row 'row'
 *
 * Use this only when columnMode() is DateTime, Month or Day
 */
void ColumnPrivate::setTimeAt(int row, QTime new_value) {
	if (m_columnMode != AbstractColumn::ColumnMode::DateTime && m_columnMode != AbstractColumn::ColumnMode::Month
		&& m_columnMode != AbstractColumn::ColumnMode::Day)
		return;

	if (!m_data)
		initDataContainer();

	setDateTimeAt(row, QDateTime(dateAt(row), new_value));
}

/**
 * \brief Set the content of row 'row'
 *
 * Use this only when columnMode() is DateTime, Month or Day
 */
void ColumnPrivate::setDateTimeAt(int row, const QDateTime& new_value) {
	if (m_columnMode != AbstractColumn::ColumnMode::DateTime && m_columnMode != AbstractColumn::ColumnMode::Month
		&& m_columnMode != AbstractColumn::ColumnMode::Day)
		return;

	if (!m_data)
		initDataContainer();

	invalidate();

	Q_EMIT m_owner->dataAboutToChange(m_owner);
	if (row >= rowCount())
		resizeTo(row + 1);

	static_cast<QVector<QDateTime>*>(m_data)->replace(row, new_value);
	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
}

/**
 * \brief Replace a range of values
 * \param first first index which should be replaced. If first < 0, the complete vector
 * will be replaced
 * \param new_values
 * Use this only when columnMode() is DateTime, Month or Day
 */
void ColumnPrivate::replaceDateTimes(int first, const QVector<QDateTime>& new_values) {
	if (m_columnMode != AbstractColumn::ColumnMode::DateTime && m_columnMode != AbstractColumn::ColumnMode::Month
		&& m_columnMode != AbstractColumn::ColumnMode::Day)
		return;

	if (!m_data)
		initDataContainer();

	invalidate();

	Q_EMIT m_owner->dataAboutToChange(m_owner);

	if (first < 0)
		*static_cast<QVector<QDateTime>*>(m_data) = new_values;
	else {
		const int num_rows = new_values.size();
		resizeTo(first + num_rows);

		for (int i = 0; i < num_rows; ++i)
			static_cast<QVector<QDateTime>*>(m_data)->replace(first + i, new_values.at(i));
	}

	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
}

/**
 * \brief Set the content of row 'row'
 *
 * Use this only when columnMode() is Numeric
 */
void ColumnPrivate::setValueAt(int row, double new_value) {
	// DEBUG(Q_FUNC_INFO);
	if (m_columnMode != AbstractColumn::ColumnMode::Double)
		return;

	if (!m_data)
		initDataContainer();

	invalidate();

	Q_EMIT m_owner->dataAboutToChange(m_owner);
	if (row >= rowCount())
		resizeTo(row + 1);

	static_cast<QVector<double>*>(m_data)->replace(row, new_value);
	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
}

/**
 * \brief Replace a range of values
 *
 * Use this only when columnMode() is Numeric
 */
void ColumnPrivate::replaceValues(int first, const QVector<double>& new_values) {
	// DEBUG(Q_FUNC_INFO);
	if (m_columnMode != AbstractColumn::ColumnMode::Double)
		return;

	if (!m_data)
		initDataContainer();

	invalidate();

	Q_EMIT m_owner->dataAboutToChange(m_owner);
	if (first < 0)
		*static_cast<QVector<double>*>(m_data) = new_values;
	else {
		const int num_rows = new_values.size();
		resizeTo(first + num_rows);

		double* ptr = static_cast<QVector<double>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[first + i] = new_values.at(i);
	}

	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
}

void ColumnPrivate::initLabels() {
	if (!m_labels) {
		switch (m_columnMode) {
		case AbstractColumn::ColumnMode::Double:
			m_labels = new QMap<double, QString>();
			break;
		case AbstractColumn::ColumnMode::Integer:
			m_labels = new QMap<int, QString>();
			break;
		case AbstractColumn::ColumnMode::BigInt:
			m_labels = new QMap<qint64, QString>();
			break;
		case AbstractColumn::ColumnMode::Text:
			m_labels = new QMap<QString, QString>();
			break;
		case AbstractColumn::ColumnMode::DateTime:
		case AbstractColumn::ColumnMode::Month:
		case AbstractColumn::ColumnMode::Day:
			m_labels = new QMap<QDateTime, QString>();
			break;
		}
	}
}

void ColumnPrivate::addValueLabel(const QString& value, const QString& label) {
	if (m_columnMode != AbstractColumn::ColumnMode::Text)
		return;

	initLabels();
	static_cast<QMap<QString, QString>*>(m_labels)->operator[](value) = label;
}

void ColumnPrivate::addValueLabel(const QDateTime& value, const QString& label) {
	if (m_columnMode != AbstractColumn::ColumnMode::DateTime && m_columnMode != AbstractColumn::ColumnMode::Day
		&& m_columnMode != AbstractColumn::ColumnMode::Month)
		return;

	initLabels();
	static_cast<QMap<QDateTime, QString>*>(m_labels)->operator[](value) = label;
}

void ColumnPrivate::addValueLabel(double value, const QString& label) {
	if (m_columnMode != AbstractColumn::ColumnMode::Double)
		return;

	initLabels();
	static_cast<QMap<double, QString>*>(m_labels)->operator[](value) = label;
}

void ColumnPrivate::addValueLabel(int value, const QString& label) {
	if (m_columnMode != AbstractColumn::ColumnMode::Integer)
		return;

	initLabels();
	static_cast<QMap<int, QString>*>(m_labels)->operator[](value) = label;
}

void ColumnPrivate::addValueLabel(qint64 value, const QString& label) {
	if (m_columnMode != AbstractColumn::ColumnMode::BigInt)
		return;

	initLabels();
	static_cast<QMap<qint64, QString>*>(m_labels)->operator[](value) = label;
}

/**
 * \brief Set the content of row 'row'
 *
 * Use this only when columnMode() is Integer
 */
void ColumnPrivate::setIntegerAt(int row, int new_value) {
	// DEBUG(Q_FUNC_INFO);
	if (m_columnMode != AbstractColumn::ColumnMode::Integer)
		return;

	if (!m_data)
		initDataContainer();

	invalidate();

	Q_EMIT m_owner->dataAboutToChange(m_owner);
	if (row >= rowCount())
		resizeTo(row + 1);

	static_cast<QVector<int>*>(m_data)->replace(row, new_value);
	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
}

/**
 * \brief Replace a range of values
 *
 * Use this only when columnMode() is Integer
 */
void ColumnPrivate::replaceInteger(int first, const QVector<int>& new_values) {
	// DEBUG(Q_FUNC_INFO);
	if (m_columnMode != AbstractColumn::ColumnMode::Integer)
		return;

	if (!m_data)
		initDataContainer();

	invalidate();

	Q_EMIT m_owner->dataAboutToChange(m_owner);

	if (first < 0)
		*static_cast<QVector<int>*>(m_data) = new_values;
	else {
		const int num_rows = new_values.size();
		resizeTo(first + num_rows);

		int* ptr = static_cast<QVector<int>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[first + i] = new_values.at(i);
	}

	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
}

/**
 * \brief Set the content of row 'row'
 *
 * Use this only when columnMode() is BigInt
 */
void ColumnPrivate::setBigIntAt(int row, qint64 new_value) {
	// DEBUG(Q_FUNC_INFO);
	if (m_columnMode != AbstractColumn::ColumnMode::BigInt)
		return;

	if (!m_data)
		initDataContainer();

	invalidate();

	Q_EMIT m_owner->dataAboutToChange(m_owner);
	if (row >= rowCount())
		resizeTo(row + 1);

	static_cast<QVector<qint64>*>(m_data)->replace(row, new_value);
	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
}

/**
 * \brief Replace a range of values
 *
 * Use this only when columnMode() is BigInt
 */
void ColumnPrivate::replaceBigInt(int first, const QVector<qint64>& new_values) {
	// DEBUG(Q_FUNC_INFO);
	if (m_columnMode != AbstractColumn::ColumnMode::BigInt)
		return;

	if (!m_data)
		initDataContainer();

	invalidate();

	Q_EMIT m_owner->dataAboutToChange(m_owner);

	if (first < 0)
		*static_cast<QVector<qint64>*>(m_data) = new_values;
	else {
		const int num_rows = new_values.size();
		resizeTo(first + num_rows);

		qint64* ptr = static_cast<QVector<qint64>*>(m_data)->data();
		for (int i = 0; i < num_rows; ++i)
			ptr[first + i] = new_values.at(i);
	}

	if (!m_owner->m_suppressDataChangedSignal)
		Q_EMIT m_owner->dataChanged(m_owner);
}

/*!
 * Updates the properties. Will be called, when data in the column changed.
 * The properties will be used to speed up some algorithms.
 * See where variable properties will be used.
 */
void ColumnPrivate::updateProperties() {
	// DEBUG(Q_FUNC_INFO);

	// TODO: for double Properties::Constant will never be used. Use an epsilon (difference smaller than epsilon is zero)
	int rows = rowCount();
	if (rowCount() == 0) {
		properties = AbstractColumn::Properties::No;
		available.properties = true;
		return;
	}

	double prevValue = NAN;
	int prevValueInt = 0;
	qint64 prevValueBigInt = 0;
	qint64 prevValueDatetime = 0;

	if (m_columnMode == AbstractColumn::ColumnMode::Integer)
		prevValueInt = integerAt(0);
	else if (m_columnMode == AbstractColumn::ColumnMode::BigInt)
		prevValueBigInt = bigIntAt(0);
	else if (m_columnMode == AbstractColumn::ColumnMode::Double)
		prevValue = valueAt(0);
	else if (m_columnMode == AbstractColumn::ColumnMode::DateTime || m_columnMode == AbstractColumn::ColumnMode::Month
			 || m_columnMode == AbstractColumn::ColumnMode::Day)
		prevValueDatetime = dateTimeAt(0).toMSecsSinceEpoch();
	else {
		properties = AbstractColumn::Properties::No;
		available.properties = true;
		return;
	}

	int monotonic_decreasing = -1;
	int monotonic_increasing = -1;

	double value;
	int valueInt;
	qint64 valueBigInt;
	qint64 valueDateTime;
	for (int row = 1; row < rows; row++) {
		if (!m_owner->isValid(row) || m_owner->isMasked(row)) {
			// if there is one invalid or masked value, the property is No, because
			// otherwise it's difficult to find the correct index in indexForValue().
			// You don't know if you should increase the index or decrease it when
			// you hit an invalid value
			properties = AbstractColumn::Properties::No;
			available.properties = true;
			return;
		}

		if (m_columnMode == AbstractColumn::ColumnMode::Integer) {
			valueInt = integerAt(row);

			if (valueInt > prevValueInt) {
				monotonic_decreasing = 0;
				if (monotonic_increasing < 0)
					monotonic_increasing = 1;
				else if (monotonic_increasing == 0)
					break; // when nor increasing, nor decreasing, break

			} else if (valueInt < prevValueInt) {
				monotonic_increasing = 0;
				if (monotonic_decreasing < 0)
					monotonic_decreasing = 1;
				else if (monotonic_decreasing == 0)
					break; // when nor increasing, nor decreasing, break

			} else {
				if (monotonic_increasing < 0 && monotonic_decreasing < 0) {
					monotonic_decreasing = 1;
					monotonic_increasing = 1;
				}
			}

			prevValueInt = valueInt;
		} else if (m_columnMode == AbstractColumn::ColumnMode::BigInt) {
			valueBigInt = bigIntAt(row);

			if (valueBigInt > prevValueBigInt) {
				monotonic_decreasing = 0;
				if (monotonic_increasing < 0)
					monotonic_increasing = 1;
				else if (monotonic_increasing == 0)
					break; // when nor increasing, nor decreasing, break

			} else if (valueBigInt < prevValueBigInt) {
				monotonic_increasing = 0;
				if (monotonic_decreasing < 0)
					monotonic_decreasing = 1;
				else if (monotonic_decreasing == 0)
					break; // when nor increasing, nor decreasing, break

			} else {
				if (monotonic_increasing < 0 && monotonic_decreasing < 0) {
					monotonic_decreasing = 1;
					monotonic_increasing = 1;
				}
			}

			prevValueBigInt = valueBigInt;
		} else if (m_columnMode == AbstractColumn::ColumnMode::Double) {
			value = valueAt(row);

			if (std::isnan(value)) {
				monotonic_increasing = 0;
				monotonic_decreasing = 0;
				break;
			}

			if (value > prevValue) {
				monotonic_decreasing = 0;
				if (monotonic_increasing < 0)
					monotonic_increasing = 1;
				else if (monotonic_increasing == 0)
					break; // when nor increasing, nor decreasing, break

			} else if (value < prevValue) {
				monotonic_increasing = 0;
				if (monotonic_decreasing < 0)
					monotonic_decreasing = 1;
				else if (monotonic_decreasing == 0)
					break; // when nor increasing, nor decreasing, break

			} else {
				if (monotonic_increasing < 0 && monotonic_decreasing < 0) {
					monotonic_decreasing = 1;
					monotonic_increasing = 1;
				}
			}

			prevValue = value;
		} else if (m_columnMode == AbstractColumn::ColumnMode::DateTime || m_columnMode == AbstractColumn::ColumnMode::Month
				   || m_columnMode == AbstractColumn::ColumnMode::Day) {
			valueDateTime = dateTimeAt(row).toMSecsSinceEpoch();

			if (valueDateTime > prevValueDatetime) {
				monotonic_decreasing = 0;
				if (monotonic_increasing < 0)
					monotonic_increasing = 1;
				else if (monotonic_increasing == 0)
					break; // when nor increasing, nor decreasing, break

			} else if (valueDateTime < prevValueDatetime) {
				monotonic_increasing = 0;
				if (monotonic_decreasing < 0)
					monotonic_decreasing = 1;
				else if (monotonic_decreasing == 0)
					break; // when nor increasing, nor decreasing, break

			} else {
				if (monotonic_increasing < 0 && monotonic_decreasing < 0) {
					monotonic_decreasing = 1;
					monotonic_increasing = 1;
				}
			}

			prevValueDatetime = valueDateTime;
		}
	}

	properties = AbstractColumn::Properties::NonMonotonic;
	if (monotonic_increasing > 0 && monotonic_decreasing > 0) {
		properties = AbstractColumn::Properties::Constant;
		DEBUG("	setting column CONSTANT")
	} else if (monotonic_decreasing > 0) {
		properties = AbstractColumn::Properties::MonotonicDecreasing;
		DEBUG("	setting column MONTONIC DECREASING")
	} else if (monotonic_increasing > 0) {
		properties = AbstractColumn::Properties::MonotonicIncreasing;
		DEBUG("	setting column MONTONIC INCREASING")
	}

	available.properties = true;
}

////////////////////////////////////////////////////////////////////////////////
//@}
////////////////////////////////////////////////////////////////////////////////

/**
 * \brief Return the interval attribute representing the formula strings
 */
IntervalAttribute<QString> ColumnPrivate::formulaAttribute() const {
	return m_formulas;
}

/**
 * \brief Replace the interval attribute for the formula strings
 */
void ColumnPrivate::replaceFormulas(const IntervalAttribute<QString>& formulas) {
	m_formulas = formulas;
}

void ColumnPrivate::calculateStatistics() {
	PERFTRACE(QStringLiteral("calculate column statistics"));
	statistics = AbstractColumn::ColumnStatistics();

	if (m_owner->columnMode() == AbstractColumn::ColumnMode::Text) {
		calculateTextStatistics();
		return;
	}

	if (!m_owner->isNumeric())
		return;

	//######  location measures  #######
	int rowValuesSize = rowCount();
	double columnSum = 0.0;
	double columnProduct = 1.0;
	double columnSumNeg = 0.0;
	double columnSumSquare = 0.0;
	statistics.minimum = qInf();
	statistics.maximum = -qInf();
	std::unordered_map<double, int> frequencyOfValues;
	QVector<double> rowData;
	rowData.reserve(rowValuesSize);

	for (int row = 0; row < rowValuesSize; ++row) {
		double val = valueAt(row);
		if (std::isnan(val) || m_owner->isMasked(row))
			continue;

		if (val < statistics.minimum)
			statistics.minimum = val;
		if (val > statistics.maximum)
			statistics.maximum = val;
		columnSum += val;
		columnSumNeg += (1.0 / val); // will be Inf when val == 0
		columnSumSquare += val * val;
		columnProduct *= val;
		if (frequencyOfValues.find(val) != frequencyOfValues.end())
			frequencyOfValues.operator[](val)++;
		else
			frequencyOfValues.insert(std::make_pair(val, 1));
		rowData.push_back(val);
	}

	const size_t notNanCount = rowData.size();

	if (notNanCount == 0) {
		available.statistics = true;
		available.min = true;
		available.max = true;
		return;
	}

	if (rowData.size() < rowValuesSize)
		rowData.squeeze();

	statistics.size = notNanCount;
	statistics.arithmeticMean = columnSum / notNanCount;

	// geometric mean
	if (statistics.minimum <= -100.) // invalid
		statistics.geometricMean = qQNaN();
	else if (statistics.minimum < 0) { // interpret as percentage (/100) and add 1
		columnProduct = 1.; // recalculate
		for (auto val : rowData)
			columnProduct *= val / 100. + 1.;
		// n-th root and convert back to percentage changes
		statistics.geometricMean = 100. * (std::pow(columnProduct, 1.0 / notNanCount) - 1.);
	} else if (statistics.minimum == 0) { // replace zero values with 1
		columnProduct = 1.; // recalculate
		for (auto val : rowData)
			columnProduct *= (val == 0.) ? 1. : val;
		statistics.geometricMean = std::pow(columnProduct, 1.0 / notNanCount);
	} else
		statistics.geometricMean = std::pow(columnProduct, 1.0 / notNanCount);

	statistics.harmonicMean = notNanCount / columnSumNeg;
	statistics.contraharmonicMean = columnSumSquare / columnSum;

	// calculate the mode, the most frequent value in the data set
	int maxFreq = 0;
	double mode = NAN;
	for (const auto& it : frequencyOfValues) {
		if (it.second > maxFreq) {
			maxFreq = it.second;
			mode = it.first;
		}
	}
	// check how many times the max frequency occurs in the data set.
	// if more than once, we have a multi-modal distribution and don't show any mode
	int maxFreqOccurance = 0;
	for (const auto& it : frequencyOfValues) {
		if (it.second == maxFreq)
			++maxFreqOccurance;

		if (maxFreqOccurance > 1) {
			mode = NAN;
			break;
		}
	}
	statistics.mode = mode;

	// sort the data to calculate the percentiles
	std::sort(rowData.begin(), rowData.end());
	statistics.firstQuartile = gsl_stats_quantile_from_sorted_data(rowData.constData(), 1, notNanCount, 0.25);
	statistics.median = gsl_stats_quantile_from_sorted_data(rowData.constData(), 1, notNanCount, 0.50);
	statistics.thirdQuartile = gsl_stats_quantile_from_sorted_data(rowData.constData(), 1, notNanCount, 0.75);
	statistics.percentile_1 = gsl_stats_quantile_from_sorted_data(rowData.constData(), 1, notNanCount, 0.01);
	statistics.percentile_5 = gsl_stats_quantile_from_sorted_data(rowData.constData(), 1, notNanCount, 0.05);
	statistics.percentile_10 = gsl_stats_quantile_from_sorted_data(rowData.constData(), 1, notNanCount, 0.1);
	statistics.percentile_90 = gsl_stats_quantile_from_sorted_data(rowData.constData(), 1, notNanCount, 0.9);
	statistics.percentile_95 = gsl_stats_quantile_from_sorted_data(rowData.constData(), 1, notNanCount, 0.95);
	statistics.percentile_99 = gsl_stats_quantile_from_sorted_data(rowData.constData(), 1, notNanCount, 0.99);
	statistics.iqr = statistics.thirdQuartile - statistics.firstQuartile;
	statistics.trimean = (statistics.firstQuartile + 2. * statistics.median + statistics.thirdQuartile) / 4.;

	//######  dispersion and shape measures  #######
	statistics.variance = 0.;
	statistics.meanDeviation = 0.;
	statistics.meanDeviationAroundMedian = 0.;
	double centralMoment_r3 = 0.;
	double centralMoment_r4 = 0.;
	QVector<double> absoluteMedianList;
	absoluteMedianList.reserve(notNanCount);
	absoluteMedianList.resize(notNanCount);

	for (size_t row = 0; row < notNanCount; ++row) {
		double val = rowData.value(row);
		statistics.variance += gsl_pow_2(val - statistics.arithmeticMean);
		statistics.meanDeviation += std::abs(val - statistics.arithmeticMean);

		absoluteMedianList[row] = std::abs(val - statistics.median);
		statistics.meanDeviationAroundMedian += absoluteMedianList[row];

		centralMoment_r3 += gsl_pow_3(val - statistics.arithmeticMean);
		centralMoment_r4 += gsl_pow_4(val - statistics.arithmeticMean);
	}

	// normalize
	statistics.variance = (notNanCount != 1) ? statistics.variance / (notNanCount - 1) : NAN;
	statistics.meanDeviationAroundMedian = statistics.meanDeviationAroundMedian / notNanCount;
	statistics.meanDeviation = statistics.meanDeviation / notNanCount;

	// standard deviation
	statistics.standardDeviation = std::sqrt(statistics.variance);

	//"median absolute deviation" - the median of the absolute deviations from the data's median.
	std::sort(absoluteMedianList.begin(), absoluteMedianList.end());
	statistics.medianDeviation = gsl_stats_quantile_from_sorted_data(absoluteMedianList.data(), 1, notNanCount, 0.50);

	// skewness and kurtosis
	centralMoment_r3 = centralMoment_r3 / notNanCount;
	centralMoment_r4 = centralMoment_r4 / notNanCount;
	statistics.skewness = centralMoment_r3 / gsl_pow_3(statistics.standardDeviation);
	statistics.kurtosis = (centralMoment_r4 / gsl_pow_4(statistics.standardDeviation)) - 3.0;

	// entropy
	double entropy = 0.;
	for (const auto& v : frequencyOfValues) {
		const double frequencyNorm = static_cast<double>(v.second) / notNanCount;
		entropy += (frequencyNorm * std::log2(frequencyNorm));
	}

	statistics.entropy = -entropy;

	available.statistics = true;
	available.min = true;
	available.max = true;
}

void ColumnPrivate::calculateTextStatistics() {
	if (!available.dictionary)
		initDictionary();

	int valid = 0;
	for (int row = 0; row < rowCount(); ++row) {
		if (m_owner->isMasked(row))
			continue;

		++valid;
	}

	statistics.size = valid;
	statistics.unique = m_dictionary.count();
	available.statistics = true;
}
