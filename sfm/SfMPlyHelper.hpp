
// Copyright (c) 2012, 2013 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENMVG_SFM_PLY_HELPER_H
#define OPENMVG_SFM_PLY_HELPER_H

#include "openMVG/numeric/numeric.h"

#include <fstream>
#include <string>
#include <vector>

namespace openMVG{
	namespace plyHelper{

		/// Export 3D point vector to PLY format
		static bool exportToPly(const std::vector<Vec3> & vec_points,
			const std::string & sFileName)
		{
			std::ofstream outfile;
			outfile.open(sFileName.c_str(), std::ios_base::out);

			outfile << "ply"
				<< std::endl << "format ascii 1.0"
				<< std::endl << "element vertex " << vec_points.size()
				<< std::endl << "property float x"
				<< std::endl << "property float y"
				<< std::endl << "property float z"
				<< std::endl << "property uchar red"
				<< std::endl << "property uchar green"
				<< std::endl << "property uchar blue"
				<< std::endl << "end_header" << std::endl;

			for (size_t i = 0; i < vec_points.size(); ++i)
			{
				outfile << vec_points[i].transpose()
					<< " 255 255 255" << "\n";
			}
			bool bOk = outfile.good();
			outfile.close();
			return bOk;
		}

		/// Export 3D point vector and camera position to PLY format
		static bool exportToPly(const std::vector<Vec3> & points,
			const std::vector<Vec3> & camPos,
			const std::string & sFileName,
			const std::vector<Vec3>& coloredPoints = std::vector<Vec3>())
		{
			std::ofstream outfile;
			outfile.open(sFileName.c_str(), std::ios_base::out);

			outfile << "ply"
				<< '\n' << "format ascii 1.0"
				<< '\n' << "element vertex " << points.size() + camPos.size()
				<< '\n' << "property float x"
				<< '\n' << "property float y"
				<< '\n' << "property float z"
				<< '\n' << "property uchar red"
				<< '\n' << "property uchar green"
				<< '\n' << "property uchar blue"
				<< '\n' << "end_header" << std::endl;

			for (size_t i = 0; i < points.size(); ++i) 
			{
				if (coloredPoints.size() < 1)
					outfile << points[i].transpose()
					<< " 255 255 255" << "\n";
				else
					outfile << points[i].transpose()
					<< " " << coloredPoints[i].transpose() << "\n";
			}

			for (const auto& cam : camPos)
			{
				outfile << cam.transpose()
					<< " 0 255 0" << "\n";
			}

			outfile.flush();
			bool bOk = outfile.good();
			outfile.close();
			return bOk;
		}

	} // namespace plyHelper
} // namespace openMVG

#endif // OPENMVG_SFM_PLY_HELPER_H