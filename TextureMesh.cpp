#include "TextureMesh.h"
#include "stlplus3/filesystemSimplified/file_system.hpp"
#include "openMVG/image/image.hpp"
#include "openMVG/exif_IO/exif_IO_openExif.hpp"

namespace mesh
{
	TextureMesh::TextureMesh(const QString& pmvsDir)
		:pmvsDir(pmvsDir)
	{
		moveToThread(this);

		std::string logPath = "log/TextureMesh.log";
		std::string logDir = stlplus::folder_part(logPath);

		if (!stlplus::folder_exists(logDir))
		{
			stlplus::folder_create(logDir);
		}

		imgWidth = 0;
		imgHeight = 0;

		logFile.open(logPath.c_str());

		stdFile = std::cout.rdbuf(logFile.rdbuf());
		std::cerr.rdbuf(logFile.rdbuf());
	}


	TextureMesh::~TextureMesh()
	{
		std::cout.rdbuf(stdFile);

		//delete stdFile;
		if (logFile.is_open()) logFile.close();
	}


	void TextureMesh::run()
	{
		std::vector <Camera> cameras;
		std::vector <Point3D> points;
		std::vector <openMVG::Vec3f> vertexes;
		std::vector <Face> faces;
		std::vector < std::vector<int> > connections;

		emit sendStatues(QStringLiteral("Texture: get the image width and height !"));
		if (!setImgWH(pmvsDir.toStdString()))
		{
			std::cout << "setImgWH failure !" << std::endl;
			result = false;
			return;
		}
		emit sendStatues(QStringLiteral("Texture: LoadBundle file ....!"));
		LoadBundle(pmvsDir.toStdString() + "/bundle.rd.out", cameras);

		std::vector<std::string> fileList = stlplus::folder_files(pmvsDir.toStdString() + "/models");

		emit sendStatues(QStringLiteral("Texture: LoadPMVSPatch ....!"));
		for (auto &str : fileList)
		{
			if (stlplus::extension_part(str) == "patch")
			{
				std::cout << str << std::endl;

				std::vector <Point3D> tmp;

				LoadPMVSPatch(pmvsDir.toStdString() + "/models/" + str, tmp);

				points.insert(points.end(), tmp.begin(), tmp.end());
			}
		}

		if (points.empty())
		{
			result = false;
			return;
		}

		emit sendStatues(QStringLiteral("Texture: LoadPlyMesh ....!"));
		if (!LoadPlyMesh(pmvsDir.toStdString() + "/models/mesh.ply", vertexes, faces))
		{
			result = false;
			return;
		}

		emit sendStatues(QStringLiteral("Texture: AssignVisibleCameras ....!"));
		AssignVisibleCameras(vertexes, points, cameras, faces);

		emit sendStatues(QStringLiteral("Texture: AssignTexture ....!"));
		AssignTexture(vertexes, cameras, faces);

		SaveFaces(pmvsDir.toStdString() + "/models/output.mesh", faces, vertexes, cameras);
		SavePoints(pmvsDir.toStdString() + "/models/output.point", points);
		SaveWavefrontOBJ(pmvsDir.toStdString() + "/models/output.obj", faces, vertexes, cameras);

		std::cout << "Done!" << std::endl;

		emit sendStatues(QStringLiteral("Texture: is ok!"));
	}


