#pragma once

#include "openMVG/exif_IO/exif_IO_openExif.hpp"
#include "openMVG/sensorWidthDatabase/ParseDatabase.hpp"
#include "openMVG/image/image.hpp"
#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"
#include "openMVG/features/features.hpp"

/// Generic Image Collection image matching
#include "openMVG/matching_image_collection/Matcher_AllInMemory.hpp"
#include "openMVG/matching_image_collection/GeometricFilter.hpp"
#include "openMVG/matching_image_collection/F_ACRobust.hpp"
#include "openMVG/matching_image_collection/E_ACRobust.hpp"
#include "openMVG/matching_image_collection/H_ACRobust.hpp"
#include "SfM/pairwiseAdjacencyDisplay.hpp"
#include "openMVG/matching/matcher_brute_force.hpp"
#include "openMVG/matching/matcher_kdtree_flann.hpp"
#include "openMVG/matching/indMatch_utils.hpp"

/// Feature detector and descriptor interface
#include "patented/sift/SIFT.hpp"

#include "SfM/SfMIncrementalEngine.hpp"
#include "SfM/SfMGlobalEngine.hpp"

#include "sfm/document.h"
#include "cmvs/bundle.h"

#include "pmvs/findMatch.h"
#include "pmvs/option.h"

//#include "TextureMesh.h"

#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace sfm
{
	enum eGeometricModel
	{
		FUNDAMENTAL_MATRIX = 0,
		ESSENTIAL_MATRIX = 1,
		HOMOGRAPHY_MATRIX = 2
	};

	class SFMToolKits
	{
	public:
		SFMToolKits();
		~SFMToolKits();

	private:
		static bool computeMatch(const std::string& imgDir, const std::string& outDir, const char gModel = 'f', float distratio = 0.8f, bool octminus1 = false, float peakThreshold = 0.01f);
		static bool exportToBundlerFormat(const std::string& sOutFile, const std::string& sOutListFile, const Document & doc);
		static bool exportToCMPMVSFormat(const std::string& imgDir, const std::string& outDir, const Document & doc);
		static bool exportToPMVSFormat(const std::string& imgDir, const std::string& outDir, const Document & doc, const int resolution = 1, const int cpuNu = 8);
	public:
		static bool computeFocal(const std::string& imgDir, const std::string& outDir, const std::string& dataBase = "config/cameras.txt", double focalPixPermm = -1);
		static bool computeMatchF(const std::string& imgDir, const std::string& outDir, float distratio = 0.8f, bool octminus1 = false, float peakThreshold = 0.01f);
		static bool computeMatchE(const std::string& imgDir, const std::string& outDir, float distratio = 0.8f, bool octminus1 = false, float peakThreshold = 0.01f);
		static bool computeMatchH(const std::string& imgDir, const std::string& outDir, float distratio = 0.8f, bool octminus1 = false, float peakThreshold = 0.01f);

		static bool incrementalSfM(const std::string& imgDir, const std::string& matcheDir, const std::string& outDir);
		static bool globalSfM(const std::string& imgDir, const std::string& matcheDir, const std::string& outDir);
		static bool exportToPMVS(const std::string& sfmOutDir, const std::string& outDir);
		static bool exportToCMPMVS(const std::string& sfmOutDir, const std::string& outDir);
		static bool exportToMESHLAB(const std::string& sfmOutDir, const std::string& ply, const std::string& outDir);
		static bool cmvs(const std::string& sfmOutDir);
		static bool pmvs(const std::string& pmvsOutDir, const std::string& optionFile);
		//static bool mesh(const std::string& pmvsOutDir);
	};
}

