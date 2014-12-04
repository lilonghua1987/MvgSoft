#include "ComputeMatch.h"

namespace mvg
{
	ComputeMatch::ComputeMatch(QObject *parent, const std::string& fileName)
		: QThread(parent)
		, fileName(fileName)
		, maxResidualError(4.0)
	{
		result = true;

		std::string logDir = stlplus::folder_part(fileName);

		if (!stlplus::folder_exists(logDir))
		{
			stlplus::folder_create(logDir);
		}

		logFile.open(fileName.c_str());

		stdFile = std::cout.rdbuf(logFile.rdbuf());
		std::cerr.rdbuf(logFile.rdbuf());

		std::cout << " currentThreadId : " << this->currentThreadId << std::endl;
	}

	ComputeMatch::~ComputeMatch()
	{
		std::cout.rdbuf(stdFile);

		//delete stdFile;
		if (logFile.is_open()) logFile.close();
	}

	void ComputeMatch::setParamters(const QString& imgDir, const QString& outDir, float distratio, bool octminus1, float peakThreshold)
	{
		this->imgDir = imgDir;
		this->outDir = outDir;
		this->distratio = distratio;
		this->octminus1 = octminus1;
		this->peakThreshold = peakThreshold;

		std::cout << "setParamater is ok !" << std::endl;
	}

	void ComputeMatch::run()
	{
		std::cout << "ghost" << std::endl;	
	}

	bool ComputeMatch::preProcess()
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

		//-- Two alias to ease access to image filenames and image sizes

