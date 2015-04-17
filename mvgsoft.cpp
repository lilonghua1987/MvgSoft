#include "mvgsoft.h"

MvgSoft::MvgSoft(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	cThread = nullptr;
	iThread = nullptr;
	gThread = nullptr;
	pThread = nullptr;
	tThread = nullptr;
	lThread = nullptr;
	chTread = nullptr;

	connect(ui.actionFiles, SIGNAL(triggered()), this, SLOT(selectInputDir()));
	connect(ui.actionComputeFocal, SIGNAL(triggered()), this, SLOT(computeFocal()));
	connect(ui.actionEMatch, SIGNAL(triggered()), this, SLOT(matchE()));
	connect(ui.actionFMatch, SIGNAL(triggered()), this, SLOT(matchF()));
	connect(ui.actionHMatch, SIGNAL(triggered()), this, SLOT(matchH()));
	connect(ui.actionIncremental, SIGNAL(triggered()), this, SLOT(incrementalReconstruction()));
	connect(ui.actionGlobal, SIGNAL(triggered()), this, SLOT(globalReconstruction()));
	connect(ui.actionCMPMVS, SIGNAL(triggered()), this, SLOT(exportToCMPMVS()));
	connect(ui.actionPMVS, SIGNAL(triggered()), this, SLOT(exportToPMVS()));
	connect(ui.actionMeshLab, SIGNAL(triggered()), this, SLOT(exportToMESHLAB()));
	connect(ui.actionCMVS, SIGNAL(triggered()), this, SLOT(cmvs()));
	connect(ui.actionPMVS2, SIGNAL(triggered()), this, SLOT(pmvs()));
	connect(ui.actionMesh, SIGNAL(triggered()), this, SLOT(mesh()));
	connect(ui.actionLibViso, SIGNAL(triggered()), this, SLOT(libViso()));
	connect(ui.actionColor, SIGNAL(triggered()), this, SLOT(colorHarm()));
}

MvgSoft::~MvgSoft()
{
	if (cThread) delete cThread;
	if (iThread) delete iThread;
	if (gThread) delete gThread;
	if (pThread) delete pThread;
	if (tThread) delete tThread;
	if (lThread) delete lThread;
	if (chTread) delete chTread;
}


void MvgSoft::updateStatues(const QString& message)
{
	this->ui.statusBar->showMessage(message);
}


void MvgSoft::updateFinished()
{
	if (cThread)
	{
		switch (cThread->getModel())
		{
		case mvg::eMatrix:
			{
				this->ui.actionEMatch->setDisabled(false);
				this->ui.actionEMatch->setEnabled(true);
			}
			break;
		case mvg::fMatrix:
			{
				this->ui.actionFMatch->setDisabled(false);
				this->ui.actionFMatch->setEnabled(true);
			}
			break;
		case mvg::hMatrix:
			{
				this->ui.actionHMatch->setDisabled(false);
				this->ui.actionHMatch->setEnabled(true);
			}
			break;
		default:
			break;
		}

		if (cThread->getResult())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("ComputeMatch successfully !"));
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("ComputeMatch failure !"));
		}

		cThread->quit();
		delete cThread;
		cThread = nullptr;
	}

	if (iThread)
	{
		this->ui.actionIncremental->setDisabled(false);
		this->ui.actionIncremental->setEnabled(true);

		if (iThread->getResult())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("IncrementalSfM successfully !"));
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("IncrementalSfM failure !"));
		}

		iThread->quit();
		delete iThread;
		iThread = nullptr;
	}

	if (gThread)
	{
		this->ui.actionGlobal->setDisabled(false);
		this->ui.actionGlobal->setEnabled(true);

		if (gThread->getResult())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("GlobalSfM successfully !"));
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("GlobalSfM failure !"));
		}

		gThread->quit();
		delete gThread;
		gThread = nullptr;
	}

	if (pThread)
	{
		this->ui.actionPMVS2->setDisabled(false);
		this->ui.actionPMVS2->setEnabled(true);

		if (pThread->getResult())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("PMVS2 successfully !"));
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("PMVS2 failure !"));
		}

		pThread->quit();
		delete pThread;
		pThread = nullptr;
	}

	if (tThread)
	{
		this->ui.actionMesh->setDisabled(false);
		this->ui.actionMesh->setEnabled(true);

		if (tThread->getResult())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("TextureMesh successfully !"));
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("TextureMesh failure !"));
		}

		tThread->quit();
		delete tThread;
		tThread = nullptr;
	}

	if (lThread)
	{
		this->ui.actionLibViso->setDisabled(false);
		this->ui.actionLibViso->setEnabled(true);

		if (lThread->getResult())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("LibViso successfully !"));
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("LibViso failure !"));
		}

		lThread->quit();
		delete lThread;
		lThread = nullptr;
	}
}


