/***************************************************************************
    File                 : WorksheetDock.cpp
    Project              : LabPlot
    --------------------------------------------------------------------
    Copyright            : (C) 2010 by Alexander Semke (alexander.semke*web.de)
    Copyright            : (C) 2012 by Stefan Gerlach (stefan.gerlach*uni-konstanz.de)
    							(use @ for *)
    Description          : widget for worksheet properties
                           
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

#include "WorksheetDock.h"
#include "worksheet/Worksheet.h"
#include "worksheet/plots/PlotArea.h"
#include <QTimer>
#include <QPrinter>
#include <KUrlCompletion>
#include <QFileDialog>

#include <kdebug.h>
#include <KConfig>

// a couple of standard sizes in mm, taken from qprinter.cpp
static const float qt_paperSizes[][2] = {
    {210, 297}, // A4
    {176, 250}, // B5
    {215.9f, 279.4f}, // Letter
    {215.9f, 355.6f}, // Legal
    {190.5f, 254}, // Executive
    {841, 1189}, // A0
    {594, 841}, // A1
    {420, 594}, // A2
    {297, 420}, // A3
    {148, 210}, // A5
    {105, 148}, // A6
    {74, 105}, // A7
    {52, 74}, // A8
    {37, 52}, // A8
    {1000, 1414}, // B0
    {707, 1000}, // B1
    {31, 44}, // B10
    {500, 707}, // B2
    {353, 500}, // B3
    {250, 353}, // B4
    {125, 176}, // B6
    {88, 125}, // B7
    {62, 88}, // B8
    {33, 62}, // B9
    {163, 229}, // C5E
    {105, 241}, // US Common
    {110, 220}, // DLE
    {210, 330}, // Folio
    {431.8f, 279.4f}, // Ledger
    {279.4f, 431.8f} // Tabloid
};

/*!
  \class GuiObserver
  \brief  Provides a widget for editing the properties of the worksheets currently selected in the project explorer.

  \ingroup kdefrontend
*/

WorksheetDock::WorksheetDock(QWidget *parent): QWidget(parent){
	ui.setupUi(this);

	ui.pbLoad->setIcon(KIcon("document-open"));
	ui.pbSave->setIcon(KIcon("document-save"));
	ui.pbSaveDefault->setIcon(KIcon("document-save-as"));
	ui.pbCopy->setIcon(KIcon("edit-copy"));
	ui.pbPaste->setIcon(KIcon("edit-paste"));

	//Background-tab
	ui.cbBackgroundColorStyle->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	ui.kleBackgroundFileName->setClearButtonShown(true);
	ui.bOpen->setIcon( KIcon("document-open") );

	KUrlCompletion *comp = new KUrlCompletion();
    ui.kleBackgroundFileName->setCompletionObject(comp);

	  //adjust layouts in the tabs
	QGridLayout* layout;
	for (int i=0; i<ui.tabWidget->count(); ++i){
		layout=static_cast<QGridLayout*>(ui.tabWidget->widget(i)->layout());
		if (!layout)
			continue;

		layout->setContentsMargins(2,2,2,2);
		layout->setHorizontalSpacing(2);
		layout->setVerticalSpacing(2);
	}

	//SLOTs
	
	//General
	connect( ui.leName, SIGNAL(returnPressed()), this, SLOT(nameChanged()) );
	connect( ui.leComment, SIGNAL(returnPressed()), this, SLOT(commentChanged()) );
	connect( ui.cbSize, SIGNAL(currentIndexChanged(int)), this, SLOT(sizeChanged(int)) );
	connect( ui.sbWidth, SIGNAL(valueChanged(double)), this, SLOT(sizeChanged()) );
	connect( ui.sbHeight, SIGNAL(valueChanged(double)), this, SLOT(sizeChanged()) );
	connect( ui.cbOrientation, SIGNAL(currentIndexChanged(int)), this, SLOT(orientationChanged(int)) );
	
	//Background
	connect( ui.cbBackgroundType, SIGNAL(currentIndexChanged(int)), this, SLOT(backgroundTypeChanged(int)) );
	connect( ui.cbBackgroundColorStyle, SIGNAL(currentIndexChanged(int)), this, SLOT(backgroundColorStyleChanged(int)) );
	connect( ui.cbBackgroundImageStyle, SIGNAL(currentIndexChanged(int)), this, SLOT(backgroundImageStyleChanged(int)) );
	connect(ui.bOpen, SIGNAL(clicked(bool)), this, SLOT(selectFile()));
	connect( ui.kleBackgroundFileName, SIGNAL(returnPressed()), this, SLOT(fileNameChanged()) );
	connect( ui.kleBackgroundFileName, SIGNAL(clearButtonClicked()), this, SLOT(fileNameChanged()) );
	connect( ui.kcbBackgroundFirstColor, SIGNAL(changed (const QColor &)), this, SLOT(backgroundFirstColorChanged(const QColor&)) );
	connect( ui.kcbBackgroundSecondColor, SIGNAL(changed (const QColor &)), this, SLOT(backgroundSecondColorChanged(const QColor&)) );
	connect( ui.sbBackgroundOpacity, SIGNAL(valueChanged(int)), this, SLOT(opacityChanged(int)) );
	
	connect( ui.pbLoad, SIGNAL(clicked()), this, SLOT(loadSettings()));
	connect( ui.pbSave, SIGNAL(clicked()), this, SLOT(saveSettings()));
	connect( ui.pbSaveDefault, SIGNAL(clicked()), this, SLOT(saveDefaults()));

	this->retranslateUi();
}

