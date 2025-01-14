/*
	File                 : InfoElement.h
	Project              : LabPlot
	Description          : Marker which can highlight points of curves and
						   show their values
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2020 Martin Marmsoler <martin.marmsoler@gmail.com>
	SPDX-FileCopyrightText: 2020-2022 Alexander Semke <alexander.semke@web.de>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef INFOELEMENT_H
#define INFOELEMENT_H

#include "WorksheetElement.h"
#include "backend/worksheet/TextLabel.h"

class CartesianPlot;
class CustomPoint;
class InfoElementPrivate;
class Line;
class QAction;
class QGraphicsItem;
class QMenu;
class XYCurve;

class InfoElement : public WorksheetElement {
	Q_OBJECT

public:
	InfoElement(const QString& name, CartesianPlot*);
	InfoElement(const QString& name, CartesianPlot*, const XYCurve*, double logicalPos);
	virtual void setParentGraphicsItem(QGraphicsItem* item) override;
	~InfoElement();

	struct MarkerPoints_T {
		MarkerPoints_T() = default;
		MarkerPoints_T(CustomPoint* custompoint, const XYCurve* curve, QString curvePath)
			: customPoint(custompoint)
			, curve(curve)
			, curvePath(curvePath) {
		}
		CustomPoint* customPoint{nullptr};
		const XYCurve* curve{nullptr};
		QString curvePath;
	};

	void save(QXmlStreamWriter*) const override;
	bool load(XmlStreamReader*, bool preview) override;
	void loadThemeConfig(const KConfig&) override;

	TextLabel* title();
	void addCurve(const XYCurve*, CustomPoint* = nullptr);
	void addCurvePath(QString& curvePath, CustomPoint* = nullptr);
	bool assignCurve(const QVector<XYCurve*>&);
	void removeCurve(const XYCurve*);
	void setZValue(qreal) override;
	int markerPointsCount();
	MarkerPoints_T markerPointAt(int index);
	int gluePointsCount();
	TextLabel::GluePoint gluePoint(int index);
	TextLabel::TextWrapper createTextLabelText();
	QMenu* createContextMenu() override;

	bool isTextLabel() const;
	double setMarkerpointPosition(double x);
	int currentIndex(double new_x, double* found_x = nullptr);

	void retransform() override;
	void handleResize(double horizontalRatio, double verticalRatio, bool pageResize) override;

	BASIC_D_ACCESSOR_DECL(double, positionLogical, PositionLogical)
	BASIC_D_ACCESSOR_DECL(int, gluePointIndex, GluePointIndex)
	CLASS_D_ACCESSOR_DECL(QString, connectionLineCurveName, ConnectionLineCurveName)
	Line* verticalLine() const;
	Line* connectionLine() const;

	virtual void setVisible(bool on) override;

	typedef InfoElementPrivate Private;

public Q_SLOTS:
	void labelPositionChanged(TextLabel::PositionWrapper);
	void labelVisibleChanged(bool);
	void pointPositionChanged(const PositionWrapper&);
	void childRemoved(const AbstractAspect* parent, const AbstractAspect* before, const AbstractAspect* child);
	void childAdded(const AbstractAspect*);
	void labelBorderShapeChanged();
	void labelTextWrapperChanged(TextLabel::TextWrapper);
	void moveElementBegin();
	void moveElementEnd();
	void curveVisibilityChanged();
	void curveDataChanged();
	void curveCoordinateSystemIndexChanged(int);

private:
	Q_DECLARE_PRIVATE(InfoElement)
	TextLabel* m_title{nullptr};
	QVector<struct MarkerPoints_T> markerpoints;
	bool m_menusInitialized{false};
	bool m_suppressChildRemoved{false};
	bool m_suppressChildPositionChanged{false};
	bool m_setTextLabelText{false};
	/*!
	 * This variable is set when a curve is moved in the order, because there
	 * the curve is removed and readded and we would like to ignore this remove and
	 * add. Because of single thread it makes no problems.
	 */
	bool m_curveGetsMoved{false};

	void init();
	void initActions();
	void initMenus();
	void initCurveConnections(const XYCurve*);

Q_SIGNALS:
	void gluePointIndexChanged(const int);
	void connectionLineCurveNameChanged(const QString&);
	void positionLogicalChanged(const double);
	void labelBorderShapeChangedSignal();
	void curveRemoved(const QString&);
};

#endif // INFOELEMENT_H