	bool TextureMesh::LoadBundle(const std::string &filename, std::vector <Camera> &cameras)
	{
		std::ifstream bundle(filename.c_str());

		if (!bundle) {
			std::cerr << "LoadBundle(): Error opening " << filename << " for reading" << std::endl;
			return false;
		}

		std::cout << "Loading " << filename << " ... " << std::endl;

		std::stringstream str;
		char line[1024];
		int num;

		bundle.getline(line, sizeof(line)); // header
		bundle.getline(line, sizeof(line));

		str.str(line);
		str >> num;

		cameras.resize(num);

		std::cout << "    cameras = " << cameras.size() << std::endl;

		for (int i = 0; i < num; i++)
		{
			Camera new_camera;

			bundle.getline(line, sizeof(line)); // focal, r1, r2

			str.str(line);
			str.clear();
			str >> new_camera.focal;

			bundle.getline(line, sizeof(line)); // rotation 1
			str.str(line);
			str.clear();
			str >> new_camera.R(0, 0);
			str >> new_camera.R(0, 1);
			str >> new_camera.R(0, 2);

			bundle.getline(line, sizeof(line)); // rotation 2
			str.str(line);
			str.clear();
			str >> new_camera.R(1, 0);
			str >> new_camera.R(1, 1);
			str >> new_camera.R(1, 2);

			bundle.getline(line, sizeof(line)); // rotation 3
			str.str(line);
			str.clear();
			str >> new_camera.R(2, 0);
			str >> new_camera.R(2, 1);
			str >> new_camera.R(2, 2);

			bundle.getline(line, sizeof(line)); // translation
			str.str(line);
			str.clear();
			str >> new_camera.t(0, 0);
			str >> new_camera.t(1, 0);
			str >> new_camera.t(2, 0);

			// Camera position and normal
			{
				// http://phototour.cs.washington.edu/bundler/bundler-v0.4-manual.html
				// -R' * t
				float r[9];
				float t[3];

				for (int i = 0; i < 3; i++)
				{
					for (int j = 0; j < 3; j++)
					{
						r[i * 3 + j] = new_camera.R(i, j);
					}
				}

				for (int i = 0; i < 3; i++)
				{
					t[i] = new_camera.t(i, 0);
				}

				new_camera.x = -r[6] * t[2] - r[3] * t[1] - r[0] * t[0];
				new_camera.y = -r[7] * t[2] - r[4] * t[1] - r[1] * t[0];
				new_camera.z = -r[8] * t[2] - r[5] * t[1] - r[2] * t[0];

				new_camera.nx = -r[6];
				new_camera.ny = -r[7];
				new_camera.nz = -r[8];;
			}

			cameras[i] = new_camera;
		}

		bundle.close();

		return true;
	}

	bool TextureMesh::LoadPMVSPatch(const std::string& patch_file, std::vector <Point3D> &points)
	{
		std::ifstream input(patch_file.c_str());

		if (!input)
		{
			std::cerr << "LoadPMVSPatch(): Error opening " << patch_file << " for reading" << std::endl;
			return false;
		}

		std::cout << "Loading " << patch_file << " ... " << std::endl;

		char line[2048];
		char word[128];
		std::stringstream str;
		int num_pts;
		float not_used;

		input.getline(line, sizeof(line)); // header
		str.str(line);
		str >> word;

		if (word != std::string("PATCHES"))
		{
			std::cerr << "LoadPMVSPatch(): Incorrect header" << std::endl;
			return false;
		}

		input.getline(line, sizeof(line)); // number of points
		str.str(line);
		str.clear();
		str >> num_pts;

		std::cout << "    points = " << num_pts << std::endl;

		points.resize(num_pts);

		for (int i = 0; i < num_pts; i++)
		{
			Point3D &pt = points[i];
			int num;

			input.getline(line, sizeof(line)); // another header
			//assert(string(line) == "PATCHS");

			input.getline(line, sizeof(line)); // position
			str.str(line);
			str.clear();
			str >> pt.x;
			str >> pt.y;
			str >> pt.z;

			input.getline(line, sizeof(line)); // normal
			str.str(line);
			str.clear();
			str >> not_used;
			str >> not_used;
			str >> not_used;

			input.getline(line, sizeof(line)); // debugging stuff
			input.getline(line, sizeof(line)); // num images visible in
			str.str(line);
			str.clear();
			str >> num;

			if (num <= 0) return false;

			input.getline(line, sizeof(line)); // image index
			str.str(line);
			str.clear();

			for (int j = 0; j < num; j++)
			{
				int idx;
				str >> idx;
				pt.visible_in.push_back(idx);
			}

			input.getline(line, sizeof(line)); // possibly visible in
			str.str(line);
			str.clear();
			str >> num;

			input.getline(line, sizeof(line)); // image index
			str.str(line);
			str.clear();

			for (int j = 0; j < num; j++) {
				int idx;
				str >> idx;
				pt.visible_in.push_back(idx);
			}

			input.getline(line, sizeof(line)); // blank line

			if (input.eof()) {
				std::cerr << "LoadPMVSPatch(): Premature end of file" << std::endl;
				return false;
			}
		}

		return true;
	}

