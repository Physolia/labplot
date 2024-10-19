/*
 *	File                 : ActionsManager.cpp
 *	Project              : LabPlot
 *	Description 	     : Class managing all actions and their containers (menus and toolbars) in MainWin
 *	--------------------------------------------------------------------
 *	SPDX-FileCopyrightText: 2024 Alexander Semke <alexander.semke@web.de>
 *	SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "backend/core/AbstractAspect.h"
#include "backend/core/Project.h"
#include "backend/core/Settings.h"
#include "backend/datapicker/Datapicker.h"
#include "backend/matrix/Matrix.h"
#include "backend/spreadsheet/Spreadsheet.h"
#include "frontend/ActionsManager.h"
#include "frontend/MainWin.h"
#include "frontend/note/NoteView.h"
#include "frontend/matrix/MatrixView.h"
#include "frontend/spreadsheet/SpreadsheetView.h"
#include "frontend/widgets/FITSHeaderEditDialog.h"
#include "frontend/widgets/LabelWidget.h"
#include "frontend/widgets/MemoryWidget.h"
#include "frontend/worksheet/WorksheetPreviewWidget.h"
#include "frontend/worksheet/WorksheetView.h"
#include "frontend/datapicker/DatapickerView.h"

#ifdef HAVE_CANTOR_LIBS
#include "backend/notebook/Notebook.h"
#include "frontend/notebook/NotebookView.h"
#include <cantor/backend.h>
#endif

#include <QActionGroup>
#include <QJsonArray>
#include <QMenuBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>

#include <KActionCollection>
#include <KActionMenu>
#include <KColorScheme>
#include <KColorSchemeManager>
#include <KColorSchemeMenu>
#include <KCompressionDevice>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KRecentFilesAction>
#include <KStandardAction>
#include <KToggleAction>
#include <KToggleFullScreenAction>
#include <KToolBar>
#include <kxmlguifactory.h>

#ifdef HAVE_PURPOSE
#include <Purpose/AlternativesModel>
#include <Purpose/Menu>
#include <QMimeType>
#include <purpose_version.h>
#endif

#ifdef HAVE_TOUCHBAR
#include "3rdparty/kdmactouchbar/src/kdmactouchbar.h"
#endif

#include <DockWidget.h>
#include <DockAreaWidget.h>
#include <DockManager.h>


/*!
 *  \class ActionsManager
 *  \brief The GUI observer looks for the selection changes in the main window
 *  and shows/hides the correspondings dock widgets, toolbars etc.
 *  This class is intended to simplify (or not to overload) the code in MainWin.
 *
 *  \ingroup frontend
 */

ActionsManager::ActionsManager(MainWin* mainWin) : m_mainWindow(mainWin) {
	initActions();
}

void ActionsManager::init() {
		// all toolbars created via the KXMLGUI framework are locked on default:
	//  * on the very first program start, unlock all toolbars
	//  * on later program starts, set stored lock status
	// Furthermore, we want to show icons only after the first program start.
	KConfigGroup groupMain = Settings::group(QStringLiteral("MainWindow"));
	if (groupMain.exists()) {
		// KXMLGUI framework automatically stores "Disabled" for the key "ToolBarsMovable"
		// in case the toolbars are locked -> load this value
		const QString& str = groupMain.readEntry(QLatin1String("ToolBarsMovable"), "");
		bool locked = (str == QLatin1String("Disabled"));
		KToolBar::setToolBarsLocked(locked);
	}

	auto* factory = m_mainWindow->factory();

	// in case we're starting for the first time, put all toolbars into the IconOnly mode
	// and maximize the main window. The occurence of LabPlot's own section "MainWin"
	// indicates whether this is the first start or not
	groupMain = Settings::group(QStringLiteral("MainWin"));
	if (!groupMain.exists()) {
		// first start
		KToolBar::setToolBarsLocked(false);

		// show icons only
		const auto& toolbars = factory->containers(QLatin1String("ToolBar"));
		for (auto* container : toolbars) {
			auto* toolbar = dynamic_cast<QToolBar*>(container);
			if (toolbar)
				toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
		}

		m_mainWindow->showMaximized();
	}

	initMenus();

	auto* mainToolBar = qobject_cast<QToolBar*>(factory->container(QLatin1String("main_toolbar"), m_mainWindow));

#ifdef HAVE_CANTOR_LIBS
	auto* tbNotebook = new QToolButton(mainToolBar);
	tbNotebook->setPopupMode(QToolButton::MenuButtonPopup);
	tbNotebook->setMenu(m_newNotebookMenu); // it is possible for m_newNotebookMenu to be null when we have no backends
	auto* lastAction = mainToolBar->actions().at(mainToolBar->actions().count() - 2);
	mainToolBar->insertWidget(lastAction, tbNotebook);
#endif

	auto* tbImport = new QToolButton(mainToolBar);
	tbImport->setPopupMode(QToolButton::MenuButtonPopup);
	tbImport->setMenu(m_importMenu);
	tbImport->setDefaultAction(m_importFileAction);
	auto* lastAction_ = mainToolBar->actions().at(mainToolBar->actions().count() - 1);
	mainToolBar->insertWidget(lastAction_, tbImport);

	// hamburger menu
	m_hamburgerMenu = KStandardAction::hamburgerMenu(nullptr, nullptr, m_mainWindow->actionCollection());
	m_mainWindow->toolBar()->addAction(m_hamburgerMenu);
	m_hamburgerMenu->hideActionsOf(m_mainWindow->toolBar());
	m_hamburgerMenu->setMenuBar(m_mainWindow->menuBar());

	// load recently used projects
	m_recentProjectsAction->loadEntries(Settings::group(QStringLiteral("Recent Files")));

	// read the settings of MainWin
	const KConfigGroup& groupMainWin = Settings::group(QStringLiteral("MainWin"));

	// show memory info
	m_memoryInfoAction->setEnabled(m_mainWindow->statusBar()->isEnabled()); // disable/enable menu with statusbar
	bool memoryInfoShown = groupMainWin.readEntry(QLatin1String("ShowMemoryInfo"), true);
	// DEBUG(Q_FUNC_INFO << ", memory info enabled in config: " << memoryInfoShown)
	m_memoryInfoAction->setChecked(memoryInfoShown);
	if (memoryInfoShown)
		toggleMemoryInfo();

	// connect(mainWin->m_projectExplorer, &ProjectExplorer::selectedAspectsChanged, this, &ActionsManager::selectedAspectsChanged);
	// connect(mainWin->m_projectExplorer, &ProjectExplorer::hiddenAspectSelected, this, &ActionsManager::hiddenAspectSelected);
}