void WorksheetDock::setWorksheets(QList<Worksheet*> list){
	m_initializing = true;
	m_worksheetList = list;

	Worksheet* worksheet=list.first();
  
  //if there are more then one worksheets in the list, disable the name and comment field in the tab "general"
  if (list.size()==1){
	ui.lName->setEnabled(true);
	ui.leName->setEnabled(true);
	ui.lComment->setEnabled(true);
	ui.leComment->setEnabled(true);

	ui.leName->setText(worksheet->name());
	ui.leComment->setText(worksheet->comment());
  }else{
	ui.lName->setEnabled(false);
	ui.leName->setEnabled(false);
	ui.lComment->setEnabled(false);
	ui.leComment->setEnabled(false);

	ui.leName->setText("");
	ui.leComment->setText("");
  }
  
	//show the properties of the first worksheet
  
 	//General-tab
 	float w=Worksheet::convertFromSceneUnits(worksheet->pageRect().width(), Worksheet::Centimeter);
	float h=Worksheet::convertFromSceneUnits(worksheet->pageRect().height(), Worksheet::Centimeter);
	ui.sbWidth->setValue(w);
	ui.sbHeight->setValue(h);
	
	updatePaperSize();
	
	//Background-tab
	ui.cbBackgroundColorStyle->setCurrentIndex( worksheet->backgroundColorStyle() );
	ui.cbBackgroundImageStyle->setCurrentIndex( worksheet->backgroundImageStyle() );
	ui.kleBackgroundFileName->setText( worksheet->backgroundFileName() );
	ui.kcbBackgroundFirstColor->setColor( worksheet->backgroundFirstColor() );
	ui.kcbBackgroundSecondColor->setColor( worksheet->backgroundSecondColor() );
	ui.sbBackgroundOpacity->setValue(worksheet->backgroundOpacity()*100 );
	// this at last since others emmit backgroundColorStyleChanges
	// and enable SecondColor button, etc.!
	ui.cbBackgroundType->setCurrentIndex(worksheet->backgroundType() );

	m_initializing = false;
}

