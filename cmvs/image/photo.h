#ifndef IMAGE_PHOTO_H
#define IMAGE_PHOTO_H

#include "numeric/vec4.h"
#include "image.h"
#include "camera.h"

namespace cpmvs_image {

// Cphoto is an image with camera parameters
class Cphoto : public Cimage, public Ccamera {
 public:
  Cphoto(void);
  virtual ~Cphoto();

  virtual void init(const std::string name, const std::string mname,
		    const std::string cname, const int maxLevel = 1);

  virtual void init(const std::string name, const std::string mname,
                    const std::string ename,
		    const std::string cname, const int maxLevel = 1);
  
  // grabTex given 2D sampling information
  void grabTex(const int level, const pmvs::Vec2f& icoord,
	       const pmvs::Vec2f& xaxis, const pmvs::Vec2f& yaxis, const int size,
	       std::vector<pmvs::Vec3f>& tex, const int normalizef = 1) const;

  // grabTex given 3D sampling information
  void grabTex(const int level, const pmvs::Vec4f& coord,
	       const pmvs::Vec4f& pxaxis, const pmvs::Vec4f& pyaxis, const pmvs::Vec4f& pzaxis,
	       const int size, std::vector<pmvs::Vec3f>& tex, float& weight,
               const int normalizef = 1) const;


  inline pmvs::Vec3f getColor(const float fx, const float fy, const int level) const;  
  inline pmvs::Vec3f getColor(const pmvs::Vec4f& coord, const int level) const;
  inline int getMask(const pmvs::Vec4f& coord, const int level) const;
  inline int getEdge(const pmvs::Vec4f& coord, const int level) const;
  
  static float idot(const std::vector<pmvs::Vec3f>& tex0,
		    const std::vector<pmvs::Vec3f>& tex1);

  static void idotC(const std::vector<pmvs::Vec3f>& tex0,
		    const std::vector<pmvs::Vec3f>& tex1, double* idc);

  static void normalize(std::vector<pmvs::Vec3f>& tex);

  static float ssd(const std::vector<pmvs::Vec3f>& tex0,
		   const std::vector<pmvs::Vec3f>& tex1);
 protected:
};

pmvs::Vec3f Cphoto::getColor(const float fx, const float fy, const int level) const {
  return Cimage::getColor(fx, fy, level);
};
  
pmvs::Vec3f Cphoto::getColor(const pmvs::Vec4f& coord, const int level) const {
  const pmvs::Vec3f icoord = project(coord, level);
  return Cimage::getColor(icoord[0], icoord[1], level);
};
  
int Cphoto::getMask(const pmvs::Vec4f& coord, const int level) const {
  if (m_masks[level].empty())
    return 1;
  
  const pmvs::Vec3f icoord = project(coord, level);
  return Cimage::getMask(icoord[0], icoord[1], level);
};

int Cphoto::getEdge(const pmvs::Vec4f& coord, const int level) const {
  if (m_edges[level].empty())
    return 1;
  
  const pmvs::Vec3f icoord = project(coord, level);

  if (icoord[0] < 0 || m_widths[level] - 1 <= icoord[0] ||
      icoord[1] < 0 || m_heights[level] - 1 <= icoord[1])
    return 0;
  
  return Cimage::getEdge(icoord[0], icoord[1], level);
};

};

#endif // PHOTO_H
