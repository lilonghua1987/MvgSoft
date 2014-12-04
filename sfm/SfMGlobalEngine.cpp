
// Copyright (c) 2012, 2013, 2014 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "SfMGlobalEngine.hpp"
#include "indexedImageGraph.hpp"
#include "indexedImageGraphExport.hpp"
#include "SfMIOHelper.hpp"
#include "SfMRobust.hpp"
#include "SfMPlyHelper.hpp"
#include "SfMGlobal_tij_computation.hpp"
#include "SfMBundleAdjustmentHelper_tonly.hpp"
#include "SfMGlobalEngine_triplet_t_estimator.hpp"

#include "openMVG/features/features.hpp"
#include "openMVG/image/image.hpp"
#include "openMVG/matching/indMatch_utils.hpp"
#include "openMVG/matching/indexed_sort.hpp"
#include "openMVG/multiview/triangulation_nview.hpp"

#include "openMVG/graph/connectedComponent.hpp"

#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"
#include "third_party/vectorGraphics/svgDrawer.hpp"
#include "third_party/stlAddition/stlMap.hpp"
#include "third_party/histogram/histogram.hpp"

#include "openMVG/linearProgramming/linearProgrammingInterface.hpp"
#include "openMVG/linearProgramming/linearProgrammingOSI_X.hpp"
#ifdef OPENMVG_HAVE_MOSEK
#include "openMVG/linearProgramming/linearProgrammingMOSEK.hpp"
#endif

#include "openMVG/robust_estimation/robust_estimator_ACRansac.hpp"
#include "openMVG/robust_estimation/robust_estimator_ACRansacKernelAdaptator.hpp"

#undef DYNAMIC
#include "openMVG/bundle_adjustment/problem_data_container.hpp"
#include "openMVG/bundle_adjustment/pinhole_ceres_functor.hpp"

#include "lemon/list_graph.h"
#include <lemon/connectivity.h>

// Rotation
#include "openMVG/multiview/rotation_averaging.hpp"
#include "openMVG/multiview/rotation_averaging_l1.hpp"

#include "third_party/stlplus3/subsystems/timer.hpp"

#include <numeric>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <sstream>

using namespace openMVG;
using namespace openMVG::graphUtils;
using namespace svg;

namespace openMVG{

	typedef SIOPointFeature FeatureT;
	typedef std::vector<FeatureT> featsT;

	/// Return in radian the rotation amplitude of the given rotation matrix
	template <typename Mat>
	inline double
		getRotationMagnitude(const Mat & R)
	{
			double cos_theta = 0.5 * (R.trace() - 1.0);
			cos_theta = clamp(cos_theta, -1.0, 1.0);
			return std::acos(cos_theta);
		}

	GlobalReconstructionEngine::GlobalReconstructionEngine(const std::string & sImagePath,
		const std::string & sMatchesPath, const std::string & sOutDirectory, bool bHtmlReport)
		: ReconstructionEngine(sImagePath, sMatchesPath, sOutDirectory)
	{
		_bHtmlReport = bHtmlReport;
		if (!stlplus::folder_exists(sOutDirectory)) {
			stlplus::folder_create(sOutDirectory);
		}
		if (_bHtmlReport)
		{
			_htmlDocStream = auto_ptr<htmlDocument::htmlDocumentStream>(
				new htmlDocument::htmlDocumentStream("GlobalReconstructionEngine SFM report."));
			_htmlDocStream->pushInfo(
				htmlDocument::htmlMarkup("h1", std::string("Current directory: ") +
				sImagePath));
			_htmlDocStream->pushInfo("<hr>");
		}

		std::string logPath = "log/log_sfm_GlobalReconstructionEngine.log";
		std::string logDir = stlplus::folder_part(logPath);

		if (!stlplus::folder_exists(logDir))
		{
			stlplus::folder_create(logDir);
		}

		logF.open(logPath.c_str());

		stdFile = std::cout.rdbuf(logF.rdbuf());
		std::cerr.rdbuf(logF.rdbuf());
	}

	GlobalReconstructionEngine::~GlobalReconstructionEngine()
	{
		ofstream htmlFileStream(string(stlplus::folder_append_separator(_sOutDirectory) +
			"Reconstruction_Report.html").c_str());
		htmlFileStream << _htmlDocStream->getDoc();

		std::cout.rdbuf(stdFile);

		//delete stdFile;
		if (logF.is_open()) logF.close();
	}

	void GlobalReconstructionEngine::setRotationMethod(unsigned _chooseM)
	{
		if (_chooseM < 0 || _chooseM > ROTATION_AVERAGING_L2) chooseM = 0;
		else chooseM = _chooseM;

		switch (chooseM)
		{
		case 1:
			std::cout << "\n Your choosed :" << "   - 1 -> MST based rotation + L1 rotation averaging" << "\n";
			break;
		case 2:
			std::cout << "\n Your choosed :" << "   - 2 -> dense L2 global rotation computation" << "\n";
			break;
		default:
			break;
		}

		std::cout
			<< "-------------------------------" << "\n"
			<< " Choose your rotation averaging method: " << "\n"
			<< "   - 1 -> MST based rotation + L1 rotation averaging" << "\n"
			<< "   - 2 -> dense L2 global rotation computation" << "\n";
	}

	void GlobalReconstructionEngine::rotationInference()
	{
		std::cout
			<< "---------------\n"
			<< "-- INFERENCE on " << _map_Matches_F.size() << " EGs count.\n"
			<< "---------------" << std::endl
			<< " /!\\  /!\\  /!\\  /!\\  /!\\  /!\\  /!\\ \n"
			<< "--- ITERATED BAYESIAN INFERENCE IS NOT RELEASED, SEE C.ZACH CODE FOR MORE INFORMATION" << std::endl
			<< " /!\\  /!\\  /!\\  /!\\  /!\\  /!\\  /!\\ \n" << std::endl
			<< " A simple inference scheme is used here :" << std::endl
			<< "\t only the relative error composition to identity on cycle of length 3 is used." << std::endl;

		//-------------------
		// Triplet inference (test over the composition error)
		//-------------------
		std::vector< graphUtils::Triplet > vec_triplets;
		tripletListing(vec_triplets);
		//-- Rejection triplet that are 'not' identity rotation (error to identity > 2°)
		tripletRotationRejection(vec_triplets);
	}

	bool GlobalReconstructionEngine::computeGlobalRotations(
		ERotationAveragingMethod eRotationAveragingMethod,
		const std::map<size_t, size_t> & map_cameraNodeToCameraIndex,
		const std::map<size_t, size_t> & map_cameraIndexTocameraNode,
		const RelativeType & map_relatives,
		std::map<size_t, Mat3> & map_globalR) const
	{
		// Build relative information for only the largest considered Connected Component
		// - it requires to change the camera indexes, because RotationAveraging is working only with
		//   index ranging in [0 - nbCam], (map_cameraNodeToCameraIndex utility)

		switch (eRotationAveragingMethod)
		{
		case ROTATION_AVERAGING_L2:
		{
									  using namespace openMVG::rotation_averaging;
									  std::vector<std::pair<std::pair<size_t, size_t>, Mat3> > vec_relativeRotEstimate;
									  for (auto& relatives : map_relatives)
									  {
										  std::pair<size_t, size_t> newPair(
											  map_cameraNodeToCameraIndex.find(relatives.first.first)->second,
											  map_cameraNodeToCameraIndex.find(relatives.first.second)->second);
										  vec_relativeRotEstimate.push_back(std::make_pair(newPair, relatives.second.first));
									  }

									  std::vector<Mat3> vec_ApprRotMatrix;
									  if (!rotation_averaging::L2RotationAveraging(map_cameraIndexTocameraNode.size(),
										  vec_relativeRotEstimate,
										  vec_ApprRotMatrix))
									  {
										  return false;
									  }

									  //-- Setup the averaged rotation
									  for (int i = 0; i < vec_ApprRotMatrix.size(); ++i)
									  {
										  map_globalR[map_cameraIndexTocameraNode.find(i)->second] = vec_ApprRotMatrix[i];
									  }

									  return true;
		}
			break;
		case ROTATION_AVERAGING_L1:
		{
									  using namespace openMVG::rotation_averaging::l1;

									  //-- Compute the mean number of matches to prepare weight computation
									  std::vector<double> vec_count;
									  for (auto& relatives : map_relatives)
									  {
										  // Find the number of support point for this pair
										  float weight = 0.f; // the relative rotation correspondence point support
										  auto iterMatches = _map_Matches_F.find(relatives.first);
										  if (iterMatches != _map_Matches_F.end())
										  {
											  vec_count.push_back(iterMatches->second.size());
										  }
									  }
									  float thTrustPair = (std::accumulate(vec_count.begin(), vec_count.end(), 0.0f) / vec_count.size()) / 2.0f;

									  std::vector<RelRotationData> vec_relativeRotEstimate;
									  for (auto& relatives : map_relatives)
									  {
										  // Find the number of support point for this pair
										  float weight = 0.f; // the relative rotation correspondence point support
										  auto iterMatches = _map_Matches_F.find(relatives.first);
										  if (iterMatches != _map_Matches_F.end())
										  {
											  weight = std::min((float)iterMatches->second.size() / thTrustPair, 1.f);
										  }
										  vec_relativeRotEstimate.push_back(RelRotationData(
											  map_cameraNodeToCameraIndex.find(relatives.first.first)->second,
											  map_cameraNodeToCameraIndex.find(relatives.first.second)->second,
											  relatives.second.first, weight));
									  }

									  //- Solve the global rotation estimation problem:
									  Matrix3x3Arr vec_globalR(map_cameraIndexTocameraNode.size());
									  size_t nMainViewID = 0;
									  std::vector<bool> vec_inliers;
									  bool bRotAveraging = GlobalRotationsRobust(vec_relativeRotEstimate, vec_globalR, nMainViewID, 0.0f, &vec_inliers);

									  std::cout << "\ninliers : " << std::endl;
									  std::copy(vec_inliers.begin(), vec_inliers.end(), ostream_iterator<bool>(std::cout, " "));
									  std::cout << std::endl;

									  //-- Setup the averaged rotation
									  for (int i = 0; i < vec_globalR.size(); ++i)
									  {
										  map_globalR[map_cameraIndexTocameraNode.find(i)->second] = vec_globalR[i];
									  }
									  return bRotAveraging;
		}
			break;
		default:
			std::cerr << "Unknown rotation averaging method: " << (int)eRotationAveragingMethod << std::endl;
		}
		return false;
	}

