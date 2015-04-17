#include "ColorHarmonization.h"
#include <third_party/stlplus3/filesystemSimplified/file_system.hpp>

namespace mvg{
	ColorHarmonization::ColorHarmonization(QObject *parent)
		: QThread(parent)
	{
		result = true;

		std::string logPath = "log/ColorHarmonization.log";
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


	ColorHarmonization::~ColorHarmonization()
	{
		std::cout.rdbuf(stdFile);

		//delete stdFile;
		if (logFile.is_open()) logFile.close();
	}

	void ColorHarmonization::setParamters(const QString& imgDir, const QString& matcheDir, const QString& outDir)
	{
		this->imgDir = imgDir;
		this->matcheDir = matcheDir;
		this->outDir = outDir;
	}

	void ColorHarmonization::run(){
		if (!stlplus::folder_exists(outDir.toStdString()))
			stlplus::folder_create(outDir.toStdString());

		clock_t timeStart = clock();
		emit sendStatues(QStringLiteral("Initial  ColorHarmonization !"));
		std::string matchPath = stlplus::folder_part(matcheDir.toStdString());
		openMVG::ColorHarmonizationEngineGlobal colorEngine(imgDir.toStdString(), matchPath, matcheDir.toStdString(), outDir.toStdString());
		colorEngine.setRefParmas(1,1);

		emit sendStatues(QStringLiteral("ColorHarmonization ....."));

		if (colorEngine.Process())
		{
			clock_t timeEnd = clock();
			std::cout << std::endl
				<< " ColorHarmonization took : "
				<< (timeEnd - timeStart) / CLOCKS_PER_SEC << " seconds." << std::endl;
			result = true;
		}
		else
		{
			std::cout << "\n error:" << "\n Something goes wrong in the Structure from Motion process" << std::endl;
			result = false;
		}

		emit sendStatues(QStringLiteral("ColorHarmonization is ok !"));
	}
}
