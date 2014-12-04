#include "SFMToolKits.h"


namespace sfm
{
	SFMToolKits::SFMToolKits()
	{
	}


	SFMToolKits::~SFMToolKits()
	{
	}

	bool SFMToolKits::computeFocal(const std::string& imgDir, const std::string& outDir, const std::string& dataBase, double focalPixPermm)
	{
		std::ofstream log;
		std::string logPath = "log/log_sfm_computeFocal.log";
		std::string logDir = stlplus::folder_part(logPath);

		if (!stlplus::folder_exists(logDir))
		{
			stlplus::folder_create(logDir);
		}
		log.open(logPath.c_str());
		if (!stlplus::folder_exists(outDir))
		{
			stlplus::folder_create(outDir);
		}


		std::vector<std::string> vec_image = stlplus::folder_files(imgDir);
		// Write the new file
		std::ofstream listTXT(stlplus::create_filespec(outDir,"lists.txt").c_str());
		if (listTXT)
		{
			std::sort(vec_image.begin(), vec_image.end());
			for (std::vector<std::string>::const_iterator iter_image = vec_image.begin();
				iter_image != vec_image.end();
				iter_image++)
			{
				// Read meta data to fill width height and focalPixPermm
				std::string sImageFilename = stlplus::create_filespec(imgDir, *iter_image);

				size_t width = -1;
				size_t height = -1;

				std::unique_ptr<Exif_IO> exifReader(new Exif_IO_OpenExif());
				exifReader->open(sImageFilename);

				// Consider the case where focal is provided

				std::ostringstream os;
				//If image do not contains meta data
				if (!exifReader->doesHaveExifInfo() || focalPixPermm != -1)
				{
					Image<unsigned char> image;
					if (openMVG::ReadImage(sImageFilename.c_str(), &image))  {
						width = image.Width();
						height = image.Height();
					}
					else
					{
						Image<RGBColor> imageRGB;
						if (openMVG::ReadImage(sImageFilename.c_str(), &imageRGB)) {
							width = imageRGB.Width();
							height = imageRGB.Height();
						}
						else
						{
							Image<RGBAColor> imageRGBA;
							if (openMVG::ReadImage(sImageFilename.c_str(), &imageRGBA))  {
								width = imageRGBA.Width();
								height = imageRGBA.Height();
							}
							else
								continue; // image is not considered, cannot be read
						}
					}
					if (focalPixPermm == -1)
						os << *iter_image << ";" << width << ";" << height << std::endl;
					else
						os << *iter_image << ";" << width << ";" << height << ";"
						<< focalPixPermm << ";" << 0 << ";" << width / 2.0 << ";"
						<< 0 << ";" << focalPixPermm << ";" << height / 2.0 << ";"
						<< 0 << ";" << 0 << ";" << 1 << std::endl;

				}
				else // If image contains meta data
				{
					double focal = focalPixPermm;
					width = exifReader->getWidth();
					height = exifReader->getHeight();
					std::string sCamName = exifReader->getBrand();
					std::string sCamModel = exifReader->getModel();

					std::vector<Datasheet> vec_database;
					Datasheet datasheet;
					if (parseDatabase(dataBase, vec_database))
					{
						if (getInfo(sCamName, sCamModel, vec_database, datasheet))
						{
							// The camera model was found in the database so we can compute it's approximated focal length
							double ccdw = datasheet._sensorSize;
							focal = (std::max)(width, height) * exifReader->getFocal() / ccdw;
							os << *iter_image << ";" << width << ";" << height << ";" << focal << ";" << sCamName << ";" << sCamModel << std::endl;
						}
						else
						{
							log << "Camera \"" << sCamName << "\" model \"" << sCamModel << "\" doesn't exist in the database" << std::endl;
							os << *iter_image << ";" << width << ";" << height << ";" << sCamName << ";" << sCamModel << std::endl;
						}
					}
					else
					{
						log << "Sensor width database \"" << dataBase << "\" doesn't exist." << std::endl;
						log << "Please consider add your camera model in the database." << std::endl;
						os << *iter_image << ";" << width << ";" << height << ";" << sCamName << ";" << sCamModel << std::endl;
					}
				}
				log << os.str();
				listTXT << os.str();
			}
		}
		listTXT.close();
		if (log.is_open()) log.close();
		return true;
	}