	bool GlobalReconstructionEngine::Process()
	{

		//---------------------------------
		//-- Global Calibration -----------
		//---------------------------------

		//-------------------
		// Load data
		//-------------------

		if (!ReadInputData())  {
			std::cout << "\nError while parsing input data" << std::endl;
			return false;
		}

		//-- Export input graph
		{
			typedef lemon::ListGraph Graph;
			imageGraph::indexedImageGraph putativeGraph(_map_Matches_F, _vec_fileNames);

			// Save the graph before cleaning:
			imageGraph::exportToGraphvizData(
				stlplus::create_filespec(_sOutDirectory, "input_graph"),
				putativeGraph.g);
		}


		stlplus::timer total_reconstruction_timer;

		//-------------------
		// Only keep the largest biedge connected subgraph
		//-------------------

		if (!CleanGraph())
			return false;

		//-------------------
		// Compute relative R|t
		//-------------------
		ComputeRelativeRt();
		std::cout << "map_relatives size = " << mapRelatives.size() << std::endl;

		//-------------------
		// Rotation inference
		//-------------------

		//-- Putative triplets for relative translations computation
		std::vector< graphUtils::Triplet > vec_triplets;
		{
			stlplus::timer timer_Inference;

			rotationInference();

			//-------------------
			// keep the largest biedge connected subgraph
			//-------------------
			if (!CleanGraph())
				return false;

			// recompute possible triplets since some nodes have been possibly removed
			tripletListing(vec_triplets);

			double time_Inference = timer_Inference.elapsed();

			//---------------------------------------
			//-- Export geometric filtered matches
			//---------------------------------------
			std::ofstream file(string(_sMatchesPath + "/matches.filtered.txt").c_str());
			if (file.is_open())
				PairedIndMatchToStream(_map_Matches_F, file);
			file.close();

			//-------------------
			// List remaining camera node Id
			//-------------------
			std::set<size_t> set_indeximage;
			for (auto& matchF : _map_Matches_F)
			{
				set_indeximage.insert(matchF.first.first);
				set_indeximage.insert(matchF.first.second);
			}
			// Build correspondences tables (cameraIds <=> GraphIds)
			for (std::set<size_t>::const_iterator iterSet = set_indeximage.begin();
				iterSet != set_indeximage.end(); ++iterSet)
			{
				map_cameraIndexTocameraNode[std::distance(set_indeximage.begin(), iterSet)] = *iterSet;
				map_cameraNodeToCameraIndex[*iterSet] = std::distance(set_indeximage.begin(), iterSet);
			}

			std::cout << "\n Remaining cameras after inference filter : \n"
				<< map_cameraIndexTocameraNode.size() << " from a total of " << _vec_fileNames.size() << std::endl;

			//-- Export statistics about the rotation inference step:
			if (_bHtmlReport)
			{
				using namespace htmlDocument;
				std::ostringstream os;
				os << "Rotation inference.";
				_htmlDocStream->pushInfo("<hr>");
				_htmlDocStream->pushInfo(htmlMarkup("h1", os.str()));

				os.str("");
				os << "-------------------------------" << "<br>"
					<< "-- #Camera count: " << set_indeximage.size() << " remains "
					<< "-- from " << _vec_fileNames.size() << " input images.<br>"
					<< "-- timing : " << time_Inference << " second <br>"
					<< "-------------------------------" << "<br>";
				_htmlDocStream->pushInfo(os.str());
			}
		}

		//----------------------------
		// Rotation averaging
		//----------------------------

		std::map<std::size_t, Mat3> map_globalR;
		{
			std::cout << "\n-------------------------------" << "\n"
				<< " Global rotations computation: " << "\n"
				<< "   - Ready to compute " << map_cameraIndexTocameraNode.size() << " global rotations." << "\n"
				<< "     from " << mapRelatives.size() << " relative rotations\n" << std::endl;

			int iChoice = 1;
			/*do
			{
			std::cout
			<< "-------------------------------" << "\n"
			<< " Choose your rotation averaging method: " << "\n"
			<< "   - 1 -> MST based rotation + L1 rotation averaging" << "\n"
			<< "   - 2 -> dense L2 global rotation computation" << "\n";
			} while (!(std::cin >> iChoice) || iChoice < 0 || iChoice > ROTATION_AVERAGING_L2);*/

			if (!computeGlobalRotations(
				ERotationAveragingMethod(chooseM),
				map_cameraNodeToCameraIndex,
				map_cameraIndexTocameraNode,
				mapRelatives,
				map_globalR))
			{
				std::cerr << "Failed to compute the global rotations." << std::endl;
				return false;
			}
		}

		//-------------------
		// Relative translations estimation (Triplet based translation computation)
		//-------------------
		std::vector<openMVG::lInfinityCV::relativeInfo > vec_initialRijTijEstimates;
		STLPairWiseMatches newpairMatches;
		{
			std::cout << "\n-------------------------------" << "\n"
				<< " Relative translations computation: " << "\n"
				<< "-------------------------------" << std::endl;

			// Compute putative translations with an edge coverage algorithm

			stlplus::timer timerLP_triplet;

			computePutativeTranslation_EdgesCoverage(map_globalR, vec_triplets, vec_initialRijTijEstimates, newpairMatches);
			double timeLP_triplet = timerLP_triplet.elapsed();
			std::cout << "TRIPLET COVERAGE TIMING : " << timeLP_triplet << " seconds" << std::endl;

			//-- Export triplet statistics:
			if (_bHtmlReport)
			{
				using namespace htmlDocument;
				std::ostringstream os;
				os << "Triplet statistics.";
				_htmlDocStream->pushInfo("<hr>");
				_htmlDocStream->pushInfo(htmlMarkup("h1", os.str()));

				os.str("");
				os << "-------------------------------" << "<br>"
					<< "-- #Effective triplet estimates: " << vec_initialRijTijEstimates.size() / 3
					<< " from " << vec_triplets.size() << " triplets.<br>"
					<< "-- resulting in " << vec_initialRijTijEstimates.size() << " translation estimation.<br>"
					<< "-- timing to obtain the relative translations : " << timeLP_triplet << " seconds.<br>"
					<< "-------------------------------" << "<br>";
				_htmlDocStream->pushInfo(os.str());
			}
		}

		//--Check the relative translation graph:
		//--> Consider only the connected component compound by the translation graph
		//-- Robust translation estimation can perform inference and remove some bad conditioned triplets

		{
			std::set<size_t> set_representedImageIndex;
			for (size_t i = 0; i < vec_initialRijTijEstimates.size(); ++i)
			{
				const openMVG::lInfinityCV::relativeInfo & rel = vec_initialRijTijEstimates[i];
				set_representedImageIndex.insert(rel.first.first);
				set_representedImageIndex.insert(rel.first.second);
			}
			std::cout << "\n\n"
				<< "We targeting to estimates : " << map_globalR.size()
				<< " and we have estimation for : " << set_representedImageIndex.size() << " images" << std::endl;

			//-- PRINT IMAGE THAT ARE NOT INSIDE THE TRIPLET GRAPH
			for (std::map<size_t, Mat3>::const_iterator iter = map_globalR.begin();
				iter != map_globalR.end(); ++iter)
			{
				if (set_representedImageIndex.find(iter->first) == set_representedImageIndex.end())
				{
					std::cout << "Missing image index: " << iter->first << std::endl;
					map_globalR.erase(map_cameraIndexTocameraNode[iter->first]);
				}
			}

			//- Build the map of camera index and node Ids that are listed by the triplets of translations
			map_cameraIndexTocameraNode.clear();
			map_cameraNodeToCameraIndex.clear();
			for (std::set<size_t>::const_iterator iterSet = set_representedImageIndex.begin();
				iterSet != set_representedImageIndex.end(); ++iterSet)
			{
				map_cameraIndexTocameraNode[std::distance(set_representedImageIndex.begin(), iterSet)] = *iterSet;
				map_cameraNodeToCameraIndex[*iterSet] = std::distance(set_representedImageIndex.begin(), iterSet);
			}

			std::cout << "\nRemaining cameras after inference filter : \n"
				<< map_cameraIndexTocameraNode.size() << " from a total of " << _vec_fileNames.size() << std::endl;
		}

		//-------------------
		//-- GLOBAL TRANSLATIONS ESTIMATION from initial triplets t_ij guess
		//-------------------

		{
			const int iNview = map_cameraNodeToCameraIndex.size(); // The remaining camera nodes count in the graph

			std::cout << "\n-------------------------------" << "\n"
				<< " Global translations computation: " << "\n"
				<< "   - Ready to compute " << iNview << " global translations." << "\n"
				<< "     from " << vec_initialRijTijEstimates.size() << " relative translations\n" << std::endl;

			//-- Update initial estimates in range [0->Ncam]
			for (size_t i = 0; i < vec_initialRijTijEstimates.size(); ++i)
			{
				lInfinityCV::relativeInfo & rel = vec_initialRijTijEstimates[i];
				std::pair<size_t, size_t> newPair(
					map_cameraNodeToCameraIndex[rel.first.first],
					map_cameraNodeToCameraIndex[rel.first.second]);
				rel.first = newPair;
			}

			stlplus::timer timerLP_translation;
			clock_t start_timeLP = clock();

			bool bNormal = false;
			double gamma = -1.0;
			std::vector<double> vec_solution;
			{
				vec_solution.resize(iNview * 3 + vec_initialRijTijEstimates.size() / 3 + 1);
				using namespace openMVG::linearProgramming;
#ifdef OPENMVG_HAVE_MOSEK
				MOSEK_SolveWrapper solverLP(vec_solution.size());
#else
				OSI_CLP_SolverWrapper solverLP(vec_solution.size());
#endif

				lInfinityCV::Tifromtij_ConstraintBuilder_OneLambdaPerTrif cstBuilder(vec_initialRijTijEstimates);

				LP_Constraints_Sparse constraint;
				//-- Setup constraint and solver
				cstBuilder.Build(constraint);
				solverLP.setup(constraint);
				//--
				// Solving
				bool bFeasible = solverLP.solve();
				std::cout << " \n Feasibility " << bFeasible << std::endl;
				//--
				if (bFeasible)
				{
					solverLP.getSolution(vec_solution);
					gamma = vec_solution[vec_solution.size() - 1];
				}
			}

			double timeLP_translation = timerLP_translation.elapsed();
			double timeLP_translation_clock = double(clock() - start_timeLP) / CLOCKS_PER_SEC;

			//-- Export triplet statistics:
			if (_bHtmlReport)
			{
				using namespace htmlDocument;
				std::ostringstream os;
				os << "Translation fusion statistics.";
				_htmlDocStream->pushInfo("<hr>");
				_htmlDocStream->pushInfo(htmlMarkup("h1", os.str()));

				os.str("");
				os << "-------------------------------" << "<br>"
					<< "-- #relative estimates: " << vec_initialRijTijEstimates.size()
					<< " converge with gamma: " << gamma << ".<br>"
					<< " timing: " << timeLP_translation << ".<br>"
					<< " timing clocks (s): " << timeLP_translation_clock << ".<br>"
					<< "-------------------------------" << "<br>";
				_htmlDocStream->pushInfo(os.str());
			}

			std::cout << "Found solution:\n";
			std::copy(vec_solution.begin(), vec_solution.end(), std::ostream_iterator<double>(std::cout, " "));

			std::vector<double> vec_camTranslation(iNview * 3, 0);
			std::copy(&vec_solution[0], &vec_solution[iNview * 3], &vec_camTranslation[0]);

			std::vector<double> vec_camRelLambdas(&vec_solution[iNview * 3], &vec_solution[iNview * 3 + vec_initialRijTijEstimates.size() / 3]);
			std::cout << "\ncam position: " << std::endl;
			std::copy(vec_camTranslation.begin(), vec_camTranslation.end(), std::ostream_iterator<double>(std::cout, " "));
			std::cout << "\ncam Lambdas: " << std::endl;
			std::copy(vec_camRelLambdas.begin(), vec_camRelLambdas.end(), std::ostream_iterator<double>(std::cout, " "));

			// Build a Pinhole camera for each considered Id
			std::vector<Vec3>  vec_C;
			for (size_t i = 0; i < iNview; ++i)
			{
				Vec3 t(vec_camTranslation[i * 3], vec_camTranslation[i * 3 + 1], vec_camTranslation[i * 3 + 2]);
				const size_t camNodeId = map_cameraIndexTocameraNode[i];
				const Mat3 & Ri = map_globalR[camNodeId];
				const Mat3 & _K = _vec_intrinsicGroups[0].m_K;   // The same K matrix is used by all the camera
				_map_camera[camNodeId] = PinholeCamera(_K, Ri, t);
				//-- Export camera center
				vec_C.push_back(_map_camera[camNodeId]._C);
			}
			plyHelper::exportToPly(vec_C, stlplus::create_filespec(_sOutDirectory, "cameraPath", "ply"));
		}

		//-------------------
		//-- Initial triangulation of the scene from the computed global motions
		//-------------------
		{
			// Build tracks from selected triplets (Union of all the validated triplet tracks (newpairMatches))
			//  triangulate tracks
			//  refine translations
			{
				TracksBuilder tracksBuilder;
				tracksBuilder.Build(newpairMatches);
				tracksBuilder.Filter(3);
				tracksBuilder.ExportToSTL(_map_selectedTracks);

				std::cout << std::endl << "Track stats" << std::endl;
				{
					std::ostringstream osTrack;
					//-- Display stats :
					//    - number of images
					//    - number of tracks
					std::set<size_t> set_imagesId;
					TracksUtilsMap::ImageIdInTracks(_map_selectedTracks, set_imagesId);
					osTrack << "------------------" << "\n"
						<< "-- Tracks Stats --" << "\n"
						<< " Tracks number: " << tracksBuilder.NbTracks() << "\n"
						<< " Images Id: " << "\n";
					std::copy(set_imagesId.begin(),
						set_imagesId.end(),
						std::ostream_iterator<size_t>(osTrack, ", "));
					osTrack << "\n------------------" << "\n";

					std::map<size_t, size_t> map_Occurence_TrackLength;
					TracksUtilsMap::TracksLength(_map_selectedTracks, map_Occurence_TrackLength);
					osTrack << "TrackLength, Occurrence" << "\n";
					for (std::map<size_t, size_t>::const_iterator iter = map_Occurence_TrackLength.begin();
						iter != map_Occurence_TrackLength.end(); ++iter)  {
						osTrack << "\t" << iter->first << "\t" << iter->second << "\n";
					}
					osTrack << "\n";
					std::cout << osTrack.str();
				}
			}

			// Triangulation of all the tracks
			_vec_allScenes.resize(_map_selectedTracks.size());
			{
				std::vector<double> vec_residuals;
				vec_residuals.resize(_map_selectedTracks.size());
				std::set<size_t> set_idx_to_remove;

				jobs = 0;

				int cpuN = (std::min)((int)std::thread::hardware_concurrency(), (int)_map_selectedTracks.size());
				if (cpuN >= 1)
				{
					std::cout << "SelectedTracks -> CPU Cores :" << cpuN << std::endl;
					std::vector<std::thread> threads(cpuN);

					for (auto& t : threads) t = std::thread([&set_idx_to_remove, &vec_residuals](GlobalReconstructionEngine& arg){ arg.runSelectedTracks(set_idx_to_remove, vec_residuals); }, std::ref(*this));

					for (auto& t : threads) t.join();
				}

				//-- Remove useless tracks and 3D points
				{
					std::vector<Vec3> vec_allScenes_cleaned;
					for (size_t i = 0; i < _vec_allScenes.size(); ++i)
					{
						if (find(set_idx_to_remove.begin(), set_idx_to_remove.end(), i) == set_idx_to_remove.end())
						{
							vec_allScenes_cleaned.push_back(_vec_allScenes[i]);
						}
					}
					_vec_allScenes = vec_allScenes_cleaned;

					for (std::set<size_t>::const_iterator iter = set_idx_to_remove.begin();
						iter != set_idx_to_remove.end(); ++iter)
					{
						_map_selectedTracks.erase(*iter);
					}
					std::cout << "\n Tracks have been removed : " << set_idx_to_remove.size() << std::endl;
				}
				plyHelper::exportToPly(_vec_allScenes, stlplus::create_filespec(_sOutDirectory, "raw_pointCloud_LP", "ply"));

				{
					// Display some statistics of reprojection errors
					std::cout << "\n\nResidual statistics:\n" << std::endl;
					minMaxMeanMedian<double>(vec_residuals.begin(), vec_residuals.end());
					double min, max, mean, median;
					minMaxMeanMedian<double>(vec_residuals.begin(), vec_residuals.end(), min, max, mean, median);

					Histogram<float> histo(0, *max_element(vec_residuals.begin(), vec_residuals.end())*1.1);
					histo.Add(vec_residuals.begin(), vec_residuals.end());
					std::cout << std::endl << "Residual Error pixels : " << std::endl << histo.ToString() << std::endl;

					// Histogram between 0 and 10 pixels
					{
						std::cout << "\n Histogram between 0 and 10 pixels: \n";
						Histogram<float> histo(0, 10, 20);
						histo.Add(vec_residuals.begin(), vec_residuals.end());
						std::cout << std::endl << "Residual Error pixels : " << std::endl << histo.ToString() << std::endl;
					}

					//-- Export initial triangulation statistics
					if (_bHtmlReport)
					{
						using namespace htmlDocument;
						std::ostringstream os;
						os << "Initial triangulation statistics.";
						_htmlDocStream->pushInfo("<hr>");
						_htmlDocStream->pushInfo(htmlMarkup("h1", os.str()));

						os.str("");
						os << "-------------------------------" << "<br>"
							<< "-- #tracks: " << _map_selectedTracks.size() << ".<br>"
							<< "-- #observation: " << vec_residuals.size() << ".<br>"
							<< "-- residual mean (RMSE): " << std::sqrt(mean) << ".<br>"
							<< "-------------------------------" << "<br>";
						_htmlDocStream->pushInfo(os.str());
					}
				}
			}
		}

		//-------------------
		//-- Bundle Adjustment on translation and structure
		//-------------------

		bundleAdjustment_t_Xi(_map_camera, _vec_allScenes, _map_selectedTracks);
		bundleAdjustment_Rt_Xi(_map_camera, _vec_allScenes, _map_selectedTracks);

		//-- Export statistics about the global process
		if (_bHtmlReport)
		{
			using namespace htmlDocument;
			std::ostringstream os;
			os << "Global calibration summary triangulation statistics.";
			_htmlDocStream->pushInfo("<hr>");
			_htmlDocStream->pushInfo(htmlMarkup("h1", os.str()));

			os.str("");
			os << "-------------------------------" << "<br>"
				<< "-- Have calibrated: " << _map_camera.size() << " from "
				<< _vec_fileNames.size() << " input images.<br>"
				<< "-- The scene contains " << _map_selectedTracks.size() << " 3D points.<br>"
				<< "-- Total reconstruction time (Inference, global rot, translation fusion, Ba1, Ba2): "
				<< total_reconstruction_timer.elapsed() << " seconds.<br>"
				<< "-------------------------------" << "<br>";
			_htmlDocStream->pushInfo(os.str());
		}

		std::cout << std::endl
			<< "-------------------------------" << "\n"
			<< "-- Have calibrated: " << _map_camera.size() << " from "
			<< _vec_fileNames.size() << " input images.\n"
			<< "-- The scene contains " << _map_selectedTracks.size() << " 3D points.\n"
			<< "-- Total reconstruction time (Inference, global rot, translation fusion, Ba1, Ba2): "
			<< total_reconstruction_timer.elapsed() << " seconds.\n"
			<< "Relative rotations time was excluded\n"
			<< "-------------------------------" << std::endl;

		return true;
	}

