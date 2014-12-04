
// Copyright (c) 2012, 2013 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENMVG_SFM_RECONSTRUCTION_DATA_H
#define OPENMVG_SFM_RECONSTRUCTION_DATA_H

#include "openMVG/numeric/numeric.h"

#include <iostream>
#include <iterator>
#include <string>
#include <map>
#include <set>
#include <iomanip>
#include <vector>
#include <clocale>
#include <thread>
#include <atomic>
#include <mutex>

#include "openMVG/image/image.hpp"
#include "openMVG/tracks/tracks.hpp"
#include "openMVG/cameras/PinholeCamera.hpp"
#include "openMVG/cameras/BrownPinholeCamera.hpp"

#include "SfMPlyHelper.hpp"
#include "third_party/stlAddition/stlMap.hpp"
#include "third_party/stlplus3/filesystemSimplified/file_system.hpp"

namespace openMVG{

	using namespace std;

	// A simple container and undistort function for the Brown's distortion model [1]
	// Variables:
	// (x,y): 2D point in the image (pixel)
	// (u,v): the undistorted 2D point (pixel)
	// radial_distortion (k1, k2, k3, ...): vector containing the radial distortion
	// (cx,cy): camera principal point
	// tangential factors are not considered here
	//
	// Equation:
	// u = x + (x - cx) * (k1 * r^2 + k2 * r^4 +...)
	// v = y + (y - cy) * (k1 * r^2 + k2 * r^4 +...)
	//
	// [1] Decentering distortion of lenses.
	//      Brown, Duane C
	//      Photometric Engineering 1966
	struct BrownDistoModel
	{
		Vec2 m_disto_center; // distortion center
		Vec m_radial_distortion; // radial distortion factor
		double m_f; // focal

		inline void ComputeUndistortedCoordinates(double xu, double yu, double &xd, double& yd) const
		{
			Vec2 point(xu, yu);
			Vec2 principal_point(m_disto_center);
			Vec2 point_centered = point - principal_point;

			double u = point_centered.x() / m_f;
			double v = point_centered.y() / m_f;
			double radius_squared = u * u + v * v;

			double coef_radial = 0.0;
			for (int i = m_radial_distortion.size() - 1; i >= 0; --i) {
				coef_radial = (coef_radial + m_radial_distortion[i]) * radius_squared;
			}

			Vec2 undistorted_point = point + point_centered * coef_radial;
			xd = undistorted_point(0);
			yd = undistorted_point(1);
		}
	};

	/// Undistort an image according a given Distortion model
	template <typename Image>
	Image undistortImage(
		const Image& I,
		const BrownDistoModel& d,
		RGBColor fillcolor = BLACK,
		bool bcenteringPPpoint = false)
	{
		int w = I.Width();
		int h = I.Height();
		double cx = w * .5, cy = h * .5;
		Vec2 offset(0, 0);
		if (bcenteringPPpoint)
			offset = Vec2(cx, cy) - d.m_disto_center;

		Image J(w, h);

		std::atomic_int jobs; jobs = 0;

		int cpuN = (std::min)((int)std::thread::hardware_concurrency(), h);
		if (cpuN < 1) return J;
		std::cout << "setClusters -> CPU Cores :" << cpuN << std::endl;
		std::vector<std::thread> threads(cpuN);

		for (auto& t : threads) t = std::thread([&](){
			while (true)
			{
				int && j = jobs++;

				if (j >= h) break;

				for (int i = 0; i < w; i++)
				{
					double xu, yu, xd, yd;
					xu = double(i);
					yu = double(j);
					d.ComputeUndistortedCoordinates(xu, yu, xd, yd);
					xd -= offset(0);
					yd -= offset(1);
					if (!J.Contains((int)yd, (int)xd))
						J(j, i) = fillcolor;
					else
						J(j, i) = SampleLinear(I, (float)yd, (float)xd);
				}
			}
		});

		for (auto& t : threads) t.join();

		return J;
	}

	/// Represent data in order to make 3D reconstruction process easier
	struct reconstructorHelper
	{
		// Reconstructed tracks (updated during the process)
		std::set<size_t> set_trackId;
		std::map<size_t, Vec3> map_3DPoints; // Associated 3D point

		// Reconstructed camera information
		std::set<size_t> set_imagedId;
		std::map<size_t, BrownPinholeCamera> map_Camera;

