#pragma once

#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <memory>

#include <QThread>

#include "ANNWrapper.h"

namespace mesh
{
	struct Point3D
	{
		float x, y, z;
		std::vector <int> visible_in;
	};

	struct Camera
	{
		float focal;
		openMVG::Matf R, t;
		openMVG::Matf invK;

		float x, y, z; // position
		float nx, ny, nz; // normal

		Camera()
		{
			R = openMVG::Matf::Zero(3, 3);
			t = openMVG::Matf::Zero(3, 1);
		}
	};

	struct Face
	{
		int image_num;
		unsigned int v1, v2, v3;
		float x, y, z; // centre of triangle
		float nx, ny, nz;
		float u[3], v[4]; // texture UV coords
		std::vector <int> visible_in;
	};

	class TextureMesh : public QThread
	{
		Q_OBJECT

	public:
		TextureMesh(const QString& pmvsDir);
		~TextureMesh();

		virtual bool getResult()const
		{
			return result;
		}

	protected:
		virtual void run();

	signals:
		void sendStatues(const QString& message);

	private:
		bool setImgWH(const std::string& pmvsDir);
		bool LoadPlyMesh(const std::string &filename, std::vector <openMVG::Vec3f> &vertex, std::vector <Face> &face);
		bool LoadBundle(const std::string &filename, std::vector <Camera> &cameras);
		bool LoadPMVSPatch(const std::string& patch_file, std::vector <Point3D> &points);
		bool NormalFrom3Vertex(const openMVG::Vec3f &v1, const openMVG::Vec3f &v2, const openMVG::Vec3f &v3, float &nx, float &ny, float &nz);
		bool AssignTexture(const std::vector <openMVG::Vec3f> &vertexes, const std::vector <Camera> &cameras, std::vector <Face> &faces);
		bool AssignVisibleCameras(const std::vector <openMVG::Vec3f> &vertexes, const std::vector <Point3D> &points, const std::vector <Camera> &cameras, std::vector <Face> &faces);
		bool SaveFaces(const std::string &filename, const std::vector <Face> &faces, const std::vector <openMVG::Vec3f> &vertexes, const std::vector <Camera> &cameras);
		bool SavePoints(const std::string &filename, std::vector <Point3D> &points);
		bool SaveWavefrontOBJ(const std::string &filename, const std::vector <Face> &faces, const std::vector <openMVG::Vec3f> &vertexes, const std::vector <Camera> &cameras);

	private:
		unsigned imgWidth;
		unsigned imgHeight;
		QString pmvsDir;

		bool result;

		//log
		std::ofstream logFile;
		std::streambuf *stdFile;
	};

}