	bool TextureMesh::SavePoints(const std::string &filename, std::vector <Point3D> &points)
	{
		std::ofstream output(filename.c_str());

		if (!output) {
			std::cerr << "Can't open " << filename << " for writing" << std::endl;
			return false;
		}

		std::cout << "Saving " << filename << " ..." << std::endl;

		output << points.size() << std::endl;

		for (size_t i = 0; i < points.size(); i++) {
			output.write((char*)&points[i].x, sizeof(float)* 3);
		}

		return true;
	}

	bool TextureMesh::LoadPlyMesh(const std::string &filename, std::vector <openMVG::Vec3f> &vertexes, std::vector <Face> &faces)
	{
		std::ifstream input(filename.c_str());

		if (!input)
		{
			std::cerr << "LoadPlyMesh(): Error opening " << filename << " for reading" << std::endl;
			return false;
		}

		std::cout << "Loading " << filename << " ... " << std::endl;

		char line[1024];
		std::string token;

		input.getline(line, sizeof(line));
		std::stringstream str(line);
		str >> token;

		if (token != "ply")
		{
			std::cerr << "Not a PLY format" << std::endl;
			return false;
		}

		size_t num_vertex, num_face;
		while (input.getline(line, sizeof(line)))
		{
			str.str(line);
			str.clear();
			str >> token;

			if (token == "element") {
				str >> token;

				if (token == "vertex") {
					str >> num_vertex;
				}
				else if (token == "face") {
					str >> num_face;
				}
			}
			else if (token == "end_header") {
				break;
			}
		}

		std::cout << "    vertexes = " << num_vertex << std::endl;
		std::cout << "    faces = " << num_face << std::endl;

		vertexes.resize(num_vertex);
		faces.resize(num_face);

		for (size_t i = 0; i < num_vertex; i++) {
			input.getline(line, sizeof(line));
			str.str(line);
			str.clear();

			str >> vertexes[i].x();
			str >> vertexes[i].y();
			str >> vertexes[i].z();
		}

		for (size_t i = 0; i < num_face; i++) {
			int n;

			input.getline(line, sizeof(line));
			str.str(line);
			str.clear();

			str >> n;
			str >> faces[i].v1;
			str >> faces[i].v2;
			str >> faces[i].v3;

			faces[i].x = (vertexes[faces[i].v1].x() + vertexes[faces[i].v2].x() + vertexes[faces[i].v3].x()) / 3.0f;
			faces[i].y = (vertexes[faces[i].v1].y() + vertexes[faces[i].v2].y() + vertexes[faces[i].v3].y()) / 3.0f;
			faces[i].z = (vertexes[faces[i].v1].z() + vertexes[faces[i].v2].z() + vertexes[faces[i].v3].z()) / 3.0f;

			NormalFrom3Vertex(vertexes[faces[i].v1], vertexes[faces[i].v2], vertexes[faces[i].v3], faces[i].nx, faces[i].ny, faces[i].nz);
		}

		return true;
	}

	bool TextureMesh::NormalFrom3Vertex(const openMVG::Vec3f &v1, const openMVG::Vec3f &v2, const openMVG::Vec3f &v3, float &nx, float &ny, float &nz)
	{
		float x1 = v2.x() - v1.x();
		float y1 = v2.y() - v1.y();
		float z1 = v2.z() - v1.z();

		float x2 = v3.x() - v2.x();
		float y2 = v3.y() - v2.y();
		float z2 = v3.z() - v2.z();

		float mag1 = sqrt(x1*x1 + y1*y1 + z1*z1);
		float mag2 = sqrt(x2*x2 + y2*y2 + z2*z2);

		x1 /= mag1;
		y1 /= mag1;
		z1 /= mag1;

		x2 /= mag2;
		y2 /= mag2;
		z2 /= mag2;

		nx = y1*z2 - z1*y2;
		ny = z1*x2 - x1*z2;
		nz = x1*y2 - y1*x2;

		return true;
	}

