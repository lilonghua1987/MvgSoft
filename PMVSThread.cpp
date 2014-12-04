#include "PMVSThread.h"

namespace mvg
{
	PMVSThread::PMVSThread(QObject *parent)
		: QThread(parent)
	{
		result = true;

		std::string logPath = "log/PMVS.log";
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

	PMVSThread::~PMVSThread()
	{
		std::cout.rdbuf(stdFile);

		//delete stdFile;
		if (logFile.is_open()) logFile.close();
	}

	void PMVSThread::setParamters(const QString& sfmDir, const QString& optionPath)
	{
		this->sfmDir = sfmDir;
		this->optionPath = optionPath;
	}

	void PMVSThread::run()
	{
		if (!stlplus::folder_exists(sfmDir.toStdString()))
		{
			std::cerr << "\nPMVS output directory doesn't exist" << std::endl;
			result = false;
			return ;
		}

		emit sendStatues(QStringLiteral("Initial Option ....."));
		PMVS3::Soption option;
		if (!option.init(stlplus::folder_append_separator(sfmDir.toStdString()), optionPath.toStdString()))
		{
			result = false;
			return;
		}

		PMVS3::CfindMatch findMatch;
		emit sendStatues(QStringLiteral("Initial PMVS2 paramaters...."));
		findMatch.init(option);
		emit sendStatues(QStringLiteral("PMVS2 is running ......"));
		findMatch.run();

		findMatch.write(stlplus::folder_append_separator(sfmDir.toStdString()) + "models/" + stlplus::basename_part(optionPath.toStdString()));
		emit sendStatues(QStringLiteral("PMVS2 is ok !"));
	}
}
