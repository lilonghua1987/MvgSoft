#pragma once

#include <QThread>

#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"
#include "SfM/SfMGlobalEngine.hpp"

namespace mvg
{
	class GlobalSfM : public QThread
	{
		Q_OBJECT

	public:
		GlobalSfM(QObject *parent);
		~GlobalSfM();

		virtual void setParamters(const QString& imgDir, const QString& matcheDir, const QString& outDir);
		virtual bool getResult()const
		{
			return result;
		}

	protected:
		virtual void run();

	signals:
		void sendStatues(const QString& message);

	protected:
		QString imgDir;
		QString outDir;
		QString matcheDir;

		bool result;

		//log
		std::ofstream logFile;
		std::streambuf *stdFile;

	};
}
