#include "GlobalSfM.h"

namespace mvg
{
	GlobalSfM::GlobalSfM(QObject *parent)
		: QThread(parent)
	{
		result = true;

		std::string logPath = "log/GlobalSfM.log";
		std::string logDir = stlplus::folder_part(logPath);

		if (!stlplus::folder_exists(logDir))
		{
			stlplus::folder_create(logDir);
		}

		logFile.open(logPath.c_str());

		stdFile = std::cout.rdbuf(logFile.rdbuf());
		std::cerr.rdbuf(logFile.rdbuf());

		std::cout << " currentThreadId : " << this->currentThreadId << std::endl;
	}

	GlobalSfM::~GlobalSfM()
	{
		std::cout.rdbuf(stdFile);

		//delete stdFile;
		if (logFile.is_open()) logFile.close();
	}

	void GlobalSfM::setParamters(const QString& imgDir, const QString& matcheDir, const QString& outDir)
	{
		this->imgDir = imgDir;
		this->matcheDir = matcheDir;
		this->outDir = outDir;
	}

	void GlobalSfM::run()
	{
		if (!stlplus::folder_exists(outDir.toStdString()))
			stlplus::folder_create(outDir.toStdString());

		//---------------------------------------
		// Global reconstruction process
		//---------------------------------------

		bool bColoredPointCloud = true;

		clock_t timeStart = clock();
		openMVG::GlobalReconstructionEngine to3DEngine(imgDir.toStdString(), matcheDir.toStdString(), outDir.toStdString(), true);
		to3DEngine.setRotationMethod(1);

		emit sendStatues(QStringLiteral("GlobalReconstructionEngine ....."));

		if (to3DEngine.Process())
		{
			clock_t timeEnd = clock();
			std::cout << std::endl << " Total Ac-Global-Sfm took : " << (timeEnd - timeStart) / CLOCKS_PER_SEC << std::endl;

			//-- Export computed data to disk
			to3DEngine.ExportToOpenMVGFormat(bColoredPointCloud);

			result = true;
		}
		else
		{
			result = false;
		}

		emit sendStatues(QStringLiteral("GlobalReconstructionEngine is ok !"));
	}
}
