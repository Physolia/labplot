#ifndef XYANALYSISCURVEDOCK_H
#define XYANALYSISCURVEDOCK_H

#include "XYCurveDock.h"
#include "backend/worksheet/plots/cartesian/XYAnalysisCurve.h"

class TreeViewComboBox;

class XYAnalysisCurveDock : public XYCurveDock {
public:
	enum class RequiredDataSource { XY, Y, YY2 };

	explicit XYAnalysisCurveDock(QWidget* parent, RequiredDataSource required = RequiredDataSource::XY);

protected:
	void showResult(const XYAnalysisCurve* curve, QTextEdit* teResult);
	virtual QString customText() const;

	void setAnalysisCurves(QList<XYCurve*>);
	void setModel();
	void setBaseWidgets(TimedLineEdit* nameLabel, ResizableTextEdit* commentLabel, QPushButton* recalculate, QComboBox* cbDataSourceType = nullptr);
	void enableRecalculate() const;
	virtual void updateSettings(const AbstractColumn*){};

	QVector<XYAnalysisCurve*> m_analysisCurves;
	XYAnalysisCurve* m_analysisCurve{nullptr};
	RequiredDataSource m_requiredDataSource{RequiredDataSource::XY};
	QPushButton* m_recalculateButton{nullptr};
	QComboBox* cbDataSourceType{nullptr};
	TreeViewComboBox* cbDataSourceCurve{nullptr};
	TreeViewComboBox* cbXDataColumn{nullptr};
	TreeViewComboBox* cbYDataColumn{nullptr};
	TreeViewComboBox* cbY2DataColumn{nullptr};

protected Q_SLOTS:
	// SLOTs for changes triggered in the dock
	void dataSourceCurveChanged(const QModelIndex&);
	void xDataColumnChanged(const QModelIndex&);
	void yDataColumnChanged(const QModelIndex&);
	void y2DataColumnChanged(const QModelIndex&);

	// SLOTs for changes triggered in the analysis curve
	void curveDataSourceTypeChanged(XYAnalysisCurve::DataSourceType);
	void curveDataSourceCurveChanged(const XYCurve*);
	void curveXDataColumnChanged(const AbstractColumn*);
	void curveYDataColumnChanged(const AbstractColumn*);
};

#endif // XYANALYSISCURVEDOCK_H
