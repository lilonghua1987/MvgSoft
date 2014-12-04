#include <fstream>
#include <iterator>
#include <numeric> //PM

#ifdef _OPENMP
#include <omp.h>
#endif

#include "graclus.h"
#include "bundle.h"
#define _USE_MATH_DEFINES
#include <math.h>

extern "C"
{
int boundary_points;
int spectral_initialization = 0;
int cutType;
int memory_saving;
};


using namespace boost;
using namespace CMVS;

Cbundle::Cbundle(void)
{
  m_CPU = 8;
  m_junit = 100;
  m_debug = 0;
  m_puf = NULL;
  m_ptree = NULL;

  std::string logPath = "log/log_sfm_CMVS.log";
  std::string logDir = stlplus::folder_part(logPath);

  if (!stlplus::folder_exists(logDir))
  {
	  stlplus::folder_create(logDir);
  }

  logF.open(logPath.c_str());

  stdFile = std::cout.rdbuf(logF.rdbuf());
  std::cerr.rdbuf(logF.rdbuf());
}

Cbundle::~Cbundle() 
{
	std::cout.rdbuf(stdFile);

	//delete stdFile;
	if (logF.is_open()) logF.close();
}

bool Cbundle::prep(const std::string prefix, const int imageThreshold,
                   const int tau, const float scoreRatioThreshold,
                   const float coverageThreshold,
                   const int pnumThreshold, const int CPU) {
  if (pnumThreshold != 0) {
	  std::cerr << "Should use pnumThreshold = 0" << endl;
      return false;
  }
  
  m_prefix = stlplus::folder_append_separator(prefix);
  m_imageThreshold = imageThreshold;
  m_tau = tau;
  m_scoreRatioThreshold = scoreRatioThreshold;
  m_coverageThreshold = coverageThreshold;
  m_pnumThreshold = pnumThreshold;
  m_CPU = CPU;
  
  m_linkThreshold = 2.0f;

  m_dscale = 1 / 100.0f;
  m_dscale2 = 1.0f;
  
  std::string bPath = m_prefix + "bundle.rd.out";
  std::cout << bPath << " : " << "Reading bundle..." << std::flush;
  if (!readBundle(bPath))
	  return false;
  std::cerr << std::endl;

  vector<int> images;
  for (int c = 0; c < m_cnum; ++c)
    images.push_back(c);

  m_dlevel = 7;
  m_maxLevel = 12;
  m_pss.init(images, m_prefix, m_maxLevel + 1, 5, 0);
  
  std::cerr << "Set widths/heights..." << std::flush;
  setWidthsHeightsLevels();
  std::cerr << "done" << std::flush;

  return true;
}

bool Cbundle::prep2(void) {
  // Used in mergeSfMP now.
  {
    m_pweights.resize((int)m_coords.size(), 1);
    m_sfms2.resize((int)m_coords.size());
    startTimer();
    setScoreThresholds();
	std::cerr << "done\t" << curTimer() / CLOCKS_PER_SEC << " secs" << std::endl;
    startTimer();
	std::cerr << "slimNeighborsSetLinks..." << std::flush;
    slimNeighborsSetLinks();
	std::cerr << "done\t" << curTimer() / CLOCKS_PER_SEC << " secs" << std::endl;
  }
  
  // Improve visibility by using texture analysis
  startTimer();  
  std::cerr << "mergeSFM..." << std::flush;
  mergeSfMP();
  std::cerr << '\t' << curTimer() / CLOCKS_PER_SEC << " secs" << std::endl;

  m_sfms2.clear();
  m_sfms2.resize((int)m_coords.size());
  std::cerr << "setScoreThresholds..." << std::flush;
  startTimer();
  setScoreThresholds();
  std::cerr << "done\t" << curTimer() / CLOCKS_PER_SEC << " secs" << std::endl;

  // Remove redundant images first
  std::cerr << "sRemoveImages... " << std::flush;
  startTimer();

  sRemoveImages();
  std::cerr << '\t' << curTimer() / CLOCKS_PER_SEC << " secs" << std::endl;

  // use m_removed to change m_visibles and update m_neighbors
  startTimer();
  resetVisibles();
  setNeighbors();
  std::cerr << "slimNeighborsSetLinks..." << std::flush;
  slimNeighborsSetLinks();
  std::cerr << "done\t" << curTimer() / CLOCKS_PER_SEC << " secs" << std::endl;
  
  // Init m_timages by mutually exclusive clustering
  setTimages();
  m_oimages.resize((int)m_timages.size());  

  return true;
}

bool Cbundle::run(const std::string prefix, const int imageThreshold,
                  const int tau, const float scoreRatioThreshold,
                  const float coverageThreshold,
                  const int pnumThreshold, const int CPU) 
{
  startTimer();
  
  if (!prep(prefix, imageThreshold, tau, scoreRatioThreshold, coverageThreshold, pnumThreshold, CPU))
	  return false;

  std::cerr << '\t' << curTimer() / CLOCKS_PER_SEC << " secs" << std::endl;

  if (!prep2()) return false;
  
  // Assumed variables that must be set properly here
  std::cerr << "Adding images: " << std::endl;
  startTimer();
  // Add images
  // Repeat until all the clusters become at most m_imageThreshold.
  while (1) {
    addImagesP();
    
    int change = 0;
    vector<vector<int> > newtimages;
    cout << "Divide: " << flush;
    for (int i = 0; i < (int)m_timages.size(); ++i) {
      if ((int)m_timages[i].size() <= m_imageThreshold) {        
        newtimages.push_back(m_timages[i]);
        continue;
      }
      else {
        cout << i << ' ';
        
        change = 1;
        // divide
        vector<vector<int> > vvi;
        divideImages(m_timages[i], vvi);
        for (int j = 0; j < (int)vvi.size(); ++j)
          newtimages.push_back(vvi[j]);
      }
    }

    cout << endl;
    
    m_timages.swap(newtimages);
    if (change == 0)
      break;
  }
  std::cerr << "done\t" << curTimer() / CLOCKS_PER_SEC << " secs" << std::endl;
  
  m_oimages.resize((int)m_timages.size());

  writeCameraCenters();
  
  // Output results
  writeVis();
  //writeGroups();
  genOption();

  return true;
}

float Cbundle::computeLink(const int image0, const int image1) {
  vector<int> common;
  set_intersection(m_vpoints[image0].begin(), m_vpoints[image0].end(),
                   m_vpoints[image1].begin(), m_vpoints[image1].end(),
                   back_inserter(common));

  float score = 0.0f;
  for (int i = 0; i < (int)common.size(); ++i) {
    const int pid = common[i];
    vector<int> vtmp;
    vtmp.push_back(image0);    vtmp.push_back(image1);
    const float ftmp = computeScore2(m_coords[pid], vtmp);
    if (m_sfms2[pid].m_score != 0.0f)
      score += m_pweights[pid] *
        ftmp / (m_sfms2[pid].m_scoreThreshold / m_scoreRatioThreshold);
  }

  return score;
}

