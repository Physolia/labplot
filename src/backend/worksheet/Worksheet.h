/***************************************************************************
    File                 : Worksheet.h
    Project              : LabPlot/SciDAVis
    Description          : Worksheet (2D visualization) part
    --------------------------------------------------------------------
    Copyright            : (C) 2009 Tilman Benkert (thzs*gmx.net)
	Copyright            : (C) 2011-2012 by Alexander Semke (alexander.semke*web.de)
                           (replace * with @ in the email addresses)
                           
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

#ifndef WORKSHEET_H
#define WORKSHEET_H

#include "core/AbstractPart.h"
#include "core/AbstractScriptingEngine.h"
#include "worksheet/WorksheetModel.h"
#include "worksheet/plots/PlotArea.h"
#include "lib/macros.h"

class QGraphicsScene;
class QGraphicsItem;
class WorksheetGraphicsScene;
class QRectF;

class WorksheetPrivate;

class Worksheet: public AbstractPart, public scripted{
	Q_OBJECT

	public:
		Worksheet(AbstractScriptingEngine *engine, const QString &name);
		~Worksheet();

		enum Unit {Millimeter, Centimeter, Inch, Point};
		static float convertToMillimeter(const float value, const Worksheet::Unit unit);
		static float convertFromMillimeter(const float value, const Worksheet::Unit unit);

		virtual QIcon icon() const;
		virtual bool fillProjectMenu(QMenu *menu);
		virtual QMenu *createContextMenu();
		virtual QWidget *view() const;

		virtual void save(QXmlStreamWriter *) const;
		virtual bool load(XmlStreamReader *);

		QRectF pageRect() const;
		void setPageRect(const QRectF &rect, const bool scaleContent=false);
		WorksheetGraphicsScene *scene() const;
		void update();

		BASIC_D_ACCESSOR_DECL(qreal, backgroundOpacity, BackgroundOpacity);
		BASIC_D_ACCESSOR_DECL(PlotArea::BackgroundType, backgroundType, BackgroundType);
		BASIC_D_ACCESSOR_DECL(PlotArea::BackgroundColorStyle, backgroundColorStyle, BackgroundColorStyle);
		BASIC_D_ACCESSOR_DECL(PlotArea::BackgroundImageStyle, backgroundImageStyle, BackgroundImageStyle);
		CLASS_D_ACCESSOR_DECL(QBrush, backgroundBrush, BackgroundBrush);
		CLASS_D_ACCESSOR_DECL(QColor, backgroundFirstColor, BackgroundFirstColor);
		CLASS_D_ACCESSOR_DECL(QColor, backgroundSecondColor, BackgroundSecondColor);
		CLASS_D_ACCESSOR_DECL(QString, backgroundFileName, BackgroundFileName);
		
		typedef WorksheetPrivate Private;

	private:
		WorksheetPrivate* const d;

	 private slots:
		void handleAspectAdded(const AbstractAspect *handleAspect);
		void handleAspectAboutToBeRemoved(const AbstractAspect *handleAspect);
		void childSelected();

	 signals:
		void requestProjectContextMenu(QMenu *menu);
		void itemSelected(QGraphicsItem*);
		void requestUpdate();
};

#endif
