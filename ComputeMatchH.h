#pragma once

#include "ComputeMatch.h"

namespace mvg
{
	class ComputeMatchH : public ComputeMatch
	{
	public:
		ComputeMatchH(QObject *parent);
		~ComputeMatchH();

		virtual void runGThread();

	protected:
		virtual void run();
		virtual void extractFeature();
		virtual void putativeMatch();
		virtual void postProcess();

	private:
		void extractFeatureThread();

	private:
		std::unique_ptr<GeometricFilter_HMatrix_AC> filter;

	};
}
