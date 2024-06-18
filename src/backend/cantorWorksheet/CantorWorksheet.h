/*
	File                 : CantorWorksheet.h
	Project              : LabPlot
	Description          : Aspect providing a Cantor Worksheets for Multiple backends
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2015 Garvit Khatri <garvitdelhi@gmail.com>
	SPDX-FileCopyrightText: 2016-2023 Alexander Semke <alexander.semke@web.de>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CANTORWORKSHEET_H
#define CANTORWORKSHEET_H

#include <backend/core/AbstractPart.h>
#if defined(HAVE_CANTOR_LIBS) && !defined(SDK)
#include <cantor/session.h>
#endif

namespace Cantor {
class PanelPlugin;
class WorksheetAccessInterface;
}

namespace KParts {
class ReadWritePart;
}

class QAbstractItemModel;
class CantorWorksheetView;
class Column;

class CantorWorksheet : public AbstractPart {
	Q_OBJECT

public:
	explicit CantorWorksheet(const QString& name, bool loading = false);

	bool init(QByteArray* content = nullptr);
	const QString& error() const;

	QWidget* view() const override;
	QMenu* createContextMenu() override;
	void fillColumnContextMenu(QMenu*, Column*);
	QIcon icon() const override;

	bool exportView() const override;
	bool printView() override;
	bool printPreview() const override;

	void evaluate();
	void restart();

	void save(QXmlStreamWriter*) const override;
	bool load(XmlStreamReader*, bool preview) override;

	QString backendName();
	KParts::ReadWritePart* part();
	QList<Cantor::PanelPlugin*> getPlugins();

private:
	mutable CantorWorksheetView* m_view{nullptr};
	QString m_backendName;
	QString m_error;
#ifdef HAVE_CANTOR_LIBS
	Cantor::Session* m_session{nullptr};
#endif
	KParts::ReadWritePart* m_part{nullptr};
	QList<Cantor::PanelPlugin*> m_plugins;
	bool m_pluginsLoaded{false};
	QAbstractItemModel* m_variableModel{nullptr};
	Cantor::WorksheetAccessInterface* m_worksheetAccess{nullptr};

	void parseData(int row);

private Q_SLOTS:
	void dataChanged(const QModelIndex&);
	void rowsInserted(const QModelIndex& parent, int first, int last);
	void rowsAboutToBeRemoved(const QModelIndex& parent, int first, int last);
	void modelReset();
	void modified();

Q_SIGNALS:
	void requestProjectContextMenu(QMenu*);
#ifdef HAVE_CANTOR_LIBS
	void statusChanged(Cantor::Session::Status);
#endif
};

#endif // CANTORWORKSHEET_H