	//-- Export data to openMVG format:
	bool GlobalReconstructionEngine::ExportToOpenMVGFormat(bool bColoredPointCloud) const
	{
		// Export to openMVG format
		std::cout << std::endl << "Export 3D scene to openMVG format" << std::endl
			<< " -- Point cloud color: " << (bColoredPointCloud ? "ON" : "OFF") << std::endl;

		std::vector<Vec3> vec_tracksColor;
		if (bColoredPointCloud)
		{
			std::cout << std::endl << "Compute the color of each track..." << std::endl;
			ColorizeTracks(_map_selectedTracks, vec_tracksColor);
		}

		const std::string & sOutDirectory = stlplus::folder_append_separator(_sOutDirectory) + "SfM_Output";  //Export directory
		const std::vector<std::string> & vec_fileNames = _vec_fileNames; // vector of image filenames
		const std::string & sImagePath = _sImagePath;  // The images path
		const openMVG::tracks::STLMAPTracks & map_reconstructed = _map_selectedTracks; // Tracks (Visibility)
		bool bExportImage = true;

		bool bOk = true;
		if (!stlplus::is_folder(sOutDirectory))
		{
			stlplus::folder_create(sOutDirectory);
			bOk = stlplus::is_folder(sOutDirectory);
		}

		// Create basis directory structure
		stlplus::folder_create(stlplus::folder_append_separator(sOutDirectory) + "cameras");
		stlplus::folder_create(stlplus::folder_append_separator(sOutDirectory) + "clouds");
		stlplus::folder_create(stlplus::folder_append_separator(sOutDirectory) + "images");

		if (bOk &&
			stlplus::is_folder(stlplus::folder_append_separator(sOutDirectory) + "cameras") &&
			stlplus::is_folder(stlplus::folder_append_separator(sOutDirectory) + "clouds") &&
			stlplus::is_folder(stlplus::folder_append_separator(sOutDirectory) + "images")
			)
		{
			bOk = true;
		}
		else
		{
			std::cerr << "Cannot access to one of the desired output directory" << std::endl;
			return false;
		}

		if (bOk)
		{
			//Export Camera as binary files
			std::map<size_t, size_t> map_cameratoIndex;
			size_t count = 0;
			for (auto& mcam : _map_camera)
			{
				map_cameratoIndex[mcam.first] = count;
				const Mat34 & PMat = mcam.second._P;
				std::ofstream file(
					stlplus::create_filespec(stlplus::folder_append_separator(sOutDirectory) + "cameras",
					stlplus::basename_part(vec_fileNames[mcam.first])
					, "bin").c_str(), std::ios::out | std::ios::binary);
				file.write((const char*)PMat.data(), (std::streamsize)(3 * 4)*sizeof(double));
				bOk &= (!file.fail());
				file.close();
				++count;
			}

			//Export 3D point and tracks

			size_t nc = _map_camera.size();

			// Clipping planes (near and far Z depth per view)
			std::vector<double> znear(nc, numeric_limits<double>::max()), zfar(nc, 0);
			// Cloud
			std::ofstream f_cloud(
				stlplus::create_filespec(stlplus::folder_append_separator(sOutDirectory) + "clouds",
				"calib", "ply").c_str());
			std::ofstream f_visibility(
				stlplus::create_filespec(stlplus::folder_append_separator(sOutDirectory) + "clouds",
				"visibility", "txt").c_str());

			if (!f_cloud.is_open()) {
				std::cerr << "cannot save cloud" << std::endl;
				return false;
			}

			if (!f_visibility.is_open()) {
				std::cerr << "cannot save cloud desc" << std::endl;
				return false;
			}

			f_cloud << "ply\nformat ascii 1.0\ncomment generated by the Global OpenMVG Calibration Engine" << "\n";
			f_cloud << "element vertex " << _vec_allScenes.size() << "\n";
			f_cloud << "property float x\nproperty float y\nproperty float z" << "\n";
			f_cloud << "property uchar red\nproperty uchar green\nproperty uchar blue" << "\n";
			f_cloud << "property float confidence\nproperty list uchar int visibility" << "\n";
			f_cloud << "element face 0\nproperty list uchar int vertex_index" << "\n";
			f_cloud << "end_header" << "\n";
			size_t i = 0;
			for (auto iter = map_reconstructed.begin(); iter != map_reconstructed.end(); ++iter, ++i)
			{
				// Look through the track and add point position
				const tracks::submapTrack & track = iter->second;

				const Vec3 & pos = _vec_allScenes[i];

				if (bColoredPointCloud)
				{
					const Vec3 & color = vec_tracksColor[i];
					f_cloud << pos.transpose() << " " << color.transpose() << " " << 3.14;
				}
				else
					f_cloud << pos.transpose() << " 255 255 255 " << 3.14;

				std::ostringstream s_visibility;
				std::set< size_t > set_imageIndex;
				for (auto & trk : track)
				{
					size_t imageId = trk.first;

					if (map_cameratoIndex.find(imageId) != map_cameratoIndex.end())
					{
						set_imageIndex.insert(map_cameratoIndex[imageId]);
						const PinholeCamera & cam = (_map_camera.find(imageId))->second;
						double z = Depth(cam._R, cam._t, pos);
						znear[map_cameratoIndex[imageId]] = std::min(znear[map_cameratoIndex[imageId]], z);
						zfar[map_cameratoIndex[imageId]] = std::max(zfar[map_cameratoIndex[imageId]], z);
					}
					s_visibility << trk.first << " " << trk.second << " ";
				}


				//export images indexes
				f_cloud << " " << set_imageIndex.size() << " ";
				copy(set_imageIndex.begin(), set_imageIndex.end(), std::ostream_iterator<size_t>(f_cloud, " "));
				f_cloud << std::endl;

				f_visibility << pos.transpose() << " " << set_imageIndex.size() << " ";
				f_visibility << s_visibility.str() << "\n";
			}
			f_cloud.close();
			f_visibility.close();

			// Views
			std::ofstream f(stlplus::create_filespec(stlplus::folder_append_separator(sOutDirectory), "views", "txt").c_str());

			if (!f.is_open())
			{
				std::cerr << "Cannot write views" << std::endl;
				return false;
			}
			f << "images\ncameras\n" << nc << "\n";

			count = 0;
			for (auto iter = _map_camera.begin(); iter != _map_camera.end(); ++iter)
			{
				size_t camIndex = iter->first;
				f << vec_fileNames[camIndex]
					<< ' ' << _vec_intrinsicGroups[_vec_camImageNames[camIndex].m_intrinsicId].m_w
					<< ' ' << _vec_intrinsicGroups[_vec_camImageNames[camIndex].m_intrinsicId].m_h
					<< ' ' << stlplus::basename_part(vec_fileNames[camIndex]) << ".bin"
					<< ' ' << znear[count] / 2
					<< ' ' << zfar[count] * 2
					<< "\n";
				++count;
			}
			f.close();

			// EXPORT IMAGES
			if (bExportImage)
			{
				std::cout << " -- Export the calibrated image set, it can take some time ..." << std::endl;
				for (auto iter = _map_camera.begin(); iter != _map_camera.end(); ++iter)
				{
					size_t imageIndex = iter->first;
					std::string sImageName = vec_fileNames[imageIndex];
					stlplus::file_copy(stlplus::create_filespec(sImagePath, sImageName),
						stlplus::create_filespec(stlplus::folder_append_separator(sOutDirectory) + "images",
						stlplus::basename_part(sImageName),
						stlplus::extension_part(sImageName)));
				}
			}
			return true;
		}
		return true;
	}

