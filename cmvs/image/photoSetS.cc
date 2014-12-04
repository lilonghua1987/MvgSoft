#include <fstream>
#include <algorithm>
#include "photoSetS.h"
#define _USE_MATH_DEFINES
#include <math.h>

namespace cpmvs_image
{
	CphotoSetS::CphotoSetS(void) {
	}

	CphotoSetS::~CphotoSetS() {
	}


	void CphotoSetS::init(const std::vector<int>& images, const std::string prefix,
		const int maxLevel, const int size, const int alloc) {
		m_images = images;
		m_num = (int)images.size();

		for (int i = 0; i < (int)images.size(); ++i)
			m_dict[images[i]] = i;

		m_prefix = prefix;
		m_maxLevel = std::max(1, maxLevel);
		m_photos.resize(m_num);
		std::cerr << "Reading images: " << std::flush;
		for (int index = 0; index < m_num; ++index) {
			const int image = m_images[index];

			char test0[1024], test1[1024];
			sprintf(test0, "%svisualize/%08d.ppm", prefix.c_str(), image);
			sprintf(test1, "%svisualize/%08d.jpg", prefix.c_str(), image);
			if (std::ifstream(test0) || std::ifstream(test1)) {
				char name[1024], mname[1024], ename[1024], cname[1024];

				// Set name
				sprintf(name, "%svisualize/%08d", prefix.c_str(), image);
				sprintf(mname, "%smasks/%08d", prefix.c_str(), image);
				sprintf(ename, "%sedges/%08d", prefix.c_str(), image);
				sprintf(cname, "%stxt/%08d.txt", prefix.c_str(), image);

				m_photos[index].init(name, mname, ename, cname, m_maxLevel);
				if (alloc)
					m_photos[index].alloc();
				else
					m_photos[index].alloc(1);
				std::cerr << '*' << std::flush;
			}
			// try 4 digits
			else {
				char name[1024], mname[1024], ename[1024], cname[1024];

				// Set name
				sprintf(name, "%svisualize/%04d", prefix.c_str(), image);
				sprintf(mname, "%smasks/%04d", prefix.c_str(), image);
				sprintf(ename, "%sedges/%04d", prefix.c_str(), image);
				sprintf(cname, "%stxt/%04d.txt", prefix.c_str(), image);

				m_photos[index].init(name, mname, ename, cname, m_maxLevel);
				if (alloc)
					m_photos[index].alloc();
				else
					m_photos[index].alloc(1);
				std::cerr << '*' << std::flush;
			}

			/*
			const int image = m_images[index];
			char name[1024], mname[1024], ename[1024], cname[1024];

			// Set name
			sprintf(name, "%svisualize/%08d", prefix.c_str(), image);
			sprintf(mname, "%smasks/%08d", prefix.c_str(), image);
			sprintf(ename, "%sedges/%08d", prefix.c_str(), image);
			sprintf(cname, "%stxt/%08d.txt", prefix.c_str(), image);

			m_photos[index].init(name, mname, ename, cname, m_maxLevel);
			if (alloc)
			m_photos[index].alloc();
			else
			m_photos[index].alloc(1);
			cerr << '*' << flush;
			*/
		}
		std::cerr << std::endl;
		const int margin = size / 2;
		m_size = 2 * margin + 1;
	}

	void CphotoSetS::free(void) {
		for (int index = 0; index < (int)m_photos.size(); ++index)
			m_photos[index].free();
	}

	void CphotoSetS::free(const int level) {
		for (int index = 0; index < (int)m_photos.size(); ++index)
			m_photos[index].free(level);
	}

	void CphotoSetS::setEdge(const float threshold) {
		for (int index = 0; index < m_num; ++index)
			m_photos[index].setEdge(threshold);
	}

	void CphotoSetS::write(const std::string outdir) {
		for (int index = 0; index < m_num; ++index) {
			const int image = m_images[index];
			char buffer[1024];
			sprintf(buffer, "%s%08d.txt", outdir.c_str(), image);

			m_photos[index].write(buffer);
		}
	}

	// get x and y axis to collect textures given reference index and normal
	void CphotoSetS::getPAxes(const int index, const pmvs::Vec4f& coord, const pmvs::Vec4f& normal,
		pmvs::Vec4f& pxaxis, pmvs::Vec4f& pyaxis) const{
		m_photos[index].getPAxes(coord, normal, pxaxis, pyaxis);
	}

	void CphotoSetS::grabTex(const int index, const int level, const pmvs::Vec2f& icoord,
		const pmvs::Vec2f& xaxis, const pmvs::Vec2f& yaxis,
		std::vector<pmvs::Vec3f>& tex, const int normalizef) const{
		m_photos[index].grabTex(level, icoord, xaxis, yaxis, m_size, tex, normalizef);
	}