		for (auto iter_camInfo = camList.begin(); iter_camInfo != camList.end(); iter_camInfo++)
		{
			imagesSize.push_back(std::make_pair(focalList[iter_camInfo->m_intrinsicId].m_w, focalList[iter_camInfo->m_intrinsicId].m_h));
			fileNames.push_back(stlplus::create_filespec(imgDir.toStdString(), iter_camInfo->m_sImageName));
		}
		return true;
	}

	void ComputeMatch::extractFeature()
	{
		std::cout << "\n\nEXTRACT FEATURES" << std::endl;
		imagesSize.resize(fileNames.size());

		Image<unsigned char> imageGray;

#pragma omp parallel for private(imageGray)
		for (int i = 0; i < (int)fileNames.size(); ++i)
		{
			std::string sFeat = stlplus::create_filespec(outDir.toStdString(), stlplus::basename_part(fileNames[i]), "feat");
			std::string sDesc = stlplus::create_filespec(outDir.toStdString(), stlplus::basename_part(fileNames[i]), "desc");

			//If descriptors or features file are missing, compute them
			if (!stlplus::file_exists(sFeat) || !stlplus::file_exists(sDesc))
			{
				if (!ReadImage(fileNames[i].c_str(), &imageGray))
					continue;

				//Convert to float
				Image<float> img(imageGray.GetMat().cast<float>());
				// Compute features and descriptors and export them to files
				KeypointSetT kpSet;
				SIFTDetector(img, kpSet.features(), kpSet.descriptors(), octminus1, true, peakThreshold);
				kpSet.saveToBinFile(sFeat, sDesc);
			}
		}
	}

	void ComputeMatch::putativeMatch()
	{
		// If the matches already exists, reload them
		if (stlplus::file_exists(outDir.toStdString() + "/matches.putative.txt"))
		{
			PairedIndMatchImport(outDir.toStdString() + "/matches.putative.txt", mapPutativesMatches);
			std::cout << std::endl << "PUTATIVE MATCHES -- PREVIOUS RESULTS LOADED" << std::endl;
		}
		else // Compute the putatives matches
		{
			Matcher_AllInMemory<KeypointSetT, MatcherT> collectionMatcher(distratio);
			if (collectionMatcher.loadData(fileNames, outDir.toStdString()))
			{
				std::cout << std::endl << "PUTATIVE MATCHES" << std::endl;
				collectionMatcher.Match(fileNames, mapPutativesMatches);
				//---------------------------------------
				//-- Export putative matches
				//---------------------------------------
				std::ofstream file(std::string(outDir.toStdString() + "/matches.putative.txt").c_str());
				if (file.is_open())
					PairedIndMatchToStream(mapPutativesMatches, file);
				file.close();
			}
		}
	}

	void ComputeMatch::exportResult()
	{

		//-- export putative matches Adjacency matrix
		PairWiseMatchingToAdjacencyMatrixSVG(fileNames.size(),
			mapPutativesMatches,
			stlplus::create_filespec(outDir.toStdString(), "PutativeAdjacencyMatrix", "svg"));

		std::ofstream file(string(outDir.toStdString() + "/" + modelFileName).c_str());
		if (file.is_open())
			PairedIndMatchToStream(mapGeometricMatches, file);
		file.close();

		//-- export Adjacency matrix
		std::cout << "\n Export Adjacency Matrix of the pairwise's geometric matches"
			<< std::endl;
		PairWiseMatchingToAdjacencyMatrixSVG(fileNames.size(),
			mapGeometricMatches,
			stlplus::create_filespec(outDir.toStdString(), "GeometricAdjacencyMatrix", "svg"));
	}

	void ComputeMatch::runFThread()
	{
		while (true)
		{
			int && index = --jobs;

			if (index < 0) break;

			std::cout << "ThreadID = " << std::this_thread::get_id() << ", jobs = " << index << std::endl;;

			std::string sFeat = stlplus::create_filespec(outDir.toStdString(), stlplus::basename_part(fileNames[index]), "feat");
			std::string sDesc = stlplus::create_filespec(outDir.toStdString(), stlplus::basename_part(fileNames[index]), "desc");

			//std::unique_lock<std::mutex> lck(mtx);
			//If descriptors or features file are missing, compute them
			if (!stlplus::file_exists(sFeat) || !stlplus::file_exists(sDesc))
			{
				Image<unsigned char> imageGray;
				if (ReadImage(fileNames[index].c_str(), &imageGray))
				{
					//Convert to float
					Image<float> img(imageGray.GetMat().cast<float>());
					// Compute features and descriptors and export them to files
					KeypointSetT kpSet;
					//KeypointSetT dkpSet;
					std::unique_lock<std::mutex> lck(mtx);
					SIFTDetector(img, kpSet.features(), kpSet.descriptors(), octminus1, true, peakThreshold);
					//DSIFTDetector(imageGray, dkpSet.features(), dkpSet.descriptors());
					kpSet.saveToBinFile(sFeat, sDesc);
					mapFeat[index] = std::move(kpSet.features());
					mapDesc[index] = std::move(kpSet.descriptors());
				}
			}
			else
			{
				//std::cout << "Load feature : " << sFeat << std::endl;
				std::unique_lock<std::mutex> lck(mtx);
				loadFeatsFromFile(sFeat, mapFeat[index]);
				loadDescsFromBinFile(sDesc, mapDesc[index]);
			}
		}
	}

	void ComputeMatch::runMThread()
	{
		while (true)
		{
			int && i = --jobs;

			if (i < 0) break;

			std::cout << "ThreadID = " << std::this_thread::get_id() << ", jobs = " << i << std::endl;

			// Load features and descriptors of Inth image
			std::map<size_t, std::vector<FeatureT> >::const_iterator iter_FeaturesI = mapFeat.begin();
			std::map<size_t, DescsT >::const_iterator iter_DescriptorI = mapDesc.begin();
			std::advance(iter_FeaturesI, i);
			std::advance(iter_DescriptorI, i);

			const std::vector<FeatureT> & featureSetI = iter_FeaturesI->second;
			const size_t featureSetI_Size = iter_FeaturesI->second.size();
			const DescBin_typeT * tab0 =
				reinterpret_cast<const DescBin_typeT *>(&iter_DescriptorI->second[0]);

			MatcherT matcher;
			(matcher.Build(tab0, featureSetI_Size, DescriptorT::static_size));

			for (size_t j = i + 1; j < fileNames.size(); ++j)
			{
				// Load descriptor of Jnth image
				std::map<size_t, std::vector<FeatureT> >::const_iterator iter_FeaturesJ = mapFeat.begin();
				std::map<size_t, DescsT >::const_iterator iter_DescriptorJ = mapDesc.begin();
				std::advance(iter_FeaturesJ, j);
				std::advance(iter_DescriptorJ, j);

				const std::vector<FeatureT> & featureSetJ = iter_FeaturesJ->second;
				const size_t featureSetJ_Size = iter_FeaturesJ->second.size();
				const DescBin_typeT * tab1 =
					reinterpret_cast<const DescBin_typeT *>(&iter_DescriptorJ->second[0]);

				const size_t NNN__ = 2;
				std::vector<int> mIndexs;
				std::vector<typename MatcherT::DistanceType> fDistance;

				//Find left->right
				matcher.SearchNeighbours(tab1, featureSetJ.size(), &mIndexs, &fDistance, NNN__);

				std::vector<IndMatch> filteredMatches;
				std::vector<int> NNRatioIndexes;
				NNdistanceRatio(fDistance.begin(), // distance start
					fDistance.end(),  // distance end
					NNN__, // Number of neighbor in iterator sequence (minimum required 2)
					NNRatioIndexes, // output (index that respect Lowe Ratio)
					Square(distratio)); // squared dist ratio due to usage of a squared metric

				for (size_t k = 0; k < NNRatioIndexes.size() - 1 && NNRatioIndexes.size()>0; ++k)
				{
					const size_t index = NNRatioIndexes[k];
					filteredMatches.push_back(IndMatch(mIndexs[index*NNN__], index));
				}

				// Remove duplicates
				IndMatch::getDeduplicated(filteredMatches);

				// Remove matches that have the same X,Y coordinates
				IndMatchDecorator<float> matchDeduplicator(filteredMatches, featureSetI, featureSetJ);
				matchDeduplicator.getDeduplicated(filteredMatches);

				std::unique_lock<std::mutex> lck(mtx);
				mapPutativesMatches.insert(make_pair(make_pair(i, j), filteredMatches));
				//lck.unlock();
			}
		}
	}
}