	bool testIntrinsicsEquality(
		SfMIO::IntrinsicCameraInfo const &ci1,
		SfMIO::IntrinsicCameraInfo const &ci2)
	{
		return ci1.m_K == ci2.m_K;
	}

	bool GlobalReconstructionEngine::ReadInputData()
	{
		if (!stlplus::is_folder(_sImagePath) ||
			!stlplus::is_folder(_sMatchesPath) ||
			!stlplus::is_folder(_sOutDirectory))
		{
			std::cerr << std::endl
				<< "One of the required directory is not a valid directory" << std::endl;
			return false;
		}

		// a. Read images names
		std::string sListsFile = stlplus::create_filespec(_sMatchesPath, "lists", "txt");
		std::string sComputedMatchesFile_E = stlplus::create_filespec(_sMatchesPath, "matches.e", "txt");
		if (!stlplus::is_file(sListsFile) ||
			!stlplus::is_file(sComputedMatchesFile_E))
		{
			std::cerr << std::endl
				<< "One of the input required file is not a present (lists.txt, matches.e.txt)" << std::endl;
			return false;
		}

		// a. Read images names
		{
			if (!openMVG::SfMIO::loadImageList(_vec_camImageNames,
				_vec_intrinsicGroups,
				sListsFile))
			{
				std::cerr << "\nEmpty image list." << std::endl;
				return false;
			}
			else
			{
				// Check there is only one intrinsic group
				std::vector<openMVG::SfMIO::IntrinsicCameraInfo>::iterator iterF =
					std::unique(_vec_intrinsicGroups.begin(), _vec_intrinsicGroups.end(), testIntrinsicsEquality);
				_vec_intrinsicGroups.resize(std::distance(_vec_intrinsicGroups.begin(), iterF));
				if (_vec_intrinsicGroups.size() == 1)
				{
					for (auto& camImg : _vec_camImageNames)
						camImg.m_intrinsicId = 0;
				}
				else
				{
					std::cout << "There is more than one focal group in the lists.txt file." << std::endl
						<< "Only one focal group is supported for the global calibration chain" << std::endl;
					return false;
				}

				for (auto& camImg : _vec_camImageNames)
				{
					_vec_fileNames.push_back(camImg.m_sImageName);
				}
			}
		}

		// b. Read matches (Essential)
		if (!matching::PairedIndMatchImport(sComputedMatchesFile_E, _map_Matches_F))
		{
			std::cerr << "Unable to read the Essential matrix matches" << std::endl;
			return false;
		}

		// Read features:
		size_t camIndex = 0;
		for (const auto& fileName : _vec_fileNames)
		{
			if (!loadFeatsFromFile(
				stlplus::create_filespec(_sMatchesPath, stlplus::basename_part(fileName), ".feat"),
				_map_feats[camIndex]))
			{
				std::cerr << "Bad reading of feature files" << std::endl;
				return false;
			}
			++camIndex;
		}
		return true;
	}

	bool GlobalReconstructionEngine::CleanGraph()
	{
		// Create a graph from pairwise correspondences:
		// - remove not biedge connected component,
		// - keep the largest connected component.

		typedef lemon::ListGraph Graph;
		imageGraph::indexedImageGraph putativeGraph(_map_Matches_F, _vec_fileNames);

		// Save the graph before cleaning:
		imageGraph::exportToGraphvizData(
			stlplus::create_filespec(_sOutDirectory, "initialGraph"),
			putativeGraph.g);

		// Remove not bi-edge connected edges
		typedef Graph::EdgeMap<bool> EdgeMapAlias;
		EdgeMapAlias cutMap(putativeGraph.g);

		if (lemon::biEdgeConnectedCutEdges(putativeGraph.g, cutMap) > 0)
		{
			// Some edges must be removed because they don't follow the biEdge condition.
			typedef Graph::EdgeIt EdgeIterator;
			EdgeIterator itEdge(putativeGraph.g);
			for (EdgeMapAlias::MapIt it(cutMap); it != INVALID; ++it, ++itEdge)
			{
				if (*it)
					putativeGraph.g.erase(itEdge); // remove the not bi-edge element
			}
		}

		// Graph is bi-edge connected, but still many connected components can exist
		// Keep only the largest one
		STLPairWiseMatches matches_filtered;
		int connectedComponentCount = lemon::countConnectedComponents(putativeGraph.g);
		std::cout << "\n"
			<< "GlobalReconstructionEngine::CleanGraph() :: => connected Component : "
			<< connectedComponentCount << std::endl;
		if (connectedComponentCount > 1)
		{
			// Keep only the largest connected component
			// - list all CC size
			// - if the largest one is meet, keep all the edges that belong to this node

			const std::map<size_t, std::set<lemon::ListGraph::Node> > map_subgraphs = exportGraphToMapSubgraphs(putativeGraph.g);
			size_t count = std::numeric_limits<size_t>::min();
			std::map<size_t, std::set<lemon::ListGraph::Node> >::const_iterator iterLargestCC = map_subgraphs.end();
			for (std::map<size_t, std::set<lemon::ListGraph::Node> >::const_iterator iter = map_subgraphs.begin();
				iter != map_subgraphs.end(); ++iter)
			{
				if (iter->second.size() > count)  {
					count = iter->second.size();
					iterLargestCC = iter;
				}
				std::cout << "Connected component of size : " << iter->second.size() << std::endl;
			}

			//-- Remove all nodes that are not listed in the largest CC
			for (std::map<size_t, std::set<lemon::ListGraph::Node> >::const_iterator iter = map_subgraphs.begin();
				iter != map_subgraphs.end(); ++iter)
			{
				if (iter == iterLargestCC)
				{
					// list all nodes and outgoing edges and update the matching list
					const std::set<lemon::ListGraph::Node> & ccSet = iter->second;
					for (std::set<lemon::ListGraph::Node>::const_iterator iter2 = ccSet.begin();
						iter2 != ccSet.end(); ++iter2)
					{
						typedef Graph::OutArcIt OutArcIt;
						for (OutArcIt e(putativeGraph.g, *iter2); e != INVALID; ++e)
						{
							size_t Idu = (*putativeGraph.map_nodeMapIndex)[putativeGraph.g.target(e)];
							size_t Idv = (*putativeGraph.map_nodeMapIndex)[putativeGraph.g.source(e)];
							STLPairWiseMatches::iterator iterF = _map_Matches_F.find(std::make_pair(Idu, Idv));
							if (iterF != _map_Matches_F.end())
							{
								matches_filtered.insert(*iterF);
							}
							iterF = _map_Matches_F.find(std::make_pair(Idv, Idu));
							if (iterF != _map_Matches_F.end())
							{
								matches_filtered.insert(*iterF);
							}
						}
					}
					// update the matching list
					_map_Matches_F = matches_filtered;
				}
				else
				{
					// remove the edges from the graph
					const std::set<lemon::ListGraph::Node> & ccSet = iter->second;
					for (std::set<lemon::ListGraph::Node>::const_iterator iter2 = ccSet.begin();
						iter2 != ccSet.end(); ++iter2)
					{
						typedef Graph::OutArcIt OutArcIt;
						for (OutArcIt e(putativeGraph.g, *iter2); e != INVALID; ++e)
						{
							putativeGraph.g.erase(e);
						}
						//putativeGraph.g.erase(*iter2);
					}
				}
			}
		}

		// Save the graph after cleaning:
		imageGraph::exportToGraphvizData(
			stlplus::create_filespec(_sOutDirectory, "cleanedGraph"),
			putativeGraph.g);

		std::cout << "\n"
			<< "Cardinal of nodes: " << lemon::countNodes(putativeGraph.g) << "\n"
			<< "Cardinal of edges: " << lemon::countEdges(putativeGraph.g) << std::endl
			<< std::endl;

		return true;
	}

