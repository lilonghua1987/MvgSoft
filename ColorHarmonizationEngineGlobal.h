#pragma once
#include "SfMEngine.hpp"

#include <openMVG/numeric/numeric.h>
#include <openMVG/features/features.hpp>
#include <openMVG/tracks/tracks.hpp>

#include <memory>
#include <fstream>
#include <thread>
#include <mutex>
#include <atomic>

namespace openMVG{
	class ColorHarmonizationEngineGlobal : public ReconstructionEngine
	{
	public:
		ColorHarmonizationEngineGlobal(const std::string & sImagePath, const std::string & sMatchesPath, const std::string sMatchesFile,
			const std::string & sOutDirectory, const int selectionMethod = -1, const int imgRef = -1);
		~ColorHarmonizationEngineGlobal();

		virtual bool Process();

	private:
		bool CleanGraph();

		/// Read input data (point correspondences)
		bool ReadInputData();

	public:
		bool setRefParmas(const int selectionMethod = -1, const int imgRef = -1);
		const std::vector< std::string > & getFilenamesVector() const { return _vec_fileNames; }
		const std::vector< std::pair< size_t, size_t > > & getImagesSize() const { return _vec_imageSize; }

	private:
		int selectionMethod;
		int imgRef;
		std::string sMatchesFile;

		std::vector< std::string > _vec_fileNames; // considered images
		std::map< size_t, std::vector< SIOPointFeature > > _map_feats; // feature per images

		std::vector< std::pair< size_t, size_t > > _vec_imageSize; // Size of each image

		openMVG::tracks::STLPairWiseMatches _map_Matches; // pairwise geometric matches

		//log
		std::ofstream logF;
		std::streambuf *stdFile;
	};
}