// update Size and Orientation checkbox when width/height changes
void WorksheetDock::updatePaperSize() {
	//Check, whether the size is one of the QPrinter::PaperSize
	int i=0;
	int w=ui.sbWidth->value();
	int h=ui.sbHeight->value();

	//check the portrait-orientation first
	while ( !(w==qt_paperSizes[i][0] && h==qt_paperSizes[i][1]) && i<30 ){
		i++;
	}
	
	if (i!=30){
		ui.cbOrientation->setCurrentIndex(0);  //a QPrinter::PaperSize  in portrait-orientation was found
	}else{
		//check for the landscape-orientation
		i=0;
		while ( !(w==qt_paperSizes[i][1] && h==qt_paperSizes[i][0]) && i<30 ){
			i++;
		}
		
		if (i!=30)
			ui.cbOrientation->setCurrentIndex(1); //a QPrinter::PaperSize  in landscape-orientation was found
	}
	
	//determine the position of the QPrinter::PaperSize in the combobox
	for (int index=0; index<31; index++){
		if (ui.cbSize->itemData(index).toInt() == i){
			ui.cbSize->setCurrentIndex(index);
			break;
		}
	}
}

//************************************************************
//****************** SLOTS ********************************
//************************************************************
void WorksheetDock::retranslateUi(){
	m_initializing = true;
	
	//Geometry
	ui.cbOrientation->addItem(i18n("portrait"));
	ui.cbOrientation->addItem(i18n("landscape"));

    ui.cbSize->addItem(i18n("A0"), QPrinter::A0);
    ui.cbSize->addItem(i18n("A1"), QPrinter::A1);
    ui.cbSize->addItem(i18n("A2"), QPrinter::A2);
    ui.cbSize->addItem(i18n("A3"), QPrinter::A3);
    ui.cbSize->addItem(i18n("A4"), QPrinter::A4);
    ui.cbSize->addItem(i18n("A5"), QPrinter::A5);
    ui.cbSize->addItem(i18n("A6"), QPrinter::A6);
    ui.cbSize->addItem(i18n("A7"), QPrinter::A7);
    ui.cbSize->addItem(i18n("A8"), QPrinter::A8);
    ui.cbSize->addItem(i18n("A9"), QPrinter::A9);
    ui.cbSize->addItem(i18n("B0"), QPrinter::B0);
    ui.cbSize->addItem(i18n("B1"), QPrinter::B1);
    ui.cbSize->addItem(i18n("B2"), QPrinter::B2);
    ui.cbSize->addItem(i18n("B3"), QPrinter::B3);
    ui.cbSize->addItem(i18n("B4"), QPrinter::B4);
    ui.cbSize->addItem(i18n("B5"), QPrinter::B5);
    ui.cbSize->addItem(i18n("B6"), QPrinter::B6);
    ui.cbSize->addItem(i18n("B7"), QPrinter::B7);
    ui.cbSize->addItem(i18n("B8"), QPrinter::B8);
    ui.cbSize->addItem(i18n("B9"), QPrinter::B9);
    ui.cbSize->addItem(i18n("B10"), QPrinter::B10);
    ui.cbSize->addItem(i18n("C5E"), QPrinter::C5E);
    ui.cbSize->addItem(i18n("DLE"), QPrinter::DLE);
    ui.cbSize->addItem(i18n("Executive"), QPrinter::Executive);
    ui.cbSize->addItem(i18n("Folio"), QPrinter::Folio);
    ui.cbSize->addItem(i18n("Ledger"), QPrinter::Ledger);
    ui.cbSize->addItem(i18n("Legal"), QPrinter::Legal);
    ui.cbSize->addItem(i18n("Letter"), QPrinter::Letter);
    ui.cbSize->addItem(i18n("Tabloid"), QPrinter::Tabloid);
    ui.cbSize->addItem(i18n("US Common #10 Envelope"), QPrinter::Comm10E);
	ui.cbSize->addItem(i18n("Custom"), QPrinter::Custom);
	
	ui.sbWidth->setSuffix(i18n("cm"));
	ui.sbHeight->setSuffix(i18n("cm"));
	
	//Background
	ui.cbBackgroundType->addItem(i18n("color"));
	ui.cbBackgroundType->addItem(i18n("image"));

	ui.cbBackgroundColorStyle->addItem(i18n("single color"));
	ui.cbBackgroundColorStyle->addItem(i18n("horizontal linear gradient"));
	ui.cbBackgroundColorStyle->addItem(i18n("vertical linear gradient"));
	ui.cbBackgroundColorStyle->addItem(i18n("diagonal linear gradient (start from top left)"));
	ui.cbBackgroundColorStyle->addItem(i18n("diagonal linear gradient (start from bottom left)"));
	ui.cbBackgroundColorStyle->addItem(i18n("radial gradient"));
// 		//TODO ui.cbBackgroundColorStyle->addItem(i18n("custom gradient"));
// 
	ui.cbBackgroundImageStyle->addItem(i18n("scaled and cropped"));
	ui.cbBackgroundImageStyle->addItem(i18n("scaled"));
	ui.cbBackgroundImageStyle->addItem(i18n("scaled, keep proportions"));
	ui.cbBackgroundImageStyle->addItem(i18n("centered"));
	ui.cbBackgroundImageStyle->addItem(i18n("tiled"));
	ui.cbBackgroundImageStyle->addItem(i18n("center tiled"));

	m_initializing = false;
}


