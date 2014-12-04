#include "ComputeMatchE.h"

namespace mvg
{
	ComputeMatchE::ComputeMatchE(QObject *parent)
		: ComputeMatch(parent, "log/ComputeMatchE.log")
	{
		model = eMatrix;
		modelFileName = "matches.e.txt";
	}

	ComputeMatchE::~ComputeMatchE()
	{

	}

	void ComputeMatchE::run()
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

	bool ComputeMatchE::preProcess()
	{
		// Create output dir
		if (!stlplus::folder_exists(outDir.toStdString()))
			stlplus::folder_create(outDir.toStdString());

		std::string sListsFile = stlplus::create_filespec(outDir.toStdString(), "lists.txt");
		if (!stlplus::is_file(sListsFile))
		{
			std::cout << "The input file \"" << sListsFile << "\" is missing" << std::endl;
			return false;
		}

		typedef openMVG::SfMIO::CameraInfo sfmCF;
		typedef openMVG::SfMIO::IntrinsicCameraInfo sfmICF;

		std::vector<sfmCF> camList;
		std::vector<sfmICF> focalList;
		if (!openMVG::SfMIO::loadImageList(camList, focalList, sListsFile))
		{
			std::cout << "error: " << "\nEmpty image list." << std::endl;
			return false;
		}


		//只能存在一个本质矩阵（照片只能来之于同一<型号>照相机）
		//-- In the case of the essential matrix we check if only one K matrix is present.
		//-- Due to the fact that the generic framework allows only one K matrix for the
		// robust essential matrix estimation in image collection.
		auto iterF = std::unique(focalList.begin(), focalList.end(), [](sfmICF const &ci1, sfmICF const &ci2)->bool{return ci1.m_K == ci2.m_K; });

		focalList.resize(std::distance(focalList.begin(), iterF));

		if (focalList.size() == 1)
		{
			// Set all the intrinsic ID to 0
			for (size_t i = 0; i < camList.size(); ++i)
				camList[i].m_intrinsicId = 0;

			K = focalList.at(0).m_K;
		}
		else
		{
			std::cout << "There is more than one focal group in the lists.txt file." << std::endl
				<< "Only one focal group is supported for the image collection robust essential matrix estimation." << std::endl;
			return false;
		}

		//-- Two alias to ease access to image filenames and image sizes

		for (auto iter_camInfo = camList.begin(); iter_camInfo != camList.end(); iter_camInfo++)
		{
			imagesSize.push_back(std::make_pair(focalList[iter_camInfo->m_intrinsicId].m_w, focalList[iter_camInfo->m_intrinsicId].m_h));
			fileNames.push_back(stlplus::create_filespec(imgDir.toStdString(), iter_camInfo->m_sImageName));
		}
		return true;
	}

	void ComputeMatchE::extractFeature()
	{
		jobs = (int)fileNames.size();
		int cpuN = (std::min)(getCPU(), (int)fileNames.size());
		if (cpuN < 1) return;
		std::cout << "extractFeature -> CPU Cores :" << cpuN << std::endl;
		std::vector<std::thread> threads(cpuN);

		//for (auto& t : threads) t = std::thread(callFThread, std::ref(*this));

		for (auto& t : threads) t = std::thread([](ComputeMatchE& arg){arg.runFThread(); }, std::ref(*this));

		for (auto& t : threads) t.join();
	}

	void ComputeMatchE::putativeMatch()
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

				for (auto& t : threads) t = std::thread([](ComputeMatchE& arg){arg.runMThread(); }, std::ref(*this));

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

	void ComputeMatchE::postProcess()
	{
		if (!mapFeat.empty() && !mapDesc.empty())
		{
			jobs = (int)mapPutativesMatches.size();

			int cpuN = (std::min)(getCPU(), (int)mapPutativesMatches.size());
			if (cpuN < 1) return;
			std::cout << "CollectionGeometricFilter -> CPU Cores :" << cpuN << std::endl;
			filter = std::unique_ptr<GeometricFilter_EMatrix_AC>(new GeometricFilter_EMatrix_AC(K, maxResidualError));
			std::vector<std::thread> threads(cpuN);

			for (auto& t : threads) t = std::thread([](ComputeMatchE& arg){arg.runGThread(); }, std::ref(*this));

			for (auto& t : threads) t.join();

			//-- Perform an additional check to remove pairs with poor overlap
			std::vector<IndexedMatchPerPair::key_type> removePairs;
			for (auto iterMap = mapGeometricMatches.begin(); iterMap != mapGeometricMatches.end(); ++iterMap)
			{
				size_t putativePhotometricCount = mapPutativesMatches.find(iterMap->first)->second.size();
				size_t putativeGeometricCount = iterMap->second.size();
				float ratio = putativeGeometricCount / (float)putativePhotometricCount;

				if (putativeGeometricCount < 50 || ratio < .3f)
				{
					// the pair will be removed
					removePairs.push_back(iterMap->first);
				}
			}
			//-- remove discarded pairs
			for (auto iter = removePairs.begin(); iter != removePairs.end(); ++iter)
			{
				mapGeometricMatches.erase(*iter);
			}

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

	void ComputeMatchE::runGThread()
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