		// Per camera confidence (A contrario estimated threshold error)
		std::map<size_t, double> map_ACThreshold;

		bool exportToPly(const std::string & sFileName, const std::vector<Vec3>& colors = std::vector<Vec3>()) const
		{
			// get back 3D point into a vector (map value to vector transformation)
			std::vector<Vec3> reconstructed3DPoints;
			reconstructed3DPoints.reserve(map_3DPoints.size());
			std::transform(map_3DPoints.begin(),
				map_3DPoints.end(),
				std::back_inserter(reconstructed3DPoints),
				RetrieveValue());
			//-- Add camera position to the Point cloud
			std::vector<Vec3> camPos;
			for (const auto& camera : map_Camera)
			{
				camPos.push_back(camera.second._C);
			}
			return plyHelper::exportToPly(reconstructed3DPoints, camPos, sFileName, colors);
		}

		bool ExportToOpenMVGFormat(
			const std::string & sOutDirectory,  //Export directory
			const std::vector<std::string> & vec_fileNames, // vector of image filenames
			const std::string & sImagePath,  // The images path
			const std::vector< std::pair<size_t, size_t> > & vec_imageSize, // Size of each image
			const openMVG::tracks::STLMAPTracks & map_reconstructed, // Tracks (Visibility)
			const std::vector<Vec3>& colors = std::vector<Vec3>(), // Tracks color
			bool bExportImage = true //Export image ?    
			) const
		{
			bool bOk = true;
			if (!stlplus::is_folder(sOutDirectory))
			{
				stlplus::folder_create(sOutDirectory);
				bOk = stlplus::is_folder(sOutDirectory);
			}

			// Create basis directory structure
			stlplus::folder_create(stlplus::folder_append_separator(sOutDirectory) + "cameras");
			stlplus::folder_create(stlplus::folder_append_separator(sOutDirectory) + "cameras_disto");
			stlplus::folder_create(stlplus::folder_append_separator(sOutDirectory) + "clouds");
			stlplus::folder_create(stlplus::folder_append_separator(sOutDirectory) + "images");

			if (bOk &&
				stlplus::is_folder(stlplus::folder_append_separator(sOutDirectory) + "cameras") &&
				stlplus::is_folder(stlplus::folder_append_separator(sOutDirectory) + "cameras_disto") &&
				stlplus::is_folder(stlplus::folder_append_separator(sOutDirectory) + "clouds") &&
				stlplus::is_folder(stlplus::folder_append_separator(sOutDirectory) + "images")
				)
			{
				bOk = true;
			}
			else
			{
				std::cerr << "Cannot access to one of the desired output directory" << std::endl;
			}

			if (bOk)
			{
				//Export Camera as binary files
				std::map<size_t, size_t> map_cameratoIndex;
				size_t count = 0;
				for (const auto& camera : map_Camera)
				{
					map_cameratoIndex[camera.first] = count;
					const Mat34 & PMat = camera.second._P;
					std::ofstream file(
						stlplus::create_filespec(stlplus::folder_append_separator(sOutDirectory) + "cameras",
						stlplus::basename_part(vec_fileNames[camera.first])
						, "bin").c_str(), std::ios::out | std::ios::binary);
					file.write((const char*)PMat.data(), (std::streamsize)(3 * 4)*sizeof(double));

					bOk &= (!file.fail());
					file.close();
					++count;
				}

				//-- Export the camera with disto
				for (const auto& camera : map_Camera)
				{
					const BrownPinholeCamera & cam = camera.second;
					std::ofstream file(
						stlplus::create_filespec(stlplus::folder_append_separator(sOutDirectory) + "cameras_disto",
						stlplus::basename_part(vec_fileNames[camera.first])
						, "txt").c_str(), std::ios::out);
					// Save intrinsic data:
					file << cam._f << " "
						<< cam._ppx << " "
						<< cam._ppy << " "
						<< cam._k1 << " "
						<< cam._k2 << " "
						<< cam._k3 << "\n";
					// Save extrinsic data
					const Mat3 & R = cam._R;
					file << R(0, 0) << " " << R(0, 1) << " " << R(0, 2) << "\n"
						<< R(1, 0) << " " << R(1, 1) << " " << R(1, 2) << "\n"
						<< R(2, 0) << " " << R(2, 1) << " " << R(2, 2) << "\n";
					file << cam._t(0) << " " << cam._t(1) << " " << cam._t(2) << "\n";
					bOk &= (!file.fail());
					file.close();
				}

				//Export 3D point and tracks

				size_t nc = map_Camera.size();
				size_t nt = set_trackId.size();

				// Clipping planes (near and far Z depth per view)
				std::vector<double> znear(nc, (numeric_limits<double>::max)()), zfar(nc, 0);
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
				f_cloud << "ply\nformat ascii 1.0\ncomment generated by OpenMVG Calibration Engine" << "\n";
				f_cloud << "element vertex " << nt << "\n";
				f_cloud << "property float x\nproperty float y\nproperty float z" << "\n";
				f_cloud << "property uchar red\nproperty uchar green\nproperty uchar blue" << "\n";
				f_cloud << "property float confidence\nproperty list uchar int visibility" << "\n";
				f_cloud << "element face 0\nproperty list uchar int vertex_index" << "\n";
				f_cloud << "end_header" << "\n";
				size_t pointCount = 0;
				for (const auto& trackId : set_trackId)
				{
					// Look through the track and add point position
					const tracks::submapTrack & track = (map_reconstructed.find(trackId))->second;

					Vec3 pos = map_3DPoints.find(trackId)->second;

					if (colors.size() > 0)
					{
						const Vec3 & color = colors[pointCount];
						f_cloud << pos.transpose() << " " << color.transpose() << " " << 3.14;
					}
					else
						f_cloud << pos.transpose() << " 255 255 255 " << 3.14;

					std::ostringstream s_visibility;

					std::set< size_t > set_imageIndex;
					for (tracks::submapTrack::const_iterator iterTrack = track.begin();
						iterTrack != track.end();
						++iterTrack)
					{
						size_t imageId = iterTrack->first;

						if (map_cameratoIndex.find(imageId) != map_cameratoIndex.end())
						{
							set_imageIndex.insert(map_cameratoIndex[imageId]);
							const BrownPinholeCamera & cam = (map_Camera.find(imageId))->second;
							double z = Depth(cam._R, cam._t, pos);
							znear[map_cameratoIndex[imageId]] = (std::min)(znear[map_cameratoIndex[imageId]], z);
							zfar[map_cameratoIndex[imageId]] = (std::max)(zfar[map_cameratoIndex[imageId]], z);
						}

						s_visibility << iterTrack->first << " " << iterTrack->second << " ";
					}

					//export images indexes
					f_cloud << " " << set_imageIndex.size() << " ";
					copy(set_imageIndex.begin(), set_imageIndex.end(), std::ostream_iterator<size_t>(f_cloud, " "));
					f_cloud << std::endl;

					f_visibility << pos.transpose() << " " << set_imageIndex.size() << " ";
					f_visibility << s_visibility.str() << "\n";

					++pointCount;
				}
				f_cloud.close();
				f_visibility.close();

				// Views
				f_cloud.open(stlplus::create_filespec(stlplus::folder_append_separator(sOutDirectory),
					"views", "txt").c_str());
				if (!f_cloud.is_open())
				{
					std::cerr << "Cannot write views" << endl;
					return false;
				}
				f_cloud << "images\ncameras\n" << nc << "\n";

				count = 0;
				for (const auto& camera : map_Camera)
				{
					size_t camIndex = camera.first;
					f_cloud << vec_fileNames[camIndex]
						<< ' ' << vec_imageSize[camIndex].first
						<< ' ' << vec_imageSize[camIndex].second
						<< ' ' << stlplus::basename_part(vec_fileNames[camIndex]) << ".bin"
						<< ' ' << znear[count] / 2
						<< ' ' << zfar[count] * 2
						<< "\n";
					++count;
				}
				f_cloud.close();

				// EXPORT un-distorted IMAGES
				if (bExportImage)
				{
					std::cout << " -- Export the undistorted image set, it can take some time ..." << std::endl;
					for (const auto& camera : map_Camera)
					{
						// Get distortion information of the image
						const BrownPinholeCamera & cam = camera.second;
						BrownDistoModel distoModel;
						distoModel.m_disto_center = Vec2(cam._ppx, cam._ppy);
						distoModel.m_radial_distortion = Vec3(cam._k1, cam._k2, cam._k3);
						distoModel.m_f = cam._f;

						// Build the output filename from the input one
						size_t imageIndex = camera.first;
						std::string sImageName = vec_fileNames[imageIndex];
						std::string sOutImagePath =
							stlplus::create_filespec(stlplus::folder_append_separator(sOutDirectory) + "images",
							stlplus::basename_part(sImageName),
							stlplus::extension_part(sImageName));

						if (distoModel.m_radial_distortion.norm() == 0)
						{
							// Distortion is null, perform a direct copy of the image
							stlplus::file_copy(stlplus::create_filespec(sImagePath, sImageName), sOutImagePath);
						}
						else
						{
							// Image with no null distortion
							// - Open the image, undistort it and export it
							Image<RGBColor > image;
							if (ReadImage(stlplus::create_filespec(sImagePath, sImageName).c_str(), &image))
							{
								//Image<RGBColor> imageU = undistortImage(image, distoModel);
								auto&& imageU = undistortImage(image, distoModel);
								WriteImage(sOutImagePath.c_str(), imageU);
							}
						}
					}
				}
			}
			return bOk;
		}

