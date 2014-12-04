#ifndef PMVS3_DETECTFEATURES_H
#define PMVS3_DETECTFEATURES_H

/*
 * A main class to detect features
 */

#include <string>
#include <list>
#include <thread>
#include <atomic>
#include <mutex>
//#include <pthread.h>
#include "cmvs/image/photoSetS.h"
#include "point.h"

namespace cpmvs_image {
  class CphotoSetS;
};

namespace PMVS3 {

class CdetectFeatures {
 public:
  CdetectFeatures(void);
  virtual ~CdetectFeatures();

  void run(const cpmvs_image::CphotoSetS& pss,
           const int num, const int csize, const int level,
           const int CPU = 1);

  std::vector<std::vector<Cpoint> > m_points;
  
 protected:
	 const cpmvs_image::CphotoSetS* m_ppss;
  int m_csize;
  int m_level;
  
  //----------------------------------------------------------------------
  // thread related
  //----------------------------------------------------------------------  
  //pthread_rwlock_t m_rwlock;
  int m_CPU;
  std::atomic_int jobs;
  std::mutex mtx;

  //std::list<int> m_jobs;
  
  void runThread(void);
};
};

#endif // PMVS3_DETECTFEATURES_H
