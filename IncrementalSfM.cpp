#include "IncrementalSfM.h"

namespace mvg
{
	IncrementalSfM::IncrementalSfM(QObject *parent)
		: QThread(parent)
	{
		result = true;

		std::string logPath = "log/IncrementalSfM.log";
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

	IncrementalSfM::~IncrementalSfM()
	{
		std::cout.rdbuf(stdFile);

		//delete stdFile;
		if (logFile.is_open()) logFile.close();
	}

	void IncrementalSfM::setParamters(const QString& imgDir, const QString& matcheDir, const QString& outDir)
	{
		this->imgDir = imgDir;
		this->matcheDir = matcheDir;
		this->outDir = outDir;
	}

	void IncrementalSfM::run()
	{
		if (!stlplus::folder_exists(outDir.toStdString()))
			stlplus::folder_create(outDir.toStdString());

		bool bColoredPointCloud = true;

		//---------------------------------------
		// Incremental reconstruction process
		//---------------------------------------

		clock_t timeStart = clock();
		emit sendStatues(QStringLiteral("Initial  IncrementalReconstructionEngine !"));
		openMVG::IncrementalReconstructionEngine to3DEngine(imgDir.toStdString(), matcheDir.toStdString(), outDir.toStdString(), true);

		emit sendStatues(QStringLiteral("IncrementalReconstructionEngine ....."));
		if (to3DEngine.Process())
		{
			clock_t timeEnd = clock();
			std::cout << std::endl << " Ac-Sfm took : " << (timeEnd - timeStart) / CLOCKS_PER_SEC << " seconds." << std::endl;

			const openMVG::reconstructorHelper & reconstructorHelperRef = to3DEngine.refToReconstructorHelper();
			std::vector<openMVG::Vec3> tracksColor;
			if (bColoredPointCloud)
			{
				// Compute the color of each track
				to3DEngine.ColorizeTracks(tracksColor);
			}
			reconstructorHelperRef.exportToPly(
				stlplus::create_filespec(outDir.toStdString(), "FinalColorized", ".ply"), tracksColor);

			// Export to openMVG format
			std::cout << std::endl << "Export 3D scene to openMVG format" << std::endl
				<< " -- Point cloud color: " << (bColoredPointCloud ? "ON" : "OFF") << std::endl;

			reconstructorHelperRef.ExportToOpenMVGFormat(
				stlplus::folder_append_separator(outDir.toStdString()) + "SfM_output",
				to3DEngine.getFilenamesVector(),
				imgDir.toStdString(),
				to3DEngine.getImagesSize(),
				to3DEngine.getTracks(),
				tracksColor
				);

			
			result = true;
		}
		else
		{
			std::cout << "\n error:" << "\n Something goes wrong in the Structure from Motion process" << std::endl;
			result = false;
		}

		emit sendStatues(QStringLiteral("IncrementalReconstructionEngine is ok !"));
	}
}
