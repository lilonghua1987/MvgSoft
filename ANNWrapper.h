#pragma once

#include <vector>
#include "openMVG/matching/matcher_kdtree_flann.hpp"
#include "openMVG/numeric/numeric.h"

namespace mvg
{
	class ANNWrapper
	{
	public:
		ANNWrapper();
		~ANNWrapper();

		void Free();
		void SetPoints(const std::vector <openMVG::Vec3f> &P);
		void FindClosest(const openMVG::Vec3f &P, double *sq_distance, int *index);
		void SetResults(int a);

	private:
		typedef openMVG::matching::ArrayMatcher_Kdtree_Flann<float, flann::L2<float>> KFLANN;
		KFLANN              matcher;
		openMVG::Matf       dataset;               // data points
		openMVG::Vec3f	    m_query_pt;				// query point
		Eigen::VectorXi     m_nnidx;                // near neighbor indices
		KFLANN::DistanceType*	m_dists;					// near neighbor distances

		static const int dim = 3;
		int m_k;
	};
}

