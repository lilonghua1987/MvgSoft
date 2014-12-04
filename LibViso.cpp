#include "LibViso.h"

LibViso::LibViso(const QString& imgDir)
    :imgDir(imgDir)
{
	moveToThread(this);

	result = true;

	std::string logPath = "log/LibViso.log";
	std::string logDir = stlplus::folder_part(logPath);

	if (!stlplus::folder_exists(logDir))
	{
		stlplus::folder_create(logDir);
	}

	logFile.open(logPath.c_str());

	stdFile = std::cout.rdbuf(logFile.rdbuf());
	std::cerr.rdbuf(logFile.rdbuf());

	std::cout << " currentThreadId : " << this->currentThreadId << std::endl;
}

LibViso::~LibViso()
{
	std::cout.rdbuf(stdFile);

	//delete stdFile;
	if (logFile.is_open()) logFile.close();
}

void LibViso::run()
{
	if (!stlplus::folder_exists(imgDir.toStdString()))
	{
		std::cerr << "\nImgDir : " << imgDir.toStdString() << " directory doesn't exist" << std::endl;
		result = false;
		return;
	}

	emit sendStatues(QStringLiteral("Initial Option ....."));
	// set most important visual odometry parameters
	// for a full parameter list, look at: viso_stereo.h
	libviso::VisualOdometryMono::parameters param;

	// calibration parameters for sequence 2010_03_09_drive_0019 
	param.calib.f = 645.24; // focal length in pixels
	param.calib.cu = 635.96; // principal point (u-coordinate) in pixels
	param.calib.cv = 194.13; // principal point (v-coordinate) in pixels

	param.height = 1.6;
	param.pitch = -0.08;
	param.bucket.max_features = 1000; //disable bucketing

	// init visual odometry
	libviso::VisualOdometryMono viso(param);
	libviso::Reconstruction  R; R.setCalibration(param.calib.f, param.calib.cu, param.calib.cv);

	emit sendStatues(QStringLiteral("libviso running ....."));
	libviso::Matrix poseG = libviso::Matrix::eye(4);
	std::vector<libviso::Reconstruction::point3d> pointCloud;
	bool replace = false;
	// loop through all frames
	for (int32_t i = 0; i<373; i++)
	{

		// input file names
		char base_name[256]; sprintf(base_name, "%06d.png", i);
		std::string limgName = imgDir.toStdString() + "/I1_" + base_name;

		std::vector<libviso::Matrix> points;
		// catch image read/write errors here
		try {

			// load left and right input image
			Image<unsigned char> limg;

			if (!openMVG::ReadImage(limgName.c_str(), &limg))
				continue;

			// image dimensions
			int32_t width = limg.Width();
			int32_t height = limg.Height();

			// status
			std::cout << "Processing: Frame: " << "->width : " << width << " ,  height" << height << i << std::endl;;

			// compute visual odometry
			int32_t dims[] = { width, height, width };
			if (viso.process(limg.data(), dims, replace))
			{
				if (i < 2) continue;
				// on success, update current pose
				libviso::Matrix pose = viso.getMotion();
				if (pose.val != nullptr)
				{
					//pose = libviso::Matrix::inv(pose);
					poseG = poseG * pose;

					std::vector<libviso::Matcher::p_match> p_matched = viso.getMatches();

					// output some statistics
					double num_matches = viso.getNumberOfMatches();
					double num_inliers = viso.getNumberOfInliers();
					std::cout << ", Matches: " << num_matches;
					std::cout << ", Inliers: " << 100.0*num_inliers / num_matches << " %" << ", Current pose: " << std::endl;
					std::cout << pose << std::endl << std::endl;

					R.update(p_matched, pose, 2, 2, 30, 3);
				}
				else
				{
					replace = true;
				}

				std::vector<libviso::Reconstruction::point3d> points = R.getPoints();

				for (const auto& p : points)
				{
					libviso::Matrix temp(1, 4);
					temp.val[0][0] = p.x;
					temp.val[0][1] = p.y;
					temp.val[0][2] = p.z;
					temp.val[0][3] = 1;
					temp = temp * pose;

					pointCloud.push_back(libviso::Reconstruction::point3d((float)temp.val[0][0], (float)temp.val[0][1], (float)temp.val[0][2]));
				}

			}

			// catch image read errors here
		}
		catch (...) 
		{
			std::cerr << "ERROR: Couldn't read input files!" << std::endl;
			result = false;
			return ;
		}
	}
	exportPLY(pointCloud, imgDir.toStdString() + "/pointClouds.ply");
	// output
	std::cout << "Demo complete! Exiting ..." << std::endl;
	emit sendStatues(QStringLiteral("LibViso is ok !"));
}

/// Export 3D point vector and camera position to PLY format
void LibViso::exportPLY(const std::vector<libviso::Reconstruction::point3d>& pointCloud, const std::string& fileName)
{
	std::ofstream outfile;
	outfile.open(fileName.c_str(), std::ios_base::out);

	outfile << "ply"
		<< '\n' << "format ascii 1.0"
		<< '\n' << "element vertex " << pointCloud.size()
		<< '\n' << "property float x"
		<< '\n' << "property float y"
		<< '\n' << "property float z"
		<< '\n' << "property uchar red"
		<< '\n' << "property uchar green"
		<< '\n' << "property uchar blue"
		<< '\n' << "end_header" << std::endl;

	for (const auto& p : pointCloud)
	{
		outfile << p.x << " " << p.y << " " << p.z << "\n";
		outfile << "0 0 255 " << "\n";
	}

	outfile.flush();
	outfile.close();
}
