#pragma once

#include <thread>
#include <mutex>
#include <map>
#include <string>
#include <vector>
#include <list>

#include "openMVG/matching/indMatch.hpp"
#include "openMVG/features/features.hpp"
#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"

namespace mvg
{
	template <typename KeypointSetT, typename MatcherT>
	class MatcherThread
	{
		// Alias to internal stored Feature and Descriptor type
		typedef typename KeypointSetT::FeatureT FeatureT;
		typedef std::vector<FeatureT> FeatsT;
		typedef typename KeypointSetT::DescriptorT DescriptorT;
		typedef std::vector<DescriptorT > DescsT; // A collection of descriptors
		// Alias to Descriptor value type
		typedef typename DescriptorT::bin_type DescBin_typeT;
		/// The structure used to store corresponding point index per images pairs
		typedef std::map< std::pair<size_t, size_t>, std::vector<openMVG::matching::IndMatch> > IndexedMatchPerPair;

	public:
		MatcherThread(const std::vector<std::string>& fileNames, const std::string& matchDir, float distRatio)
			:fileNames(fileNames)
			, matchDir(matchDir)
			, distRatio(distRatio)
		{
			std::cout << "MatcherThread created" << std::endl;
			for (size_t i = 0; i < fileNames.size(); i++)
			{
				lJobs.push_back(i);
				mJobs.push_back(i);
			}
		}
		~MatcherThread(){};

		bool loadData(std::map<size_t, FeatsT >& mfeat, std::map<size_t, DescsT >& mdesc)
		{
			bool result = true;
			while (true)
			{
				int index = -1;

				std::unique_lock<std::mutex> lck(mtx);
				if (!lJobs.empty())
				{
					index = lJobs.front();
					lJobs.pop_front();
				}
				lck.unlock();

				std::cout << "ThreadID = " << std::this_thread::get_id() << std::endl;;

				if (index == -1) break;

				const std::string sFeatJ = stlplus::create_filespec(matchDir,
					stlplus::basename_part(fileNames[index]), "feat");
				const std::string sDescJ = stlplus::create_filespec(matchDir,
					stlplus::basename_part(fileNames[index]), "desc");

				bool fOK = loadFeatsFromFile(sFeatJ, mfeat[index]);
				bool dOK = loadDescsFromBinFile(sDescJ, mdesc[index]);

				lck.lock();
				result &= (fOK&dOK);
				lck.unlock();
			}

			return result;
		}

