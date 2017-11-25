/***************************************************************************
    File                 : LabPlotProjectParser.h
    Project              : LabPlot
    Description          : parser for LabPlot projects
    --------------------------------------------------------------------
    Copyright            : (C) 2017 Alexander Semke (alexander.semke@web.de)

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

#include "backend/datasources/projects/LabPlotProjectParser.h"
#include "backend/core/AspectTreeModel.h"
#include "backend/core/Project.h"

/*!
\class LabPlotProjectParser
\brief parser for LabPlot projects.

\ingroup datasources
*/

LabPlotProjectParser::LabPlotProjectParser() : ProjectParser() {
	m_topLevelClasses<<"Folder"<<"Workbook"<<"Spreadsheet"<<"Matrix"<<"Worksheet"<<"CantorWorksheet"<<"Datapicker"<<"LiveDataSource";
}

LabPlotProjectParser::~LabPlotProjectParser() {
	if (m_project != nullptr)
		delete m_project;
}

QAbstractItemModel* LabPlotProjectParser::model() {
	WAIT_CURSOR;
	if (m_project == nullptr)
		m_project = new Project();

	AspectTreeModel* model = nullptr;
	bool rc = m_project->load(m_projectFileName, true);
	if (rc) {
		model = new AspectTreeModel(m_project);
		model->setReadOnly(true);
	}

	RESET_CURSOR;
	return model;
}

void LabPlotProjectParser::importTo(Folder* folder, const QStringList& selectedPathes) {
	QDEBUG("Starting the import of " + m_projectFileName);

	//import the selected objects into a temporary project
	Project* project = new Project();
	project->setPathesToLoad(selectedPathes);
	project->load(m_projectFileName);

	//move all children from the temp project to the target folder
	for (auto* child : project->children<AbstractAspect>()) {
		project->removeChild(child);
		folder->addChild(child);
	}
	delete project;

	QDEBUG("Import of " + m_projectFileName + " done.");
}