	bool SFMToolKits::computeMatch(const std::string& imgDir, const std::string& outDir, const char gModel, float distratio, bool octminus1, float peakThreshold)
	{
		std::ofstream log;
		std::string logPath = "log/log_sfm_computeMatch.log";
		std::string logDir = stlplus::folder_part(logPath);

		if (!stlplus::folder_exists(logDir))
		{
			stlplus::folder_create(logDir);
		}
		log.open(logPath.c_str());
		if (!stlplus::folder_exists(outDir))
		{
			stlplus::folder_create(outDir);
		}

		eGeometricModel gModelToCompute = FUNDAMENTAL_MATRIX;
		std::string sGeometricMatchesFilename = "";
		switch (gModel)
		{
		case 'f': case 'F':
			gModelToCompute = FUNDAMENTAL_MATRIX;
			sGeometricMatchesFilename = "matches.f.txt";
			break;
		case 'e': case 'E':
			gModelToCompute = ESSENTIAL_MATRIX;
			sGeometricMatchesFilename = "matches.e.txt";
			break;
		case 'h': case 'H':
			gModelToCompute = HOMOGRAPHY_MATRIX;
			sGeometricMatchesFilename = "matches.h.txt";
			break;
		default:
			std::cerr << "Unknown geometric model" << std::endl;
			return EXIT_FAILURE;
		}


		// Create output dir
		if (!stlplus::folder_exists(outDir))
			stlplus::folder_create(outDir);

		//---------------------------------------
		// a. List images
		//---------------------------------------
		std::string sListsFile = stlplus::create_filespec(outDir, "lists.txt");
		if (!stlplus::is_file(sListsFile)) 
		{
			log << "The input file \"" << sListsFile << "\" is missing" << std::endl;
			if (log.is_open()) log.close();
			return false;
		}

		typedef openMVG::SfMIO::CameraInfo sfmCF;
		typedef openMVG::SfMIO::IntrinsicCameraInfo sfmICF;

		std::vector<sfmCF> camList;
		std::vector<sfmICF> focalList;
		if (!openMVG::SfMIO::loadImageList(camList, focalList, sListsFile))
		{
			log << "error: " << "\nEmpty image list." << std::endl;
			if (log.is_open()) log.close();
			return false;
		}

		if (gModelToCompute == ESSENTIAL_MATRIX)
		{
			//只能存在一个本质矩阵（照片只能来之于同一<型号>照相机）
			//-- In the case of the essential matrix we check if only one K matrix is present.
			//-- Due to the fact that the generic framework allows only one K matrix for the
			// robust essential matrix estimation in image collection.
			std::vector<sfmICF>::iterator iterF =
				std::unique(focalList.begin(), focalList.end(), [](sfmICF const &ci1, sfmICF const &ci2)->bool{return ci1.m_K == ci2.m_K; });
			focalList.resize(std::distance(focalList.begin(), iterF));

			if (focalList.size() == 1)
			{
				// Set all the intrinsic ID to 0
				for (size_t i = 0; i < camList.size(); ++i)
					camList[i].m_intrinsicId = 0;
			}
			else  
			{
				log << "There is more than one focal group in the lists.txt file." << std::endl
					<< "Only one focal group is supported for the image collection robust essential matrix estimation." << std::endl;
				if (log.is_open()) log.close();
				return false;
			}
		}

		//-- Two alias to ease access to image filenames and image sizes
		std::vector<std::string> fileNames;
		std::vector<std::pair<size_t, size_t> > imagesSize;
		for (std::vector<sfmCF>::const_iterator iter_camInfo = camList.begin(); iter_camInfo != camList.end(); iter_camInfo++)
		{
			imagesSize.push_back(std::make_pair(focalList[iter_camInfo->m_intrinsicId].m_w, focalList[iter_camInfo->m_intrinsicId].m_h));
			fileNames.push_back(stlplus::create_filespec(imgDir, iter_camInfo->m_sImageName));
		}

		//---------------------------------------
		// b.Compute features and descriptor
		//    - extract sift features and descriptor
		//    - if keypoints already computed, re-load them
		//    - else save features and descriptors on disk
		//---------------------------------------

		typedef Descriptor<unsigned char, 128> DescriptorT;
		typedef SIOPointFeature FeatureT;
		typedef std::vector<FeatureT> FeatsT;
		typedef vector<DescriptorT > DescsT;
		typedef KeypointSet<FeatsT, DescsT > KeypointSetT;

		{
			log << "\n\nEXTRACT FEATURES" << std::endl;
			imagesSize.resize(fileNames.size());

			Image<unsigned char> imageGray;

#pragma omp parallel for private(imageGray)
			for (int i = 0; i < (int)fileNames.size(); ++i)  
			{
				std::string sFeat = stlplus::create_filespec(outDir,stlplus::basename_part(fileNames[i]), "feat");
				std::string sDesc = stlplus::create_filespec(outDir,stlplus::basename_part(fileNames[i]), "desc");

				//If descriptors or features file are missing, compute them
				if (!stlplus::file_exists(sFeat) || !stlplus::file_exists(sDesc))
				{
					if (!ReadImage(fileNames[i].c_str(), &imageGray)) continue;

					Image<float> img(imageGray.GetMat().cast<float>());
					// Compute features and descriptors and export them to files
					KeypointSetT kpSet;
					SIFTDetector(img, kpSet.features(), kpSet.descriptors(), octminus1, true, peakThreshold);
					kpSet.saveToBinFile(sFeat, sDesc);
				}
			}
		}

		//---------------------------------------
		// c. Compute putative descriptor matches
		//    - L2 descriptor matching
		//    - Keep correspondences only if NearestNeighbor ratio is ok
		//---------------------------------------
		IndexedMatchPerPair map_PutativesMatches;
		// Define the matcher and the used metric (Squared L2)
		// ANN matcher could be defined as follow:
		typedef flann::L2<DescriptorT::bin_type> MetricT;
		typedef ArrayMatcher_Kdtree_Flann<DescriptorT::bin_type, MetricT> MatcherT;
		// Brute force matcher can be defined as following:
		//typedef L2_Vectorized<DescriptorT::bin_type> MetricT;
		//typedef ArrayMatcherBruteForce<DescriptorT::bin_type, MetricT> MatcherT;

		// If the matches already exists, reload them
		if (stlplus::file_exists(outDir + "/matches.putative.txt"))
		{
			PairedIndMatchImport(outDir + "/matches.putative.txt", map_PutativesMatches);
			log << std::endl << "PUTATIVE MATCHES -- PREVIOUS RESULTS LOADED" << std::endl;
		}
		else // Compute the putatives matches
		{
			Matcher_AllInMemory<KeypointSetT, MatcherT> collectionMatcher(distratio);
			if (collectionMatcher.loadData(fileNames, outDir))
			{
				log << std::endl << "PUTATIVE MATCHES" << std::endl;
				collectionMatcher.Match(fileNames, map_PutativesMatches);
				//---------------------------------------
				//-- Export putative matches
				//---------------------------------------
				std::ofstream file(std::string(outDir + "/matches.putative.txt").c_str());
				if (file.is_open())
					PairedIndMatchToStream(map_PutativesMatches, file);
				file.close();
			}
		}
		//-- export putative matches Adjacency matrix
		PairWiseMatchingToAdjacencyMatrixSVG(fileNames.size(),
			map_PutativesMatches,
			stlplus::create_filespec(outDir, "PutativeAdjacencyMatrix", "svg"));


		//---------------------------------------
		// d. Geometric filtering of putative matches
		//    - AContrario Estimation of the desired geometric model
		//    - Use an upper bound for the a contrario estimated threshold
		//---------------------------------------
		IndexedMatchPerPair map_GeometricMatches;

		ImageCollectionGeometricFilter<FeatureT> collectionGeomFilter;
		const double maxResidualError = 4.0;
		if (collectionGeomFilter.loadData(fileNames, outDir))
		{
			log << std::endl << " - GEOMETRIC FILTERING - " << std::endl;
			switch (gModelToCompute)
			{
			case FUNDAMENTAL_MATRIX:
			{
				collectionGeomFilter.Filter(
					GeometricFilter_FMatrix_AC(maxResidualError),
					map_PutativesMatches,
					map_GeometricMatches,
					imagesSize);
			}
				break;
			case ESSENTIAL_MATRIX:
			{
				collectionGeomFilter.Filter(
					GeometricFilter_EMatrix_AC(focalList[0].m_K, maxResidualError),
					map_PutativesMatches,
					map_GeometricMatches,
					imagesSize);

				//-- Perform an additional check to remove pairs with poor overlap
				std::vector<IndexedMatchPerPair::key_type> vec_toRemove;
				for (IndexedMatchPerPair::const_iterator iterMap = map_GeometricMatches.begin();
					iterMap != map_GeometricMatches.end(); ++iterMap)
				{
					size_t putativePhotometricCount = map_PutativesMatches.find(iterMap->first)->second.size();
					size_t putativeGeometricCount = iterMap->second.size();
					float ratio = putativeGeometricCount / (float)putativePhotometricCount;
					if (putativeGeometricCount < 50 || ratio < .3f)  {
						// the pair will be removed
						vec_toRemove.push_back(iterMap->first);
					}
				}
				//-- remove discarded pairs
				for (std::vector<IndexedMatchPerPair::key_type>::const_iterator
					iter = vec_toRemove.begin(); iter != vec_toRemove.end(); ++iter)
				{
					map_GeometricMatches.erase(*iter);
				}
			}
				break;
			case HOMOGRAPHY_MATRIX:
			{

				collectionGeomFilter.Filter(
					GeometricFilter_HMatrix_AC(maxResidualError),
					map_PutativesMatches,
					map_GeometricMatches,
					imagesSize);
			}
				break;
			}

			//---------------------------------------
			//-- Export geometric filtered matches
			//---------------------------------------
			std::ofstream file(string(outDir + "/" + sGeometricMatchesFilename).c_str());
			if (file.is_open())
				PairedIndMatchToStream(map_GeometricMatches, file);
			file.close();

			//-- export Adjacency matrix
			log << "\n Export Adjacency Matrix of the pairwise's geometric matches"
				<< std::endl;
			PairWiseMatchingToAdjacencyMatrixSVG(fileNames.size(),
				map_GeometricMatches,
				stlplus::create_filespec(outDir, "GeometricAdjacencyMatrix", "svg"));
		}
		if (log.is_open()) log.close();
		return true;
	}

