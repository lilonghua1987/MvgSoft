#include <iostream>
#include <fstream>
#include <cstdlib>
#define _USE_MATH_DEFINES
#include <math.h>
#include "option.h"
#include <algorithm>

using namespace PMVS3;

Soption::Soption(void)
{
	m_level = 1;          m_csize = 2;
	m_threshold = 0.7;    m_wsize = 7;
	m_minImageNum = 3;    m_CPU = 4;
	m_setEdge = 0.0f;     m_useBound = 0;
	m_useVisData = 0;     m_sequence = -1;
	m_tflag = -10;
	m_oflag = -10;

	// Max angle must be at least this big
	m_maxAngleThreshold = 10.0f * M_PI / 180.0f;
	// The smaller the tighter
	m_quadThreshold = 2.5f;

	std::string logPath = "log/log_sfm_PMVS.log";
	std::string logDir = stlplus::folder_part(logPath);

	if (!stlplus::folder_exists(logDir))
	{
		stlplus::folder_create(logDir);
	}

	logF.open(logPath.c_str());

	stdFile = std::cout.rdbuf(logF.rdbuf());
	std::cerr.rdbuf(logF.rdbuf());
}

Soption::~Soption()
{
	std::cout.rdbuf(stdFile);

	//delete stdFile;
	if (logF.is_open()) logF.close();
}

bool Soption::init(const std::string prefix, const std::string option)
{
	m_prefix = prefix;
	std::ifstream ifstr(option);

	while (1)
	{
		std::string name;
		ifstr >> name;
		if (ifstr.eof())
			break;
		if (name[0] == '#')
		{
			char buffer[1024];
			ifstr.putback('#');
			ifstr.getline(buffer, 1024);
			continue;
		}
		if (name == "level")             ifstr >> m_level;
		else if (name == "csize")        ifstr >> m_csize;
		else if (name == "threshold")    ifstr >> m_threshold;
		else if (name == "wsize")        ifstr >> m_wsize;
		else if (name == "minImageNum")  ifstr >> m_minImageNum;
		else if (name == "CPU")          ifstr >> m_CPU;
		else if (name == "setEdge")      ifstr >> m_setEdge;
		else if (name == "useBound")     ifstr >> m_useBound;
		else if (name == "useVisData")   ifstr >> m_useVisData;
		else if (name == "sequence")     ifstr >> m_sequence;
		else if (name == "timages")
		{
			ifstr >> m_tflag;

			if (m_tflag == -1)
			{
				int firstimage, lastimage;
				ifstr >> firstimage >> lastimage;
				for (int i = firstimage; i < lastimage; ++i)
					m_timages.push_back(i);
			}
			else if (0 < m_tflag)
			{
				for (int i = 0; i < m_tflag; ++i)
				{
					int itmp;          ifstr >> itmp;
					m_timages.push_back(itmp);
				}
			}
			else
			{
				std::cerr << "tflag is not valid: " << m_tflag << std::flush;
				return false;
			}
		}
		else if (name == "oimages")
		{
			ifstr >> m_oflag;
			if (m_oflag == -1)
			{
				int firstimage, lastimage;
				ifstr >> firstimage >> lastimage;
				for (int i = firstimage; i < lastimage; ++i)
					m_oimages.push_back(i);
			}
			else if (0 <= m_oflag)
			{
				for (int i = 0; i < m_oflag; ++i)
				{
					int itmp;          ifstr >> itmp;
					m_oimages.push_back(itmp);
				}
			}
			else if (m_oflag != -2 && m_oflag != -3)
			{
				std::cerr << "oflag is not valid: " << m_oflag << std::flush;
				return false;
			}
		}
		else if (name == "quad")      ifstr >> m_quadThreshold;
		else if (name == "maxAngle")
		{
			ifstr >> m_maxAngleThreshold;
			m_maxAngleThreshold *= M_PI / 180.0f;
		}
		else
		{
			std::cerr << "Unrecognizable option: " << name << std::flush;
			return false;
		}
	}
	ifstr.close();

	if (m_tflag == -10 || m_oflag == -10) 
	{
		std::cerr << "m_tflag and m_oflag not specified: "
			<< m_tflag << ' ' << m_oflag << std::flush;
		return false;
	}

	//----------------------------------------------------------------------
	std::string sbimages = prefix + std::string("bimages.dat");

	for (int i = 0; i < (int)m_timages.size(); ++i)
		m_dict[m_timages[i]] = i;

	if (m_oflag == -2) if (!initOimages()) return false;
	if (!initVisdata()) return false;

	if (m_useBound && !sbimages.empty())
		if (!initBindexes(sbimages)) return false;

	std::cerr << "--------------------------------------------------" << std::endl
		<< "--- Summary of specified options ---" << std::endl;
	std::cerr << "# of timages: " << (int)m_timages.size();
	if (m_tflag == -1)
		std::cerr << " (range specification)" << std::flush;
	else
		std::cerr << " (enumeration)" << std::flush;
	std::cerr << "# of oimages: " << (int)m_oimages.size();
	if (m_oflag == -1)
		std::cerr << " (range specification)" << std::flush;
	else if (0 <= m_oflag)
		std::cerr << " (enumeration)" << std::flush;
	else if (m_oflag == -2)
		std::cerr << " (vis.dat is used)" << std::flush;
	else if (m_oflag == -3)
		std::cerr << " (not used)" << std::flush;

	std::cerr << "level: " << m_level << "  csize: " << m_csize << std::endl
		<< "threshold: " << m_threshold << "  wsize: " << m_wsize << std::endl
		<< "minImageNum: " << m_minImageNum << "  CPU: " << m_CPU << std::endl
		<< "useVisData: " << m_useVisData << "  sequence: " << m_sequence << std::endl << std::flush;
	std::cerr << "--------------------------------------------------" << std::endl << std::flush;

	return true;
}

