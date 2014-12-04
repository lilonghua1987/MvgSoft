#pragma once

#include <iostream>
#include <numeric>

#include "openMVG/features/feature.hpp"
#include "openMVG/features/descriptor.hpp"

using namespace std;
using namespace openMVG;

extern "C" {
#include "patented/sift/vl/dsift.h"
}

namespace openMVG {
	template<typename type = float>
	static bool DSIFTDetector(const Image<unsigned char>& I,
		std::vector<SIOPointFeature>& feats,
		std::vector<Descriptor<type, 128> >& descs)
	{
		int w = I.Width(), h = I.Height();
		//Convert to float
		Image<float> If(I.GetMat().cast<float>());

		//vl_constructor();
		//std::cout << "vl_dsift_new" << std::endl;
		auto filt = vl_dsift_new(w, h);

		//std::cout << "vl_dsift_process" << std::endl;
		vl_dsift_process(filt, If.data());

		int nkeys = vl_dsift_get_keypoint_num(filt);
		std::cout << "nkeys = " << nkeys << std::endl;
		int desSize = vl_dsift_get_descriptor_size(filt);
		std::cout << "Descriptor size = " << desSize << std::endl;
		auto keyPoints = vl_dsift_get_keypoints(filt);
		auto descriptors = vl_dsift_get_descriptors(filt);

		Descriptor<type, 128> descriptor;
		SIOPointFeature fp;

		for (int i = 0; i < nkeys; ++i)
		{
			fp.x() = keyPoints[i].x;
			fp.y() = keyPoints[i].y;
			fp.scale() = keyPoints[i].s;
			fp.orientation() = keyPoints[i].norm;
			feats.push_back(fp);
			std::copy(descriptors + desSize * i, descriptors + desSize * (i + 1), descriptor.getData());
			descs.push_back(descriptor);
		}

		vl_dsift_delete(filt);

		//vl_destructor();

		return true;
	}


} // namespace openMVG