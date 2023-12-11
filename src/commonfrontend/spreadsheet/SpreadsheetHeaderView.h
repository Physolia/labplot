/*
	File                 : SpreadsheetHeaderView.h
	Project              : LabPlot
	Description          : Horizontal header for SpreadsheetView displaying comments in a second header
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2016 Alexander Semke <alexander.semke@web.de>
	SPDX-FileCopyrightText: 2007 Tilman Benkert <thzs@gmx.net>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SPREADSHEETHEADERVIEW_H
#define SPREADSHEETHEADERVIEW_H

#include <QHeaderView>

class SpreadsheetCommentsHeaderView : public QHeaderView {
	Q_OBJECT

public:
	/*!
	 \class SpreadsheetCommentsHeaderView
	 \brief Slave header for SpreadsheetDoubleHeaderView
		 This class is only to be used by SpreadsheetDoubleHeaderView.
	 It allows for displaying two horizontal headers in a SpreadsheetView.
	 A SpreadsheetCommentsHeaderView displays the column comments
	 in a second header below the normal header. It is completely
	 controlled by a SpreadsheetDoubleHeaderView object and thus has
	 a master-slave relationship to it. This would be an inner class
	 of SpreadsheetDoubleHeaderView if Qt allowed this.
		 \ingroup commonfrontend
	*/
	explicit SpreadsheetCommentsHeaderView(QWidget* parent = nullptr);
	~SpreadsheetCommentsHeaderView() override;

	void setModel(QAbstractItemModel*) override;
	friend class SpreadsheetHeaderView; // access to paintSection (protected)
};

class SpreadsheetSparkLineHeaderView : public QHeaderView {
	Q_OBJECT
public:
	explicit SpreadsheetSparkLineHeaderView(QWidget* parent = nullptr);

	~SpreadsheetSparkLineHeaderView() override;

	void setModel(QAbstractItemModel*) override;
	friend class SpreadsheetHeaderView; // access to paintSection (protected)

};

class SpreadsheetHeaderView : public QHeaderView {
	Q_OBJECT

public:
	explicit SpreadsheetHeaderView(QWidget* parent = nullptr);
	~SpreadsheetHeaderView() override;

	void setModel(QAbstractItemModel*) override;
	QSize sizeHint() const override;
	void addSlaveHeader(QHeaderView* slaveHeader);

	void showComments(bool on = true);
	bool areCommentsShown() const;

	void showSparkLines(bool on = true);
	bool areSparkLinesShown() const;

private:
	SpreadsheetCommentsHeaderView* m_commentSlave;
	SpreadsheetSparkLineHeaderView* m_sparkLineSlave;
	bool m_showComments;
	bool m_showSparkLines;


private Q_SLOTS:
	void refresh();
	void headerDataChanged(Qt::Orientation, int logicalFirst, int logicalLast);

protected:
	void paintSection(QPainter*, const QRect&, int logicalIndex) const override;
};

#endif