	void GlobalReconstructionEngine::ComputeRelativeRt()
	{
		// For each pair, compute the rotation from pairwise point matches:

		jobs = 0;

		int cpuN = (std::min)((int)std::thread::hardware_concurrency(), (int)_map_Matches_F.size());
		if (cpuN < 1) return;
		std::cout << "ComputeRelativeRt -> CPU Cores :" << cpuN << std::endl;
		std::vector<std::thread> threads(cpuN);

		for (auto& t : threads) t = std::thread([](GlobalReconstructionEngine& arg){ arg.runRelativeRtThread(); }, std::ref(*this));

		for (auto& t : threads) t.join();
	}

	void GlobalReconstructionEngine::tripletListing(std::vector< graphUtils::Triplet > & vec_triplets) const
	{
		vec_triplets.clear();

		imageGraph::indexedImageGraph putativeGraph(_map_Matches_F, _vec_fileNames);

		List_Triplets<imageGraph::indexedImageGraph::GraphT>(putativeGraph.g, vec_triplets);

		//Change triplets to ImageIds
		for (size_t i = 0; i < vec_triplets.size(); ++i)
		{
			graphUtils::Triplet & triplet = vec_triplets[i];
			size_t I = triplet.i, J = triplet.j, K = triplet.k;
			I = (*putativeGraph.map_nodeMapIndex)[putativeGraph.g.nodeFromId(I)];
			J = (*putativeGraph.map_nodeMapIndex)[putativeGraph.g.nodeFromId(J)];
			K = (*putativeGraph.map_nodeMapIndex)[putativeGraph.g.nodeFromId(K)];
			size_t triplet_[3] = { I, J, K };
			std::sort(&triplet_[0], &triplet_[3]);
			triplet = graphUtils::Triplet(triplet_[0], triplet_[1], triplet_[2]);
		}
	}

	void GlobalReconstructionEngine::tripletRotationRejection(std::vector< graphUtils::Triplet > & vec_triplets)
	{
		RelativeType map_relatives_validated;

		// DETECTION OF ROTATION OUTLIERS
		std::vector< graphUtils::Triplet > vec_triplets_validated;

		std::vector<double> vec_errToIdentityPerTriplet;
		vec_errToIdentityPerTriplet.reserve(vec_triplets.size());
		// Compute for each length 3 cycles: the composition error
		//  Error to identity rotation.
		for (size_t i = 0; i < vec_triplets.size(); ++i)
		{
			const graphUtils::Triplet & triplet = vec_triplets[i];
			size_t I = triplet.i, J = triplet.j, K = triplet.k;

			//-- Find the three rotations
			const std::pair<size_t, size_t> ij = std::make_pair(I, J);
			const std::pair<size_t, size_t> ji = std::make_pair(J, I);

			Mat3 RIJ;
			if (mapRelatives.find(ij) != mapRelatives.end())
				RIJ = mapRelatives.find(ij)->second.first;
			else
				RIJ = mapRelatives.find(ji)->second.first.transpose();

			const std::pair<size_t, size_t> jk = std::make_pair(J, K);
			const std::pair<size_t, size_t> kj = std::make_pair(K, J);

			Mat3 RJK;
			if (mapRelatives.find(jk) != mapRelatives.end())
				RJK = mapRelatives.find(jk)->second.first;
			else
				RJK = mapRelatives.find(kj)->second.first.transpose();

			const std::pair<size_t, size_t> ki = std::make_pair(K, I);
			const std::pair<size_t, size_t> ik = std::make_pair(I, K);

			Mat3 RKI;
			if (mapRelatives.find(ki) != mapRelatives.end())
				RKI = mapRelatives.find(ki)->second.first;
			else
				RKI = mapRelatives.find(ik)->second.first.transpose();

			Mat3 Rot_To_Identity = RIJ * RJK * RKI; // motion composition
			double angularErrorDegree = R2D(getRotationMagnitude(Rot_To_Identity));
			vec_errToIdentityPerTriplet.push_back(angularErrorDegree);

			if (angularErrorDegree < 2.0)
			{
				vec_triplets_validated.push_back(triplet);

				if (mapRelatives.find(ij) != mapRelatives.end())
					map_relatives_validated[ij] = mapRelatives.find(ij)->second;
				else
					map_relatives_validated[ji] = mapRelatives.find(ji)->second;

				if (mapRelatives.find(jk) != mapRelatives.end())
					map_relatives_validated[jk] = mapRelatives.find(jk)->second;
				else
					map_relatives_validated[kj] = mapRelatives.find(kj)->second;

				if (mapRelatives.find(ki) != mapRelatives.end())
					map_relatives_validated[ki] = mapRelatives.find(ki)->second;
				else
					map_relatives_validated[ik] = mapRelatives.find(ik)->second;
			}
		}

		mapRelatives = std::move(map_relatives_validated);

		// Display statistics about rotation triplets error:
		std::cout << "\nStatistics about rotation triplets:" << std::endl;
		minMaxMeanMedian<double>(vec_errToIdentityPerTriplet.begin(), vec_errToIdentityPerTriplet.end());

		std::sort(vec_errToIdentityPerTriplet.begin(), vec_errToIdentityPerTriplet.end());

		Histogram<double> histo(0.0, *max_element(vec_errToIdentityPerTriplet.begin(), vec_errToIdentityPerTriplet.end()), 180);
		histo.Add(vec_errToIdentityPerTriplet.begin(), vec_errToIdentityPerTriplet.end());

		svgHisto histosvg;
		histosvg.draw(histo.GetHist(),
			std::make_pair<float, float>(0.0f, (float)(*max_element(vec_errToIdentityPerTriplet.begin(), vec_errToIdentityPerTriplet.end()))),
			stlplus::create_filespec(this->_sOutDirectory, "Triplet_Rotation_Residual_180.svg"),
			600, 300);

		histo = Histogram<double>(0.0, *max_element(vec_errToIdentityPerTriplet.begin(), vec_errToIdentityPerTriplet.end()), 20);
		histo.Add(vec_errToIdentityPerTriplet.begin(), vec_errToIdentityPerTriplet.end());

		histosvg.draw(histo.GetHist(),
			std::make_pair<float, float>(0.0f, (float)(*max_element(vec_errToIdentityPerTriplet.begin(), vec_errToIdentityPerTriplet.end()))),
			stlplus::create_filespec(this->_sOutDirectory, "Triplet_Rotation_Residual_20.svg"),
			600, 300);

		typedef lemon::ListGraph Graph;
		imageGraph::indexedImageGraph putativeGraph(_map_Matches_F, _vec_fileNames);

		Graph::EdgeMap<bool> edge_filter(putativeGraph.g, false);
		Graph::NodeMap<bool> node_filter(putativeGraph.g, true);

		typedef SubGraph<Graph > subGraphT;
		subGraphT sg(putativeGraph.g, node_filter, edge_filter);

		// Look all edges of the graph and look if exist in one triplet
		for (Graph::EdgeIt iter(putativeGraph.g); iter != INVALID; ++iter)
		{
			size_t Idu = (*putativeGraph.map_nodeMapIndex)[sg.u(iter)];
			size_t Idv = (*putativeGraph.map_nodeMapIndex)[sg.v(iter)];
			//-- Look if the edge Idu,Idv exists in the trifocal tensor list
			for (size_t i = 0; i < vec_triplets_validated.size(); ++i)
			{
				const graphUtils::Triplet & triplet = vec_triplets_validated[i];
				if (triplet.contain(std::make_pair(Idu, Idv)))
				{
					edge_filter[iter] = true;
					break;
				}
			}
		}

		imageGraph::exportToGraphvizData(
			stlplus::create_filespec(_sOutDirectory, "cleanedGraphTripletRotation"),
			sg);

		{
			std::cout << "\nTriplets filtering based on error on cycles \n";
			std::cout << "Before : " << vec_triplets.size() << " triplets \n"
				<< "After : " << vec_triplets_validated.size() << std::endl;
			std::cout << "There is " << lemon::countConnectedComponents(sg)
				<< " Connected Component in the filtered graph" << std::endl;
		}

		vec_triplets.clear();
		vec_triplets = vec_triplets_validated;

		size_t removedEdgesCount = 0;

		//-- Remove false edges from the rejected triplets
		{
			for (Graph::EdgeIt iter(putativeGraph.g); iter != INVALID; ++iter)
			{
				if (!edge_filter[iter])
				{
					removedEdgesCount++;

					size_t Idu = (*putativeGraph.map_nodeMapIndex)[sg.u(iter)];
					size_t Idv = (*putativeGraph.map_nodeMapIndex)[sg.v(iter)];

					//-- Clean relatives matches
					STLPairWiseMatches::iterator iterF = _map_Matches_F.find(std::make_pair(Idu, Idv));
					if (iterF != _map_Matches_F.end())
					{
						_map_Matches_F.erase(iterF);
					}
					else
					{
						iterF = _map_Matches_F.find(std::make_pair(Idv, Idu));
						if (iterF != _map_Matches_F.end())
							_map_Matches_F.erase(iterF);
					}

					//-- Clean relative motions
					auto iterF2 = mapRelatives.find(std::make_pair(Idu, Idv));
					if (iterF2 != mapRelatives.end())
					{
						mapRelatives.erase(iterF2);
					}
					else
					{
						iterF2 = mapRelatives.find(std::make_pair(Idv, Idu));
						if (iterF2 != mapRelatives.end())
							mapRelatives.erase(iterF2);
					}
				}
			}
		}

		std::cout << "\n Relatives edges removed by triplet checking : " << removedEdgesCount << std::endl;
	}

