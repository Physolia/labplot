/***************************************************************************
    File                 : Curve3D.h
    Project              : LabPlot
    Description          : 3D curve class
    --------------------------------------------------------------------
    Copyright            : (C) 2015 by Minh Ngo (minh@fedoraproject.org)

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

#ifndef PLOT3D_CURVE3D_H
#define PLOT3D_CURVE3D_H

#include "Base3D.h"
#include "backend/lib/macros.h"

class AbstractColumn;
class Curve3DPrivate;
class Curve3D : public Base3D {
		Q_OBJECT
		Q_DECLARE_PRIVATE(Curve3D)
		Q_DISABLE_COPY(Curve3D)
	public:
		Curve3D(vtkRenderer* renderer = 0);
		virtual ~Curve3D();

		virtual void save(QXmlStreamWriter*) const;
		virtual bool load(XmlStreamReader*);

		POINTER_D_ACCESSOR_DECL(const AbstractColumn, xColumn, XColumn);
		POINTER_D_ACCESSOR_DECL(const AbstractColumn, yColumn, YColumn);
		POINTER_D_ACCESSOR_DECL(const AbstractColumn, zColumn, ZColumn);

		const QString& xColumnPath() const;
		const QString& yColumnPath() const;
		const QString& zColumnPath() const;

		BASIC_D_ACCESSOR_DECL(float, pointRadius, PointRadius);
		BASIC_D_ACCESSOR_DECL(bool, showEdges, ShowEdges);
		BASIC_D_ACCESSOR_DECL(bool, isClosed, IsClosed);

		typedef Curve3D BaseClass;
		typedef Curve3DPrivate Private;

	private slots:
		void xColumnAboutToBeRemoved(const AbstractAspect*);
		void yColumnAboutToBeRemoved(const AbstractAspect*);
		void zColumnAboutToBeRemoved(const AbstractAspect*);

	signals:
		friend class Curve3DSetXColumnCmd;
		friend class Curve3DSetYColumnCmd;
		friend class Curve3DSetZColumnCmd;
		friend class Curve3DSetPointRadiusCmd;
		friend class Curve3DSetShowEdgesCmd;
		friend class Curve3DSetIsClosedCmd;
		void xColumnChanged(const AbstractColumn*);
		void yColumnChanged(const AbstractColumn*);
		void zColumnChanged(const AbstractColumn*);
		void pointRadiusChanged(float);
		void showEdgesChanged(bool);
		void isClosedChanged(bool);
};

#endif