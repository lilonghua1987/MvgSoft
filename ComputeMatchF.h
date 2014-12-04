#pragma once

#include "ComputeMatch.h"

namespace mvg
{
	class ComputeMatchF : public ComputeMatch
	{
	public:
		ComputeMatchF(QObject *parent);
		~ComputeMatchF();

		virtual void runGThread();

	protected:
		virtual void run();
		virtual void extractFeature();
		virtual void putativeMatch();
		virtual void postProcess();

	private:
		void extractFeatureThread();

	private:
		std::unique_ptr<GeometricFilter_FMatrix_AC> filter;

	};
}