	void GlobalReconstructionEngine::bundleAdjustment_t_Xi(
		std::map<size_t, PinholeCamera> & map_camera,
		std::vector<Vec3> & vec_allScenes,
		const STLMAPTracks & map_tracksSelected)
	{
		using namespace std;

		const size_t nbCams = map_camera.size();
		const size_t nbPoints3D = vec_allScenes.size();

		// Count the number of measurement (sum of the reconstructed track length)
		size_t nbmeasurements = 0;
		for (STLMAPTracks::const_iterator iterTracks = map_tracksSelected.begin();
			iterTracks != map_tracksSelected.end(); ++iterTracks)
		{
			const submapTrack & subTrack = iterTracks->second;
			nbmeasurements += subTrack.size();
		}

		// Setup a BA problem
		using namespace openMVG::bundle_adjustment;
		BA_Problem_data<3> ba_problem; // Will refine translation and 3D points

		// Configure the size of the problem
		ba_problem.num_cameras_ = nbCams;
		ba_problem.num_points_ = nbPoints3D;
		ba_problem.num_observations_ = nbmeasurements;

		ba_problem.point_index_.reserve(ba_problem.num_observations_);
		ba_problem.camera_index_.reserve(ba_problem.num_observations_);
		ba_problem.observations_.reserve(2 * ba_problem.num_observations_);

		ba_problem.num_parameters_ = 3 * ba_problem.num_cameras_ + 3 * ba_problem.num_points_;
		ba_problem.parameters_.reserve(ba_problem.num_parameters_);

		// Fill camera
		std::vector<double> vec_Rot(map_camera.size() * 3, 0.0);
		size_t i = 0;
		std::map<size_t, size_t> map_camIndexToNumber;
		for (std::map<size_t, PinholeCamera >::const_iterator iter = map_camera.begin();
			iter != map_camera.end();  ++iter, ++i)
		{
			// in order to map camera index to contiguous number
			map_camIndexToNumber.insert(std::make_pair(iter->first, i));

			Mat3 R = iter->second._R;
			double angleAxis[3];
			ceres::RotationMatrixToAngleAxis((const double*)R.data(), angleAxis);
			vec_Rot[i * 3] = angleAxis[0];
			vec_Rot[i * 3 + 1] = angleAxis[1];
			vec_Rot[i * 3 + 2] = angleAxis[2];

			// translation
			const Vec3 & t = iter->second._t;
			ba_problem.parameters_.push_back(t[0]);
			ba_problem.parameters_.push_back(t[1]);
			ba_problem.parameters_.push_back(t[2]);
		}

		// Fill 3D points
		for (std::vector<Vec3>::const_iterator iter = vec_allScenes.begin();
			iter != vec_allScenes.end();
			++iter)
		{
			const Vec3 & pt3D = *iter;
			ba_problem.parameters_.push_back(pt3D[0]);
			ba_problem.parameters_.push_back(pt3D[1]);
			ba_problem.parameters_.push_back(pt3D[2]);
		}

		// Fill the measurements
		size_t j = 0;
		for (STLMAPTracks::const_iterator iterTracks = map_tracksSelected.begin();
			iterTracks != map_tracksSelected.end(); ++iterTracks, ++j)
		{
			const size_t trackId = iterTracks->first;
			// Look through the track and add point position
			const tracks::submapTrack & track = iterTracks->second;

			for (tracks::submapTrack::const_iterator iterTrack = track.begin();
				iterTrack != track.end();
				++iterTrack)
			{
				size_t imageId = iterTrack->first;
				size_t featId = iterTrack->second;

				// If imageId reconstructed :
				//  - Add measurements (the feature position)
				//  - Add camidx (map the image number to the camera index)
				//  - Add ptidx (the 3D corresponding point index) (must be increasing)

				//if ( set_camIndex.find(imageId) != set_camIndex.end())
				{
					const std::vector<SIOPointFeature> & vec_feats = _map_feats[imageId];
					const SIOPointFeature & ptFeat = vec_feats[featId];
					const PinholeCamera & cam = map_camera[imageId];

					double ppx = cam._K(0, 2), ppy = cam._K(1, 2);
					ba_problem.observations_.push_back(ptFeat.x() - ppx);
					ba_problem.observations_.push_back(ptFeat.y() - ppy);

					ba_problem.point_index_.push_back(j);
					ba_problem.camera_index_.push_back(map_camIndexToNumber[imageId]);
				}
			}
		}

		// The same K matrix is used by all the camera
		const Mat3 _K = _vec_intrinsicGroups[0].m_K;

		// Create residuals for each observation in the bundle adjustment problem. The
		// parameters for cameras and points are added automatically.
		ceres::Problem problem;
		// Set a LossFunction to be less penalized by false measurements
		//  - set it to NULL if you don't want use a lossFunction.
		ceres::LossFunction * p_LossFunction = new ceres::HuberLoss(Square(4.0));
		for (size_t m = 0; m < ba_problem.num_observations(); ++m) {
			// Each Residual block takes a point and a camera as input and outputs a 2
			// dimensional residual. Internally, the cost function stores the observed
			// image location and compares the reprojection against the observation.

			ceres::CostFunction* cost_function =
				new ceres::AutoDiffCostFunction<PinholeReprojectionError_t, 2, 3, 3>(
				new PinholeReprojectionError_t(
				&ba_problem.observations()[2 * m + 0],
				_K(0, 0),
				&vec_Rot[ba_problem.camera_index_[m] * 3]));

			problem.AddResidualBlock(cost_function,
				p_LossFunction,
				ba_problem.mutable_camera_for_observation(m),
				ba_problem.mutable_point_for_observation(m));
		}
		// Configure a BA engine and run it
		//  Make Ceres automatically detect the bundle structure.
		ceres::Solver::Options options;
		options.preconditioner_type = ceres::JACOBI;
		options.linear_solver_type = ceres::SPARSE_SCHUR;
		if (ceres::IsSparseLinearAlgebraLibraryTypeAvailable(ceres::SUITE_SPARSE))
			options.sparse_linear_algebra_library_type = ceres::SUITE_SPARSE;
		else
		if (ceres::IsSparseLinearAlgebraLibraryTypeAvailable(ceres::CX_SPARSE))
			options.sparse_linear_algebra_library_type = ceres::CX_SPARSE;
		else
		{
			// No sparse backend for Ceres.
			// Use dense solving
			options.linear_solver_type = ceres::DENSE_SCHUR;
		}
		options.minimizer_progress_to_stdout = false;
		options.logging_type = ceres::SILENT;
		//#ifdef USE_OPENMP
		//		options.num_threads = omp_get_num_threads();
		//		options.num_linear_solver_threads = omp_get_num_threads();
		//#endif // USE_OPENMP

		options.num_threads = static_cast<int>(std::thread::hardware_concurrency());
		options.num_linear_solver_threads = static_cast<int>(std::thread::hardware_concurrency());

		// Solve BA
		ceres::Solver::Summary summary;
		ceres::Solve(options, &problem, &summary);
		std::cout << summary.FullReport() << std::endl;

		// If no error, get back refined parameters
		if (summary.termination_type != ceres::DID_NOT_RUN &&
			summary.termination_type != ceres::USER_ABORT &&
			summary.termination_type != ceres::NUMERICAL_FAILURE)
		{
			// Get back 3D points
			i = 0;
			for (std::vector<Vec3>::iterator iter = vec_allScenes.begin();
				iter != vec_allScenes.end(); ++iter, ++i)
			{
				const double * pt = ba_problem.mutable_points() + i * 3;
				Vec3 & pt3D = *iter;
				pt3D = Vec3(pt[0], pt[1], pt[2]);
			}
			plyHelper::exportToPly(vec_allScenes, stlplus::create_filespec(_sOutDirectory, "raw_pointCloud_BA_T_Xi", "ply"));

			// Get back camera
			i = 0;
			for (std::map<size_t, PinholeCamera >::iterator iter = map_camera.begin();
				iter != map_camera.end(); ++iter, ++i)
			{
				const double * cam = ba_problem.mutable_cameras() + i * 3;

				Vec3 t(cam[0], cam[1], cam[2]);
				PinholeCamera & sCam = iter->second;
				Mat3 K = sCam._K;
				Mat3 R = sCam._R;
				sCam = PinholeCamera(K, R, t);
			}

			{
				//-- Export First bundle adjustment statistics
				if (_bHtmlReport)
				{
					using namespace htmlDocument;
					std::ostringstream os;
					os << "First Bundle Adjustment (Ti, Xjs) statistics.";
					_htmlDocStream->pushInfo("<hr>");
					_htmlDocStream->pushInfo(htmlMarkup("h1", os.str()));

					os.str("");
					os << "-------------------------------" << "<br>"
						<< "-- #tracks: " << map_tracksSelected.size() << ".<br>"
						<< "-- #observation: " << ba_problem.num_observations_ << ".<br>"
						<< "-- initial residual mean (RMSE): " << std::sqrt(summary.initial_cost / ba_problem.num_observations_) << ".<br>"
						<< "-- residual mean (RMSE): " << std::sqrt(summary.final_cost / ba_problem.num_observations_) << ".<br>"
						<< "-- Nb Steps required until convergence : " << summary.num_successful_steps + summary.num_unsuccessful_steps << ".<br>"
						<< "-------------------------------" << "<br>";
					_htmlDocStream->pushInfo(os.str());
				}
			}
		}
	}