	bool SFMToolKits::computeMatchE(const std::string& imgDir, const std::string& outDir, float distratio, bool octminus1, float peakThreshold)
	{
		return computeMatch(imgDir, outDir, 'e', distratio,octminus1,peakThreshold);
	}

	bool SFMToolKits::computeMatchF(const std::string& imgDir, const std::string& outDir, float distratio, bool octminus1, float peakThreshold)
	{
		return computeMatch(imgDir, outDir, 'f', distratio, octminus1, peakThreshold);
	}

	bool SFMToolKits::computeMatchH(const std::string& imgDir, const std::string& outDir, float distratio, bool octminus1, float peakThreshold)
	{
		return computeMatch(imgDir, outDir, 'h', distratio, octminus1, peakThreshold);
	}

	bool SFMToolKits::incrementalSfM(const std::string& imgDir, const std::string& matcheDir, const std::string& outDir)
	{
		std::ofstream log;
		std::string logPath = "log/log_sfm_incrementalSfM.log";
		std::string logDir = stlplus::folder_part(logPath);

		if (!stlplus::folder_exists(logDir))
		{
			stlplus::folder_create(logDir);
		}
		log.open(logPath.c_str());

		if (!stlplus::folder_exists(outDir))
			stlplus::folder_create(outDir);

		bool bPmvsExport = false;
		bool bRefinePPandDisto = true;
		bool bColoredPointCloud = true;
		std::pair<size_t, size_t> initialPair(0, 0);

		//---------------------------------------
		// Incremental reconstruction process
		//---------------------------------------

		clock_t timeStart = clock();
		IncrementalReconstructionEngine to3DEngine(imgDir,matcheDir,outDir,true);

		to3DEngine.setInitialPair(initialPair);
		to3DEngine.setIfRefinePrincipalPointAndRadialDisto(bRefinePPandDisto);

		if (to3DEngine.Process())
		{
			clock_t timeEnd = clock();
			log << std::endl << " Ac-Sfm took : " << (timeEnd - timeStart) / CLOCKS_PER_SEC << " seconds." << std::endl;

			const reconstructorHelper & reconstructorHelperRef = to3DEngine.refToReconstructorHelper();
			std::vector<Vec3> tracksColor;
			if (bColoredPointCloud)
			{
				// Compute the color of each track
				to3DEngine.ColorizeTracks(tracksColor);
			}
			reconstructorHelperRef.exportToPly(
				stlplus::create_filespec(outDir, "FinalColorized", ".ply"), tracksColor);

			// Export to openMVG format
			log << std::endl << "Export 3D scene to openMVG format" << std::endl
				<< " -- Point cloud color: " << (bColoredPointCloud ? "ON" : "OFF") << std::endl;

			reconstructorHelperRef.ExportToOpenMVGFormat(
				stlplus::folder_append_separator(outDir) + "SfM_output",
				to3DEngine.getFilenamesVector(),
				imgDir,
				to3DEngine.getImagesSize(),
				to3DEngine.getTracks(),
				tracksColor
				);

			// Manage export data to desired format
			// -> PMVS
			if (bPmvsExport) 
			{
				log << std::endl << "Export 3D scene to PMVS format" << std::endl;
				reconstructorHelperRef.exportToPMVSFormat(
					stlplus::folder_append_separator(outDir) + "PMVS",
					to3DEngine.getFilenamesVector(),
					imgDir);
			}
			if (log.is_open()) log.close();
			return true;
		}
		else
		{
			log << "\n error:" << "\n Something goes wrong in the Structure from Motion process" << std::endl;
			if (log.is_open()) log.close();
			return false;
		}
	}