// "General"-tab
void WorksheetDock::nameChanged(){
  if (m_initializing)
	return;
  
  m_worksheetList.first()->setName(ui.leName->text());
}

void WorksheetDock::commentChanged(){
  if (m_initializing)
	return;
  
  m_worksheetList.first()->setComment(ui.leComment->text());
}

void WorksheetDock::sizeChanged(int i){
	int index  = ui.cbSize->itemData(i).toInt();
    
	if (index==QPrinter::Custom){
		ui.sbWidth->setEnabled(true);
		ui.sbHeight->setEnabled(true);
		ui.lOrientation->hide();
		ui.cbOrientation->hide();
		return;
	}else{
		ui.sbWidth->setEnabled(false);
		ui.sbHeight->setEnabled(false);
		ui.lOrientation->show();
		ui.cbOrientation->show();
	}
	
	if (m_initializing)
		return;
  
	float w, h;
	if (ui.cbOrientation->currentIndex() == 0){
		w=qt_paperSizes[index][0];
		h=qt_paperSizes[index][1];
	}else{
		w=qt_paperSizes[index][1];
		h=qt_paperSizes[index][0];
	}
	
	bool scaleContent = ui.chScaleContent->isChecked();
	w = Worksheet::convertToSceneUnits(w, Worksheet::Millimeter);
	h = Worksheet::convertToSceneUnits(h, Worksheet::Millimeter);
	foreach(Worksheet* worksheet, m_worksheetList){
		worksheet->setPageRect(QRect(0,0,w,h), scaleContent);
	}
	
	m_initializing = true;
	ui.sbWidth->setValue(w/10);
	ui.sbHeight->setValue(h/10);
	m_initializing=false;
}

void WorksheetDock::sizeChanged(){
  if (m_initializing)
	return;

  	int w = Worksheet::convertToSceneUnits(ui.sbWidth->value(), Worksheet::Centimeter);
	int h = Worksheet::convertToSceneUnits(ui.sbHeight->value(), Worksheet::Centimeter);
	bool scaleContent = ui.chScaleContent->isChecked();
	foreach(Worksheet* worksheet, m_worksheetList){
		worksheet->setPageRect(QRect(0,0,w,h), scaleContent);
	}
}

void WorksheetDock::orientationChanged(int index){
	Q_UNUSED(index);
  if (m_initializing)
	return;

  this->sizeChanged(ui.cbSize->currentIndex());
}

// "Background"-tab
void WorksheetDock::opacityChanged(int value){
  if (m_initializing)
	return;

	qreal opacity = (float)value/100;
	foreach(Worksheet* worksheet, m_worksheetList){
		worksheet->setBackgroundOpacity(opacity);
	}
}

