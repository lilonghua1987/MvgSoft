#pragma once

#include <QThread>

#include <omp.h>
#include <thread>
#include <mutex>
#include <atomic>

#include "openMVG/exif_IO/exif_IO_openExif.hpp"
#include "openMVG/sensorWidthDatabase/ParseDatabase.hpp"
#include "openMVG/image/image.hpp"
#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"
#include "openMVG/features/features.hpp"

/// Generic Image Collection image matching
#include "openMVG/matching_image_collection/Matcher_AllInMemory.hpp"
//#include "openMVG/matching_image_collection/GeometricFilter.hpp"
#include "openMVG/matching_image_collection/F_ACRobust.hpp"
#include "openMVG/matching_image_collection/E_ACRobust.hpp"
#include "openMVG/matching_image_collection/H_ACRobust.hpp"
#include "openMVG/matching/matcher_brute_force.hpp"
#include "openMVG/matching/matcher_kdtree_flann.hpp"
#include "openMVG/matching/indMatch_utils.hpp"

/// Feature detector and descriptor interface
#include "patented/sift/SIFT.hpp"
#include "DSIFT.hpp"

#include "sfm/SfMIOHelper.hpp"
#include "sfm/pairwiseAdjacencyDisplay.hpp"

namespace mvg
{
	enum GModel
	{
		fMatrix,eMatrix,hMatrix
	};
	class ComputeMatch : public QThread
	{
		Q_OBJECT

	public:
		ComputeMatch(QObject *parent, const std::string& fileName = "log/ComputeMatch.log");
		virtual ~ComputeMatch();

		virtual void setParamters(const QString& imgDir, const QString& outDir, float distratio, bool octminus1, float peakThreshold);
		virtual bool getResult()const
		{
			return result;
		}

		virtual GModel getModel()const
		{
			return model;
		}

		virtual void runFThread();
		virtual void runMThread();
		virtual void runGThread() = 0;

	protected:
		virtual void run();
		virtual bool preProcess();
		virtual void extractFeature();
		virtual void putativeMatch();
		virtual void postProcess() = 0;
		virtual void exportResult();

		int getCPU()
		{
			return static_cast<int>(std::thread::hardware_concurrency());
		}

	signals:
		void sendStatues(const QString& message);

	protected:
		QString imgDir;
		QString outDir;
		GModel model;
		float distratio;
		bool octminus1;
		float peakThreshold;

		const double maxResidualError;

		std::string modelFileName;
		std::vector<std::string> fileNames;
		std::vector<std::pair<size_t, size_t> > imagesSize;
		IndexedMatchPerPair mapPutativesMatches;
		IndexedMatchPerPair mapGeometricMatches;

		//SIFT
		typedef Descriptor<unsigned char, 128> DescriptorT;
		//typedef Descriptor<float, 128> DescriptorT;
		typedef SIOPointFeature FeatureT;
		typedef std::vector<FeatureT> FeatsT;
		typedef vector<DescriptorT > DescsT;
		typedef KeypointSet<FeatsT, DescsT > KeypointSetT;

		typedef DescriptorT::bin_type DescBin_typeT;

		// Define the matcher and the used metric (Squared L2)
		// ANN matcher could be defined as follow:
		typedef flann::L2<DescriptorT::bin_type> MetricT;
		typedef ArrayMatcher_Kdtree_Flann<DescriptorT::bin_type, MetricT> MatcherT;
		// Brute force matcher can be defined as following:
		//typedef L2_Vectorized<DescriptorT::bin_type> MetricT;
		//typedef ArrayMatcherBruteForce<DescriptorT::bin_type, MetricT> MatcherT;

		// Features per image
		std::map<size_t, FeatsT > mapFeat;
		// Descriptors per image as contiguous memory
		std::map<size_t, DescsT > mapDesc;

		bool result;
		QString statues;

		//thread
		std::mutex mtx;
		//std::list<int> mJobs;
		std::atomic_int jobs;

		//log
		std::string fileName;
		std::ofstream logFile;
		std::streambuf *stdFile;
	};
}

