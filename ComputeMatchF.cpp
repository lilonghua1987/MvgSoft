#include "ComputeMatchF.h"

namespace mvg
{
	ComputeMatchF::ComputeMatchF(QObject *parent)
		: ComputeMatch(parent, "log/ComputeMatchF.log")
	{
		model = fMatrix;
		modelFileName = "matches.f.txt";
	}

	ComputeMatchF::~ComputeMatchF()
	{

	}

	void ComputeMatchF::run()
	{
		//---------------------------------------
		// a. List images
		//---------------------------------------
		statues = QStringLiteral("List Images !");
		emit sendStatues(statues);

		if (!(result = preProcess())) return;

		//---------------------------------------
		// b.Compute features and descriptor
		//    - extract sift features and descriptor
		//    - if keypoints already computed, re-load them
		//    - else save features and descriptors on disk
		//---------------------------------------
		statues = QStringLiteral("Compute features and descriptor !");
		emit sendStatues(statues);

		extractFeature();

		//---------------------------------------
		// c. Compute putative descriptor matches
		//    - L2 descriptor matching
		//    - Keep correspondences only if NearestNeighbor ratio is ok
		//---------------------------------------
		statues = QStringLiteral("Compute putative descriptor matches !");
		emit sendStatues(statues);

		putativeMatch();


		//---------------------------------------
		// d. Geometric filtering of putative matches
		//    - AContrario Estimation of the desired geometric model
		//    - Use an upper bound for the a contrario estimated threshold
		//---------------------------------------
		statues = QStringLiteral("Geometric filtering of putative matches !");
		emit sendStatues(statues);

		postProcess();
	}

	void ComputeMatchF::extractFeature()
	{
		jobs = (int)fileNames.size();

		int cpuN = (std::min)(getCPU(), (int)fileNames.size());
		if (cpuN < 1) return;
		std::cout << "extractFeature -> CPU Cores :" << cpuN << std::endl;
		std::vector<std::thread> threads(cpuN);

		for (auto& t : threads) t = std::thread([](ComputeMatchF& arg){arg.runFThread(); }, std::ref(*this));

		for (auto& t : threads) t.join();
	}

	void ComputeMatchF::putativeMatch()
	{
		std::cout << "putativeMatch" << std::endl;

		if (stlplus::file_exists(outDir.toStdString() + "/matches.putative.txt"))
		{
			PairedIndMatchImport(outDir.toStdString() + "/matches.putative.txt", mapPutativesMatches);
			std::cout << std::endl << "PUTATIVE MATCHES -- PREVIOUS RESULTS LOADED" << std::endl;
		}
		else // Compute the putatives matches
		{

			if (!mapFeat.empty() && !mapDesc.empty())
			{
				jobs = (int)fileNames.size();

				int cpuN = (std::min)(getCPU(), (int)fileNames.size());
				if (cpuN < 1) return;
				std::cout << "putativeMatch -> CPU Cores :" << cpuN << std::endl;
				std::vector<std::thread> threads(cpuN);

				for (auto& t : threads) t = std::thread([](ComputeMatchF& arg){arg.runMThread(); }, std::ref(*this));

				for (auto& t : threads) t.join();
				//-- Export putative matches
				//---------------------------------------
				std::ofstream file(std::string(outDir.toStdString() + "/matches.putative.txt").c_str());
				if (file.is_open())
					PairedIndMatchToStream(mapPutativesMatches, file);
				file.close();
			}

			//-- export putative matches Adjacency matrix
			PairWiseMatchingToAdjacencyMatrixSVG(fileNames.size(),
				mapPutativesMatches,
				stlplus::create_filespec(outDir.toStdString(), "PutativeAdjacencyMatrix", "svg"));
		}
	}

	void ComputeMatchF::postProcess()
	{
		if (!mapFeat.empty() && !mapDesc.empty())
		{
			jobs = (int)mapPutativesMatches.size();

			int cpuN = (std::min)(getCPU(), (int)mapPutativesMatches.size());
			if (cpuN < 1) return;
			std::cout << "GeometricFilter -> CPU Cores :" << cpuN << std::endl;

			filter = std::unique_ptr<GeometricFilter_FMatrix_AC>(new GeometricFilter_FMatrix_AC(maxResidualError));

			std::vector<std::thread> threads(cpuN);

			for (auto& t : threads) t = std::thread([](ComputeMatchF& arg){arg.runGThread(); }, std::ref(*this));

			for (auto& t : threads) t.join();

			//---------------------------------------
			//-- Export geometric filtered matches
			//---------------------------------------
			statues = QStringLiteral("Export geometric filtered matches !");
			emit sendStatues(statues);

			exportResult();

			statues = QStringLiteral("Compute match is ok !");
			emit sendStatues(statues);
		}
	}

	void ComputeMatchF::runGThread()
	{
		while (true)
		{
			int && i = --jobs;

			if (i < 0) break;

			std::cout << "ThreadID = " << std::this_thread::get_id() << ", jobs = " << i << std::endl;

			IndexedMatchPerPair::const_iterator iter = mapPutativesMatches.begin();
			advance(iter, i);

			const size_t iIndex = iter->first.first;
			const size_t jIndex = iter->first.second;
			const std::vector<IndMatch> & putativeMatches = iter->second;

			// Load features of Inth and Jnth images
			std::map<size_t, std::vector<FeatureT> >::const_iterator iterFeatsI = mapFeat.begin();
			std::map<size_t, std::vector<FeatureT> >::const_iterator iterFeatsJ = mapFeat.begin();
			std::advance(iterFeatsI, iIndex);
			std::advance(iterFeatsJ, jIndex);
			const std::vector<FeatureT> & kpSetI = iterFeatsI->second;
			const std::vector<FeatureT> & kpSetJ = iterFeatsJ->second;

			//-- Copy point to array in order to estimate fundamental matrix :
			const size_t n = putativeMatches.size();
			Mat xI(2, n), xJ(2, n);

			for (size_t i = 0; i < putativeMatches.size(); ++i)
			{
				const FeatureT & imaA = kpSetI[putativeMatches[i]._i];
				const FeatureT & imaB = kpSetJ[putativeMatches[i]._j];
				xI.col(i) = Vec2f(imaA.coords()).cast<double>();
				xJ.col(i) = Vec2f(imaB.coords()).cast<double>();
			}

			//-- Apply the geometric filter
			{
				std::vector<size_t> inliers;
				// Use a copy in order to copy use internal functor parameters
				// and use it safely in multi-thread environment
				filter->Fit(xI, imagesSize[iIndex], xJ, imagesSize[jIndex], inliers);

				if (!inliers.empty())
				{
					std::vector<IndMatch> filteredMatches;
					filteredMatches.reserve(inliers.size());
					for (size_t i = 0; i < inliers.size(); ++i)
					{
						filteredMatches.push_back(putativeMatches[inliers[i]]);
					}

					std::unique_lock<std::mutex> lck(mtx);
					mapGeometricMatches[std::make_pair(iIndex, jIndex)] = filteredMatches;
					//lck.unlock();
				}
			}
		}
	}
}