ActionsManager::~ActionsManager() {
	m_recentProjectsAction->saveEntries(Settings::group(QStringLiteral("Recent Files")));
}

void ActionsManager::initActions() {
	auto* collection = m_mainWindow->actionCollection();

	// ******************** File-menu *******************************
	// add some standard actions
	m_newProjectAction = KStandardAction::openNew(
		this,
		[=]() {
			m_mainWindow->newProject(true);
		},
		collection);
	m_openProjectAction = KStandardAction::open(this, static_cast<void (MainWin::*)()>(&MainWin::openProject), collection);
	m_recentProjectsAction = KStandardAction::openRecent(this, &MainWin::openRecentProject, collection);
	// m_closeAction = KStandardAction::close(this, &MainWin::closeProject, collection);
	// collection->setDefaultShortcut(m_closeAction, QKeySequence()); // remove the shortcut, QKeySequence::Close will be used for closing sub-windows
	m_saveAction = KStandardAction::save(this, &MainWin::saveProject, collection);
	m_saveAsAction = KStandardAction::saveAs(this, &MainWin::saveProjectAs, collection);
	m_printAction = KStandardAction::print(this, &MainWin::print, collection);
	m_printPreviewAction = KStandardAction::printPreview(this, &MainWin::printPreview, collection);

	QAction* openExample = new QAction(i18n("&Open Example"), collection);
	openExample->setIcon(QIcon::fromTheme(QLatin1String("folder-documents")));
	collection->addAction(QLatin1String("file_example_open"), openExample);
	connect(openExample, &QAction::triggered, m_mainWindow, &MainWin::exampleProjectsDialog);

	m_fullScreenAction = KStandardAction::fullScreen(this, &ActionsManager::toggleFullScreen, m_mainWindow, collection);

	// QDEBUG(Q_FUNC_INFO << ", preferences action name:" << KStandardAction::name(KStandardAction::Preferences))
	KStandardAction::preferences(this, &MainWin::settingsDialog, collection);
	// QAction* action = collection->action(KStandardAction::name(KStandardAction::Preferences)));
	KStandardAction::quit(this, &MainWin::close, collection);

	// New Folder/Workbook/Spreadsheet/Matrix/Worksheet/Datasources
	m_newWorkbookAction = new QAction(QIcon::fromTheme(QLatin1String("labplot-workbook-new")), i18n("Workbook"), this);
	collection->addAction(QLatin1String("new_workbook"), m_newWorkbookAction);
	m_newWorkbookAction->setWhatsThis(i18n("Creates a new workbook for collection spreadsheets, matrices and plots"));
	connect(m_newWorkbookAction, &QAction::triggered, m_mainWindow, &MainWin::newWorkbook);

	m_newDatapickerAction = new QAction(QIcon::fromTheme(QLatin1String("color-picker-black")), i18n("Data Extractor"), this);
	m_newDatapickerAction->setWhatsThis(i18n("Creates a data extractor for getting data from a picture"));
	collection->addAction(QLatin1String("new_datapicker"), m_newDatapickerAction);
	connect(m_newDatapickerAction, &QAction::triggered, m_mainWindow, &MainWin::newDatapicker);

	m_newSpreadsheetAction = new QAction(QIcon::fromTheme(QLatin1String("labplot-spreadsheet-new")), i18n("Spreadsheet"), this);
	// 	m_newSpreadsheetAction->setShortcut(Qt::CTRL+Qt::Key_Equal);
	m_newSpreadsheetAction->setWhatsThis(i18n("Creates a new spreadsheet for data editing"));
	collection->addAction(QLatin1String("new_spreadsheet"), m_newSpreadsheetAction);
	connect(m_newSpreadsheetAction, &QAction::triggered, m_mainWindow, &MainWin::newSpreadsheet);

	m_newMatrixAction = new QAction(QIcon::fromTheme(QLatin1String("labplot-matrix-new")), i18n("Matrix"), this);
	// 	m_newMatrixAction->setShortcut(Qt::CTRL+Qt::Key_Equal);
	m_newMatrixAction->setWhatsThis(i18n("Creates a new matrix for data editing"));
	collection->addAction(QLatin1String("new_matrix"), m_newMatrixAction);
	connect(m_newMatrixAction, &QAction::triggered, m_mainWindow, &MainWin::newMatrix);

	m_newWorksheetAction = new QAction(QIcon::fromTheme(QLatin1String("labplot-worksheet-new")), i18n("Worksheet"), this);
	// 	m_newWorksheetAction->setShortcut(Qt::ALT+Qt::Key_X);
	m_newWorksheetAction->setWhatsThis(i18n("Creates a new worksheet for data plotting"));
	collection->addAction(QLatin1String("new_worksheet"), m_newWorksheetAction);
	connect(m_newWorksheetAction, &QAction::triggered, m_mainWindow, &MainWin::newWorksheet);

	m_newNotesAction = new QAction(QIcon::fromTheme(QLatin1String("document-new")), i18n("Note"), this);
	m_newNotesAction->setWhatsThis(i18n("Creates a new note for arbitrary text"));
	collection->addAction(QLatin1String("new_notes"), m_newNotesAction);
	connect(m_newNotesAction, &QAction::triggered, m_mainWindow, &MainWin::newNotes);

	m_newFolderAction = new QAction(QIcon::fromTheme(QLatin1String("folder-new")), i18n("Folder"), this);
	m_newFolderAction->setWhatsThis(i18n("Creates a new folder to collect sheets and other elements"));
	collection->addAction(QLatin1String("new_folder"), m_newFolderAction);
	connect(m_newFolderAction, &QAction::triggered, m_mainWindow, &MainWin::newFolder);

	//"New file datasources"
	m_newLiveDataSourceAction = new QAction(QIcon::fromTheme(QLatin1String("edit-text-frame-update")), i18n("Live Data Source..."), this);
	m_newLiveDataSourceAction->setWhatsThis(i18n("Creates a live data source to read data from a real time device"));
	collection->addAction(QLatin1String("new_live_datasource"), m_newLiveDataSourceAction);
	connect(m_newLiveDataSourceAction, &QAction::triggered, m_mainWindow, &MainWin::newLiveDataSource);

	// Import/Export
	m_importFileAction = new QAction(QIcon::fromTheme(QLatin1String("document-import")), i18n("From File..."), this);
	collection->setDefaultShortcut(m_importFileAction, Qt::CTRL | Qt::SHIFT | Qt::Key_I);
	m_importFileAction->setWhatsThis(i18n("Import data from a regular file"));
	connect(m_importFileAction, &QAction::triggered, this, [=]() {
		m_mainWindow->importFileDialog();
	});

	// second "import from file" action, with a shorter name, to be used in the sub-menu of the "Import"-menu.
	// the first action defined above will be used in the toolbar and touchbar where we need the more detailed name "Import From File".
	m_importFileAction_2 = new QAction(QIcon::fromTheme(QLatin1String("document-import")), i18n("From File..."), this);
	collection->addAction(QLatin1String("import_file"), m_importFileAction_2);
	m_importFileAction_2->setWhatsThis(i18n("Import data from a regular file"));
	connect(m_importFileAction_2, &QAction::triggered, this, [=]() {
		m_mainWindow->importFileDialog();
	});

	m_importKaggleDatasetAction = new QAction(QIcon::fromTheme(QLatin1String("labplot-kaggle")), i18n("From kaggle.com..."), this);
	m_importKaggleDatasetAction->setWhatsThis(i18n("Import data from kaggle.com"));
	collection->addAction(QLatin1String("import_dataset_kaggle"), m_importKaggleDatasetAction);
	connect(m_importKaggleDatasetAction, &QAction::triggered, m_mainWindow, &MainWin::importKaggleDatasetDialog);

	m_importSqlAction = new QAction(QIcon::fromTheme(QLatin1String("network-server-database")), i18n("From SQL Database..."), this);
	m_importSqlAction->setWhatsThis(i18n("Import data from a SQL database"));
	collection->addAction(QLatin1String("import_sql"), m_importSqlAction);
	connect(m_importSqlAction, &QAction::triggered, m_mainWindow, &MainWin::importSqlDialog);

	m_importDatasetAction = new QAction(QIcon::fromTheme(QLatin1String("database-index")), i18n("From Dataset Collection..."), this);
	m_importDatasetAction->setWhatsThis(i18n("Import data from an online dataset"));
	collection->addAction(QLatin1String("import_dataset_datasource"), m_importDatasetAction);
	connect(m_importDatasetAction, &QAction::triggered, m_mainWindow, &MainWin::importDatasetDialog);

	m_importLabPlotAction = new QAction(QIcon::fromTheme(QLatin1String("project-open")), i18n("LabPlot Project..."), this);
	m_importLabPlotAction->setWhatsThis(i18n("Import a project from a LabPlot project file (.lml)"));
	collection->addAction(QLatin1String("import_labplot"), m_importLabPlotAction);
	connect(m_importLabPlotAction, &QAction::triggered, m_mainWindow, &MainWin::importProjectDialog);

#ifdef HAVE_LIBORIGIN
	m_importOpjAction = new QAction(QIcon::fromTheme(QLatin1String("project-open")), i18n("Origin Project (OPJ)..."), this);
	m_importOpjAction->setWhatsThis(i18n("Import a project from an OriginLab Origin project file (.opj)"));
	collection->addAction(QLatin1String("import_opj"), m_importOpjAction);
	connect(m_importOpjAction, &QAction::triggered, m_mainWindow, &MainWin::importProjectDialog);
#endif

	m_exportAction = new QAction(QIcon::fromTheme(QLatin1String("document-export")), i18n("Export..."), this);
	m_exportAction->setWhatsThis(i18n("Export selected element"));
	collection->setDefaultShortcut(m_exportAction, Qt::CTRL | Qt::SHIFT | Qt::Key_E);
	collection->addAction(QLatin1String("export"), m_exportAction);
	connect(m_exportAction, &QAction::triggered, m_mainWindow, &MainWin::exportDialog);

#ifdef HAVE_PURPOSE
	m_shareAction = new QAction(QIcon::fromTheme(QLatin1String("document-share")), i18n("Share"), this);
	collection->addAction(QLatin1String("share"), m_shareAction);
#endif

	// Tools
	auto* action = new QAction(QIcon::fromTheme(QLatin1String("color-management")), i18n("Color Maps Browser"), this);
	action->setWhatsThis(i18n("Open dialog to browse through the available color maps."));
	collection->addAction(QLatin1String("color_maps"), action);
	// connect(action, &QAction::triggered, this, [=]() {
	// 	auto* dlg = new ColorMapsDialog(this);
	// 	dlg->exec();
	// 	delete dlg;
	// });

#ifdef HAVE_FITS
	action = new QAction(QIcon::fromTheme(QLatin1String("editor")), i18n("FITS Metadata Editor..."), this);
	action->setWhatsThis(i18n("Open editor to edit FITS meta data"));
	collection->addAction(QLatin1String("edit_fits"), action);
	connect(action, &QAction::triggered, m_mainWindow, &MainWin::editFitsFileDialog);
#endif

	// Edit
	// Undo/Redo-stuff
	m_undoAction = KStandardAction::undo(this, SLOT(undo()), collection);
	m_redoAction = KStandardAction::redo(this, SLOT(redo()), collection);
	m_historyAction = new QAction(QIcon::fromTheme(QLatin1String("view-history")), i18n("Undo/Redo History..."), this);
	collection->addAction(QLatin1String("history"), m_historyAction);
	connect(m_historyAction, &QAction::triggered, m_mainWindow, &MainWin::historyDialog);

#ifdef Q_OS_MAC
	m_undoIconOnlyAction = new QAction(m_undoAction->icon(), QString());
	connect(m_undoIconOnlyAction, &QAction::triggered, this, &MainWin::undo);

	m_redoIconOnlyAction = new QAction(m_redoAction->icon(), QString());
	connect(m_redoIconOnlyAction, &QAction::triggered, this, &MainWin::redo);
#endif
	// TODO: more menus
	//  Appearance
	// Analysis: see WorksheetView.cpp
	// Drawing
	// Script

	// Windows
	m_closeWindowAction = new QAction(i18n("&Close"), this);
	collection->setDefaultShortcut(m_closeWindowAction, QKeySequence::Close);
	m_closeWindowAction->setStatusTip(i18n("Close the active window"));
	collection->addAction(QLatin1String("close window"), m_closeWindowAction);

	m_closeAllWindowsAction = new QAction(i18n("Close &All"), this);
	m_closeAllWindowsAction->setStatusTip(i18n("Close all the windows"));
	collection->addAction(QLatin1String("close all windows"), m_closeAllWindowsAction);

	m_nextWindowAction = new QAction(QIcon::fromTheme(QLatin1String("go-next-view")), i18n("Ne&xt"), this);
	collection->setDefaultShortcut(m_nextWindowAction, QKeySequence::NextChild);
	m_nextWindowAction->setStatusTip(i18n("Move the focus to the next window"));
	collection->addAction(QLatin1String("next window"), m_nextWindowAction);

	m_prevWindowAction = new QAction(QIcon::fromTheme(QLatin1String("go-previous-view")), i18n("Pre&vious"), this);
	collection->setDefaultShortcut(m_prevWindowAction, QKeySequence::PreviousChild);
	m_prevWindowAction->setStatusTip(i18n("Move the focus to the previous window"));
	collection->addAction(QLatin1String("previous window"), m_prevWindowAction);

	// Actions for window visibility
	auto* windowVisibilityActions = new QActionGroup(this);
	windowVisibilityActions->setExclusive(true);

	m_visibilityFolderAction = new QAction(QIcon::fromTheme(QLatin1String("folder")), i18n("Current &Folder Only"), windowVisibilityActions);
	m_visibilityFolderAction->setCheckable(true);
	m_visibilityFolderAction->setData(static_cast<int>(Project::DockVisibility::folderOnly));

	m_visibilitySubfolderAction =
		new QAction(QIcon::fromTheme(QLatin1String("folder-documents")), i18n("Current Folder and &Subfolders"), windowVisibilityActions);
	m_visibilitySubfolderAction->setCheckable(true);
	m_visibilitySubfolderAction->setData(static_cast<int>(Project::DockVisibility::folderAndSubfolders));

	m_visibilityAllAction = new QAction(i18n("&All"), windowVisibilityActions);
	m_visibilityAllAction->setCheckable(true);
	m_visibilityAllAction->setData(static_cast<int>(Project::DockVisibility::allDocks));

	connect(windowVisibilityActions, &QActionGroup::triggered, m_mainWindow, &MainWin::setDockVisibility);

	// show/hide the status and menu bars
	//  KMainWindow should provide a menu that allows showing/hiding of the statusbar via showStatusbar()
	//  see https://api.kde.org/frameworks/kxmlgui/html/classKXmlGuiWindow.html#a3d7371171cafabe30cb3bb7355fdfed1
	// KXMLGUI framework automatically stores "Disabled" for the key "StatusBar"
	KConfigGroup groupMain = Settings::group(QStringLiteral("MainWindow"));
	const QString& str = groupMain.readEntry(QLatin1String("StatusBar"), "");
	bool statusBarDisabled = (str == QLatin1String("Disabled"));
	DEBUG(Q_FUNC_INFO << ", statusBar enabled in config: " << !statusBarDisabled)
	m_mainWindow->createStandardStatusBarAction();
	m_statusBarAction = KStandardAction::showStatusbar(this, &ActionsManager::toggleStatusBar, collection);
	m_statusBarAction->setChecked(!statusBarDisabled);
	m_mainWindow->statusBar()->setEnabled(!statusBarDisabled); // setVisible() does not work

	KStandardAction::showMenubar(this, &ActionsManager::toggleMenuBar, collection);

	// show/hide the memory usage widget
	m_memoryInfoAction = new QAction(i18n("Show Memory Usage"), this);
	m_memoryInfoAction->setCheckable(true);
	connect(m_memoryInfoAction, &QAction::triggered, this, &ActionsManager::toggleMemoryInfo);

	// Actions for hiding/showing the dock widgets
	auto* docksActions = new QActionGroup(this);
	docksActions->setExclusive(false);

	m_projectExplorerDockAction = new QAction(QIcon::fromTheme(QLatin1String("view-list-tree")), i18n("Project Explorer"), docksActions);
	m_projectExplorerDockAction->setCheckable(true);
	m_projectExplorerDockAction->setChecked(true);
	collection->addAction(QLatin1String("toggle_project_explorer_dock"), m_projectExplorerDockAction);

	m_propertiesDockAction = new QAction(QIcon::fromTheme(QLatin1String("view-list-details")), i18n("Properties Explorer"), docksActions);
	m_propertiesDockAction->setCheckable(true);
	m_propertiesDockAction->setChecked(true);
	collection->addAction(QLatin1String("toggle_properties_explorer_dock"), m_propertiesDockAction);

	m_worksheetPreviewAction = new QAction(QIcon::fromTheme(QLatin1String("view-preview")), i18n("Worksheet Preview"), docksActions);
	m_worksheetPreviewAction->setCheckable(true);
	m_worksheetPreviewAction->setChecked(true);
	collection->addAction(QLatin1String("toggle_worksheet_preview_dock"), m_worksheetPreviewAction);

	connect(docksActions, &QActionGroup::triggered, this, &ActionsManager::toggleDockWidget);

	// global search
	m_searchAction = new QAction(collection);
	m_searchAction->setShortcut(QKeySequence::Find);
	// connect(m_searchAction, &QAction::triggered, this, [=]() {
	// 	if (m_project) {
	// 		if (!m_projectExplorerDock->isVisible()) {
	// 			m_projectExplorerDockAction->setChecked(true);
	// 			toggleDockWidget(m_projectExplorerDockAction);
	// 		}
	// 		m_projectExplorer->search();
	// 	}
	// });
	m_mainWindow->addAction(m_searchAction);

#ifdef HAVE_CANTOR_LIBS
	// configure CAS backends
	m_configureCASAction = new QAction(QIcon::fromTheme(QLatin1String("cantor")), i18n("Configure CAS..."), this);
	m_configureCASAction->setWhatsThis(i18n("Opens the settings for Computer Algebra Systems to modify the available systems or to enable new ones"));
	m_configureCASAction->setMenuRole(QAction::NoRole); // prevent macOS Qt heuristics to select this action for preferences
	collection->addAction(QLatin1String("configure_cas"), m_configureCASAction);
	connect(m_configureCASAction, &QAction::triggered, m_mainWindow, &MainWin::settingsDialog); // TODO: go to the Notebook page in the settings dialog directly
#endif
}