	void GlobalReconstructionEngine::bundleAdjustment_Rt_Xi(
		std::map<size_t, PinholeCamera> & map_camera,
		std::vector<Vec3> & vec_allScenes,
		const STLMAPTracks & map_tracksSelected)
	{
		using namespace std;

		const size_t nbCams = map_camera.size();
		const size_t nbPoints3D = vec_allScenes.size();

		// Count the number of measurement (sum of the reconstructed track length)
		size_t nbmeasurements = 0;
		for (STLMAPTracks::const_iterator iterTracks = map_tracksSelected.begin();
			iterTracks != map_tracksSelected.end(); ++iterTracks)
		{
			const submapTrack & subTrack = iterTracks->second;
			nbmeasurements += subTrack.size();
		}

		// Setup a BA problem
		using namespace openMVG::bundle_adjustment;
		BA_Problem_data<6> ba_problem; // Will refine translation and 3D points

		// Configure the size of the problem
		ba_problem.num_cameras_ = nbCams;
		ba_problem.num_points_ = nbPoints3D;
		ba_problem.num_observations_ = nbmeasurements;

		ba_problem.point_index_.reserve(ba_problem.num_observations_);
		ba_problem.camera_index_.reserve(ba_problem.num_observations_);
		ba_problem.observations_.reserve(2 * ba_problem.num_observations_);

		ba_problem.num_parameters_ = 6 * ba_problem.num_cameras_ + 3 * ba_problem.num_points_;
		ba_problem.parameters_.reserve(ba_problem.num_parameters_);

		// Fill camera
		size_t i = 0;
		std::map<size_t, size_t> map_camIndexToNumber;
		for (std::map<size_t, PinholeCamera >::const_iterator iter = map_camera.begin();
			iter != map_camera.end();  ++iter, ++i)
		{
			// in order to map camera index to contiguous number
			map_camIndexToNumber.insert(std::make_pair(iter->first, i));

			const Mat3 & R = iter->second._R;
			double angleAxis[3];
			ceres::RotationMatrixToAngleAxis((const double*)R.data(), angleAxis);

			// translation
			const Vec3 & t = iter->second._t;

			ba_problem.parameters_.push_back(angleAxis[0]);
			ba_problem.parameters_.push_back(angleAxis[1]);
			ba_problem.parameters_.push_back(angleAxis[2]);
			ba_problem.parameters_.push_back(t[0]);
			ba_problem.parameters_.push_back(t[1]);
			ba_problem.parameters_.push_back(t[2]);
		}

		// Fill 3D points
		for (std::vector<Vec3>::const_iterator iter = vec_allScenes.begin();
			iter != vec_allScenes.end();
			++iter)
		{
			const Vec3 & pt3D = *iter;
			ba_problem.parameters_.push_back(pt3D[0]);
			ba_problem.parameters_.push_back(pt3D[1]);
			ba_problem.parameters_.push_back(pt3D[2]);
		}

		// Fill the measurements
		i = 0;
		for (STLMAPTracks::const_iterator iterTracks = map_tracksSelected.begin();
			iterTracks != map_tracksSelected.end(); ++iterTracks, ++i)
		{
			const size_t trackId = iterTracks->first;
			// Look through the track and add point position
			const tracks::submapTrack & track = iterTracks->second;

			for (tracks::submapTrack::const_iterator iterTrack = track.begin();
				iterTrack != track.end();
				++iterTrack)
			{
				size_t imageId = iterTrack->first;
				size_t featId = iterTrack->second;

				// If imageId reconstructed :
				//  - Add measurements (the feature position)
				//  - Add camidx (map the image number to the camera index)
				//  - Add ptidx (the 3D corresponding point index) (must be increasing)

				//if ( set_camIndex.find(imageId) != set_camIndex.end())
				{
					const std::vector<SIOPointFeature> & vec_feats = _map_feats[imageId];
					const SIOPointFeature & ptFeat = vec_feats[featId];
					const PinholeCamera & cam = map_camera[imageId];

					double ppx = cam._K(0, 2), ppy = cam._K(1, 2);
					ba_problem.observations_.push_back(ptFeat.x() - ppx);
					ba_problem.observations_.push_back(ptFeat.y() - ppy);

					ba_problem.point_index_.push_back(i);
					ba_problem.camera_index_.push_back(map_camIndexToNumber[imageId]);
				}
			}
		}

		// The same K matrix is used by all the camera
		const Mat3 _K = _vec_intrinsicGroups[0].m_K;

		// Create residuals for each observation in the bundle adjustment problem. The
		// parameters for cameras and points are added automatically.
		ceres::Problem problem;
		// Set a LossFunction to be less penalized by false measurements
		//  - set it to NULL if you don't want use a lossFunction.
		ceres::LossFunction * p_LossFunction = new ceres::HuberLoss(Square(2.0));
		for (size_t k = 0; k < ba_problem.num_observations(); ++k) {
			// Each Residual block takes a point and a camera as input and outputs a 2
			// dimensional residual. Internally, the cost function stores the observed
			// image location and compares the reprojection against the observation.

			ceres::CostFunction* cost_function =
				new ceres::AutoDiffCostFunction<PinholeReprojectionError_Rt, 2, 6, 3>(
				new PinholeReprojectionError_Rt(
				&ba_problem.observations()[2 * k + 0], _K(0, 0)));

			problem.AddResidualBlock(cost_function,
				p_LossFunction,
				ba_problem.mutable_camera_for_observation(k),
				ba_problem.mutable_point_for_observation(k));
		}
		// Configure a BA engine and run it
		//  Make Ceres automatically detect the bundle structure.
		ceres::Solver::Options options;
		options.preconditioner_type = ceres::JACOBI;
		options.linear_solver_type = ceres::SPARSE_SCHUR;
		if (ceres::IsSparseLinearAlgebraLibraryTypeAvailable(ceres::SUITE_SPARSE))
			options.sparse_linear_algebra_library_type = ceres::SUITE_SPARSE;
		else
		if (ceres::IsSparseLinearAlgebraLibraryTypeAvailable(ceres::CX_SPARSE))
			options.sparse_linear_algebra_library_type = ceres::CX_SPARSE;
		else
		{
			// No sparse backend for Ceres.
			// Use dense solving
			options.linear_solver_type = ceres::DENSE_SCHUR;
		}
		options.minimizer_progress_to_stdout = false;
		options.logging_type = ceres::SILENT;
		//#ifdef USE_OPENMP
		//		options.num_threads = omp_get_num_threads();
		//		options.num_linear_solver_threads = omp_get_num_threads();
		//#endif // USE_OPENMP

		options.num_threads = static_cast<int>(std::thread::hardware_concurrency());
		options.num_linear_solver_threads = static_cast<int>(std::thread::hardware_concurrency());

		// Solve BA
		ceres::Solver::Summary summary;
		ceres::Solve(options, &problem, &summary);
		std::cout << summary.FullReport() << std::endl;

		// If no error, get back refined parameters
		if (summary.termination_type != ceres::DID_NOT_RUN &&
			summary.termination_type != ceres::USER_ABORT &&
			summary.termination_type != ceres::NUMERICAL_FAILURE)
		{
			// Get back 3D points
			i = 0;
			for (std::vector<Vec3>::iterator iter = vec_allScenes.begin();
				iter != vec_allScenes.end(); ++iter, ++i)
			{
				const double * pt = ba_problem.mutable_points() + i * 3;
				Vec3 & pt3D = *iter;
				pt3D = Vec3(pt[0], pt[1], pt[2]);
			}
			plyHelper::exportToPly(vec_allScenes, stlplus::create_filespec(_sOutDirectory, "raw_pointCloud_BA_RT_Xi", "ply"));

			// Get back camera
			i = 0;
			for (std::map<size_t, PinholeCamera >::iterator iter = map_camera.begin();
				iter != map_camera.end(); ++iter, ++i)
			{
				const double * cam = ba_problem.mutable_cameras() + i * 6;
				Mat3 R;
				// angle axis to rotation matrix
				ceres::AngleAxisToRotationMatrix(cam, R.data());

				Vec3 t(cam[3], cam[4], cam[5]);
				double focal = cam[6];

				// Update the camera
				Mat3 K = iter->second._K;
				PinholeCamera & sCam = iter->second;
				sCam = PinholeCamera(K, R, t);
			}

			{
				//-- Export Second bundle adjustment statistics
				if (_bHtmlReport)
				{
					using namespace htmlDocument;
					std::ostringstream os;
					os << "Second Bundle Adjustment (Ri, Ti, Xjs) statistics.";
					_htmlDocStream->pushInfo("<hr>");
					_htmlDocStream->pushInfo(htmlMarkup("h1", os.str()));

					os.str("");
					os << "-------------------------------" << "<br>"
						<< "-- #tracks: " << map_tracksSelected.size() << ".<br>"
						<< "-- #observation: " << ba_problem.num_observations_ << ".<br>"
						<< "-- residual mean (RMSE): " << std::sqrt(summary.final_cost / ba_problem.num_observations_) << ".<br>"
						<< "-- Nb Steps required until convergence : " << summary.num_successful_steps + summary.num_unsuccessful_steps << ".<br>"
						<< "-------------------------------" << "<br>";
					_htmlDocStream->pushInfo(os.str());
				}

				std::cout << "\n"
					<< "-------------------------------" << "\n"
					<< "-- #tracks: " << map_tracksSelected.size() << ".\n"
					<< "-- #observation: " << ba_problem.num_observations_ << ".\n"
					<< "-- residual mean (RMSE): " << std::sqrt(summary.final_cost / ba_problem.num_observations_) << ".\n"
					<< "-- Nb Steps required until convergence : " << summary.num_successful_steps + summary.num_unsuccessful_steps << ".\n"
					<< "-------------------------------" << std::endl;

			}
		}
	}

	void GlobalReconstructionEngine::ColorizeTracks(const STLMAPTracks & map_tracks, std::vector<Vec3> & vec_tracksColor) const
	{
		// Colorize each track
		//  Start with the most representative image
		//    and iterate to provide a color to each 3D point
		{

			vec_tracksColor.resize(map_tracks.size());

			// The track list that will be colored (point removed during the process)
			auto iterTBegin = map_tracks.begin();
			openMVG::tracks::STLMAPTracks mapTrackToColor(map_tracks);
			while (!mapTrackToColor.empty())
			{
				// Find the most representative image
				//  a. Count the number of visible point for each image
				//  b. Sort to find the most representative image

				std::map<size_t, size_t> map_IndexCardinal; // ImageIndex, Cardinal
				for (auto iterT = mapTrackToColor.begin(); iterT != mapTrackToColor.end(); ++iterT)
				{
					const size_t trackId = iterT->first;
					const tracks::submapTrack & track = mapTrackToColor[trackId];
					for (auto iterTrack = track.begin(); iterTrack != track.end(); ++iterTrack)
					{
						const size_t imageId = iterTrack->first;
						if (map_IndexCardinal.find(imageId) == map_IndexCardinal.end())
							map_IndexCardinal[imageId] = 1;
						else
							++map_IndexCardinal[imageId];
					}
				}

				// Find the image that is the most represented
				std::vector<size_t> vec_cardinal;
				std::transform(map_IndexCardinal.begin(),
					map_IndexCardinal.end(),
					std::back_inserter(vec_cardinal),
					RetrieveValue());
				using namespace indexed_sort;
				std::vector< sort_index_packet_descend< size_t, size_t> > packet_vec(vec_cardinal.size());
				sort_index_helper(packet_vec, &vec_cardinal[0]);

				//First index is the image with the most of matches
				std::map<size_t, size_t>::const_iterator iterTT = map_IndexCardinal.begin();
				std::advance(iterTT, packet_vec[0].index);
				const size_t indexImage = iterTT->first;
				Image<RGBColor> image;
				ReadImage(
					stlplus::create_filespec(
					_sImagePath,
					stlplus::basename_part(_vec_camImageNames[indexImage].m_sImageName),
					stlplus::extension_part(_vec_camImageNames[indexImage].m_sImageName)).c_str(), &image);

				// Iterate through the track
				std::set<size_t> set_toRemove;
				for (auto iterT = mapTrackToColor.begin(); iterT != mapTrackToColor.end(); ++iterT)
				{
					const size_t trackId = iterT->first;
					const tracks::submapTrack & track = mapTrackToColor[trackId];
					tracks::submapTrack::const_iterator iterF = track.find(indexImage);

					if (iterF != track.end())
					{
						// Color the track
						size_t featId = iterF->second;
						const SIOPointFeature & feat = _map_feats.find(indexImage)->second[featId];
						RGBColor color = image(feat.y(), feat.x());

						vec_tracksColor[std::distance(iterTBegin, map_tracks.find(trackId))] = Vec3(color.r(), color.g(), color.b());
						set_toRemove.insert(trackId);
					}
				}
				// Remove colored track
				for (std::set<size_t>::const_iterator iter = set_toRemove.begin();
					iter != set_toRemove.end(); ++iter)
				{
					mapTrackToColor.erase(*iter);
				}
			}
		}
	}