void Cbundle::slimNeighborsSetLinks(void) 
{
  const int maxneighbor = 30;
  m_links.clear();
  m_links.resize(m_cnum);

  jobs = 0;

  m_CPU = (std::min)((int)std::thread::hardware_concurrency(), m_cnum);

  if (m_CPU >= 1)
  {
	  std::cout << "slimNeighborsSetLinks -> CPU Cores :" << m_CPU << std::endl;
	  std::vector<std::thread> threads(m_CPU);

	  for (auto& t : threads) t = std::thread([&](){ 
		  while (true)
		  {
			  int && index = jobs++;

			  if (index >= m_cnum) break;

			  m_links[index].resize((int)m_neighbors[index].size());

			  for (int i = 0; i < (int)m_neighbors[index].size(); ++i)
				  m_links[index][i] = computeLink(index, m_neighbors[index][i]);

			  if ((int)m_neighbors[index].size() < 2)
				  continue;

			  vector<int> newneighbors;
			  vector<float> newlinks;

			  vector<pmvs::Vec2f> vv;
			  for (int i = 0; i < (int)m_neighbors[index].size(); ++i)
				  vv.push_back(pmvs::Vec2(-m_links[index][i], m_neighbors[index][i]));
			  sort(vv.begin(), vv.end(), pmvs::Svec2cmp<float>());

			  const int itmp = min(maxneighbor, (int)vv.size());
			  for (int i = 0; i < itmp; ++i)
			  {
				  newneighbors.push_back((int)vv[i][1]);
				  newlinks.push_back(-vv[i][0]);
			  }

			  m_neighbors[index].swap(newneighbors);
			  m_links[index].swap(newlinks);
		  }
	  });

	  for (auto& t : threads) t.join();
  }
}

void Cbundle::setScoreThresholds(void) 
{
	jobs = 0;

	m_CPU = (std::min)((int)std::thread::hardware_concurrency(), (int)m_coords.size());

	if (m_CPU >= 1)
	{
		std::cout << "setScoreThresholds -> CPU Cores :" << m_CPU << std::endl;
		std::vector<std::thread> threads(m_CPU);

		for (auto& t : threads) t = std::thread([&](){
			while (true)
			{
				int && p = jobs++;

				if (p >= (int)m_coords.size()) break;

				m_sfms2[p].m_scoreThreshold =
					computeScore2(m_coords[p], m_visibles[p], m_sfms2[p].m_uimages)
					* m_scoreRatioThreshold;
			}
		});

		for (auto& t : threads) t.join();
	}
}

void Cbundle::sRemoveImages(void) {
  m_removed.clear();
  m_removed.resize(m_cnum, 0);
    
  m_allows.resize(m_cnum);
  for (int c = 0; c < m_cnum; ++c)
    m_allows[c] = (int)ceil((int)m_vpoints[c].size() *
                            (1.0f - m_coverageThreshold));
  
  // Sort all the images in an increasing order of resolution
  vector<pmvs::Vec2i> vvi;
  for (int c = 0; c < m_cnum; ++c) {
    const int res =
      m_pss.m_photos[c].getWidth(0) * m_pss.m_photos[c].getHeight(0);
    vvi.push_back(pmvs::Vec2i(res, c));
  }
  sort(vvi.begin(), vvi.end(), pmvs::Svec2cmp<int>());
  
  const int tenth = max(1, m_cnum / 10);
  for (int i = 0; i < (int)vvi.size(); ++i) {
    if (i % tenth == 0)
		std::cerr << '*' << std::flush;
    const int image = vvi[i][1];
    checkImage(image);
  }
  std::cerr << std::endl;
    
  std::cerr << "Kept: ";
  int kept = 0;
  for (int c = 0; c < m_cnum; ++c)
    if (m_removed[c] == 0) {
      ++kept;
	  std::cerr << c << ' ';
    }
  std::cerr << std::endl << std::endl;
  
  std::cerr << "Removed: ";
  for (int c = 0; c < m_cnum; ++c)
    if (m_removed[c]) {
		std::cerr << c << ' ';
    }
  std::cerr << endl;
  std::cerr << "sRemoveImages: " << m_cnum << " -> " << kept << flush;
}

void Cbundle::resetVisibles(void) {
  // reset m_visibles. remove "removed images" from the list.
  for (int p = 0; p < (int)m_visibles.size(); ++p) {
    vector<int> newimages;

    setNewImages(p, -1, newimages);
    m_visibles[p].swap(newimages);
  }
}

void Cbundle::setNewImages(const int pid, const int rimage,
                           std::vector<int>& newimages) 
{
  for (int i = 0; i < (int)m_visibles[pid].size(); ++i) 
  {
    const int image = m_visibles[pid][i];
    if (m_removed[image] || image == rimage)
      continue;
    newimages.push_back(image);
  }
}