void ActionsManager::initMenus() {
#ifdef HAVE_PURPOSE
	m_shareMenu = new Purpose::Menu(m_mainWindow);
	m_shareMenu->model()->setPluginType(QStringLiteral("Export"));
	connect(m_shareMenu, &Purpose::Menu::finished, this, &ActionsManager::shareActionFinished);
	m_shareAction->setMenu(m_shareMenu);
#endif

	auto* factory = m_mainWindow->factory();

	// add the actions to toggle the status bar and the project and properties explorer widgets to the "View" menu.
	// this menu is created automatically when the default "full screen" action is created in initActions().
	auto* menu = dynamic_cast<QMenu*>(factory->container(QLatin1String("view"), m_mainWindow));
	if (menu) {
		menu->addSeparator();
		menu->addAction(m_projectExplorerDockAction);
		menu->addAction(m_propertiesDockAction);
		menu->addAction(m_worksheetPreviewAction);
	}

	// menu in the main toolbar for adding new aspects
	menu = dynamic_cast<QMenu*>(factory->container(QLatin1String("new"), m_mainWindow));
	if (menu)
		menu->setIcon(QIcon::fromTheme(QLatin1String("window-new")));

	// menu in the project explorer and in the toolbar for adding new aspects
	m_newMenu = new QMenu(i18n("Add New"), m_mainWindow);
	m_newMenu->setIcon(QIcon::fromTheme(QLatin1String("window-new")));
	m_newMenu->addAction(m_newFolderAction);
	m_newMenu->addAction(m_newWorkbookAction);
	m_newMenu->addAction(m_newSpreadsheetAction);
	m_newMenu->addAction(m_newMatrixAction);
	m_newMenu->addAction(m_newWorksheetAction);
	m_newMenu->addAction(m_newNotesAction);
	m_newMenu->addAction(m_newDatapickerAction);
	m_newMenu->addSeparator();
	m_newMenu->addAction(m_newLiveDataSourceAction);

	// import menu
	m_importMenu = new QMenu(m_mainWindow);
	m_importMenu->setIcon(QIcon::fromTheme(QLatin1String("document-import")));
	m_importMenu->addAction(m_importFileAction_2);
	m_importMenu->addAction(m_importSqlAction);
	m_importMenu->addAction(m_importDatasetAction);
	m_importMenu->addAction(m_importKaggleDatasetAction);
	m_importMenu->addSeparator();
	m_importMenu->addAction(m_importLabPlotAction);
#ifdef HAVE_LIBORIGIN
	m_importMenu->addAction(m_importOpjAction);
#endif

	// icon for the menu "import" in the main menu created via the rc file
	menu = qobject_cast<QMenu*>(factory->container(QLatin1String("import"), m_mainWindow));
	menu->setIcon(QIcon::fromTheme(QLatin1String("document-import")));

	// menu subwindow visibility policy
	m_visibilityMenu = new QMenu(i18n("Window Visibility"), m_mainWindow);
	m_visibilityMenu->setIcon(QIcon::fromTheme(QLatin1String("window-duplicate")));
	m_visibilityMenu->addAction(m_visibilityFolderAction);
	m_visibilityMenu->addAction(m_visibilitySubfolderAction);
	m_visibilityMenu->addAction(m_visibilityAllAction);

#ifdef HAVE_FITS
//	m_editMenu->addAction(m_editFitsFileAction);
#endif

	// set the action for the current color scheme checked
	auto group = Settings::group(QStringLiteral("Settings_General"));
	QString schemeName = group.readEntry("ColorScheme");
	// default dark scheme on Windows is not optimal (Breeze dark is better)
	// we can't find out if light or dark mode is used, so we don't switch to Breeze/Breeze dark here
	DEBUG(Q_FUNC_INFO << ", Color scheme = " << STDSTRING(schemeName))
	auto* schemesMenu = KColorSchemeMenu::createMenu(m_mainWindow->m_schemeManager, m_mainWindow);
	schemesMenu->setText(i18n("Color Scheme"));
	schemesMenu->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-color")));
	connect(schemesMenu->menu(), &QMenu::triggered, m_mainWindow, &MainWin::colorSchemeChanged);

	QMenu* settingsMenu = dynamic_cast<QMenu*>(factory->container(QLatin1String("settings"), m_mainWindow));
	if (settingsMenu) {
		auto* action = settingsMenu->insertSeparator(settingsMenu->actions().constFirst());
		settingsMenu->insertMenu(action, schemesMenu->menu());

		// add m_memoryInfoAction after the "Show status bar" action
		auto actions = settingsMenu->actions();
		const int index = actions.indexOf(m_statusBarAction);
		settingsMenu->insertAction(actions.at(index + 1), m_memoryInfoAction);
	}

	// Cantor backends to menu and context menu
#ifdef HAVE_CANTOR_LIBS
	auto backendNames = Cantor::Backend::listAvailableBackends();
#if !defined(NDEBUG) || defined(Q_OS_WIN) || defined(Q_OS_MACOS)
	WARN(Q_FUNC_INFO << ", " << backendNames.count() << " Cantor backends available:")
	for (const auto& b : backendNames)
		WARN("Backend: " << STDSTRING(b))
#endif

	// sub-menu shown in the main toolbar
	m_newNotebookMenu = new QMenu(m_mainWindow);

	if (!backendNames.isEmpty()) {
		// sub-menu shown in the main menu bar
		auto* menu = dynamic_cast<QMenu*>(factory->container(QLatin1String("new_notebook"), m_mainWindow));
		if (menu) {
			menu->setIcon(QIcon::fromTheme(QLatin1String("cantor")));
			m_newMenu->addSeparator();
			m_newMenu->addMenu(menu);
			updateNotebookActions();
		}
	}
#else
	delete this->guiFactory()->container(QStringLiteral("notebook"), m_mainWindow);
	delete this->guiFactory()->container(QStringLiteral("new_notebook"), m_mainWindow);
	delete this->guiFactory()->container(QStringLiteral("notebook_toolbar"), m_mainWindow);
#endif
}