	void GlobalReconstructionEngine::runRelativeRtThread()
	{
		while (true)
		{
			int && i = jobs++;
			if (i >= (int)_map_Matches_F.size()) break;

			std::cout << "RelativeRt ThreadID = " << std::this_thread::get_id() << ", jobs = " << i << std::endl;

			map< std::pair<size_t, size_t>, vector<IndMatch> >::const_iterator iter = _map_Matches_F.begin();
			std::advance(iter, i);

			const size_t I = iter->first.first;
			const size_t J = iter->first.second;

			const std::vector<IndMatch> & vec_matchesInd = iter->second;

			Mat x1(2, vec_matchesInd.size()), x2(2, vec_matchesInd.size());
			for (size_t k = 0; k < vec_matchesInd.size(); ++k)
			{
				x1.col(k) = _map_feats[I][vec_matchesInd[k]._i].coords().cast<double>();
				x2.col(k) = _map_feats[J][vec_matchesInd[k]._j].coords().cast<double>();
			}

			Mat3 E;
			std::vector<size_t> vec_inliers;

			std::pair<size_t, size_t> imageSize_I(
				_vec_intrinsicGroups[_vec_camImageNames[I].m_intrinsicId].m_w,
				_vec_intrinsicGroups[_vec_camImageNames[I].m_intrinsicId].m_h);

			std::pair<size_t, size_t> imageSize_J(
				_vec_intrinsicGroups[_vec_camImageNames[J].m_intrinsicId].m_w,
				_vec_intrinsicGroups[_vec_camImageNames[J].m_intrinsicId].m_h);

			const Mat3 K = _vec_intrinsicGroups[_vec_camImageNames[I].m_intrinsicId].m_K;

			double errorMax = std::numeric_limits<double>::max();
			double maxExpectedError = 2.5;
			if (!SfMRobust::robustEssential(K, K,
				x1, x2,
				&E, &vec_inliers,
				imageSize_I, imageSize_J,
				&errorMax,
				maxExpectedError))
			{
				std::cerr << " /!\\ Robust estimation failed to compute E for this pair"
					<< std::endl;
				continue;
			}
			else
			{
				//--> Estimate the best possible Rotation/Translation from E
				Mat3 R;
				Vec3 t;
				if (!SfMRobust::estimate_Rt_fromE(K, K, x1, x2, E, vec_inliers, &R, &t))
				{
					std::cout << " /!\\ Failed to compute initial R|t for the initial pair"
						<< std::endl;
					continue;
				}
				else
				{
					PinholeCamera cam1(K, Mat3::Identity(), Vec3::Zero());
					PinholeCamera cam2(K, R, t);

					std::vector<Vec3> vec_allScenes;
					vec_allScenes.resize(x1.cols());
					for (size_t k = 0; k < x1.cols(); ++k)
					{
						const Vec2 & x1_ = x1.col(k),
							&x2_ = x2.col(k);
						Vec3 X;
						TriangulateDLT(cam1._P, x1_, cam2._P, x2_, &X);
						vec_allScenes[k] = X;
					}

					// Refine Xis, tis and Ris (Bundle Adjustment)
					{
						using namespace std;

						const size_t nbCams = 2;
						const size_t nbPoints3D = vec_allScenes.size();

						// Count the number of measurement (sum of the reconstructed track length)
						size_t nbmeasurements = x1.cols() * 2;

						// Setup a BA problem
						using namespace openMVG::bundle_adjustment;
						BA_Problem_data<6> ba_problem; // Will refine translation and 3D points

						// Configure the size of the problem
						ba_problem.num_cameras_ = nbCams;
						ba_problem.num_points_ = nbPoints3D;
						ba_problem.num_observations_ = nbmeasurements;

						ba_problem.point_index_.reserve(ba_problem.num_observations_);
						ba_problem.camera_index_.reserve(ba_problem.num_observations_);
						ba_problem.observations_.reserve(2 * ba_problem.num_observations_);

						ba_problem.num_parameters_ = 6 * ba_problem.num_cameras_ + 3 * ba_problem.num_points_;
						ba_problem.parameters_.reserve(ba_problem.num_parameters_);

						// Fill camera
						{
							Mat3 R = cam1._R;
							double angleAxis[3];
							ceres::RotationMatrixToAngleAxis((const double*)R.data(), angleAxis);

							// translation
							Vec3 t = cam1._t;

							ba_problem.parameters_.push_back(angleAxis[0]);
							ba_problem.parameters_.push_back(angleAxis[1]);
							ba_problem.parameters_.push_back(angleAxis[2]);
							ba_problem.parameters_.push_back(t[0]);
							ba_problem.parameters_.push_back(t[1]);
							ba_problem.parameters_.push_back(t[2]);
						}
						{
							Mat3 R = cam2._R;
							double angleAxis[3];
							ceres::RotationMatrixToAngleAxis((const double*)R.data(), angleAxis);

							// translation
							Vec3 t = cam2._t;

							ba_problem.parameters_.push_back(angleAxis[0]);
							ba_problem.parameters_.push_back(angleAxis[1]);
							ba_problem.parameters_.push_back(angleAxis[2]);
							ba_problem.parameters_.push_back(t[0]);
							ba_problem.parameters_.push_back(t[1]);
							ba_problem.parameters_.push_back(t[2]);
						}

						// Fill 3D points
						for (std::vector<Vec3>::const_iterator iter = vec_allScenes.begin();
							iter != vec_allScenes.end();
							++iter)
						{
							const Vec3 & pt3D = *iter;
							ba_problem.parameters_.push_back(pt3D[0]);
							ba_problem.parameters_.push_back(pt3D[1]);
							ba_problem.parameters_.push_back(pt3D[2]);
						}

						// Fill the measurements
						for (size_t k = 0; k < x1.cols(); ++k) {
							const Vec2 & x1_ = x1.col(k), &x2_ = x2.col(k);

							const Mat3 & K = cam1._K;

							double ppx = K(0, 2), ppy = K(1, 2);
							ba_problem.observations_.push_back(x1_(0) - ppx);
							ba_problem.observations_.push_back(x1_(1) - ppy);
							ba_problem.point_index_.push_back(k);
							ba_problem.camera_index_.push_back(0);

							ba_problem.observations_.push_back(x2_(0) - ppx);
							ba_problem.observations_.push_back(x2_(1) - ppy);
							ba_problem.point_index_.push_back(k);
							ba_problem.camera_index_.push_back(1);
						}

						// Create residuals for each observation in the bundle adjustment problem. The
						// parameters for cameras and points are added automatically.
						ceres::Problem problem;
						// Set a LossFunction to be less penalized by false measurements
						//  - set it to NULL if you don't want use a lossFunction.
						ceres::LossFunction * p_LossFunction = new ceres::HuberLoss(Square(2.0));
						for (size_t i = 0; i < ba_problem.num_observations(); ++i) {
							// Each Residual block takes a point and a camera as input and outputs a 2
							// dimensional residual. Internally, the cost function stores the observed
							// image location and compares the reprojection against the observation.

							ceres::CostFunction* cost_function =
								new ceres::AutoDiffCostFunction<PinholeReprojectionError_Rt, 2, 6, 3>(
								new PinholeReprojectionError_Rt(
								&ba_problem.observations()[2 * i + 0], K(0, 0)));

							problem.AddResidualBlock(cost_function,
								p_LossFunction,
								ba_problem.mutable_camera_for_observation(i),
								ba_problem.mutable_point_for_observation(i));
						}

						// Configure a BA engine and run it
						//  Make Ceres automatically detect the bundle structure.
						ceres::Solver::Options options;
						// Use a dense back-end since we only consider a two view problem
						options.linear_solver_type = ceres::DENSE_SCHUR;
						options.minimizer_progress_to_stdout = false;
						options.logging_type = ceres::SILENT;

						// Solve BA
						ceres::Solver::Summary summary;
						ceres::Solve(options, &problem, &summary);

						// If no error, get back refined parameters
						if (summary.termination_type != ceres::DID_NOT_RUN &&
							summary.termination_type != ceres::USER_ABORT &&
							summary.termination_type != ceres::NUMERICAL_FAILURE)
						{
							// Get back 3D points
							size_t k = 0;
							for (std::vector<Vec3>::iterator iter = vec_allScenes.begin();
								iter != vec_allScenes.end(); ++iter, ++k)
							{
								const double * pt = ba_problem.mutable_points() + k * 3;
								Vec3 & pt3D = *iter;
								pt3D = Vec3(pt[0], pt[1], pt[2]);
							}

							// Get back camera 1
							{
								const double * cam = ba_problem.mutable_cameras() + 0 * 6;
								Mat3 R;
								// angle axis to rotation matrix
								ceres::AngleAxisToRotationMatrix(cam, R.data());

								Vec3 t(cam[3], cam[4], cam[5]);
								double focal = cam[6];

								// Update the camera
								Mat3 K = cam1._K;
								PinholeCamera & sCam = cam1;
								sCam = PinholeCamera(K, R, t);
							}
							// Get back camera 2
							{
								const double * cam = ba_problem.mutable_cameras() + 1 * 6;
								Mat3 R;
								// angle axis to rotation matrix
								ceres::AngleAxisToRotationMatrix(cam, R.data());

								Vec3 t(cam[3], cam[4], cam[5]);
								double focal = cam[6];

								// Update the camera
								Mat3 K = cam2._K;
								PinholeCamera & sCam = cam2;
								sCam = PinholeCamera(K, R, t);
							}
							RelativeCameraMotion(cam1._R, cam1._t, cam2._R, cam2._t, &R, &t);
						}
					}

					std::unique_lock<std::mutex> lck(mtx);
					mapRelatives[iter->first] = std::make_pair(R, t);
					//std::cout << "R = " << R << std::endl << "t = " << t << std::endl;
					//std::cout << "mapRelatives [ " << iter->first.first << " , " << iter->first.second << " ]" << std::endl;
				}
			}
		}
	}

	void GlobalReconstructionEngine::runSelectedTracks(std::set<size_t> & set_idx_to_remove, std::vector<double>& vec_residuals)
	{
		while (true)
		{
			int && idx = jobs++;
			if (idx >= (int)_map_selectedTracks.size()) break;

			//std::cout << "SelectedTracks ThreadID = " << std::this_thread::get_id() << ", jobs = " << idx << std::endl;

			STLMAPTracks::const_iterator iterTracks = _map_selectedTracks.begin();
			std::advance(iterTracks, idx);

			const submapTrack & subTrack = iterTracks->second;

			// Look to the features required for the triangulation task
			size_t index = 0;
			Triangulation trianObj;
			for (submapTrack::const_iterator iter = subTrack.begin(); iter != subTrack.end(); ++iter, ++index) {

				const size_t imaIndex = iter->first;
				const size_t featIndex = iter->second;
				const SIOPointFeature & pt = _map_feats[imaIndex][featIndex];
				// Build the P matrix
				trianObj.add(_map_camera[imaIndex]._P, pt.coords().cast<double>());
			}

			// Compute the 3D point and keep point index with negative depth
			const Vec3 Xs = trianObj.compute();
			_vec_allScenes[idx] = Xs;
			if (trianObj.minDepth() < 0)
			{
				std::unique_lock<std::mutex> lck(mtx);
				set_idx_to_remove.insert(idx);
			}

			//-- Compute residual over all the projections
			for (auto& sTrack : subTrack)
			{
				const size_t imaIndex = sTrack.first;
				const size_t featIndex = sTrack.second;
				const SIOPointFeature & pt = _map_feats[imaIndex][featIndex];
				vec_residuals[idx] = _map_camera[imaIndex].Residual(Xs, pt.coords().cast<double>());
			}
		}
	}

} // namespace openMVG