		void matcher(const std::map<size_t, FeatsT >& mfeat, const std::map<size_t, DescsT >& mdesc, IndexedMatchPerPair& putativesMatches)
		{
			//std::cout << "matcher started: mfeat size = " << mfeat.size() << " , mdesc size = " << mdesc.size() << std::endl;
			while (true)
			{
				int i = -1;

				std::unique_lock<std::mutex> lck(mtx);
				//mtx.lock();
				if (!mJobs.empty())
				{
					i = mJobs.front();
					mJobs.pop_front();
				}
				lck.unlock();
				//mtx.unlock();

				std::cout << "ThreadID = " << std::this_thread::get_id() << std::endl;;

				if (i == -1) break;

				// Load features and descriptors of Inth image
				typename std::map<size_t, std::vector<FeatureT> >::const_iterator iter_FeaturesI = mfeat.begin();
				typename std::map<size_t, DescsT >::const_iterator iter_DescriptorI = mdesc.begin();
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
					typename std::map<size_t, std::vector<FeatureT> >::const_iterator iter_FeaturesJ = mfeat.begin();
					typename std::map<size_t, DescsT >::const_iterator iter_DescriptorJ = mdesc.begin();
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
						Square(distRatio)); // squared dist ratio due to usage of a squared metric

					for (size_t k = 0; k < NNRatioIndexes.size() - 1 && NNRatioIndexes.size()>0; ++k)
					{
						const size_t index = NNRatioIndexes[k];
						filteredMatches.push_back(
							IndMatch(mIndexs[index*NNN__], index));
					}

					// Remove duplicates
					IndMatch::getDeduplicated(filteredMatches);

					// Remove matches that have the same X,Y coordinates
					IndMatchDecorator<float> matchDeduplicator(filteredMatches, featureSetI, featureSetJ);
					matchDeduplicator.getDeduplicated(filteredMatches);

					lck.lock();
					putativesMatches.insert(make_pair(make_pair(i, j), filteredMatches));
					lck.unlock();
				}
			}
		}

	protected:
		std::vector<std::string> fileNames;
		std::string matchDir;
		float distRatio;

		//thread
		std::mutex mtx;
		std::list<int> mJobs, lJobs;

		void runMThread(const std::map<size_t, FeatsT >& mfeat, const std::map<size_t, DescsT >& mdesc, IndexedMatchPerPair& putativesMatches, const std::vector<FeatureT> & featureSetI, size_t i, MatcherT& matcher)
		{
			std::cout << "runMThread" << std::endl;
			while (true)
			{
				int j = -1;

				std::unique_lock<std::mutex> lck(mtx);
				//mtx.lock();
				if (!mJobs.empty())
				{
					j = mJobs.front();
					mJobs.pop_front();
				}
				lck.unlock();
				//mtx.unlock();

				std::cout << "ThreadID = " << std::this_thread::get_id() << std::endl;;

				if (j == -1) break;

				// Load descriptor of Jnth image
				typename std::map<size_t, std::vector<FeatureT> >::const_iterator iter_FeaturesJ = mfeat.begin();
				typename std::map<size_t, DescsT >::const_iterator iter_DescriptorJ = mdesc.begin();
				std::advance(iter_FeaturesJ, j);
				std::advance(iter_DescriptorJ, j);

				const std::vector<FeatureT> & featureSetJ = iter_FeaturesJ->second;
				const size_t featureSetJ_Size = iter_FeaturesJ->second.size();
				const DescBin_typeT * tab1 =
					reinterpret_cast<const DescBin_typeT *>(&iter_DescriptorJ->second[0]);

				const size_t NNN__ = 2;
				std::vector<int> vec_nIndice10;
				std::vector<typename MatcherT::DistanceType> vec_fDistance10;

				//Find left->right
				matcher.SearchNeighbours(tab1, featureSetJ.size(), &vec_nIndice10, &vec_fDistance10, NNN__);

				std::vector<IndMatch> vec_FilteredMatches;
				std::vector<int> vec_NNRatioIndexes;
				NNdistanceRatio(vec_fDistance10.begin(), // distance start
					vec_fDistance10.end(),  // distance end
					NNN__, // Number of neighbor in iterator sequence (minimum required 2)
					vec_NNRatioIndexes, // output (index that respect Lowe Ratio)
					Square(distRatio)); // squared dist ratio due to usage of a squared metric

				for (size_t k = 0; k < vec_NNRatioIndexes.size() - 1 && vec_NNRatioIndexes.size()>0; ++k)
				{
					const size_t index = vec_NNRatioIndexes[k];
					vec_FilteredMatches.push_back(
						IndMatch(vec_nIndice10[index*NNN__], index));
				}

				// Remove duplicates
				IndMatch::getDeduplicated(vec_FilteredMatches);

				// Remove matches that have the same X,Y coordinates
				IndMatchDecorator<float> matchDeduplicator(vec_FilteredMatches, featureSetI, featureSetJ);
				matchDeduplicator.getDeduplicated(vec_FilteredMatches);

				lck.lock();
				putativesMatches.insert(make_pair(make_pair(i, j), vec_FilteredMatches));
				lck.unlock();
			}
		}
		static void callThread(const std::map<size_t, FeatsT >& mfeat, const std::map<size_t, DescsT >& mdesc, IndexedMatchPerPair & putativesMatches, const std::vector<FeatureT> & featureSetI, size_t i, MatcherT& matcher, void* arg)
		{
			std::cout << "callThread" << std::endl;
			MatcherThread* mThread = (MatcherThread*)arg;
			mThread->runMThread(mfeat, mdesc, putativesMatches, featureSetI, i, matcher);
		}
	};
}

