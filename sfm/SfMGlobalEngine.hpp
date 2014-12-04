
// Copyright (c) 2012, 2013, 2014 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#pragma once
#ifndef OPENMVG_GLOBAL_SFM_ENGINE_H
#define OPENMVG_GLOBAL_SFM_ENGINE_H

#include "openMVG/numeric/numeric.h"

#include "openMVG/features/feature.hpp"
#include "openMVG/cameras/PinholeCamera.hpp"
#include "SfMEngine.hpp"
#include "SfMIOHelper.hpp"

#include "openMVG/tracks/tracks.hpp"
#include "openMVG/matching/indMatch.hpp"
using namespace openMVG::tracks;

#include "openMVG/graph/triplet_finder.hpp"
#include "openMVG/linearProgramming/lInfinityCV/global_translations_fromTriplets.hpp"

#include "mutexSet.hpp"

#include "third_party/htmlDoc/htmlDoc.hpp"

#include <memory>
#include <fstream>
#include <thread>
#include <mutex>
#include <atomic>

namespace openMVG{

	//------------------
	//-- Bibliography --
	//------------------
	//- [1] "Global Fusion of Relative Motions for Robust, Accurate and Scalable Structure from Motion."
	//- Authors: Pierre MOULON, Pascal MONASSE and Renaud MARLET.
	//- Date: December 2013.
	//- Conference: ICCV.
	//- [2] "Disambiguating visual relations using loop constraints."
	//- Autors: Christopher ZACH, Manfred KLOPSCHITZ and Marc POLLEFEYS.
	//- Date: 2010
	//- Conference : CVPR.
	//- [3] "Efficient and Robust Large-Scale Rotation Averaging"
	//- Authors: Avishek Chatterjee and Venu Madhav Govindu
	//- Date: December 2013.
	//- Conference: ICCV.

	// Implementation of [1]
	// Some points differs from the [1] paper to ease open source port:
	//-- Relative rotation inference:
	//   - only the triplet rejection is performed (in [1] a Bayesian inference on cycle error is performed [2])
	//-- Global rotation computation:
	//   - in [1] they are computed by a sparse least square formulation
	//   - here, can be used:
	//    - a simple dense least square,
	//    - or, the L1 averaging method of [3].
	//-- Linear Programming solver:
	//   - in order to have the best performance it is advised to used the MOSEK LP backend.

	enum ERotationAveragingMethod
	{
		ROTATION_AVERAGING_NONE = 0,
		ROTATION_AVERAGING_L1 = 1,
		ROTATION_AVERAGING_L2 = 2
	};

	class GlobalReconstructionEngine : public ReconstructionEngine
	{
	public:
		GlobalReconstructionEngine(const std::string & sImagePath,
			const std::string & sMatchesPath, const std::string & sOutDirectory,
			bool bHtmlReport = false);

		~GlobalReconstructionEngine();

		void setRotationMethod(unsigned chooseM = 1);

		virtual bool Process();

		/// Give a color to all the 3D points
		void ColorizeTracks(
			const STLMAPTracks & map_tracks, // tracks to be colorized
			std::vector<Vec3> & vec_tracksColor // output associated color
			) const;

		bool ExportToOpenMVGFormat(bool bColoredPointCloud = true) const;

		typedef std::map< std::pair<size_t, size_t>, std::pair<Mat3, Vec3> > RelativeType;
		typedef std::pair<size_t, size_t> myEdge;

	private:
		/// Read input data (point correspondences, K matrix)
		bool ReadInputData();

		bool CleanGraph();

		void ComputeRelativeRt();

		// Detect and remove the outlier relative rotations
		void rotationInference();

		// Compute the global rotations from relative rotations
		bool computeGlobalRotations(
			ERotationAveragingMethod eRotationAveragingMethod,
			const std::map<size_t, size_t> & map_cameraNodeToCameraIndex,
			const std::map<size_t, size_t> & map_cameraIndexTocameraNode,
			const RelativeType & map_relatives,
			std::map<size_t, Mat3> & map_globalR) const;

		// List the triplet of the image connection graph (_map_Matches_F)
		void tripletListing(std::vector< graphUtils::Triplet > & vec_triplets) const;

		// Relative rotations inference on relative rotations composition error along 3 length cycles (triplets).
		void tripletRotationRejection(std::vector< graphUtils::Triplet > & vec_triplets);

		// Compute relative translations over the graph of putative triplets
		void computePutativeTranslation_EdgesCoverage(
			const std::map<std::size_t, Mat3> & map_globalR,
			const std::vector< graphUtils::Triplet > & vec_triplets,
			std::vector<openMVG::lInfinityCV::relativeInfo > & vec_initialEstimates,
			tracks::mapPairWiseMatches & newpairMatches);

		// Bundle adjustment : refine structure Xis and camera translations
		void bundleAdjustment_t_Xi(
			std::map<size_t, PinholeCamera> & map_camera,
			std::vector<Vec3> & vec_allScenes,
			const STLMAPTracks & map_tracksSelected);

		// Bundle adjustment : refine structure Xis and camera rotations and translations
		void bundleAdjustment_Rt_Xi(
			std::map<size_t, PinholeCamera> & map_camera,
			std::vector<Vec3> & vec_allScenes,
			const STLMAPTracks & map_tracksSelected);

	private:

		// -----
		// Input data
		// ----
		unsigned chooseM;
		// Image considered for the reconstruction
		std::vector<std::string> _vec_fileNames;
		std::vector<openMVG::SfMIO::CameraInfo> _vec_camImageNames;
		std::vector<openMVG::SfMIO::IntrinsicCameraInfo> _vec_intrinsicGroups;
		std::map< size_t, std::vector<SIOPointFeature> > _map_feats; // feature per images

		typedef tracks::mapPairWiseMatches STLPairWiseMatches;
		STLPairWiseMatches _map_Matches_F; // pairwise matches for Essential matrix model


		//------
		//-- Mapping between camera node Ids and cameraIndex:
		//--------------
		// Graph node Ids mapping to camera Ids
		std::map<size_t, size_t> map_cameraNodeToCameraIndex; // graph node Id to 0->Ncam
		// camera Ids to graph node Ids
		std::map<size_t, size_t> map_cameraIndexTocameraNode; // 0->Ncam correspondance to graph node Id
		//--
		//----


		RelativeType mapRelatives;

		//-----
		//-- Reconstruction data
		//-----
		// Cameras (Motion)
		std::map<size_t, PinholeCamera> _map_camera;
		// Structure 
		std::vector<Vec3> _vec_allScenes;
		// Structure visibility
		STLMAPTracks _map_selectedTracks; // reconstructed track (visibility per 3D point)
		//-----


		// -----
		// Reporting ..
		// ----
		bool _bHtmlReport;
		std::auto_ptr<htmlDocument::htmlDocumentStream> _htmlDocStream;

		//thread
		mutable std::atomic_int jobs;
		mutable std::mutex mtx;
		void runRelativeRtThread();
		void runSelectedTracks(std::set<size_t> & set_idx_to_remove, std::vector<double>& vec_residuals);
		void runTripletThread(const std::vector< graphUtils::Triplet > & triplets, std::map<size_t, size_t>& mapTracksPerTriplets);

		//log
		ofstream logF;
		streambuf *stdFile;

	};


} // namespace openMVG

#endif // OPENMVG_GLOBAL_SFM_ENGINE_H