void Cbundle::checkImage(const int image) 
{
  // For each SfM, check if it will be unsatisfied by removing image.
  //  0: unsatisfy
  //  1: satisfy->satisfy
  //  2: satisfy->unsatisfy
  m_statsT.resize((int)m_vpoints[image].size());

  jobs = 0;

  m_CPU = (std::min)((int)std::thread::hardware_concurrency(), (int)m_statsT.size());

  if (m_CPU >= 1)
  {
	  std::cout << "checkImage 1 -> CPU Cores :" << m_CPU << std::endl;
	  std::vector<std::thread> threads(m_CPU);

	  for (auto& t : threads) t = std::thread([&](){
		  while (true)
		  {
			  int && p = jobs++;

			  if (p >= (int)m_statsT.size()) break;

			  const int pid = m_vpoints[image][p];
			  if (m_sfms2[pid].m_satisfied == 0)
				  m_statsT[p] = 0;
			  else {
				  m_statsT[p] = 1;

				  // if m_uimages of p is not removed, if image is not in
				  // m_uimages, the point is still satisfied.
				  int valid = 1;
				  int inside = 0;

				  for (int i = 0; i < (int)m_sfms2[pid].m_uimages.size(); ++i) 
				  {
					  const int itmp = m_sfms2[pid].m_uimages[i];
					  if (itmp == image)
						  inside = 1;
					  if (m_removed[itmp]) 
					  {
						  valid = 0;
						  break;
					  }
				  }

				  if (valid == 1 && inside == 0)
					  continue;

				  vector<int> newimages;
				  setNewImages(pid, image, newimages);
				  const float cscore = computeScore2(m_coords[pid], newimages);
				  if (cscore < m_sfms2[pid].m_scoreThreshold)
					  m_statsT[p] = 2;
			  }
		  }
	  });

	  for (auto& t : threads) t.join();
  }
  
  // For each image, how many SFM points are removed if you remove
  // "image"
  vector<int> decrements;
  decrements.resize(m_cnum, 0);
  
  // Look at points with m_stastT[p] = 2, to see if m_allows are still
  // above thresholds.
  for (int p = 0; p < (int)m_statsT.size(); ++p)
  {
    if (m_statsT[p] == 2) 
	{
      const int pid = m_vpoints[image][p];
      for (int i = 0; i < (int)m_visibles[pid].size(); ++i) 
	  {
        const int itmp = m_visibles[pid][i];
        ++decrements[itmp];
      }
    }
  }

  // If m_allows can cover decrements, go ahead.
  int rflag = 1;
  for (int c = 0; c < m_cnum; ++c)
    if (m_allows[c] < decrements[c]) 
	{
      rflag = 0;
      break;
    }

  // remove
  if (rflag) 
  {
    m_removed[image] = 1;
    for (int p = 0; p < (int)m_statsT.size(); ++p)
      if (m_statsT[p] == 2)
        m_sfms2[m_vpoints[image][p]].m_satisfied = 0;

    for (int c = 0; c < m_cnum; ++c)
      m_allows[c] -= decrements[c];

    // Update uimages for points that are still satisfied and still
    // contains image in m_uimages

	jobs = 0;

	m_CPU = (std::min)((int)std::thread::hardware_concurrency(), (int)m_statsT.size());

	if (m_CPU >= 1)
	{
		std::cout << "setScoreThresholds -> CPU Cores :" << m_CPU << std::endl;
		std::vector<std::thread> threads(m_CPU);

		for (auto& t : threads) t = std::thread([&](){
			while (true)
			{
				int && p = jobs++;

				if (p >= (int)m_statsT.size()) break;

				const int pid = m_vpoints[image][p];
				if (m_statsT[p] == 1) 
				{
					int contain = 0;
					for (int i = 0; i < (int)m_sfms2[pid].m_uimages.size(); ++i)
					if (m_sfms2[pid].m_uimages[i] == image)
					{
						contain = 1;
						break;
					}

					if (contain)
					{
						vector<int> newimages;
						setNewImages(pid, -1, newimages);
						const float cscore =
							computeScore2(m_coords[pid], newimages, m_sfms2[pid].m_uimages);
						if (cscore < m_sfms2[pid].m_scoreThreshold)
							m_sfms2[pid].m_satisfied = 0;
					}
				}
			}
		});

		for (auto& t : threads) t.join();
	}
  }
}

void Cbundle::setNeighbors(void) 
{
  m_neighbors.clear();
  m_neighbors.resize(m_cnum);

  jobs = 0;

  m_CPU = (std::min)((int)std::thread::hardware_concurrency(), m_cnum);

  if (m_CPU >= 1)
  {
	  std::cout << "setNeighbors -> CPU Cores :" << m_CPU << std::endl;
	  std::vector<std::thread> threads(m_CPU);

	  for (auto& t : threads) t = std::thread([&](){
		  while (true)
		  {
			  int && index = jobs++;

			  if (index >= m_cnum) break;

			  vector<int> narray;
			  narray.resize(m_cnum, 0);

			  for (int p = 0; p < (int)m_visibles.size(); ++p) 
			  {
				  if (!binary_search(m_visibles[p].begin(), m_visibles[p].end(),index))
					  continue;

				  for (int i = 0; i < (int)m_visibles[p].size(); ++i)
					  narray[m_visibles[p][i]] = 1;
			  }

			  for (int i = 0; i < m_cnum; ++i)
			  if (narray[i] && i != index)
				  m_neighbors[index].push_back(i);
		  }
	  });

	  for (auto& t : threads) t.join();
  }
}

void Cbundle::setTimages(void) {
  vector<int> lhs;
  for (int c = 0; c < m_cnum; ++c)
    if (m_removed[c] == 0)
      lhs.push_back(c);

  m_timages.clear();

  if ((int)lhs.size() <= m_imageThreshold)
    m_timages.push_back(lhs);
  else
    divideImages(lhs, m_timages);
  
  std::cerr << endl << "Cluster sizes: " << std::endl;
  for (int i = 0; i < (int)m_timages.size(); ++i)
	  std::cerr << (int)m_timages[i].size() << ' ';
  std::cerr << endl;
}

void Cbundle::divideImages(const std::vector<int>& lhs,
                           std::vector<std::vector<int> >& rhs) {
  const float iratio = 125.0f / 150.0f;
  // Candidates
  list<vector<int> > candidates;
  // initialize first cluster
  candidates.push_back(vector<int>());
  vector<int>& vitmp = candidates.back();
  for (int i = 0; i < (int)lhs.size(); ++i)
    vitmp.push_back(lhs[i]);
  
  // Iterate
  while (1) {
    if (candidates.empty())
      break;
    vector<int> cand = candidates.front();
    candidates.pop_front();
    if ((int)cand.size() <= m_imageThreshold * iratio) {
      rhs.push_back(cand);
      continue;
    }
    // Divide into 2 groups
    vector<idxtype> xadj, adjncy, adjwgt, part;
    const int nparts = 2;
    const int cutType = 0;
    // Define graphs
    for (int i = 0; i < (int)cand.size(); ++i) {
      xadj.push_back((int)adjncy.size());

      for (int j = 0; j < (int)cand.size(); ++j) {
        if (i == j)
          continue;
        // Check if cand[i] and cand[j] are connected
        const int cid0 = cand[i];
        const int cid1 = cand[j];
        vector<int>::const_iterator nite = 
          find(m_neighbors[cid0].begin(), m_neighbors[cid0].end(), cid1);
        
        if (nite != m_neighbors[cid0].end()) {
          adjncy.push_back(j);
          const int offset = nite - m_neighbors[cid0].begin();
          const int weight =
            min(5000, (int)floor(10.0f * m_links[cid0][offset] + 0.5f));
          adjwgt.push_back(weight);
        }
      }
    }
    xadj.push_back((int)adjncy.size());

    Cgraclus::runE(xadj, adjncy, adjwgt, nparts, cutType, part);
    
    // Divide into 2 groups
    vector<int> cand1, cand2;
    for (int i = 0; i < (int)part.size(); ++i) {
      if (part[i] == 0)
        cand1.push_back(cand[i]);
      else
        cand2.push_back(cand[i]);
    }
    
    if (cand1.empty() || cand2.empty()) {
		std::cerr << "Error. Normalized cuts produced an empty cluster: "
           << (int)part.size() << " -> "
           << (int)cand1.size() << ' '
		   << (int)cand2.size() << std::endl;
      exit (1);
    }
    
    if ((int)cand1.size() <= m_imageThreshold * iratio)
      rhs.push_back(cand1);
    else {
      candidates.push_back(vector<int>());
      (candidates.back()).swap(cand1);
    }
    if ((int)cand2.size() <= m_imageThreshold * iratio)
      rhs.push_back(cand2);
    else {
      candidates.push_back(vector<int>());
      (candidates.back()).swap(cand2);
    }
  }
}

