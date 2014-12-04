#pragma once

#include "ComputeMatch.h"
//#include "MatcherThread.h"

namespace mvg
{
	class ComputeMatchE : public ComputeMatch
	{
	public:
		ComputeMatchE(QObject *parent);
		virtual ~ComputeMatchE();

		virtual void runGThread();

	protected:
		virtual void run();
		virtual bool preProcess();
		virtual void extractFeature();
		virtual void putativeMatch();
		virtual void postProcess();

	private:
		void extractFeatureThread();

	private:
		openMVG::Mat3 K;
		std::unique_ptr<GeometricFilter_EMatrix_AC> filter;
	};
}