bool Soption::initOimages(void)
{
	/*if (m_oflag != -2)
		return false;*/

	std::string svisdata = m_prefix + std::string("vis.dat");
	std::ifstream ifstr;
	ifstr.open(svisdata.c_str());
	if (!ifstr.is_open())
	{
		std::cerr << "No vis.dat although specified to initOimages: " << std::endl
			<< svisdata << std::flush;
		return false;
	}

	std::string header;  int num2;
	ifstr >> header >> num2;

	m_oimages.clear();
	for (int c = 0; c < num2; ++c)
	{
		int index0;
		std::map<int, int>::iterator ite0 = m_dict.find(c);
		if (ite0 == m_dict.end())
			index0 = -1;
		else
			index0 = ite0->second;
		int itmp;
		ifstr >> itmp >> itmp;
		for (int i = 0; i < itmp; ++i)
		{
			int itmp2;
			ifstr >> itmp2;
			if (index0 != -1 && m_dict.find(itmp2) == m_dict.end())
				m_oimages.push_back(itmp2);
		}
	}
	ifstr.close();

	sort(m_oimages.begin(), m_oimages.end());
	m_oimages.erase(unique(m_oimages.begin(), m_oimages.end()), m_oimages.end());
	return true;
}

// When do not use vis.dat
bool Soption::initVisdata(void)
{
	// Case classifications. Set m_visdata by using vis.dat or not.
	if (m_useVisData == 0)
	{
		const int tnum = (int)m_timages.size();
		const int onum = (int)m_oimages.size();
		const int num = tnum + onum;
		m_visdata.resize(num);
		m_visdata2.resize(num);
		for (int y = 0; y < num; ++y)
		{
			m_visdata[y].resize(num);
			for (int x = 0; x < num; ++x)
				if (x == y)
					m_visdata[y][x] = 0;
				else
				{
					m_visdata[y][x] = 1;
					m_visdata2[y].push_back(x);
				}
		}
		return true;
	}
	else
		return initVisdata2();
}

// Given m_timages and m_oimages, set m_visdata, m_visdata2
bool Soption::initVisdata2(void)
{
	std::string svisdata = m_prefix + std::string("vis.dat");

	std::vector<int> images;
	images.insert(images.end(), m_timages.begin(), m_timages.end());
	images.insert(images.end(), m_oimages.begin(), m_oimages.end());
	std::map<int, int> dict2;
	for (int i = 0; i < (int)images.size(); ++i)
		dict2[images[i]] = i;

	std::ifstream ifstr;
	ifstr.open(svisdata.c_str());

	if (!ifstr.is_open())
	{
		std::cerr << "No vis.dat although specified to initVisdata2: " << std::endl
			<< svisdata << std::flush;
		return false;
	}

	std::string header;  int num2;
	ifstr >> header >> num2;

	m_visdata2.resize((int)images.size());
	for (int c = 0; c < num2; ++c)
	{
		int index0;
		std::map<int, int>::iterator ite0 = dict2.find(c);
		if (ite0 == dict2.end())
			index0 = -1;
		else
			index0 = ite0->second;
		int itmp;
		ifstr >> itmp >> itmp;
		for (int i = 0; i < itmp; ++i)
		{
			int itmp2;
			ifstr >> itmp2;
			int index1;
			std::map<int, int>::iterator ite1 = dict2.find(itmp2);
			if (ite1 == dict2.end())
				index1 = -1;
			else
				index1 = ite1->second;

			if (index0 != -1 && index1 != -1)
				m_visdata2[index0].push_back(index1);
		}
	}
	ifstr.close();

	const int num = (int)images.size();
	m_visdata.clear();
	m_visdata.resize(num);
	for (int y = 0; y < num; ++y)
	{
		m_visdata[y].resize(num);
		fill(m_visdata[y].begin(), m_visdata[y].end(), 0);
		for (int x = 0; x < (int)m_visdata2[y].size(); ++x)
			m_visdata[y][m_visdata2[y][x]] = 1;
	}

	// check symmetry
	for (int i = 0; i < (int)m_visdata.size(); ++i)
	{
		for (int j = i + 1; j < (int)m_visdata.size(); ++j)
		{
			if (m_visdata[i][j] != m_visdata[j][i])
			{
				m_visdata[i][j] = m_visdata[j][i] = 1;
			}
		}
	}
	return true;
}

bool Soption::initBindexes(const std::string sbimages)
{
	/*if (sbimages.empty())
		return false;*/

	m_bindexes.clear();
	std::ifstream ifstr;
	ifstr.open(sbimages.c_str());
	if (!ifstr.is_open())
	{
		std::cerr << "File not found: " << sbimages << std::flush;
		return false;
	}

	int itmp;
	ifstr >> itmp;
	for (int i = 0; i < itmp; ++i)
	{
		int itmp0;
		ifstr >> itmp0;

		if (m_dict.find(itmp0) != m_dict.end())
			m_bindexes.push_back(m_dict[itmp0]);
	}
	ifstr.close();
	return true;
}