void WorksheetDock::backgroundTypeChanged(int index){
	PlotArea::BackgroundType type = (PlotArea::BackgroundType)index;

	if (type == PlotArea::Color){
		ui.lBackgroundColorStyle->show();
		ui.cbBackgroundColorStyle->show();
		ui.lBackgroundImageStyle->hide();
		ui.cbBackgroundImageStyle->hide();
		
		ui.lBackgroundFileName->hide();
		ui.kleBackgroundFileName->hide();
		ui.bOpen->hide();

		ui.lBackgroundFirstColor->show();
		ui.kcbBackgroundFirstColor->show();

		PlotArea::BackgroundColorStyle style = 
			(PlotArea::BackgroundColorStyle) ui.cbBackgroundColorStyle->currentIndex();
		if (style == PlotArea::SingleColor){
			ui.lBackgroundSecondColor->hide();
			ui.kcbBackgroundSecondColor->hide();
		}else{
			ui.lBackgroundSecondColor->show();
			ui.kcbBackgroundSecondColor->show();
		}
	}else if(type == PlotArea::Image){
		ui.lBackgroundFirstColor->hide();
		ui.kcbBackgroundFirstColor->hide();
		ui.lBackgroundSecondColor->hide();
		ui.kcbBackgroundSecondColor->hide();

		ui.lBackgroundColorStyle->hide();
		ui.cbBackgroundColorStyle->hide();
		ui.lBackgroundImageStyle->show();
		ui.cbBackgroundImageStyle->show();
		ui.lBackgroundFileName->show();
		ui.kleBackgroundFileName->show();
		ui.bOpen->show();
	}

	if (m_initializing)
		return;
   
	foreach(Worksheet* worksheet, m_worksheetList){
		worksheet->setBackgroundType(type);
  }
}

void WorksheetDock::backgroundColorStyleChanged(int index){
	PlotArea::BackgroundColorStyle style = (PlotArea::BackgroundColorStyle)index;

	if (style == PlotArea::SingleColor){
		ui.lBackgroundSecondColor->hide();
		ui.kcbBackgroundSecondColor->hide();
	}else{
		ui.lBackgroundSecondColor->show();
		ui.kcbBackgroundSecondColor->show();
	}

	if (m_initializing)
		return;
   
	foreach(Worksheet* worksheet, m_worksheetList){
		worksheet->setBackgroundColorStyle(style);
  }  
}

void WorksheetDock::backgroundImageStyleChanged(int index){
	if (m_initializing)
		return;

	PlotArea::BackgroundImageStyle style = (PlotArea::BackgroundImageStyle)index;
	foreach(Worksheet* worksheet, m_worksheetList){
		worksheet->setBackgroundImageStyle(style);
	}  
}

void WorksheetDock::backgroundFirstColorChanged(const QColor& c){
  if (m_initializing)
	return;

	foreach(Worksheet* worksheet, m_worksheetList){
		worksheet->setBackgroundFirstColor(c);
	}
}


void WorksheetDock::backgroundSecondColorChanged(const QColor& c){
  if (m_initializing)
	return;

	foreach(Worksheet* worksheet, m_worksheetList){
		worksheet->setBackgroundFirstColor(c);
	}
}

/*!
	opens a file dialog and lets the user select the image file.
*/
void WorksheetDock::selectFile() {
    QString path=QFileDialog::getOpenFileName(this, i18n("Select the image file"));
    if (path=="")
        return;

    ui.kleBackgroundFileName->setText( path );

	foreach(Worksheet* worksheet, m_worksheetList){
		worksheet->setBackgroundFileName(path);
  }  
}

void WorksheetDock::fileNameChanged(){
	if (m_initializing)
		return;
	
	QString fileName = ui.kleBackgroundFileName->text();
	foreach(Worksheet* worksheet, m_worksheetList){
		worksheet->setBackgroundFileName(fileName);
  } 
}