	// grabTex given 3D sampling information
	void CphotoSetS::grabTex(const int index, const int level, const pmvs::Vec4f& coord,
		const pmvs::Vec4f& pxaxis, const pmvs::Vec4f& pyaxis, const pmvs::Vec4f& pzaxis,
		std::vector<pmvs::Vec3f>& tex, float& weight,
		const int normalizef) const {
		m_photos[index].grabTex(level, coord, pxaxis, pyaxis, pzaxis,
			m_size, tex, weight, normalizef);
	}

	float CphotoSetS::incc(const std::vector<std::vector<pmvs::Vec3f> >& texs,
		const std::vector<float>& weights) {
		float incctmp = 0.0;
		float denom = 0.0;
		for (int i = 0; i < (int)weights.size(); ++i) {
			if (texs[i].empty())
				continue;
			for (int j = i + 1; j < (int)weights.size(); ++j) {
				if (texs[j].empty())
					continue;

				const float weight = weights[i] * weights[j];
				const float ftmp = Cphoto::idot(texs[i], texs[j]);
				incctmp += ftmp * weight;
				denom += weight;
			}
		}

		if (denom == 0.0)
			return 2.0f;
		else
			return incctmp / denom;
	}

	void CphotoSetS::getMinMaxAngles(const pmvs::Vec4f& coord, const std::vector<int>& indexes,
		float& minAngle, float& maxAngle) const {
		minAngle = M_PI;
		maxAngle = 0.0f;
		std::vector<pmvs::Vec4f> rays;  rays.resize((int)indexes.size());
		for (int i = 0; i < (int)indexes.size(); ++i) {
			const int index = indexes[i];
			rays[i] = m_photos[index].m_center - coord;
			unitize(rays[i]);
		}

		for (int i = 0; i < (int)indexes.size(); ++i) {
			for (int j = i + 1; j < (int)indexes.size(); ++j) {
				const float dot = std::max(-1.0f, std::min(1.0f, rays[i] * rays[j]));
				const float angle = acos(dot);
				minAngle = std::min(angle, minAngle);
				maxAngle = std::max(angle, maxAngle);
			}
		}
	}

	int CphotoSetS::checkAngles(const pmvs::Vec4f& coord,
		const std::vector<int>& indexes,
		const float minAngle, const float maxAngle,
		const int num) const {
		int count = 0;

		std::vector<pmvs::Vec4f> rays;  rays.resize((int)indexes.size());
		for (int i = 0; i < (int)indexes.size(); ++i) {
			const int index = indexes[i];
			rays[i] = m_photos[index].m_center - coord;
			unitize(rays[i]);
		}

		for (int i = 0; i < (int)indexes.size(); ++i) {
			for (int j = i + 1; j < (int)indexes.size(); ++j) {
				const float dot = std::max(-1.0f, std::min(1.0f, rays[i] * rays[j]));
				const float angle = acos(dot);
				if (minAngle < angle && angle < maxAngle)
					++count;
			}
		}

		//if (count < num * (num - 1) / 2)
		if (count < 1)
			return 1;
		else
			return 0;
	}

	float CphotoSetS::computeDepth(const int index, const pmvs::Vec4f& coord) const {
		return m_photos[index].computeDepth(coord);
	}

	void CphotoSetS::setDistances(void) {
		m_distances.resize(m_num);
		float avedis = 0.0f;
		int denom = 0;
		for (int i = 0; i < m_num; ++i) {
			m_distances[i].resize(m_num);
			for (int j = 0; j < m_num; ++j) {
				if (i == j)
					m_distances[i][j] = 0.0f;
				else {
					const float ftmp = norm(m_photos[i].m_center - m_photos[j].m_center);
					m_distances[i][j] = ftmp;
					avedis += ftmp;
					denom++;
				}
			}
		}
		if (denom == 0)
			return;

		avedis /= denom;
		if (avedis == 0.0f) {
			std::cerr << "All the optical centers are identical..?" << std::endl;
			exit(1);
		}

		// plus angle difference
		for (int i = 0; i < m_num; ++i) {
			pmvs::Vec4f ray0 = m_photos[i].m_oaxis;
			ray0[3] = 0.0f;
			for (int j = 0; j < m_num; ++j) {
				pmvs::Vec4f ray1 = m_photos[j].m_oaxis;
				ray1[3] = 0.0f;

				m_distances[i][j] /= avedis;
				const float margin = cos(10.0f * M_PI / 180.0f);
				const float dis = std::max(0.0f, 1.0f - ray0 * ray1 - margin);
				m_distances[i][j] += dis;
			}
		}
	}

	int CphotoSetS::image2index(const int image) const {
		std::map<int, int>::const_iterator pos = m_dict.find(image);
		if (pos == m_dict.end())
			return -1;
		else
			return pos->second;
	}
}