bool MvgSoft::openFiles()
{
	QFileDialog fileDialog(this);
	fileDialog.setWindowTitle(tr("Open&Select Images"));
	fileDialog.setDirectory(".");
	fileDialog.setViewMode(QFileDialog::Detail);
	fileDialog.setNameFilter(("Image Files(*.jpg *.jpeg)"));

	if (fileDialog.exec() == QDialog::Accepted) 
	{
		fileNameList = fileDialog.selectedFiles();
		
		if (fileNameList.size() < 2)
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("You didn't select enough image files."));
			return false;
		}
	}
	else 
	{
		QMessageBox::information(NULL, QString("MvgSoft Info"), QString("You didn't select any image files."));
		return false;
	}
	return true;
}


bool MvgSoft::selectInputDir()
{
	imgDir = selectDir(QString("Select Input Files Directory"));

	if (imgDir.size() < 1)
	{
		QMessageBox::information(NULL, QString("MvgSoft Info"), QString("You didn't select images dir."));
		return false;
	}
	else
	{
		QMessageBox::information(NULL, QString("MvgSoft Info"), QString("You select the images dir :") + imgDir);
		return true;
	}
}


bool MvgSoft::computeFocal()
{
	if (imgDir.size() < 1)
	{
		imgDir = selectDir(QString("Select Input Directory"));
	}

	QString outDir = selectDir(QString("Select Output Files Directory"));

	if (imgDir.size() > 0 && outDir.size() > 0)
	{
		if (sfm::SFMToolKits::computeFocal(imgDir.toStdString(), outDir.toStdString()))
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("Computed the Focal successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("Computed the Focal failure !"));
			return true;
		}
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::matchE()
{
	if (imgDir.size() < 1)
	{
		imgDir = selectDir(QString("Select Input Images Directory"));
	}

	QString outDir = selectDir(QString("Select Output Files Directory"));

	if (imgDir.size() > 0 && outDir.size() > 0)
	{
		/*std::thread eThread(sfm::SFMToolKits::computeMatchE, imgDir.toStdString(), outDir.toStdString(), 0.8f, false, 0.01f);
		eThread.detach();*/
		/*QFuture<bool> mR = QtConcurrent::run(sfm::SFMToolKits::computeMatchE, imgDir.toStdString(), outDir.toStdString(), 0.8f, false, 0.01f);
		if (mR.result())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("Match Essential successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("Match Essential failure !"));
			return true;
		}*/

		if (cThread)
		{
			if (cThread->isFinished())
			{
				cThread->quit();
				delete cThread;

				cThread = new mvg::ComputeMatchE(this);
				connect(cThread, SIGNAL(finished()), this, SLOT(updateFinished()));
				connect(cThread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
				cThread->setParamters(imgDir, outDir, 0.8f, false, 0.01f);
				cThread->start();
				this->ui.actionEMatch->setDisabled(true);
			}
			else
			{
				QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("Someone computeMacth thread is running !"));
				return false;
			}
		}
		else
		{
			cThread = new mvg::ComputeMatchE(this);
			connect(cThread, SIGNAL(finished()), this, SLOT(updateFinished()));
			connect(cThread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
			cThread->setParamters(imgDir, outDir, 0.8f, false, 0.01f);
			cThread->start();
			this->ui.actionEMatch->setDisabled(true);
		}

		return true;
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::matchF()
{
	if (imgDir.size() < 1)
	{
		imgDir = selectDir(QString("Select Input Images Directory"));
	}

	QString outDir = selectDir(QString("Select Output Files Directory"));

	if (imgDir.size() > 0 && outDir.size() > 0)
	{
		/*QFuture<bool> mR = QtConcurrent::run(sfm::SFMToolKits::computeMatchF, imgDir.toStdString(), outDir.toStdString(), 0.8f, false, 0.01f);
		if (mR.result())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("Match Fundamental  successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("Match Fundamental  failure !"));
			return true;
		}*/

		if (cThread)
		{
			if (cThread->isFinished())
			{
				cThread->quit();
				delete cThread;

				cThread = new mvg::ComputeMatchF(this);
				connect(cThread, SIGNAL(finished()), this, SLOT(updateFinished()));
				connect(cThread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
				cThread->setParamters(imgDir, outDir, 0.8f, false, 0.01f);
				cThread->start();
				this->ui.actionFMatch->setDisabled(true);
			}
			else
			{
				QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("Someone computeMacth thread is running !"));
				return false;
			}
		}
		else
		{
			cThread = new mvg::ComputeMatchF(this);
			connect(cThread, SIGNAL(finished()), this, SLOT(updateFinished()));
			connect(cThread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
			cThread->setParamters(imgDir, outDir, 0.8f, false, 0.01f);
			cThread->start();
			this->ui.actionFMatch->setDisabled(true);
		}
		return true;
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::matchH()
{
	if (imgDir.size() < 1)
	{
		imgDir = selectDir(QString("Select Input Images Directory"));
	}

	QString outDir = selectDir(QString("Select Output Files Directory"));

	if (imgDir.size() > 0 && outDir.size() > 0)
	{
		/*QFuture<bool> mR =  QtConcurrent::run(sfm::SFMToolKits::computeMatchH, imgDir.toStdString(), outDir.toStdString(), 0.8f, false, 0.01f);
		if (mR.result())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("Match Homography  successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("Match Homography  failure !"));
			return true;
		}*/

		if (cThread)
		{
			if (cThread->isFinished())
			{
				cThread->quit();
				delete cThread;

				cThread = new mvg::ComputeMatchH(this);
				connect(cThread, SIGNAL(finished()), this, SLOT(updateFinished()));
				connect(cThread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
				cThread->setParamters(imgDir, outDir, 0.8f, false, 0.01f);
				cThread->start();
				this->ui.actionHMatch->setDisabled(true);
			}
			else
			{
				QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("Someone computeMacth thread is running !"));
				return false;
			}
		}
		else
		{
			cThread = new mvg::ComputeMatchH(this);
			connect(cThread, SIGNAL(finished()), this, SLOT(updateFinished()));
			connect(cThread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
			cThread->setParamters(imgDir, outDir, 0.8f, false, 0.01f);
			cThread->start();
			this->ui.actionHMatch->setDisabled(true);
		}
		return true;
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::incrementalReconstruction()
{
	if (imgDir.size() < 1)
	{
		imgDir = selectDir(QString("Select Input Images Directory"));
	}

	QString outDir = selectDir(QString("Select Output Files Directory"));

	QString mDir = selectDir(QString("Select Match Directory"));

	if (imgDir.size() > 0 && outDir.size() > 0 && mDir.size())
	{
		/*QFuture<bool> mR = QtConcurrent::run(sfm::SFMToolKits::incrementalSfM, imgDir.toStdString(), mDir.toStdString(), outDir.toStdString());
		if (mR.result())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("incrementalSfM successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("incrementalSfM failure !"));
			return true;
		}*/

		iThread = new mvg::IncrementalSfM(this);
		connect(iThread, SIGNAL(finished()), this, SLOT(updateFinished()));
		connect(iThread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
		iThread->setParamters(imgDir, mDir, outDir);
		iThread->start();
		this->ui.actionIncremental->setDisabled(true);
		return true;
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::globalReconstruction()
{
	if (imgDir.size() < 1)
	{
		imgDir = selectDir(QString("Select Input Images Directory"));
	}

	QString mDir = selectDir(QString("Select Match Files Directory"));

	QString outDir = selectDir(QString("Select Output Files Directory"));

	if (imgDir.size() > 0 && outDir.size() > 0 && mDir.size())
	{
		/*QFuture<bool> mR = QtConcurrent::run(sfm::SFMToolKits::globalSfM, imgDir.toStdString(), mDir.toStdString(), outDir.toStdString());
		if (mR.result())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("globalSfM successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("globalSfM failure !"));
			return true;
		}*/

		gThread = new mvg::GlobalSfM(this);
		connect(gThread, SIGNAL(finished()), this, SLOT(updateFinished()));
		connect(gThread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
		gThread->setParamters(imgDir, mDir, outDir);
		gThread->start();
		this->ui.actionGlobal->setDisabled(true);
		return true;
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::exportToCMPMVS()
{
	QString sfmOutDir = selectDir(QString("Select SFM_output Files Directory"));

	QString outDir = selectDir(QString("Select Output Files Directory"));

	if (sfmOutDir.size() > 0 && outDir.size())
	{
		QFuture<bool> mR = QtConcurrent::run(sfm::SFMToolKits::exportToCMPMVS, sfmOutDir.toStdString(), outDir.toStdString());
		if (mR.result())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("exportToCMPMVS successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("exportToCMPMVS failure !"));
			return true;
		}
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::exportToPMVS()
{
	QString sfmOutDir = selectDir(QString("Select SFM_output Files Directory"));

	QString outDir = selectDir(QString("Select Output Files Directory"));

	if (sfmOutDir.size() > 0 && outDir.size())
	{
		QFuture<bool> mR = QtConcurrent::run(sfm::SFMToolKits::exportToPMVS, sfmOutDir.toStdString(), outDir.toStdString());
		if (mR.result())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("exportToPMVS successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("exportToPMVS failure !"));
			return true;
		}
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::exportToMESHLAB()
{
	if (imgDir.size() < 1)
	{
		imgDir = selectDir(QString("Select SFM_ouput Directory"));
	}

	QString plyPath = selectFile(QString("Select ply Files Directory"), QString("PLY Files(*.ply *.PLY)"));

	QString outDir = selectDir(QString("Select Output Files Directory"));

	if (imgDir.size() > 0 && outDir.size() > 0 && plyPath.size())
	{
		QFuture<bool> mR = QtConcurrent::run(sfm::SFMToolKits::exportToMESHLAB, imgDir.toStdString(), plyPath.toStdString(), outDir.toStdString());
		if (mR.result())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("exportT0MESHLAB successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("exportT0MESHLAB failure !"));
			return true;
		}
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::cmvs()
{
	QString sfmoutDir = selectDir(QString("Select sfm_Output Files Directory"));

	if (sfmoutDir.size())
	{
		QFuture<bool> mR = QtConcurrent::run(sfm::SFMToolKits::cmvs, sfmoutDir.toStdString());
		if (mR.result())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("cmvs successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("cmvs failure !"));
			return true;
		}
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::pmvs()
{
	QString sfmoutDir = selectDir(QString("Select PMVS_Output Files Directory"));
	QString optionPath = selectFile(QString("Select option Files Directory"), QString("All Files(*.txt *.TXT)"));

	if (sfmoutDir.size())
	{
		/*QFuture<bool> mR = QtConcurrent::run(sfm::SFMToolKits::pmvs, sfmoutDir.toStdString(), optionPath.toStdString());
		if (mR.result())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("pmvs successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("pmvs failure !"));
			return true;
		}*/

		pThread = new mvg::PMVSThread(this);
		connect(pThread, SIGNAL(finished()), this, SLOT(updateFinished()));
		connect(pThread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
		pThread->setParamters(sfmoutDir, optionPath);
		pThread->start();
		this->ui.actionPMVS2->setDisabled(true);
		return true;
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::mesh()
{
	QString sfmoutDir = selectDir(QString("Select PMVS Files Directory"));

	if (sfmoutDir.size())
	{
		/*QFuture<bool> mR = QtConcurrent::run(sfm::SFMToolKits::mesh, sfmoutDir.toStdString());
		if (mR.result())
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("mesh successfully !"));
			return true;
		}
		else
		{
			QMessageBox::information(NULL, QString("MvgSoft Info"), QString("mesh failure !"));
			return true;
		}*/

		tThread = new mesh::TextureMesh(sfmoutDir);
		connect(tThread, SIGNAL(finished()), this, SLOT(updateFinished()));
		connect(tThread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
		tThread->start();
		this->ui.actionMesh->setDisabled(true);
		return true;
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}


bool MvgSoft::libViso()
{
	QString imgDir = selectDir(QString("Select libViso Files Directory"));

	if (imgDir.size())
	{
		lThread = new LibViso(imgDir);
		connect(lThread, SIGNAL(finished()), this, SLOT(updateFinished()));
		connect(lThread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
		lThread->start();
		std::cout << "statues " << (lThread->getResult() ? "true" : "false") << std::endl;
		this->ui.actionLibViso->setDisabled(true);
		return true;
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		return false;
	}
}

bool MvgSoft::colorHarm()
{
	if (imgDir.size() < 1)
	{
		imgDir = selectDir(QString("Select Input Images Directory"));
	}

	QString outDir = selectDir(QString("Select Output Files Directory"));

	QString mDir = selectFile(QString("Select Match File"), QString("All Files(*.txt *.TXT)"));

	if (imgDir.size() > 0 && outDir.size() > 0 && mDir.size())
	{
		chTread = new mvg::ColorHarmonization(this);
		connect(chTread, SIGNAL(finished()), this, SLOT(updateFinished()));
		connect(chTread, SIGNAL(sendStatues(QString)), this, SLOT(updateStatues(QString)));
		chTread->setParamters(imgDir, mDir, outDir);
		chTread->start();
		this->ui.actionColor->setDisabled(true);
		return true;
	}
	else
	{
		QMessageBox::warning(NULL, QString("MvgSoft Info"), QString("You select directory  wrong !"));
		this->ui.actionColor->setEnabled(true);
		return false;
	}
}


QString MvgSoft::selectDir(const QString& info)
{
	return  QFileDialog::getExistingDirectory(this, info, ".",
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
}


QString MvgSoft::selectFile(const QString& info, const QString& filter)
{
	return QFileDialog::getOpenFileName(this, info, ".", filter);
}


bool MvgSoft::selectOutputDir()
{
	QString outDir = selectDir(QString("Select Output Files Directory"));

	if (outDir.size() < 1)
	{
		QMessageBox::information(NULL, QString("MvgSoft Info"), QString("You didn't select the output dir."));
		return false;
	}
	else
	{
		QMessageBox::information(NULL, QString("MvgSoft Info"), QString("You select the output dir :") + outDir);
		return true;
	}
}