	bool SFMToolKits::globalSfM(const std::string& imgDir, const std::string& matcheDir, const std::string& outDir)
	{
		if (!stlplus::folder_exists(outDir))
			stlplus::folder_create(outDir);

		//---------------------------------------
		// Global reconstruction process
		//---------------------------------------

		bool bColoredPointCloud = true;

		clock_t timeStart = clock();
		GlobalReconstructionEngine to3DEngine(imgDir,matcheDir,outDir,true);
		to3DEngine.setRotationMethod(1);

		if (to3DEngine.Process())
		{
			clock_t timeEnd = clock();
			std::cout << std::endl << " Total Ac-Global-Sfm took : " << (timeEnd - timeStart) / CLOCKS_PER_SEC << std::endl;

			//-- Export computed data to disk
			to3DEngine.ExportToOpenMVGFormat(bColoredPointCloud);

			return true;
		}
		else
		{
			return false;
		}
	}


	bool SFMToolKits::exportToBundlerFormat(const std::string& sOutFile, const std::string& sOutListFile, const Document & doc)
	{
		std::ofstream os(sOutFile.c_str());
		std::ofstream osList(sOutListFile.c_str());
		if (!os.is_open() || !osList.is_open())
		{
			return false;
		}
		else
		{
			os << "# Bundle file v0.3" << os.widen('\n')
				<< doc._map_camera.size()
				<< " " << doc._tracks.size() << os.widen('\n');

			size_t count = 0;
			for (std::map<size_t, PinholeCamera>::const_iterator iter = doc._map_camera.begin();
				iter != doc._map_camera.end(); ++iter)
			{
				const PinholeCamera & PMat = iter->second;

				Mat3 D;
				D.fill(0.0);
				D.diagonal() = Vec3(1., -1., -1.); // mapping between our pinhole and Bundler convention
				Mat3 R = D * PMat._R;
				Vec3 t = D * PMat._t;
				double focal = PMat._K(0, 0);
				double k1 = 0.0, k2 = 0.0; // distortion already removed

				os << focal << " " << k1 << " " << k2 << os.widen('\n') //f k1 k2
					<< R(0, 0) << " " << R(0, 1) << " " << R(0, 2) << os.widen('\n')   //R[0]
					<< R(1, 0) << " " << R(1, 1) << " " << R(1, 2) << os.widen('\n')   //R[1]
					<< R(2, 0) << " " << R(2, 1) << " " << R(2, 2) << os.widen('\n')  //R[2]
					<< t(0) << " " << t(1) << " " << t(2) << os.widen('\n');     //t

				osList << doc._vec_imageNames[iter->first] << " 0 " << focal << os.widen('\n');
			}

			size_t trackIndex = 0;

			for (std::map< size_t, tracks::submapTrack >::const_iterator
				iterTracks = doc._tracks.begin();
				iterTracks != doc._tracks.end();
			++iterTracks, ++trackIndex)
			{

				const size_t trackId = iterTracks->first;
				const tracks::submapTrack & map_track = iterTracks->second;

				const float * ptr3D = &doc._vec_points[trackIndex * 3];
				os << ptr3D[0] << " " << ptr3D[1] << " " << ptr3D[2] << os.widen('\n');
				os << "255 255 255" << os.widen('\n');
				os << map_track.size() << " ";
				const Vec3 vec(ptr3D[0], ptr3D[1], ptr3D[2]);
				for (tracks::submapTrack::const_iterator iterTrack = map_track.begin();
					iterTrack != map_track.end();
					++iterTrack)
				{
					const PinholeCamera & PMat = doc._map_camera.find(iterTrack->first)->second;
					Vec2 pt = PMat.Project(vec);
					os << iterTrack->first << " " << iterTrack->second << " " << pt(0) << " " << pt(1) << " ";
				}
				os << os.widen('\n');

			}
			os.close();
			osList.close();
		}
		return true;
	}


