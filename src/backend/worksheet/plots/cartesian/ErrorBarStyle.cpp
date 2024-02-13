/*
	File                 : ErrorBarStyle.cpp
	Project              : LabPlot
	Description          : ErrorBarStyle
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2024 Alexander Semke <alexander.semke@web.de>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

/*!
  \class ErrorBarStyle
  \brief This class contains the background properties of worksheet elements like worksheet background,
  plot background, the area filling in ErrorBarStyle, Histogram, etc.

  \ingroup worksheet
*/

#include "ErrorBarStyle.h"
#include "ErrorBarStylePrivate.h"
#include "backend/lib/XmlStreamReader.h"
#include "backend/lib/commandtemplates.h"
#include "backend/worksheet/Line.h"
#include "backend/worksheet/Worksheet.h"

#include <KConfigGroup>
#include <KLocalizedString>

#include <QPainter>
#include <QPainterPath>

ErrorBarStyle::ErrorBarStyle(const QString& name)
	: AbstractAspect(name, AspectType::AbstractAspect)
	, d_ptr(new ErrorBarStylePrivate(this)) {
}

ErrorBarStyle::~ErrorBarStyle() {
	delete d_ptr;
}

void ErrorBarStyle::init(const KConfigGroup& group) {
	Q_D(ErrorBarStyle);
	d->type = (Type)group.readEntry(QStringLiteral("ErrorBarsType"), static_cast<int>(Type::Simple));
	d->capSize = group.readEntry(QStringLiteral("ErrorBarsCapSize"), Worksheet::convertToSceneUnits(10, Worksheet::Unit::Point));

	d->line = new Line(QString());
	d->line->setHidden(true);
	d->line->setCreateXmlElement(false);
	addChild(d->line);
	d->line->init(group);
	connect(d->line, &Line::updatePixmapRequested, this, &ErrorBarStyle::updatePixmapRequested);
	connect(d->line, &Line::updateRequested, this, &ErrorBarStyle::updateRequested);
}

void ErrorBarStyle::draw(QPainter* painter, const QPainterPath& path) {
	Q_D(const ErrorBarStyle);
	painter->setOpacity(d->line->opacity());
	painter->setPen(d->line->pen());
	painter->setBrush(Qt::NoBrush);
	painter->drawPath(path);
}

// ##############################################################################
// ##########################  getter methods  ##################################
// ##############################################################################
BASIC_SHARED_D_READER_IMPL(ErrorBarStyle, ErrorBarStyle::Type, type, type)
BASIC_SHARED_D_READER_IMPL(ErrorBarStyle, double, capSize, capSize)
Line* ErrorBarStyle::line() const {
	Q_D(const ErrorBarStyle);
	return d->line;
}

// ##############################################################################
// #################  setter methods and undo commands ##########################
// ##############################################################################
STD_SETTER_CMD_IMPL_F_S(ErrorBarStyle, SetCapSize, double, capSize, update)
void ErrorBarStyle::setCapSize(qreal size) {
	Q_D(ErrorBarStyle);
	if (size != d->capSize)
		exec(new ErrorBarStyleSetCapSizeCmd(d, size, ki18n("%1: set error bar cap size")));
}

STD_SETTER_CMD_IMPL_F_S(ErrorBarStyle, SetType, ErrorBarStyle::Type, type, update)
void ErrorBarStyle::setType(ErrorBarStyle::Type type) {
	Q_D(ErrorBarStyle);
	if (type != d->type)
		exec(new ErrorBarStyleSetTypeCmd(d, type, ki18n("%1: error bar style type changed")));
}

// ##############################################################################
// ####################### Private implementation ###############################
// ##############################################################################
ErrorBarStylePrivate::ErrorBarStylePrivate(ErrorBarStyle* owner)
	: q(owner) {
}

QString ErrorBarStylePrivate::name() const {
	return q->parentAspect()->name();
}

void ErrorBarStylePrivate::update() {
	Q_EMIT q->updateRequested();
}

void ErrorBarStylePrivate::updatePixmap() {
	Q_EMIT q->updatePixmapRequested();
}

// ##############################################################################
// ##################  Serialization/Deserialization  ###########################
// ##############################################################################
//! Save as XML
void ErrorBarStyle::save(QXmlStreamWriter* writer) const {
	Q_D(const ErrorBarStyle);
	writer->writeAttribute(QStringLiteral("type"), QString::number(static_cast<int>(d->type)));
	writer->writeAttribute(QStringLiteral("capSize"), QString::number(d->capSize));
	d->line->save(writer);
}

//! Load from XML
bool ErrorBarStyle::load(XmlStreamReader* reader, bool preview) {
	if (preview)
		return true;

	Q_D(ErrorBarStyle);
	QString str;
	auto attribs = reader->attributes();

	READ_INT_VALUE("type", type, ErrorBarStyle::Type);
	READ_DOUBLE_VALUE("capSize", capSize);
	d->line->load(reader, preview);
	return true;
}

// ##############################################################################
// #########################  Theme management ##################################
// ##############################################################################
void ErrorBarStyle::loadThemeConfig(const KConfigGroup& group) {
	Q_D(ErrorBarStyle);
	d->line->loadThemeConfig(group);
}

void ErrorBarStyle::loadThemeConfig(const KConfigGroup& group, const QColor& themeColor) {
	Q_D(ErrorBarStyle);
	d->line->loadThemeConfig(group, themeColor);
}

void ErrorBarStyle::saveThemeConfig(KConfigGroup& group) const {
	Q_D(const ErrorBarStyle);
	d->line->saveThemeConfig(group);
}
