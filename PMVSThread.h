#pragma once

#include <QThread>

#include "pmvs/findMatch.h"
#include "pmvs/option.h"

namespace mvg
{
	class PMVSThread : public QThread
	{
		Q_OBJECT

	public:
		PMVSThread(QObject *parent);
		~PMVSThread();

		virtual void setParamters(const QString& sfmDir, const QString& optionPath);
		virtual bool getResult()const
		{
			return result;
		}

	protected:
		virtual void run();

	signals:
		void sendStatues(const QString& message);

	protected:
		QString sfmDir;
		QString optionPath;

		bool result;

		//log
		std::ofstream logFile;
		std::streambuf *stdFile;

	};
}