void MainWin::colorSchemeChanged(QAction* action) {
	// save the selected color scheme
	auto group = Settings::group(QStringLiteral("Settings_General"));
	const auto& schemeName = KLocalizedString::removeAcceleratorMarker(action->text());
	group.writeEntry(QStringLiteral("ColorScheme"), schemeName);
	group.sync();
}

/*!
 * updates the state of actions, menus and toolbars (enabled or disabled)
 * on project changes (project closes and opens)
 */
void ActionsManager::updateGUIOnProjectChanges() {
	if (m_mainWindow->m_closing)
		return;

	auto* factory = m_mainWindow->guiFactory();
	if (!m_mainWindow->m_dockManagerContent || !m_mainWindow->m_dockManagerContent->focusedDockWidget()) {
		factory->container(QLatin1String("spreadsheet"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("matrix"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("worksheet"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("datapicker"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("spreadsheet_toolbar"), m_mainWindow)->hide();
		factory->container(QLatin1String("worksheet_toolbar"), m_mainWindow)->hide();
		factory->container(QLatin1String("cartesian_plot_toolbar"), m_mainWindow)->hide();
		factory->container(QLatin1String("datapicker_toolbar"), m_mainWindow)->hide();
#ifdef HAVE_CANTOR_LIBS
		factory->container(QLatin1String("notebook"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("notebook_toolbar"), m_mainWindow)->hide();
#endif
	}

	m_mainWindow->updateTitleBar();

	// undo/redo actions are disabled in both cases - when the project is closed or opened
	m_undoAction->setEnabled(false);
	m_redoAction->setEnabled(false);
}


bool hasAction(const QList<QAction*>& actions) {
	for (const auto* action : actions) {
		if (!action->isSeparator())
			return true;
	}
	return false;
}

/*
 * updates the state of actions, menus and toolbars (enabled or disabled)
 * depending on the currently active window (worksheet or spreadsheet).
 */
void ActionsManager::updateGUI() {
	if (!m_mainWindow->m_project || m_mainWindow->m_project->isLoading())
		return;

	if (m_mainWindow->m_closing || m_mainWindow->m_projectClosing)
		return;

#ifdef HAVE_TOUCHBAR
	// reset the touchbar
	m_touchBar->clear();
	m_touchBar->addAction(m_undoIconOnlyAction);
	m_touchBar->addAction(m_redoIconOnlyAction);
	m_touchBar->addSeparator();
#endif

	auto* factory = m_mainWindow->guiFactory();
	if (!m_mainWindow->m_dockManagerContent || !m_mainWindow->m_dockManagerContent->focusedDockWidget()) {
		factory->container(QLatin1String("spreadsheet"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("matrix"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("worksheet"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("datapicker"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("spreadsheet_toolbar"), m_mainWindow)->hide();
		factory->container(QLatin1String("worksheet_toolbar"), m_mainWindow)->hide();
		factory->container(QLatin1String("cartesian_plot_toolbar"), m_mainWindow)->hide();
		factory->container(QLatin1String("datapicker_toolbar"), m_mainWindow)->hide();
#ifdef HAVE_CANTOR_LIBS
		factory->container(QLatin1String("notebook"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("notebook_toolbar"), m_mainWindow)->hide();
#endif
		m_printAction->setEnabled(false);
		m_printPreviewAction->setEnabled(false);
		m_exportAction->setEnabled(false);
		return;
	} else {
		m_printAction->setEnabled(true);
		m_printPreviewAction->setEnabled(true);
		m_exportAction->setEnabled(true);
	}

#ifdef HAVE_TOUCHBAR
	if (dynamic_cast<Folder*>(m_currentAspect)) {
		m_touchBar->addAction(m_newWorksheetAction);
		m_touchBar->addAction(m_newSpreadsheetAction);
		m_touchBar->addAction(m_newMatrixAction);
	}
#endif

	Q_ASSERT(m_mainWindow->m_currentAspect);

	// Handle the Worksheet-object
	const auto* w = dynamic_cast<Worksheet*>(m_mainWindow->m_currentAspect);
	if (!w)
		w = dynamic_cast<Worksheet*>(m_mainWindow->m_currentAspect->parent(AspectType::Worksheet));

	if (w) {
		bool update = false;
		if (w != m_mainWindow->m_lastWorksheet) {
			m_mainWindow->m_lastWorksheet = w;
			update = true;
		}

		// populate worksheet menu
		auto* view = qobject_cast<WorksheetView*>(w->view());
		auto* menu = qobject_cast<QMenu*>(factory->container(QLatin1String("worksheet"), m_mainWindow));
		if (update) {
			menu->clear();
			view->createContextMenu(menu);
		} else if (!hasAction(menu->actions()))
			view->createContextMenu(menu);
		menu->setEnabled(true);

		// populate worksheet-toolbar
		auto* toolbar = qobject_cast<QToolBar*>(factory->container(QLatin1String("worksheet_toolbar"), m_mainWindow));
		if (update) { // update because the aspect has changed
			toolbar->clear();
			view->fillToolBar(toolbar);
		} else if (!hasAction(toolbar->actions())) // update because the view was closed and the actions deleted
			view->fillToolBar(toolbar);
		toolbar->setVisible(true);
		toolbar->setEnabled(true);

		// populate the toolbar for cartesian plots
		toolbar = qobject_cast<QToolBar*>(factory->container(QLatin1String("cartesian_plot_toolbar"), m_mainWindow));
		if (update) {
			toolbar->clear();
			view->fillCartesianPlotToolBar(toolbar);
		} else if (!hasAction(toolbar->actions()))
			view->fillCartesianPlotToolBar(toolbar);
		toolbar->setVisible(true);
		toolbar->setEnabled(true);

		// populate the touchbar on Mac
#ifdef HAVE_TOUCHBAR
		view->fillTouchBar(m_touchBar);
#endif
		// hide the spreadsheet toolbar
		factory->container(QLatin1String("spreadsheet_toolbar"), m_mainWindow)->setVisible(false);
	} else {
		factory->container(QLatin1String("worksheet"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("worksheet_toolbar"), m_mainWindow)->setVisible(false);
		factory->container(QLatin1String("worksheet_toolbar"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("cartesian_plot_toolbar"), m_mainWindow)->setEnabled(false);
	}

	// Handle the Spreadsheet-object
	const auto* spreadsheet = m_mainWindow->activeSpreadsheet();
	if (spreadsheet) {
		bool update = false;
		if (spreadsheet != m_mainWindow->m_lastSpreadsheet) {
			update = true;
			m_mainWindow->m_lastSpreadsheet = spreadsheet;
		}

		// populate spreadsheet-menu
		auto* view = qobject_cast<SpreadsheetView*>(spreadsheet->view());
		auto* menu = qobject_cast<QMenu*>(factory->container(QLatin1String("spreadsheet"), m_mainWindow));
		if (update) {
			menu->clear();
			view->createContextMenu(menu);
		} else if (!hasAction(menu->actions()))
			view->createContextMenu(menu);
		menu->setEnabled(true);

		// populate spreadsheet-toolbar
		auto* toolbar = qobject_cast<QToolBar*>(factory->container(QLatin1String("spreadsheet_toolbar"), m_mainWindow));
		if (update) {
			toolbar->clear();
			view->fillToolBar(toolbar);
		} else if (!hasAction(toolbar->actions()))
			view->fillToolBar(toolbar);

		toolbar->setVisible(true);
		toolbar->setEnabled(true);

		// populate the touchbar on Mac
#ifdef HAVE_TOUCHBAR
		m_touchBar->addAction(m_importFileAction);
		view->fillTouchBar(m_touchBar);
#endif

		// spreadsheet has it's own search, unregister the shortcut for the global search here
		m_searchAction->setShortcut(QKeySequence());
	} else {
		factory->container(QLatin1String("spreadsheet"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("spreadsheet_toolbar"), m_mainWindow)->setVisible(false);
		m_searchAction->setShortcut(QKeySequence::Find);
	}

	// Handle the Matrix-object
	const auto* matrix = dynamic_cast<Matrix*>(m_mainWindow->m_currentAspect);
	if (!matrix)
		matrix = dynamic_cast<Matrix*>(m_mainWindow->m_currentAspect->parent(AspectType::Matrix));
	if (matrix) {
		// populate matrix-menu
		auto* view = qobject_cast<MatrixView*>(matrix->view());
		auto* menu = qobject_cast<QMenu*>(factory->container(QLatin1String("matrix"), m_mainWindow));
		menu->clear();
		view->createContextMenu(menu);
		menu->setEnabled(true);

		// populate the touchbar on Mac
#ifdef HAVE_TOUCHBAR
		m_touchBar->addAction(m_importFileAction);
		// view->fillTouchBar(m_touchBar);
#endif
	} else
		factory->container(QLatin1String("matrix"), m_mainWindow)->setEnabled(false);

#ifdef HAVE_CANTOR_LIBS
	const auto* notebook = dynamic_cast<Notebook*>(m_mainWindow->m_currentAspect);
	if (!notebook)
		notebook = dynamic_cast<Notebook*>(m_mainWindow->m_currentAspect->parent(AspectType::Notebook));
	if (notebook) {
		auto* view = qobject_cast<NotebookView*>(notebook->view());
		auto* menu = qobject_cast<QMenu*>(factory->container(QLatin1String("notebook"), m_mainWindow));
		menu->clear();
		view->createContextMenu(menu);
		menu->setEnabled(true);

		auto* toolbar = qobject_cast<QToolBar*>(factory->container(QLatin1String("notebook_toolbar"), m_mainWindow));
		toolbar->setVisible(true);
		toolbar->clear();
		view->fillToolBar(toolbar);
	} else {
		// no Cantor worksheet selected -> deactivate Cantor worksheet related menu and toolbar
		factory->container(QLatin1String("notebook"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("notebook_toolbar"), m_mainWindow)->setVisible(false);
	}
#endif

	const auto* datapicker = dynamic_cast<Datapicker*>(m_mainWindow->m_currentAspect);
	if (!datapicker)
		datapicker = dynamic_cast<Datapicker*>(m_mainWindow->m_currentAspect->parent(AspectType::Datapicker));
	if (!datapicker) {
		if (m_mainWindow->m_currentAspect && m_mainWindow->m_currentAspect->type() == AspectType::DatapickerCurve)
			datapicker = dynamic_cast<Datapicker*>(m_mainWindow->m_currentAspect->parentAspect());
	}

	if (datapicker) {
		// populate datapicker-menu
		auto* view = qobject_cast<DatapickerView*>(datapicker->view());
		auto* menu = qobject_cast<QMenu*>(factory->container(QLatin1String("datapicker"), m_mainWindow));
		menu->clear();
		view->createContextMenu(menu);
		menu->setEnabled(true);

		// populate spreadsheet-toolbar
		auto* toolbar = qobject_cast<QToolBar*>(factory->container(QLatin1String("datapicker_toolbar"), m_mainWindow));
		toolbar->clear();
		view->fillToolBar(toolbar);
		toolbar->setVisible(true);
	} else {
		factory->container(QLatin1String("datapicker"), m_mainWindow)->setEnabled(false);
		factory->container(QLatin1String("datapicker_toolbar"), m_mainWindow)->setVisible(false);
	}
}

#ifdef HAVE_CANTOR_LIBS
void ActionsManager::updateNotebookActions() {
	auto* menu = static_cast<QMenu*>(m_mainWindow->factory()->container(QLatin1String("new_notebook"), m_mainWindow));
	m_mainWindow->unplugActionList(QLatin1String("backends_list"));
	QList<QAction*> newBackendActions;
	menu->clear();
	for (auto* backend : Cantor::Backend::availableBackends()) {
		if (!backend->isEnabled())
			continue;

		auto* action = new QAction(QIcon::fromTheme(backend->icon()), backend->name(), m_mainWindow);
		action->setData(backend->name());
		action->setWhatsThis(i18n("Creates a new %1 notebook", backend->name()));
		m_mainWindow->actionCollection()->addAction(QLatin1String("notebook_") + backend->name(), action);
		connect(action, &QAction::triggered, m_mainWindow, &MainWin::newNotebook);
		newBackendActions << action;
		menu->addAction(action);
		m_newNotebookMenu->addAction(action);
	}

	m_mainWindow->plugActionList(QLatin1String("backends_list"), newBackendActions);

	menu->addSeparator();
	menu->addAction(m_configureCASAction);

	m_newNotebookMenu->addSeparator();
	m_newNotebookMenu->addAction(m_configureCASAction);
}
#endif

#ifdef HAVE_PURPOSE
void ActionsManager::fillShareMenu() {
	if (!m_shareMenu)
		return;

	m_shareMenu->clear(); // clear the menu, it will be refilled with the new file URL below
	QMimeType mime;
	m_shareMenu->model()->setInputData(
		QJsonObject{{QStringLiteral("mimeType"), mime.name()}, {QStringLiteral("urls"), QJsonArray{QUrl::fromLocalFile(m_mainWindow->m_project->fileName()).toString()}}});
	m_shareMenu->reload();
}

void ActionsManager::shareActionFinished(const QJsonObject& output, int error, const QString& message) {
	if (error)
		KMessageBox::error(m_mainWindow, i18n("There was a problem sharing the project: %1", message), i18n("Share"));
	else {
		const QString url = output[QStringLiteral("url")].toString();
		if (url.isEmpty())
			m_mainWindow->statusBar()->showMessage(i18n("Project shared successfully"));
		else
			KMessageBox::information(m_mainWindow->widget(),
									 i18n("You can find the shared project at: <a href=\"%1\">%1</a>", url),
									 i18n("Share"),
									 QString(),
									 KMessageBox::Notify | KMessageBox::AllowLink);
	}
}
#endif

void ActionsManager::toggleStatusBar(bool checked) {
	m_mainWindow->statusBar()->setVisible(checked); // show/hide statusbar
	m_mainWindow->statusBar()->setEnabled(checked);
	// enabled/disable memory info menu with statusbar
	m_memoryInfoAction->setEnabled(checked);
}

void ActionsManager::toggleMemoryInfo() {
	if (m_mainWindow->m_memoryInfoWidget) {
		m_mainWindow->statusBar()->removeWidget(m_mainWindow->m_memoryInfoWidget);
		delete m_mainWindow->m_memoryInfoWidget;
		m_mainWindow->m_memoryInfoWidget = nullptr;
	} else {
		m_mainWindow->m_memoryInfoWidget = new MemoryWidget(m_mainWindow->statusBar());
		m_mainWindow->statusBar()->addPermanentWidget(m_mainWindow->m_memoryInfoWidget);
	}
}

void ActionsManager::toggleMenuBar(bool checked) {
	m_mainWindow->menuBar()->setVisible(checked);
}

void ActionsManager::toggleFullScreen(bool t) {
	m_fullScreenAction->setFullScreen(m_mainWindow, t);
}

void ActionsManager::toggleDockWidget(QAction* action) {
	if (action->objectName() == QLatin1String("toggle_project_explorer_dock")) {
		if (m_mainWindow->m_projectExplorerDock->isVisible())
			m_mainWindow->m_projectExplorerDock->toggleView(false);
		else
			m_mainWindow->m_projectExplorerDock->toggleView(true);
	} else if (action->objectName() == QLatin1String("toggle_properties_explorer_dock")) {
		if (m_mainWindow->m_propertiesDock->isVisible())
			m_mainWindow->m_propertiesDock->toggleView(false);
		else
			m_mainWindow->m_propertiesDock->toggleView(true);
	} else if (action->objectName() == QLatin1String("toggle_worksheet_preview_dock"))
		m_mainWindow->m_worksheetPreviewDock->toggleView(!m_mainWindow->m_worksheetPreviewDock->isVisible());
}