void WorksheetDock::loadSettings(){
    	QString filename=QFileDialog::getOpenFileName(this, i18n("Select the file to load settings"),
			"LabPlotrc", i18n("KDE resource files (*rc)"));
    	if (filename=="")
        	return;

	KConfig config(filename, KConfig::SimpleConfig);
	KConfigGroup group = config.group( "Worksheet" );
	
	// Geometry
	ui.chScaleContent->setChecked(group.readEntry("ScaleContent", 0));
	Worksheet* worksheet=m_worksheetList.first();
	ui.sbWidth->setValue(Worksheet::convertFromSceneUnits(group.readEntry("Width",worksheet->pageRect().width()), Worksheet::Centimeter));
	ui.sbHeight->setValue(Worksheet::convertFromSceneUnits(group.readEntry("Height",worksheet->pageRect().height()), Worksheet::Centimeter));
	updatePaperSize();

	// Background-tab
	ui.cbBackgroundColorStyle->setCurrentIndex( group.readEntry("BackgroundColorStyle", (int) PlotArea::SingleColor) );
	ui.cbBackgroundImageStyle->setCurrentIndex( group.readEntry("BackgroundImageStyle", (int) PlotArea::Scaled) );
	ui.kleBackgroundFileName->setText( group.readEntry("BackgroundFileName", QString()) );
	ui.kcbBackgroundFirstColor->setColor( group.readEntry("BackgroundFirstColor", QColor(Qt::white)) );
	ui.kcbBackgroundSecondColor->setColor( group.readEntry("BackgroundSecondColor", QColor(Qt::black)) );
	ui.sbBackgroundOpacity->setValue(group.readEntry("BackgroundOpacity", 1.0)*100 );
	// this at last since others emmit backgroundColorStyleChanges
	// and enable SecondColor button, etc.!
	ui.cbBackgroundType->setCurrentIndex( group.readEntry("BackgroundType", (int) PlotArea::Color) );
	
	// Layout
	ui.leTopMargin->setText(group.readEntry("TopMargin",QString()));
	ui.leBottomMargin->setText(group.readEntry("BottomMargin",QString()));
	ui.leLeftMargin->setText(group.readEntry("LeftMargin",QString()));
	ui.leRightMargin->setText(group.readEntry("RightMargin",QString()));
	ui.leHorizontalSpacing->setText(group.readEntry("HorizontalSpacing",QString()));
	ui.leVerticalSpacing->setText(group.readEntry("VerticalSpacing",QString()));
}

void WorksheetDock::saveSettings(){
    	QString filename=QFileDialog::getSaveFileName(this, i18n("Select the file to save settings"),
			"LabPlotrc", i18n("KDE resource files (*rc)"));
    	if (filename=="")
        	return;

	KConfig config(filename, KConfig::SimpleConfig );
	save(config);
	config.sync();
}

void WorksheetDock::saveDefaults(){
	KConfig config;
	save(config);
	config.sync();
}

void WorksheetDock::save(const KConfig& config){
	KConfigGroup group = config.group( "Worksheet" );

	group.writeEntry("ScaleContent",ui.chScaleContent->isChecked());
	group.writeEntry("Width",Worksheet::convertToSceneUnits(ui.sbWidth->value(), Worksheet::Centimeter));
	group.writeEntry("Height",Worksheet::convertToSceneUnits(ui.sbHeight->value(), Worksheet::Centimeter));

	group.writeEntry("BackgroundType",ui.cbBackgroundType->currentIndex());
	group.writeEntry("BackgroundColorStyle", ui.cbBackgroundColorStyle->currentIndex());
	group.writeEntry("BackgroundImageStyle", ui.cbBackgroundImageStyle->currentIndex());
	group.writeEntry("BackgroundFileName", ui.kleBackgroundFileName->text());
	group.writeEntry("BackgroundFirstColor", ui.kcbBackgroundFirstColor->color());
	group.writeEntry("BackgroundSecondColor", ui.kcbBackgroundSecondColor->color());
	group.writeEntry("BackgroundOpacity", ui.sbBackgroundOpacity->value()/100.0);

	group.writeEntry("TopMargin", ui.leTopMargin->text());
	group.writeEntry("BottomMargin", ui.leBottomMargin->text());
	group.writeEntry("LeftMargin", ui.leLeftMargin->text());
	group.writeEntry("RightMargin", ui.leRightMargin->text());
	group.writeEntry("HorizontalSpacing", ui.leHorizontalSpacing->text());
	group.writeEntry("VerticalSpacing", ui.leVerticalSpacing->text());
}