	bool SFMToolKits::exportToCMPMVSFormat(const std::string& imgDir, const std::string& outDir, const Document & doc)
	{
		bool bOk = true;
		// Create basis directory structure
		if (!stlplus::is_folder(outDir))
		{
			stlplus::folder_create(outDir);
			bOk = stlplus::is_folder(outDir);
		}

		if (!bOk)
		{
			std::cerr << "Cannot access to one of the desired output directory" << std::endl;
			return false;
		}
		else
		{
			// Export data :
			//Camera

			size_t count = 1;
			for (std::map<size_t, PinholeCamera>::const_iterator iter = doc._map_camera.begin();
				iter != doc._map_camera.end(); ++iter, ++count)
			{
				const Mat34 & PMat = iter->second._P;
				std::ostringstream os;
				os << std::setw(5) << std::setfill('0') << count << "_P";
				std::ofstream file(
					stlplus::create_filespec(stlplus::folder_append_separator(outDir),
					os.str(), "txt").c_str());
				file << "CONTOUR\n"
					<< PMat.row(0) << "\n" << PMat.row(1) << "\n" << PMat.row(2) << std::endl;
				file.close();
			}

			// Image
			count = 1;
			int w, h; // Image size (suppose they are all the same)
			Image<RGBColor> image;
			for (std::map<size_t, PinholeCamera>::const_iterator iter = doc._map_camera.begin();
				iter != doc._map_camera.end();  ++iter, ++count)
			{
				size_t imageIndex = iter->first;
				const std::string & sImageName = doc._vec_imageNames[imageIndex];
				std::ostringstream os;
				os << std::setw(5) << std::setfill('0') << count;
				ReadImage(stlplus::create_filespec(imgDir, sImageName).c_str(), &image);
				w = image.Width();
				h = image.Height();
				std::string sCompleteImageName = stlplus::create_filespec(
					stlplus::folder_append_separator(outDir), os.str(), "jpg");
				WriteImage(sCompleteImageName.c_str(), image);
			}

			// Write the mvs_firstRun script
			std::ostringstream os;
			os << "[global]" << std::endl
				<< "dirName=\"" << stlplus::folder_append_separator(outDir) << "\"" << std::endl
				<< "prefix=\"\"" << std::endl
				<< "imgExt=\"jpg\"" << std::endl
				<< "ncams=" << doc._map_camera.size() << std::endl
				<< "width=" << w << std::endl
				<< "height=" << h << std::endl
				<< "scale=2" << std::endl
				<< "workDirName=\"_tmp_fast\"" << std::endl
				<< "doPrepareData=TRUE" << std::endl
				<< "doPrematchSifts=TRUE" << std::endl
				<< "doPlaneSweepingSGM=TRUE" << std::endl
				<< "doFuse=TRUE" << std::endl
				<< "nTimesSimplify=10" << std::endl
				<< std::endl
				<< "[prematching]" << std::endl
				<< "minAngle=3.0" << std::endl
				<< std::endl
				<< "[grow]" << std::endl
				<< "minNumOfConsistentCams=6" << std::endl
				<< std::endl
				<< "[filter]" << std::endl
				<< "minNumOfConsistentCams=2" << std::endl
				<< std::endl
				<< "#do not erase empy lines after this comment otherwise it will crash ... bug" << std::endl
				<< std::endl
				<< std::endl;

			std::ofstream file(
				stlplus::create_filespec(stlplus::folder_append_separator(outDir),
				"01_mvs_firstRun", "ini").c_str());
			file << os.str();
			file.close();

			// limitedScale
			os.str("");
			os << "[global]" << std::endl
				<< "dirName=\"" << stlplus::folder_append_separator(outDir) << "\"" << std::endl
				<< "prefix=\"\"" << std::endl
				<< "imgExt=\"jpg\"" << std::endl
				<< "ncams=" << doc._map_camera.size() << std::endl
				<< "width=" << w << std::endl
				<< "height=" << h << std::endl
				<< "scale=2" << std::endl
				<< "workDirName=\"_tmp_fast\"" << std::endl
				<< "doPrepareData=FALSE" << std::endl
				<< "doPrematchSifts=FALSE" << std::endl
				<< "doPlaneSweepingSGM=FALSE" << std::endl
				<< "doFuse=FALSE" << std::endl
				<< std::endl
				<< "[uvatlas]" << std::endl
				<< "texSide=1024" << std::endl
				<< "scale=1" << std::endl
				<< std::endl
				<< "[delanuaycut]" << std::endl
				<< "saveMeshTextured=FALSE" << std::endl
				<< std::endl
				<< "[hallucinationsFiltering]" << std::endl
				<< "useSkyPrior=FALSE" << std::endl
				<< "doLeaveLargestFullSegmentOnly=FALSE" << std::endl
				<< "doRemoveHugeTriangles=TRUE" << std::endl
				<< std::endl
				<< "[largeScale]" << std::endl
				<< "doGenerateAndReconstructSpaceMaxPts=TRUE" << std::endl
				<< "doGenerateSpace=TRUE" << std::endl
				<< "planMaxPts=3000000" << std::endl
				<< "doComputeDEMandOrtoPhoto=FALSE" << std::endl
				<< "doGenerateVideoFrames=FALSE" << std::endl
				<< std::endl
				<< "[meshEnergyOpt]" << std::endl
				<< "doOptimizeOrSmoothMesh=FALSE" << std::endl
				<< std::endl
				<< std::endl
				<< "#EOF" << std::endl
				<< std::endl
				<< std::endl;

			std::ofstream file2(
				stlplus::create_filespec(stlplus::folder_append_separator(outDir),
				"02_mvs_limitedScale", "ini").c_str());
			file2 << os.str();
			file2.close();
		}
		return bOk;
	}