	bool TextureMesh::AssignTexture(const std::vector <openMVG::Vec3f> &vertexes, const std::vector <Camera> &cameras, std::vector <Face> &faces)
	{
		openMVG::Matf X(3, 1);
		float cx = imgWidth*0.5f;
		float cy = imgHeight*0.5f;

		for (size_t i = 0; i < faces.size(); i++) {
			openMVG::Vec3f v[3];
			v[0] = vertexes[faces[i].v1];
			v[1] = vertexes[faces[i].v2];
			v[2] = vertexes[faces[i].v3];

			// Pick the first image
			int image_num = faces[i].image_num;

			const Camera &camera = cameras[image_num];

			for (int j = 0; j < 3; j++)
			{
				X(0, 0) = v[j].x();
				X(1, 0) = v[j].y();
				X(2, 0) = v[j].z();

				X = camera.R*X + camera.t;

				float xx = -X(0, 0) / X(2, 0);
				float yy = -X(1, 0) / X(2, 0);

				float u = (xx*camera.focal + cx) / imgWidth;
				float v = (yy*camera.focal + cy) / imgHeight;

				// Hmm due to some numerical issue, it is possible to (u,v) that are slightly outside [0,1]
				// Will look into it when it becomes a problem!
				//assert(u >= 0 && u <= 1);
				//assert(v >= 0 && v <= 1);

				faces[i].u[j] = u;
				faces[i].v[j] = v;
			}
		}

		return true;
	}

	bool TextureMesh::AssignVisibleCameras(const std::vector <openMVG::Vec3f> &vertexes, const std::vector <Point3D> &points, const std::vector <Camera> &cameras, std::vector <Face> &faces)
	{
		mvg::ANNWrapper ann;

		std::vector <openMVG::Vec3f> tmp(points.size());

		for (size_t i = 0; i < tmp.size(); i++) {
			tmp[i].x() = points[i].x;
			tmp[i].y() = points[i].y;
			tmp[i].z() = points[i].z;
		}

		ann.SetPoints(tmp);

		for (size_t i = 0; i < faces.size(); i++)
		{
			openMVG::Vec3f v[3];
			v[0] = vertexes[faces[i].v1];
			v[1] = vertexes[faces[i].v2];
			v[2] = vertexes[faces[i].v3];

			openMVG::Vec3f c;

			c.x() = (v[0].x() + v[1].x() + v[2].x()) / 3.0f;
			c.y() = (v[0].y() + v[1].y() + v[2].y()) / 3.0f;
			c.z() = (v[0].z() + v[1].z() + v[2].z()) / 3.0f;

			double dist2;
			int index;

			ann.FindClosest(c, &dist2, &index);

			faces[i].visible_in = points[index].visible_in;
		}

		return true;
	}

	bool TextureMesh::SaveFaces(const std::string &filename, const std::vector <Face> &faces, const std::vector <openMVG::Vec3f> &vertexes, const std::vector <Camera> &cameras)
	{
		std::ofstream output(filename.c_str());

		if (!output) {
			std::cerr << "Can't open " << filename << " for writing" << std::endl;
			return false;
		}

		std::cout << "Saving " << filename << " ..." << std::endl;

		output << faces.size() << " " << cameras.size() << std::endl;

		for (size_t i = 0; i < faces.size(); i++) {
			openMVG::Vec3f v[3];

			v[0] = vertexes[faces[i].v1];
			v[1] = vertexes[faces[i].v2];
			v[2] = vertexes[faces[i].v3];

			for (int j = 0; j < 3; j++) {
				output << v[j].x() << " ";
				output << v[j].y() << " ";
				output << v[j].z() << " ";
			}

			output << faces[i].image_num << " ";

			for (int j = 0; j < 3; j++) {
				output << faces[i].u[j] << " ";
				output << faces[i].v[j] << " ";
			}

			output << std::endl;
		}

		return true;
	}

