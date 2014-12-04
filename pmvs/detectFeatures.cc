#include <iostream>
#include <fstream>
#include "cmvs/image/image.h"
#include "detectFeatures.h"
#include "harris.h"
#include "dog.h"
#include "point.h"

namespace PMVS3
{

	CdetectFeatures::CdetectFeatures(void)
	{
	}

	CdetectFeatures::~CdetectFeatures() 
	{
	}

	void CdetectFeatures::run(const cpmvs_image::CphotoSetS& pss, const int num,
		const int csize, const int level,
		const int CPU) 
	{
		m_ppss = &pss;
		m_csize = csize;
		m_level = level;
		m_CPU = CPU;

		m_points.clear();
		m_points.resize(num);

		//----------------------------------------------------------------------

		jobs = num;
		int cpuN = (std::min)((int)std::thread::hardware_concurrency(), num);
		if (cpuN < 1) return;
		std::cout << "CdetectFeatures -> CPU Cores :" << cpuN << std::endl;
		std::vector<std::thread> threads(cpuN);

		for (auto& t : threads) t = std::thread([](CdetectFeatures& arg){arg.runThread(); }, std::ref(*this));

		for (auto& t : threads) t.join();
		//----------------------------------------------------------------------
		std::cerr << "done" << std::endl;
	}

	void CdetectFeatures::runThread(void) 
	{
		while (true) 
		{
			int && index = --jobs;

			if (index < 0)
				break;

			const int image = m_ppss->m_images[index];
			std::cerr << image << ' ' << std::flush;

			//May need file lock, because targetting images
			//should not overlap among multiple processors.    
			char buffer[1024];
			sprintf(buffer, "%smodels/%08d.affin%d", m_ppss->m_prefix.c_str(), image, m_level);
			std::ifstream ifstr;
			ifstr.open(buffer);
			if (ifstr.is_open())
			{
				ifstr.close();
				continue;
			}
			ifstr.close();

			//----------------------------------------------------------------------
			// parameters
			// for harris
			const float sigma = 4.0f;
			// for DoG
			const float firstScale = 1.0f;    const float lastScale = 3.0f;

			//----------------------------------------------------------------------
			// Harris
			{
				Charris harris;
				std::multiset<Cpoint> result;
				harris.run(m_ppss->m_photos[index].getImage(m_level),
					m_ppss->m_photos[index].Cimage::getMask(m_level),
					m_ppss->m_photos[index].Cimage::getEdge(m_level),
					m_ppss->m_photos[index].getWidth(m_level),
					m_ppss->m_photos[index].getHeight(m_level), m_csize, sigma, result);

				std::multiset<Cpoint>::reverse_iterator rbegin = result.rbegin();
				while (rbegin != result.rend())
				{
					m_points[index].push_back(*rbegin);
					rbegin++;
				}
			}

			//----------------------------------------------------------------------
			// DoG
			{
				Cdog dog;
				std::multiset<Cpoint> result;
				dog.run(m_ppss->m_photos[index].getImage(m_level),
					m_ppss->m_photos[index].Cimage::getMask(m_level),
					m_ppss->m_photos[index].Cimage::getEdge(m_level),
					m_ppss->m_photos[index].getWidth(m_level),
					m_ppss->m_photos[index].getHeight(m_level),
					m_csize, firstScale, lastScale, result);

				std::multiset<Cpoint>::reverse_iterator rbegin = result.rbegin();
				while (rbegin != result.rend())
				{
					m_points[index].push_back(*rbegin);
					rbegin++;
				}
			}
		}
	}
}