	bool SFMToolKits::exportToPMVSFormat(const std::string& imgDir, const std::string& outDir, const Document & doc, const int resolution, const int cpuNu)
	{
		bool bOk = true;
		if (!stlplus::is_folder(outDir))
		{
			stlplus::folder_create(outDir);
			bOk = stlplus::is_folder(outDir);
		}

		// Create basis directory structure
		stlplus::folder_create(stlplus::folder_append_separator(outDir) + "models");
		stlplus::folder_create(stlplus::folder_append_separator(outDir) + "txt");
		stlplus::folder_create(stlplus::folder_append_separator(outDir) + "visualize");

		if (bOk &&
			stlplus::is_folder(stlplus::folder_append_separator(outDir) + "models") &&
			stlplus::is_folder(stlplus::folder_append_separator(outDir) + "txt") &&
			stlplus::is_folder(stlplus::folder_append_separator(outDir) + "visualize")
			)
		{
			bOk = true;
		}
		else  {
			std::cerr << "Cannot access to one of the desired output directory" << std::endl;
		}

		if (bOk)
		{
			// Export data :
			//Camera

			size_t count = 0;
			for (std::map<size_t, PinholeCamera>::const_iterator iter = doc._map_camera.begin();
				iter != doc._map_camera.end(); ++iter, ++count)
			{
				const Mat34 & PMat = iter->second._P;
				std::ostringstream os;
				os << std::setw(8) << std::setfill('0') << count;
				std::ofstream file(
					stlplus::create_filespec(stlplus::folder_append_separator(outDir) + "txt",
					os.str(), "txt").c_str());
				file << "CONTOUR" << os.widen('\n')
					<< PMat.row(0) << "\n" << PMat.row(1) << "\n" << PMat.row(2) << os.widen('\n');
				file.close();
			}

			// Image
			count = 0;
			Image<RGBColor> image;
			for (std::map<size_t, PinholeCamera>::const_iterator iter = doc._map_camera.begin();
				iter != doc._map_camera.end();  ++iter, ++count)
			{
				size_t imageIndex = iter->first;
				const std::string & sImageName = doc._vec_imageNames[imageIndex];
				std::ostringstream os;
				os << std::setw(8) << std::setfill('0') << count;
				std::string srcImage = stlplus::create_filespec(imgDir, sImageName);
				std::string dstImage = stlplus::create_filespec(
					stlplus::folder_append_separator(outDir) + "visualize", os.str(), "jpg");
				if (stlplus::extension_part(srcImage) == "JPG" ||
					stlplus::extension_part(srcImage) == "jpg")
				{
					stlplus::file_copy(srcImage, dstImage);
				}
				else
				{
					ReadImage(srcImage.c_str(), &image);
					WriteImage(dstImage.c_str(), image);
				}
			}

			//pmvs_options.txt
			std::ostringstream os;
			os << "level " << resolution << os.widen('\n')
				<< "csize 2" << os.widen('\n')
				<< "threshold 0.7" << os.widen('\n')
				<< "wsize 7" << os.widen('\n')
				<< "minImageNum 3" << os.widen('\n')
				<< "CPU " << cpuNu << os.widen('\n')
				<< "setEdge 0" << os.widen('\n')
				<< "useBound 0" << os.widen('\n')
				<< "useVisData 0" << os.widen('\n')
				<< "sequence -1" << os.widen('\n')
				<< "maxAngle 10" << os.widen('\n')
				<< "quad 2.0" << os.widen('\n')
				<< "timages -1 0 " << doc._map_camera.size() << os.widen('\n')
				<< "oimages 0" << os.widen('\n'); // ?

			std::ofstream file(stlplus::create_filespec(outDir, "pmvs_options", "txt").c_str());
			file << os.str();
			file.close();
		}
		return bOk;
	}


