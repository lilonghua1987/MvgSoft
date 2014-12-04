#pragma once

#include <QThread>
#include <iostream>
#include <fstream>
#include <string>
#include "stlplus3/filesystemSimplified/file_system.hpp"
#include "openMVG/image/image.hpp"
#include "libvision/viso_mono.h"
#include "libvision/reconstruction.h"

class LibViso : public QThread
{
	Q_OBJECT

public:
	LibViso(const QString& imgDir);
	~LibViso();

	virtual bool getResult()const
	{
		return result;
	}

protected:
	virtual void run();

	void exportPLY(const std::vector<libviso::Reconstruction::point3d>& pointCloud, const std::string& fileName);
signals:
	void sendStatues(const QString& message);

private:
	QString imgDir;
	bool result;

	//log
	std::ofstream logFile;
	std::streambuf *stdFile;
};