bool Cbundle::readBundle(const std::string file)
{
  // For each valid image, a list of connected images, and the second
  // value is the number of common points.
  ifstream ifstr;  ifstr.open(file.c_str());
  if (!ifstr.is_open())
  {
	  std::cerr << "Bundle file not found: " << file << std::endl;
    return false;
  }
  while (1) 
  {
    unsigned char uctmp;
    ifstr.read((char*)&uctmp, sizeof(unsigned char));
    ifstr.putback(uctmp);
    if (uctmp == '#')
	{
      char buffer[1024];      ifstr.getline(buffer, 1024);
    }
    else
      break;
  }
  int cnum, pnum;
  ifstr >> cnum >> pnum;
  std::vector<int> ids;        ids.resize(cnum);
  std::cerr << cnum << " cameras -- " << pnum << " points in bundle file" << std::endl;
  m_cnum = 0;
  for (int c = 0; c < cnum; ++c) {
    ids[c] = -1;
    float params[15];
    for (int i = 0; i < 15; ++i)
      ifstr >> params[i];
    if (params[0] != 0.0f)
      ids[c] = m_cnum++;
  }

  m_vpoints.resize(m_cnum);

  m_coords.clear();  m_visibles.clear();
  m_colors.clear();
  m_pnum = 0;
  const int tenth = max(1, pnum / 10);
  for (int p = 0; p < pnum; ++p) {
    if (p % tenth == 0)
		std::cerr << '*' << std::flush;
    int num;    pmvs::Vec3f color;
    pmvs::Vec4f coord;
    ifstr >> coord[0] >> coord[1] >> coord[2]
          >> color >> num;
    coord[3] = 1.0f;
    
    vector<int> visibles;
    for (int i = 0; i < num; ++i) {
      int itmp;      ifstr >> itmp;
      if (cnum <= itmp) {
        double dtmp;
        ifstr >> itmp >> dtmp >> dtmp;
        continue;
      }
      if (ids[itmp] == -1) {
		  std::cerr << "impossible " << itmp << ' ' << ids[itmp] << std::endl;
        exit (1);
      }
      visibles.push_back(ids[itmp]);

      // Based on the bundler version, the number of parameters here
      // are either 1 or 3. Currently, it seems to be 3.
      double dtmp;
      ifstr >> itmp >> dtmp >> dtmp;
      //ifstr >> dtmp;
    }
    if ((int)visibles.size() < 2)
      continue;

    sort(visibles.begin(), visibles.end());
    
    for (int i = 0; i < (int)visibles.size(); ++i) {
       m_vpoints[visibles[i]].push_back(m_pnum);
    }
    
    m_visibles.push_back(visibles);
    m_coords.push_back(coord);
    m_colors.push_back(color);
    ++m_pnum;
  }
  ifstr.close();
  setNeighbors();
  
  std::cerr << endl << m_cnum << " cameras -- " << m_pnum << " points" << std::flush;
  return true;
}

void Cbundle::findPNeighbors(sfcnn<const float*, 3, float>& tree,
                             const int pid, std::vector<int>& pneighbors) 
{
  const float thresh = m_dscale2 * m_dscale2 *
    m_minScales[pid] * m_minScales[pid];

  vector<long unsigned int> ids;
  vector<double> dists;
  int k = min((int)m_coords.size(), 400);

  while (1) {
    ids.clear();
    dists.clear();
    tree.ksearch(&m_coords[pid][0], k, ids, dists);

    if (thresh < dists[k - 1])
      break;
    if (k == (int)m_coords.size())
      break;
    
    k = min((int)m_coords.size(), k * 2);
  }

  for (int i = 0; i < k; ++i)
  {
    if (thresh < dists[i])
      break;

    // If distance from pid2 is also within threshold ok
    const float thresh2 = m_dscale2 * m_dscale2 *
      m_minScales[ids[i]] * m_minScales[ids[i]];
    if (thresh2 < dists[i])
      continue;
    
    if (ids[i] != (unsigned int)pid)
      pneighbors.push_back(ids[i]);
  }
}

void Cbundle::mergeSfMPThread(void) 
{
  const int tenth = (int)m_coords.size() / 10;
  while (true) 
  {
    int && pid = jobs++;

    if (pid != -1 && m_merged[pid])
      pid = -2;
    if (m_count % tenth == 0)
		std::cerr << '*' << std::flush;
    ++m_count;

    if (pid == -2)
      continue;
	if (pid >= tenth)
      break;

    // Process, and check later if merged flag is on
    vector<int> pneighbors;
    findPNeighbors(*m_ptree, pid, pneighbors);
    const int psize = (int)pneighbors.size();
    // visible images and its neighbors
    vector<int> visibles = m_visibles[pid];
    vector<int> vitmp;
    for (int i = 0; i < (int)m_visibles[pid].size(); ++i) 
	{
      const int imagetmp = m_visibles[pid][i];
      vitmp.clear();
      mymerge(visibles, m_neighbors[imagetmp], vitmp);
      vitmp.swap(visibles);
    }
    
    vector<char> visflag(psize, 0);
    // test visibility
    for (int i = 0; i < psize; ++i) 
	{
      const int& pid2 = pneighbors[i];
      if (my_isIntersect(visibles, m_visibles[pid2]))
        visflag[i] = 1;
    }
    
    // Now lock and try to register
    std::unique_lock<std::mutex> lck(mtx);
    // If the main one is removed, over... waste.
    if (m_merged[pid] == 0)
	{
      m_merged[pid] = 1;
      for (int i = 0; i < psize; ++i) 
	  {
        const int pid2 = pneighbors[i];
        if (visflag[i] && m_merged[pid2] == 0)
		{
          m_merged[pid2] = 1;
          m_puf->union_set(pid, pid2);
        }
      }
    }
  }
}

void* Cbundle::mergeSfMPThreadTmp(void* arg) {
  ((Cbundle*)arg)->mergeSfMPThread();
  return NULL;
}

