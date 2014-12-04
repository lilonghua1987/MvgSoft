#include "ANNWrapper.h"

namespace mvg
{
	ANNWrapper::ANNWrapper()
	{
		m_k = 1;
	}

	ANNWrapper::~ANNWrapper()
	{
		if (m_dists != nullptr) delete[] m_dists;
	}

	void ANNWrapper::SetPoints(const std::vector <openMVG::Vec3f> &P)
	{

		dataset = openMVG::Matf(P.size(), dim);
		m_nnidx = Eigen::VectorXi(m_k);						// allocate near neigh indices
		m_dists = new KFLANN::DistanceType[m_k];		// allocate near neighbor m_dists

		for (unsigned int i = 0; i < P.size(); i++)
		{
			dataset(i, 0) = P.at(i).x();
			dataset(i, 1) = P.at(i).y();
			dataset(i, 2) = P.at(i).z();
		}

		matcher.Build(dataset.data(), P.size(), dim);
	}

	void ANNWrapper::FindClosest(const openMVG::Vec3f &P, double *sq_distance, int *index)
	{
		m_query_pt[0] = P.x();
		m_query_pt[1] = P.y();
		m_query_pt[2] = P.z();

		matcher.SearchNeighbour(m_query_pt.data(), m_nnidx.data(), m_dists);

		for (int i = 0; i < m_k; i++)
		{
			sq_distance[i] = m_dists[i];
			index[i] = m_nnidx[i];
		}
	}

	void ANNWrapper::SetResults(int a)
	{
		m_k = a;
	}
}