	bool TextureMesh::SaveWavefrontOBJ(const std::string &filename, const std::vector <Face> &faces, const std::vector <openMVG::Vec3f> &vertexes, const std::vector <Camera> &cameras)
	{
		int start = filename.find_last_of('/');
		int end = filename.find_last_of('.');
		int len = end - start;

		std::string base_name = filename.substr(start + 1, len - 1);
		std::string mtl_name = filename.substr(0, end) + ".mtl";

		std::ofstream output(filename.c_str());
		std::ofstream mtl(mtl_name.c_str());

		if (!output) {
			std::cerr << "Can't open " << filename << " for writing" << std::endl;
			return false;
		}

		if (!output) {
			std::cerr << "Can't open " << mtl_name << " for writing" << std::endl;
			return false;
		}

		std::cout << "Saving " << filename << " ..." << std::endl;
		std::cout << "Saving " << mtl_name << " ..." << std::endl;

		// MTL file
		for (size_t i = 0; i < cameras.size(); i++)
		{
			mtl << "newmtl Texture_" << i << std::endl;
			mtl << "Ka 1 1 1" << std::endl;
			mtl << "Kd 1 1 1" << std::endl;
			mtl << "Ks 0 0 0" << std::endl;
			mtl << "d 1.0" << std::endl;
			mtl << "illum 2" << std::endl;
			mtl << "map_Kd ../visualize/" << std::setfill('0') << std::setw(8) << i << ".jpg" << std::endl;
			mtl << std::endl;
		}

		// Writing OBJ file
		output << "mtllib " << base_name << ".mtl" << std::endl;

		// Write vertices
		for (size_t i = 0; i < vertexes.size(); i++)
		{
			output << "v " << vertexes[i].x() << " " << vertexes[i].y() << " " << vertexes[i].z() << std::endl;
		}

		// Write texture coordinates, ordered by camera and face
		for (size_t c = 0; c < cameras.size(); c++)
		{
			for (size_t f = 0; f < faces.size(); f++)
			{
				if (faces[f].image_num != (int)c)
				{
					continue;
				}

				for (int j = 0; j < 3; j++)
				{
					output << "vt " << faces[f].u[j] << " " << faces[f].v[j] << std::endl;
				}
			}
		}

		// Write face
		int idx = 0;
		for (size_t c = 0; c < cameras.size(); c++)
		{
			output << "usemtl Texture_" << c << std::endl;

			for (size_t f = 0; f < faces.size(); f++)
			{
				if (faces[f].image_num != (int)c)
				{
					continue;
				}

				output << "f ";
				output << (faces[f].v1 + 1) << "/" << (idx + 1) << " ";
				output << (faces[f].v2 + 1) << "/" << (idx + 2) << " ";
				output << (faces[f].v3 + 1) << "/" << (idx + 3) << std::endl;

				idx += 3;
			}
		}

		return true;
	}

	bool TextureMesh::setImgWH(const std::string& pmvsDir)
	{
		if (!stlplus::folder_exists(pmvsDir)) return false;

		std::string imgPath = pmvsDir + "/visualize/00000000.jpg";

		std::unique_ptr<Exif_IO> exifReader(new Exif_IO_OpenExif());
		exifReader->open(imgPath);

		if (!exifReader->doesHaveExifInfo())
		{
			Image<unsigned char> image;
			if (openMVG::ReadImage(imgPath.c_str(), &image))
			{
				imgWidth = image.Width();
				imgHeight = image.Height();
			}
			else
			{
				Image<RGBColor> imageRGB;
				if (openMVG::ReadImage(imgPath.c_str(), &imageRGB))
				{
					imgWidth = imageRGB.Width();
					imgHeight = imageRGB.Height();
				}
				else
				{
					Image<RGBAColor> imageRGBA;
					if (openMVG::ReadImage(imgPath.c_str(), &imageRGBA))
					{
						imgWidth = imageRGBA.Width();
						imgHeight = imageRGBA.Height();
					}
				}
			}

		}
		else
		{
			imgWidth = exifReader->getWidth();
			imgHeight = exifReader->getHeight();
		}

		exifReader.reset();

		if (imgWidth < 1 || imgHeight < 1)
		{
			std::cout << "Read the img failure :" << imgPath << std::endl;
			return false;
		}

		return true;
	}
}