void Cbundle::mergeSfMP(void) 
{
  // Repeat certain times until no change
  const int cpnum = (int)m_coords.size();
  m_minScales.resize(cpnum);
  for (int p = 0; p < cpnum; ++p) 
  {
    m_minScales[p] = INT_MAX/2;
    for (int i = 0; i < (int)m_visibles[p].size(); ++i)
	{
      const int image = m_visibles[p][i];
      const float stmp =
        m_pss.m_photos[image].getScale(m_coords[p], m_levels[image]);
      m_minScales[p] = min(m_minScales[p], stmp);
    }
  }
  
  m_puf = new disjoint_sets_with_storage<>(cpnum);
  for (int p = 0; p < cpnum; ++p)
    m_puf->make_set(p);

  {
    // kdtree
    vector<const float*> ppoints;
    ppoints.resize((int)m_coords.size());
    for (int i = 0; i < (int)m_coords.size(); ++i)
      ppoints[i] = &(m_coords[i][0]);
    m_ptree =
      new sfcnn<const float*, 3, float>(&ppoints[0], (int)ppoints.size());
    
    m_merged.resize((int)m_coords.size(), 0);
   /* m_jobs.clear();
    vector<int> order;
    order.resize(cpnum);
    for (int p = 0; p < cpnum; ++p)
      order[p] = p;
    random_shuffle(order.begin(), order.end());
    
    for (int p = 0; p < cpnum; ++p)
      m_jobs.push_back(order[p]);*/
    
    m_count = 0;
   /* vector<pthread_t> threads(m_CPU);
    for (int c = 0; c < m_CPU; ++c)
      pthread_create(&threads[c], NULL,
                     mergeSfMPThreadTmp, (void*)this);
    for (int c = 0; c < m_CPU; ++c)
      pthread_join(threads[c], NULL);*/
	jobs = 0;
	int cpuN = (std::min)((int)std::thread::hardware_concurrency(), cpnum);
	if (cpuN < 1) return;
	std::cout << "mergeSfMP -> CPU Cores :" << cpuN << std::endl;
	std::vector<std::thread> threads(cpuN);

	for (auto& t : threads) t = std::thread([](Cbundle& arg){arg.mergeSfMPThread(); }, std::ref(*this));

	for (auto& t : threads) t.join();

    int newpnum = 0;
    // Mapping from m_pnum to new id for reps
    vector<int> dict;
    vector<int> reps;
    dict.resize((int)m_coords.size(), -1);
    for (int p = 0; p < (int)m_coords.size(); ++p) 
	{
      const int ptmp = m_puf->find_set(p);
      if (p == ptmp) {
        dict[p] = newpnum;
        reps.push_back(p);
        ++newpnum;
      }
    }
  }
  
  // Based on m_puf, reset m_coords, m_coords, m_visibles, m_vpoints
  std::cerr << "resetPoints..." << std::flush;
  resetPoints();
  std::cerr << "done" << std::endl;

  delete m_puf;
  m_puf = NULL;
  delete m_ptree;
  m_ptree = NULL;
   
  const int npnum = (int)m_coords.size();
  std::cerr << "Rep counts: " << cpnum << " -> " << npnum << "  " << std::flush;
}

// Based on m_puf, reset m_coords, m_coords, m_visibles, m_vpoints
void Cbundle::resetPoints(void) {
  vector<int> counts;
  vector<int> smallestids;
  counts.resize((int)m_coords.size(), 0);
  smallestids.resize((int)m_coords.size(), INT_MAX/2);
  for (int p = 0; p < (int)m_coords.size(); ++p) {
    const int ptmp = m_puf->find_set(p);
    smallestids[ptmp] = min(smallestids[ptmp], p);
    ++counts[ptmp];
  }
  const int mthreshold = 2;
  vector<pmvs::Vec2i> vv2;
  for (int p = 0; p < (int)m_coords.size(); ++p)
    if (mthreshold <= counts[p])
      vv2.push_back(pmvs::Vec2i(smallestids[p], p));
  sort(vv2.begin(), vv2.end(), pmvs::Svec2cmp<int>());
  
  vector<int> newpids;
  newpids.resize((int)m_coords.size(), -1);
  int newpnum = (int)vv2.size();
  for (int i = 0; i < newpnum; ++i)
    newpids[vv2[i][1]] = i;
  
  vector<pmvs::Vec4f> newcoords;  newcoords.resize(newpnum);
  vector<vector<int> > newvisibles;  newvisibles.resize(newpnum);

  for (int p = 0; p < (int)m_coords.size(); ++p) {
    const int ptmp = m_puf->find_set(p);
    if (counts[ptmp] < mthreshold)
      continue;
    
    const int newpid = newpids[ptmp];
    newcoords[newpid] += m_coords[p];
    vector<int> vitmp;
    mymerge(newvisibles[newpid], m_visibles[p], vitmp);
    vitmp.swap(newvisibles[newpid]);
  }  
  
  for (int i = 0; i < newpnum; ++i)
    newcoords[i] = newcoords[i] / newcoords[i][3];
  
  // Update m_vpoints
  m_coords.swap(newcoords);
  m_visibles.swap(newvisibles);
  
  for (int c = 0; c < m_cnum; ++c)
    m_vpoints[c].clear();  
  
  for (int p = 0; p < (int)m_coords.size(); ++p) {
    for (int i = 0; i < (int)m_visibles[p].size(); ++i) {
      const int itmp = m_visibles[p][i];
      m_vpoints[itmp].push_back(p);
    }
  }

  m_pweights.clear();
  for (int i = 0; i < newpnum; ++i)
    m_pweights.push_back(counts[vv2[i][1]]);
}

void Cbundle::setScoresClusters(void) 
{

	jobs = 0;

	int cpuN = (std::min)((int)std::thread::hardware_concurrency(), (int)m_coords.size());
	if (cpuN < 1) return;
	std::cout << "setScoresClusters -> CPU Cores :" << cpuN << std::endl;
	std::vector<std::thread> threads(cpuN);

	for (auto& t : threads) t = std::thread([&](){
		while (true)
		{
			int && p = jobs++;

			if (p >= (int)m_coords.size()) break;
			// if m_satisfied is 0, no hope (even all the images are in the
			// cluster, not satisfied
			if (m_sfms2[p].m_satisfied == 0)
				continue;

			m_sfms2[p].m_satisfied = 2;
			setCluster(p);
		}
	});

	for (auto& t : threads) t.join();
}

void Cbundle::setCluster(const int p)
{
  m_sfms2[p].m_score = -1.0f;
  m_sfms2[p].m_cluster = -1;
  for (int c = 0; c < (int)m_timages.size(); ++c) 
  {
    // Select images in cluster
    vector<int> vitmp;
    set_intersection(m_timages[c].begin(), m_timages[c].end(),
                     m_visibles[p].begin(), m_visibles[p].end(),
                     back_inserter(vitmp));
    
    const float stmp = computeScore2(m_coords[p], vitmp);
    if (m_sfms2[p].m_score < stmp) 
	{
      m_sfms2[p].m_cluster = c;
      m_sfms2[p].m_score = stmp;
    }
  }

  // If no cluster contains 2 images
  if (m_sfms2[p].m_cluster == -1) {
    int find = 0;
    for (int j = 0; j < (int)m_visibles[p].size(); ++j) 
	{
      if (find)
        break;
      for (int c = 0; c < (int)m_timages.size(); ++c)
        if (binary_search(m_timages[c].begin(), m_timages[c].end(),
                          m_visibles[p][j])) 
		{
          m_sfms2[p].m_cluster = c;
          m_sfms2[p].m_score = 0.0f;
          find = 1;
          break;
        }
    }
    // If none of the visibles images are
    if (find == 0)
	{
		std::cerr << "Impossible in setscoresclustersthread" << endl
           << (int)m_visibles[p].size() << endl
           << (int)m_sfms2[p].m_satisfied << endl;
      //exit (1);
		return;
    }
  }

  if (m_sfms2[p].m_scoreThreshold <= m_sfms2[p].m_score) 
  {
    // SATISFIED
    m_sfms2[p].m_satisfied = 1;

    //  update m_lacks
    std::unique_lock<std::mutex> lck(mtx);    
    for (int i = 0; i < (int)m_visibles[p].size(); ++i) 
	{
      const int image = m_visibles[p][i];
      --m_lacks[image];
    }
  }
}