	bool SFMToolKits::exportToCMPMVS(const std::string& sfmOutDir, const std::string& outDir)
	{
		if (!stlplus::folder_exists(outDir))
			stlplus::folder_create(outDir);

		Document m_doc;
		if (m_doc.load(sfmOutDir))
		{
			exportToCMPMVSFormat(
				stlplus::folder_append_separator(sfmOutDir) + "images",
				stlplus::folder_append_separator(outDir) + "CMPMVS",			
				m_doc);

			return true;
		}
		else
		{
			return false;
		}
	}

	bool SFMToolKits::exportToPMVS(const std::string& sfmOutDir, const std::string& outDir)
	{
		int resolution = 1;
		int CPU = 8;

		// Create output dir
		if (!stlplus::folder_exists(outDir))
			stlplus::folder_create(outDir);

		Document m_doc;
		if (m_doc.load(sfmOutDir))
		{
			exportToPMVSFormat(
				stlplus::folder_append_separator(sfmOutDir) + "images",
				stlplus::folder_append_separator(outDir) + "PMVS",
				m_doc,
				resolution,
				CPU);

			exportToBundlerFormat(
				stlplus::folder_append_separator(outDir) +
				stlplus::folder_append_separator("PMVS") + "bundle.rd.out",
				stlplus::folder_append_separator(outDir) +
				stlplus::folder_append_separator("PMVS") + "list.txt",
				m_doc
				);

			return true;
		}
		else
		{
			return false;
		}
	}