		/// Export to PMVS format
		/// 'visualize' directory (8 digit coded image name jpg or ppm)
		/// 'txt' camera P matrix
		/// pmvs_options.txt
		/// ignore: vis.dat image links
		bool exportToPMVSFormat(
			const std::string & sOutDirectory,  //Output PMVS files directory
			const std::vector<std::string> & vec_fileNames, // vector of filenames
			const std::string & sImagePath  // The images path
			) const
		{
			bool bOk = true;
			if (!stlplus::is_folder(sOutDirectory))
			{
				stlplus::folder_create(sOutDirectory);
				bOk = stlplus::is_folder(sOutDirectory);
			}

			// Create basis directory structure
			stlplus::folder_create(stlplus::folder_append_separator(sOutDirectory) + "models");
			stlplus::folder_create(stlplus::folder_append_separator(sOutDirectory) + "txt");
			stlplus::folder_create(stlplus::folder_append_separator(sOutDirectory) + "visualize");

			if (bOk &&
				stlplus::is_folder(stlplus::folder_append_separator(sOutDirectory) + "models") &&
				stlplus::is_folder(stlplus::folder_append_separator(sOutDirectory) + "txt") &&
				stlplus::is_folder(stlplus::folder_append_separator(sOutDirectory) + "visualize")
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
				for (const auto& camera : map_Camera)
				{
					const Mat34 & PMat = camera.second._P;
					std::ostringstream os;
					os << std::setw(8) << std::setfill('0') << count;
					std::ofstream file(
						stlplus::create_filespec(stlplus::folder_append_separator(sOutDirectory) + "txt",
						os.str(), "txt").c_str());
					file << "CONTOUR\n"
						<< PMat.row(0) << "\n" << PMat.row(1) << "\n" << PMat.row(2) << std::endl;
					file.close();

					++count;
				}

				// Image
				count = 0;
				Image<RGBColor> image;
				for (const auto& camera : map_Camera)
				{
					size_t imageIndex = camera.first;
					const std::string & sImageName = vec_fileNames[imageIndex];
					std::ostringstream os;
					os << std::setw(8) << std::setfill('0') << count;
					ReadImage(stlplus::create_filespec(sImagePath, sImageName).c_str(), &image);
					std::string sCompleteImageName = stlplus::create_filespec(
						stlplus::folder_append_separator(sOutDirectory) + "visualize", os.str(), "jpg");
					WriteImage(sCompleteImageName.c_str(), image);

					++count;
				}

				//pmvs_options.txt
				std::ostringstream os;
				os << "level 1" << "\n"
					<< "csize 2" << "\n"
					<< "threshold 0.7" << "\n"
					<< "wsize 7" << "\n"
					<< "minImageNum 3" << "\n"
					<< "CPU 8" << "\n"
					<< "setEdge 0" << "\n"
					<< "useBound 0" << "\n"
					<< "useVisData 0" << "\n"
					<< "sequence -1" << "\n"
					<< "timages -1 0 " << map_Camera.size() << "\n"
					<< "oimages 0" << "\n"; // ?

				std::ofstream file(stlplus::create_filespec(sOutDirectory, "pmvs_options", "txt").c_str());
				file << os.str();
				file.close();
			}
			return bOk;
		}
	};

} // namespace openMVG

#endif // OPENMVG_SFM_RECONSTRUCTION_DATA_H