float Cbundle::angleScore(const pmvs::Vec4f& ray0, const pmvs::Vec4f& ray1)
{
  const static float lsigma = 5.0f * M_PI / 180.f;
  const static float rsigma = 15.0f * M_PI / 180.0f;
  const static float lsigma2 = 2.0f * lsigma * lsigma;
  const static float rsigma2 = 2.0f * rsigma * rsigma;
  const static float pivot = 20.0f * M_PI / 180.0f;

  const float angle = acos(min(1.0f, ray0 * ray1));
  const float diff = angle - pivot;

  if (angle < pivot)
    return exp(- diff * diff / lsigma2);
  else
    return exp(- diff * diff / rsigma2);
}

void Cbundle::setClusters(void) 
{
	jobs = 0;

	int cpuN = (std::min)((int)std::thread::hardware_concurrency(), (int)m_sfms2.size());
	if (cpuN < 1) return;
	std::cout << "setClusters -> CPU Cores :" << cpuN << std::endl;
	std::vector<std::thread> threads(cpuN);

	for (auto& t : threads) t = std::thread([&](){
		while (true)
		{
			int && p = jobs++;

			if (p >= (int)m_sfms2.size()) break;
			if (m_sfms2[p].m_satisfied != 2)
				continue;

			setCluster(p);
		}
	});

	for (auto& t : threads) t.join();
}

void Cbundle::addImagesP(void) 
{
  // set m_lacks (how many more sfm points need to be satisfied)
  m_lacks.resize(m_cnum);
  for (int c = 0; c < m_cnum; ++c)
  {
    if (m_removed[c])
      m_lacks[c] = 0;
    else
      m_lacks[c] =
        (int)floor((int)m_vpoints[c].size() * m_coverageThreshold);
  }

  // Compute best score given all the images. Upper bound on the score
  setScoresClusters();
  
  // Add images to clusters to make m_lacks at most 0 for all the
  // images. In practice, for each cluster, identify corresponding sfm
  // points that have not been satisfied. These points compute gains.
  
  // set m_adds for each sfm point, and add images

  m_addnums.clear();
  m_addnums.resize((int)m_timages.size(), 0);
  
  while (1) 
  {
    const int totalnum = addImages();
    // Very end
    if (totalnum == 0) 
	{
      break;
    }
    // If one of the cluster gets more than m_imageThreshold, divide
    int divide = 0;
    for (int i = 0; i < (int)m_timages.size(); ++i)
      if (m_imageThreshold < (int)m_timages[i].size()) 
	  {
        divide = 1;
        break;
      }
    if (divide) 
	{
      break;
    }
    
    setClusters();
  }

  for (int i = 0; i < (int)m_addnums.size(); ++i)
	  std::cerr << m_addnums[i] << ' ';
  std::cerr << endl;
  
  int totalnum = 0;
  for (int c = 0; c < (int)m_timages.size(); ++c)
    totalnum += (int)m_timages[c].size();
  int beforenum = 0;
  for (int c = 0; c < m_cnum; ++c)
    if (m_removed[c] == 0)
      ++beforenum;
  
  std::cerr << "Image nums: "
       << m_cnum << " -> " << beforenum <<  " -> " << totalnum << endl;
}

int Cbundle::addImages(void)
{
  /*m_thread = 0;
  m_jobs.clear();*/
  
  // we think about sfm points that belong to images that have not been satisfied
  vector<char> flags;
  flags.resize((int)m_coords.size(), 0);
  for (int c = 0; c < m_cnum; ++c)
  {
    if (m_lacks[c] <= 0)
      continue;
    else 
	{
      for (int i = 0; i < (int)m_vpoints[c].size(); ++i) 
	  {
        const int pid = m_vpoints[c][i];
        if (m_sfms2[pid].m_satisfied == 2)
          flags[pid] = 1;
      }
    }
  }


  jobs = 0;

  int cpuN = (std::min)((int)std::thread::hardware_concurrency(), (int)m_sfms2.size());
  if (cpuN < 1)
  {
	  std::cout << "addImages -> CPU Cores :" << cpuN << std::endl;
	  std::vector<std::thread> threads(cpuN);

	  for (auto& t : threads) t = std::thread([&]()
	  {
		  while (true)
		  {
			  int && p = jobs++;

			  if (p >= (int)m_sfms2.size()) break;
			  if (flags[p] == 0)
				  continue;
			  m_sfms2[p].m_adds.clear();

			  const int cluster = m_sfms2[p].m_cluster;
			  // Try to add an image to m_timages[cluster]
			  vector<int> cimages;
			  set_intersection(m_timages[cluster].begin(), m_timages[cluster].end(),
				  m_visibles[p].begin(), m_visibles[p].end(),
				  back_inserter(cimages));
			  sort(cimages.begin(), cimages.end());

			  for (int i = 0; i < (int)m_visibles[p].size(); ++i)
			  {
				  const int image = m_visibles[p][i];

				  if (binary_search(cimages.begin(), cimages.end(), image))
					  continue;

				  vector<int> vitmp = cimages;
				  vitmp.push_back(image);
				  const float newscore = computeScore2(m_coords[p], vitmp);
				  if (newscore <= m_sfms2[p].m_score)
					  continue;

				  m_sfms2[p].m_adds.push_back(Sadd(image,
					  (newscore - m_sfms2[p].m_score) /
					  m_sfms2[p].m_scoreThreshold));
			  }
		  }
	  });

	  for (auto& t : threads) t.join();
  }
  
  // Accumulate information from SfM points. For each cluster.
  // For each cluster, for each image, sum of gains.
  vector<map<int, float> > cands;
  cands.resize((int)m_timages.size());

  for (int p = 0; p < (int)m_sfms2.size(); ++p) 
  {
    if (flags[p] == 0)
      continue;
    
    const int cluster = m_sfms2[p].m_cluster;
    
    for (int i = 0; i < (int)m_sfms2[p].m_adds.size(); ++i) 
	{
      Sadd& stmp = m_sfms2[p].m_adds[i];
      cands[cluster][stmp.m_image] += stmp.m_gain;
    }
  }

  return addImagesSub(cands);
}

