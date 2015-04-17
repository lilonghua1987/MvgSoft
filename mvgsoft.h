#pragma once

#ifndef MVGSOFT_H
#define MVGSOFT_H

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/qmessagebox.h>
#include <QtConcurrent/qtconcurrentrun.h>
//#include <thread>
#include "ui_mvgsoft.h"
#include "SFMToolKits.h"
#include "ComputeMatchE.h"
#include "ComputeMatchF.h"
#include "ComputeMatchH.h"
#include "IncrementalSfM.h"
#include "GlobalSfM.h"
#include "PMVSThread.h"
#include "TextureMesh.h"
#include "LibViso.h"
#include "ColorHarmonization.h"

class MvgSoft : public QMainWindow
{
	Q_OBJECT

public:
	MvgSoft(QWidget *parent = 0);
	~MvgSoft();

private slots:
	bool openFiles();
	bool selectInputDir();
	bool computeFocal();
	bool matchE();
	bool matchF();
	bool matchH();
	bool incrementalReconstruction();
	bool globalReconstruction();
	bool exportToPMVS();
	bool exportToCMPMVS();
	bool exportToMESHLAB();
	bool cmvs();
	bool pmvs();
	bool mesh();
	bool libViso();
	bool colorHarm();

public slots:
	void updateStatues(const QString& message);
	void updateFinished();

private:
	QString selectDir(const QString& info);
	QString selectFile(const QString& info, const QString& filter);
	bool selectOutputDir();

private:
	Ui::MvgSoftClass ui;
	QStringList fileNameList;
	QString imgDir;

	mvg::ComputeMatch* cThread;
	mvg::IncrementalSfM* iThread;
	mvg::GlobalSfM* gThread;
	mvg::PMVSThread* pThread;
	mesh::TextureMesh* tThread;
	LibViso* lThread;
	mvg::ColorHarmonization* chTread;
};

#endif // MVGSOFT_H
