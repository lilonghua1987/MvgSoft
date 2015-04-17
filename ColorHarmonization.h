#pragma once
#include <QThread>
#include <iostream>
#include <sstream>

#include "ColorHarmonizationEngineGlobal.h"

namespace mvg{
	class ColorHarmonization : public QThread
	{
		Q_OBJECT

	public:
		ColorHarmonization(QObject *parent);
		~ColorHarmonization();

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