int Cbundle::addImagesSub(const std::vector<std::map<int, float> >& cands)
{
  // Vec3f (gain, cluster, image). Start adding starting from the good
  // one, block neighboring images.
	vector<pmvs::Vec3f> cands2;
  for (int i = 0; i < (int)m_timages.size(); ++i) 
  {
    map<int, float>::const_iterator mbegin = cands[i].begin();
    map<int, float>::const_iterator mend = cands[i].end();
    
    while (mbegin != mend) 
	{
		cands2.push_back(pmvs::Vec3f(-mbegin->second, i, mbegin->first));
      ++mbegin;
    }
  }

  if (cands2.empty())
    return 0;

  sort(cands2.begin(), cands2.end(), pmvs::Svec3cmp<float>());
  
  vector<char> blocked;
  blocked.resize(m_cnum, 0);
  vector<int> addnum;
  addnum.resize((int)m_timages.size(), 0);

  // A bit of tuning is possible here. For the paper, we used 0.7f,
  //but 0.9f seems to produce better results in general.  const float
  //gainThreshold = -cands2[0][0] * 0.7f;
  const float gainThreshold = -cands2[0][0] * 0.9f;

  // use rato threshold for blocked one. if not blocked, just keep on
  // going.
  for (int i = 0; i < (int)cands2.size(); ++i) 
  {
    const float gain = -cands2[i][0];
    if (gain < gainThreshold)
      break;
    
    const int cluster = (int)cands2[i][1];
    const int image = (int)cands2[i][2];
    
    if (blocked[image])
      continue;

    // Add
    ++addnum[cluster];
    ++m_addnums[cluster];
    blocked[image] = 1;
    for (int i = 0; i < (int)m_neighbors[image].size(); ++i)
      blocked[m_neighbors[image][i]] = 1;

    m_timages[cluster].push_back(image);
    // m_score, m_cluster, m_satisfied, m_lacks are updated in
    // setClusters.  So, no need to update anything.
  }
  
  for (int i = 0; i < (int)m_timages.size(); ++i)
    sort(m_timages[i].begin(), m_timages[i].end());
  
  return accumulate(addnum.begin(), addnum.end(), 0);
}

int Cbundle::totalNum(void) const{
  int totalnum = 0;
  for (int c = 0; c < (int)m_timages.size(); ++c)
    totalnum += (int)m_timages[c].size();
  return totalnum;
}

int Cbundle::my_isIntersect(const std::vector<int>& lhs,
                            const std::vector<int>& rhs) {
  vector<int>::const_iterator b0 = lhs.begin();
  vector<int>::const_iterator e0 = lhs.end();

  vector<int>::const_iterator b1 = rhs.begin();
  vector<int>::const_iterator e1 = rhs.end();

  while (1) {
    if (b0 == e0)
      return 0;  
    if (b1 == e1)
      return 0;

    if (*b0 == *b1) {
      return 1;
    }
    else if (*b0 < *b1)
      ++b0;
    else
      ++b1;
  }
}

void Cbundle::mymerge(const std::vector<int>& lhs,
                      const std::vector<int>& rhs,
                      std::vector<int>& output) {
  vector<int>::const_iterator b0 = lhs.begin();
  vector<int>::const_iterator e0 = lhs.end();

  vector<int>::const_iterator b1 = rhs.begin();
  vector<int>::const_iterator e1 = rhs.end();

  while (1) {
    if (b0 == e0) {
      output.insert(output.end(), b1, e1);
      break;
    }
    if (b1 == e1) {
      output.insert(output.end(), b0, e0);
      break;
    }

    if (*b0 == *b1) {
      output.push_back(*b0);
      ++b0;
      ++b1;
    }
    else if (*b0 < *b1) {
      output.push_back(*b0);
      ++b0;
    }
    else {
      output.push_back(*b1);
      ++b1;
    }
  }
}

// Calculates log2 of number.
template <typename T>
T Log2( T n )
{
    // log(n)/log(2) is log2.
    return log( n ) / log( T(2) );
}

void Cbundle::setWidthsHeightsLevels(void) {
  m_widths.resize(m_cnum);
  m_heights.resize(m_cnum);
  m_levels.resize(m_cnum);

  // Determine level. SfM was done on 2M pixels.
  for (int c = 0; c < m_cnum; ++c) {
    m_levels[c] = m_dlevel;

    m_widths[c] = m_pss.m_photos[c].getWidth(m_levels[c]);
    m_heights[c] = m_pss.m_photos[c].getHeight(m_levels[c]);
  }
}

 
float Cbundle::computeScore2(const pmvs::Vec4f& coord,
                             const std::vector<int>& images) const {
  vector<int> uimages;
  return computeScore2(coord, images, uimages);
}

float Cbundle::computeScore2(const pmvs::Vec4f& coord,
                             const std::vector<int>& images,
                             std::vector<int>& uimages) const {
  const int inum = (int)images.size();
  uimages.clear();
  if (inum < 2)
    return -1.0f;
  
  vector<pmvs::Vec4f> rays;    rays.resize(inum);
  vector<float> scales;  scales.resize(inum);
  
  for (int r = 0; r < inum; ++r) {
    rays[r] = m_pss.m_photos[images[r]].m_center - coord;
    unitize(rays[r]);
    
    scales[r] = 1.0f / m_pss.m_photos[images[r]].getScale(coord, 0);
  }

  // Find the best pair of images
  pmvs::Vec2i bestpair;
  float bestscore = -INT_MAX/2;
  for (int i = 0; i < inum; ++i)
    for (int j = i+1; j < inum; ++j) {
      const float ftmp =
        angleScore(rays[i], rays[j]) * scales[i] * scales[j];
      if (bestscore < ftmp) {
        bestscore = ftmp;
		bestpair = pmvs::Vec2i(i, j);
      }
    }

  vector<int> inout;  inout.resize(inum, 1);
  inout[bestpair[0]] = 0;         inout[bestpair[1]] = 0;
  uimages.push_back(bestpair[0]);   uimages.push_back(bestpair[1]);

  for (int t = 2; t < min(m_tau, inum); ++t) 
  {
    // Add the best new image
    int ansid = -1;
    float ansscore = -(std::numeric_limits<int>::max())/2.f;
    for (int j = 0; j < inum; ++j) 
	{
      if (inout[j] == 0)
        continue;

      float score = 0.0f;
      for (int k = 0; k < (int)uimages.size(); ++k) 
	  {
        const int iid = uimages[k];
        score += angleScore(rays[j], rays[iid]) * scales[j] * scales[iid];
      }
      if (ansscore < score)
	  {
        ansscore = score;
        ansid = j;
      }
    }
    
    if (ansid == -1)
	{
		std::cerr << "Impossible 2 in compureScore" << endl;      exit(1);
    }
    
    inout[ansid] = 0;
    uimages.push_back(ansid);
    bestscore += ansscore;
  }

  for (int i = 0; i < (int)uimages.size(); ++i)
    uimages[i] = images[uimages[i]];
  
  return bestscore;
}

