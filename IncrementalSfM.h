#pragma once

#include <QThread>
#include <iostream>

#include "SfM/SfMIncrementalEngine.hpp"

namespace mvg
{
	class IncrementalSfM : public QThread
	{
		Q_OBJECT

	public:
		IncrementalSfM(QObject *parent);
		~IncrementalSfM();

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