	bool SFMToolKits::exportToMESHLAB(const std::string& sfmOutDir, const std::string& ply, const std::string& outDir)
	{
		if (!stlplus::folder_exists(sfmOutDir)) 
		{
			std::cerr << "\nSfM directory doesn't exist" << std::endl;
			return false;
		}

		if (!stlplus::file_exists(ply)) 
		{
			std::cerr << "\nPly file doesn't exist" << std::endl;
			return false;
		}

		// Create output dir
		if (!stlplus::folder_exists(outDir))
			stlplus::folder_create(outDir);

		Document m_doc;
		std::cout << "\n Open the directory : \n" << sfmOutDir << std::endl;

		//Read SfM output directory
		if (!m_doc.load(sfmOutDir))
		{
			std::cout << "Impossible to open the openMVG SfM project." << std::endl;
			return false;
		}

		std::ofstream outfile(stlplus::create_filespec(outDir, "sceneMeshlab", "mlp").c_str());

		//Init mlp file  
		outfile << "<!DOCTYPE MeshLabDocument>" << std::endl
			<< "<MeshLabProject>" << std::endl
			<< " <MeshGroup>" << std::endl
			<< "  <MLMesh label=\"" << ply << "\" filename=\"" << ply << "\">" << std::endl
			<< "   <MLMatrix44>" << std::endl
			<< "1 0 0 0 " << std::endl
			<< "0 1 0 0 " << std::endl
			<< "0 0 1 0 " << std::endl
			<< "0 0 0 1 " << std::endl
			<< "</MLMatrix44>" << std::endl
			<< "  </MLMesh>" << std::endl
			<< " </MeshGroup>" << std::endl;

		outfile << " <RasterGroup>" << std::endl;

		std::map<size_t, PinholeCamera >::const_iterator iterCamera = m_doc._map_camera.begin();
		std::map<size_t, std::pair<size_t, size_t> >::const_iterator iterSize = m_doc._map_imageSize.begin();
		std::vector<std::string>::const_iterator iterName = m_doc._vec_imageNames.begin();
		for (;
			iterCamera != m_doc._map_camera.end();
			iterCamera++,
			iterSize++,
			iterName++)
		{
			PinholeCamera camera = iterCamera->second;
			Mat34 P = camera._P;
			for (int i = 1; i < 3; i++)
				for (int j = 0; j < 4; j++)
					P(i, j) *= -1;

			Mat3 R, K;
			Vec3 t;
			KRt_From_P(P, &K, &R, &t);

			Vec3 optical_center = R.transpose() * t;

			outfile << "  <MLRaster label=\"" << *iterName << "\">" << std::endl
				<< "   <VCGCamera TranslationVector=\""
				<< optical_center[0] << " "
				<< optical_center[1] << " "
				<< optical_center[2] << " "
				<< " 1 \""
				<< " LensDistortion=\"0 0\""
				<< " ViewportPx=\"" << iterSize->second.first << " " << iterSize->second.second << "\""
				<< " PixelSizeMm=\"" << 1 << " " << 1 << "\""
				<< " CenterPx=\"" << iterSize->second.first / 2.0 << " " << iterSize->second.second / 2.0 << "\""
				<< " FocalMm=\"" << (double)K(0, 0) << "\""
				<< " RotationMatrix=\""
				<< R(0, 0) << " " << R(0, 1) << " " << R(0, 2) << " 0 "
				<< R(1, 0) << " " << R(1, 1) << " " << R(1, 2) << " 0 "
				<< R(2, 0) << " " << R(2, 1) << " " << R(2, 2) << " 0 "
				<< "0 0 0 1 \"/>" << std::endl;
			std::string soffsetImagePath = stlplus::create_filespec(sfmOutDir, "imagesOffset");
			if (stlplus::folder_exists(soffsetImagePath))
				outfile << "   <Plane semantic=\"\" fileName=\"" << stlplus::create_filespec(soffsetImagePath,
				stlplus::basename_part(*iterName) + "_of",
				stlplus::extension_part(*iterName)) << "\"/> " << std::endl;
			else
				outfile << "   <Plane semantic=\"\" fileName=\"" << stlplus::create_filespec(stlplus::create_filespec(sfmOutDir, "images"),
				*iterName) << "\"/> " << std::endl;
			outfile << "  </MLRaster>" << std::endl;
		}

		outfile << "   </RasterGroup>" << std::endl
			<< "</MeshLabProject>" << std::endl;

		outfile.close();
		return true;
	}

	bool SFMToolKits::cmvs(const std::string& sfmOutDir)
	{
		//----------------------------------------------------------------------
		// If you want more control of the program, you can also change the
		// following two parameters.
		// scoreRatioThreshold, and coverageThreshold correspond to
		// \\lambda and \\delta in our CVPR 2010 paper.
		// Please refer to the paper for their definitions. The following are
		// brief explanations.
		//
		// CMVS tries to make sure that multi-view stereo (MVS)
		// reconstruction accuracy will be more than a certain threshold at
		// Structure-from-Motion (SfM) points. scoreRatioThreshold is this
		// threshold on the reconstruction accuracy [0, 1.0]. CMVS makes
		// sure that the ratio of "satisfied" SfM points is more than
		// coverageThreshold [0 1.0].
		//
		// Intuitively, increasing these parameters lead to more images and
		// clusters in the output.

		if (!stlplus::folder_exists(sfmOutDir))
		{
			std::cerr << "\nSfM_output directory doesn't exist" << std::endl;
			return false;
		}

		int maximage = 100;
		int CPU = 4;

		const float scoreRatioThreshold = 0.7f;
		const float coverageThreshold = 0.7f;


		const int iNumForScore = 4;
		const int pnumThreshold = 0;
		CMVS::Cbundle bundle;
		return bundle.run(sfmOutDir, maximage, iNumForScore,
			scoreRatioThreshold, coverageThreshold,
			pnumThreshold, CPU);
	}


	bool SFMToolKits::pmvs(const std::string& pmvsOutDir, const std::string& optionFile)
	{
		if (!stlplus::folder_exists(pmvsOutDir))
		{
			std::cerr << "\nPMVS output directory doesn't exist" << std::endl;
			return false;
		}

		PMVS3::Soption option;
		if (!option.init(stlplus::folder_append_separator(pmvsOutDir), optionFile)) return false;

		PMVS3::CfindMatch findMatch;
		findMatch.init(option);
		findMatch.run();

		findMatch.write(stlplus::folder_append_separator(pmvsOutDir) + "models/" + stlplus::basename_part(optionFile));
		return true;
	}

	/*bool SFMToolKits::mesh(const std::string& pmvsOutDir)
	{
		mesh::TextureMesh tMesh;
		return tMesh.run(pmvsOutDir);
	}*/
}