float Cbundle::computeScore2(const pmvs::Vec4f& coord,
                             const std::vector<int>& images,
                             const int index) const 
{
  vector<int> uimages;
  const int inum = (int)images.size();
  if (inum < 2)
    return -1.0f;
  
  vector<pmvs::Vec4f> rays;    rays.resize(inum);
  vector<float> scales;  scales.resize(inum);
  
  for (int r = 0; r < inum; ++r) {
    rays[r] = m_pss.m_photos[images[r]].m_center - coord;
    unitize(rays[r]);
    
    scales[r] = 1.0f / m_pss.m_photos[images[r]].getScale(coord, 0);
  }

  float bestscore = 0.0f;
  vector<int> inout;  inout.resize(inum, 1);
  inout[index] = 0;
  uimages.push_back(index);

  for (int t = 1; t < min(m_tau, inum); ++t) 
  {
    // Add the best new image
    int ansid = -1;
    float ansscore = -INT_MAX/2;
    for (int j = 0; j < inum; ++j) 
	{
      if (inout[j] == 0)
        continue;

      float score = 0.0f;
      for (int k = 0; k < (int)uimages.size(); ++k) 
	  {
        const int iid = uimages[k];
        score += angleScore(rays[j], rays[iid]) * scales[j] * scales[iid];
      }
      if (ansscore < score) 
	  {
        ansscore = score;
        ansid = j;
      }
    }
    
    if (ansid == -1)
	{
		std::cerr << "Impossible 2 in compureScore" << std::endl;      exit(1);
    }
    
    inout[ansid] = 0;
    uimages.push_back(ansid);
    bestscore += ansscore;
  }
  return bestscore;
}

void Cbundle::writeVis(void) 
{
  ofstream ofstr;
  char buffer[1024];
  sprintf(buffer, "%svis.dat", m_prefix.c_str());
  
  ofstr.open(buffer);
  ofstr << "VISDATA" << endl;
  ofstr << m_cnum << endl;

  int numer = 0;
  int denom = 0;
  
  for (int c = 0; c < m_cnum; ++c) 
  {
    if (m_removed[c])
      ofstr << c << ' ' << 0 << endl;
    else {
      ofstr << c << ' ' << (int)m_neighbors[c].size() << "  ";
      for (int i = 0; i < (int)m_neighbors[c].size(); ++i)
        ofstr << m_neighbors[c][i] << ' ';
      ofstr << endl;

      numer += (int)m_neighbors[c].size();
      ++denom;
    }
  }
  ofstr.close();


  std::cerr << numer / (float)denom << " images in vis on the average" << std::endl;
  
}

void Cbundle::writeCameraCenters(void) 
{
  for (int i = 0; i < (int)m_timages.size(); ++i) 
  {
    char buffer[1024];
    sprintf(buffer, "%scenters-%04d.ply", m_prefix.c_str(), i);
    
    ofstream ofstr;
    ofstr.open(buffer);
    ofstr << "ply" << endl
          << "format ascii 1.0" << endl
          << "element vertex " << (int)m_timages[i].size() << endl
          << "property float x" << endl
          << "property float y" << endl
          << "property float z" << endl
          << "end_header" << endl;
    for (int j = 0; j < (int)m_timages[i].size(); ++j) 
	{
      const int c = m_timages[i][j];
      ofstr << m_pss.m_photos[c].m_center[0] << ' '
            << m_pss.m_photos[c].m_center[1] << ' '
            << m_pss.m_photos[c].m_center[2] << endl;
    }
    ofstr.close();
  }

  {
    char buffer[1024];
    sprintf(buffer, "%scenters-all.ply", m_prefix.c_str());
    ofstream ofstr;
    ofstr.open(buffer);
    
    ofstr << "ply" << endl
          << "format ascii 1.0" << endl
          << "element vertex " << m_cnum << endl
          << "property float x" << endl
          << "property float y" << endl
          << "property float z" << endl
          << "property uchar diffuse_red" << endl
          << "property uchar diffuse_green" << endl
          << "property uchar diffuse_blue" << endl
          << "end_header" << endl;
    for (int c = 0; c < m_cnum; ++c) {
      ofstr << m_pss.m_photos[c].m_center[0] << ' '
            << m_pss.m_photos[c].m_center[1] << ' '
            << m_pss.m_photos[c].m_center[2] << ' ';
      
      if (m_removed[c])
		  ofstr << "0 255 0" << std::endl;
      else
		  ofstr << "255 0 255" << std::endl;
    }
    ofstr.close();
  }
}

void Cbundle::writeGroups(void) 
{
  char buffer[1024];
  sprintf(buffer, "%sske.dat", m_prefix.c_str());
  ofstream ofstr;
  ofstr.open(buffer);
  ofstr << "SKE" << std::endl
	  << m_cnum << ' ' << (int)m_timages.size() << std::endl;
  
  for (int c = 0; c < (int)m_timages.size(); ++c)
  {
	  ofstr << (int)m_timages[c].size() << ' ' << (int)m_oimages[c].size() << std::endl;

    for (int i = 0; i < (int)m_timages[c].size(); ++i)
      ofstr << m_timages[c][i] << ' ';

	ofstr << std::endl;

    for (int i = 0; i < (int)m_oimages[c].size(); ++i)
      ofstr << m_oimages[c][i] << ' ';

	ofstr << std::endl;
  }
}

bool Cbundle::genOption()
{
	int level = 1;

	int csize = 2;

	float threshold = 0.7f;

	int wsize = 7;

	int minImageNum = 3;

	int CPU = static_cast<int>(std::thread::hardware_concurrency());

	const int setEdge = 0;
	const int useBound = 0;
	const int useVisData = 1;
	const int sequence = -1;

	ofstream script;
	char pmvsfile[1024];

	sprintf(pmvsfile, "%spmvs.bat", m_prefix);

	script.open(pmvsfile);

	for (int c = 0; c < (int)m_timages.size(); ++c) 
	{
		ofstream ofstr;
		char buffer[1024];
		sprintf(buffer, "%soption_%04d.txt", m_prefix, c);
		ofstr.open(buffer);

		sprintf(buffer, "pmvs2 ./ option_%04d.txt", c);
		script << buffer << endl;

		ofstr << "# generated by genOption. mode 1. cluster: " << c << endl
			<< "level " << level << endl
			<< "csize " << csize << endl
			<< "threshold " << threshold << endl
			<< "wsize " << wsize << endl
			<< "minImageNum " << minImageNum << endl
			<< "CPU " << CPU << endl
			<< "setEdge " << setEdge << endl
			<< "useBound " << useBound << endl
			<< "useVisData " << useVisData << endl
			<< "sequence " << sequence << endl
			<< "maxAngle 10" << endl
			<< "quad 2.0" << endl;

		ofstr << "timages " << (int)m_timages[c].size() << ' ';
		for (int i = 0; i < (int)m_timages[c].size(); ++i)
			ofstr << m_timages[c][i] << ' ';
		ofstr << std::endl;
		ofstr << "oimages " << (int)m_oimages[c].size() << ' ';
		for (int i = 0; i < (int)m_oimages[c].size(); ++i)
			ofstr << m_oimages[c][i] << ' ';
		ofstr << std::endl;
		ofstr.close();
	}
	script << endl;
	script.close();

	return true;
}

void Cbundle::startTimer(void) {
  time(&m_tv); 
}

time_t Cbundle::curTimer(void) {
  time(&m_curtime); 
  return m_tv - m_curtime;
}
