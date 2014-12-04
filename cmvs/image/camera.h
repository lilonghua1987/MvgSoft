#ifndef IMAGE_CAMERA_H
#define IMAGE_CAMERA_H

#include <vector>
#include <string>
#include <climits>
#include <algorithm>
#include "numeric/vec4.h"
#include "numeric/mat4.h"
#include "numeric/mat3.h"

namespace cpmvs_image {

class Ccamera {
 public:
  Ccamera(void);
  virtual ~Ccamera();
  
  // Update projection matrices from intrinsics and extrinsics
  void updateProjection(void);
  // Update all the camera related parameters
  void updateCamera(void);

  virtual void init(const std::string cname, const int maxLevel);
  void write(const std::string file);
  
  inline pmvs::Vec3f project(const pmvs::Vec4f& coord, const int level) const;
  inline pmvs::Vec3f mult(const pmvs::Vec4f& coord, const int level) const;
  
  static void setProjection(const std::vector<float>& intrinsics,
			    const std::vector<float>& extrinsics,
			    std::vector<pmvs::Vec4f>& projection,
			    const int txtType);
  
  float getScale(const pmvs::Vec4f& coord, const int level) const;
  void getPAxes(const pmvs::Vec4f& coord, const pmvs::Vec4f& normal,
		pmvs::Vec4f& pxaxis, pmvs::Vec4f& pyaxis, const int level = 0) const;

  void setAxesScale(const float axesScale);
  
  static void proj2q(pmvs::Mat4& mat, double q[6]);
  static void q2proj(const double q[6], pmvs::Mat4& mat);  
  static void setProjectionSub(double params[], std::vector<pmvs::Vec4f>& projection,
			       const int level);

  float computeDistance(const pmvs::Vec4f& point) const;
  float computeDepth(const pmvs::Vec4f& point) const;  
  float computeDepthDif(const pmvs::Vec4f& rhs, const pmvs::Vec4f& lhs) const;

  // Compute where the viewing ray passing through coord intersects
  // with the plane abcd.
  pmvs::Vec4f intersect(const pmvs::Vec4f& coord, const pmvs::Vec4f& abcd) const;
  void intersect(const pmvs::Vec4f& coord, const pmvs::Vec4f& abcd,
                 pmvs::Vec4f& cross, float& distance) const;
  // Computer a 3D coordinate that projects to a given image
  // coordinate. You can specify a different depth by the third
  // component of icoord.
  pmvs::Vec4f unproject(const pmvs::Vec3f& icoord, const int m_level) const;
  
  void setK(pmvs::Mat3f& K) const;
  void setRT(pmvs::Mat4f& RT) const;

  void getR(pmvs::Mat3f& R) const;
  
  //----------------------------------------------------------------------
  // txt file name
  std::string m_cname;  
  // Optical center
  pmvs::Vec4f m_center;
  // Optical axis
  pmvs::Vec4f m_oaxis;
  
  float m_ipscale;
  // 3x4 projection matrix
  std::vector<std::vector<pmvs::Vec4f> > m_projection;
  pmvs::Vec3f m_xaxis;
  pmvs::Vec3f m_yaxis;
  pmvs::Vec3f m_zaxis;

  // intrinsic and extrinsic camera parameters. Compact form.
  std::vector<float> m_intrinsics;
  std::vector<float> m_extrinsics;
  // camera parameter type
  int m_txtType;
 protected:
  int m_maxLevel;

  float m_axesScale;

  pmvs::Vec4f getOpticalCenter(void) const;
};

inline pmvs::Vec3f Ccamera::project(const pmvs::Vec4f& coord,
			      const int level) const {
  pmvs::Vec3f vtmp;    
  for (int i = 0; i < 3; ++i)
    vtmp[i] = m_projection[level][i] * coord;

  if (vtmp[2] <= 0.0) {
    vtmp[0] = -0xffff;
    vtmp[1] = -0xffff;
    vtmp[2] = -1.0f;
    return vtmp;
  }
  else
    vtmp /= vtmp[2];
  
  vtmp[0] = (std::max)((float)(INT_MIN + 3.0f),
		     (std::min)((float)(INT_MAX - 3.0f),
			      vtmp[0]));
  vtmp[1] = (std::max)((float)(INT_MIN + 3.0f),
		     (std::min)((float)(INT_MAX - 3.0f),
			      vtmp[1]));
  
  return vtmp;
};

inline pmvs::Vec3f Ccamera::mult(const pmvs::Vec4f& coord,
			      const int level) const {
  pmvs::Vec3f vtmp;    
  for (int i = 0; i < 3; ++i)
    vtmp[i] = m_projection[level][i] * coord;
  
  return vtmp;
};
 
template<class T>
float computeEPD(const pmvs::TMat3<T>& F, const pmvs::TVec3<T>& p0, const pmvs::TVec3<T>& p1) {
	pmvs::TVec3<T> line = F * p1;
  const T ftmp = sqrt(line[0] * line[0] + line[1] * line[1]);
  if (ftmp == 0.0)
    return 0.0;

  line /= ftmp;
  return fabs(line * p0);
};

template<class T>
void setF(const Ccamera& lhs, const Ccamera& rhs,
	pmvs::TMat3<T>& F, const int level = 0) {
	const pmvs::TVec4<T>& p00 = lhs.m_projection[level][0];
	const pmvs::TVec4<T>& p01 = lhs.m_projection[level][1];
	const pmvs::TVec4<T>& p02 = lhs.m_projection[level][2];

	const pmvs::TVec4<T>& p10 = rhs.m_projection[level][0];
	const pmvs::TVec4<T>& p11 = rhs.m_projection[level][1];
	const pmvs::TVec4<T>& p12 = rhs.m_projection[level][2];

	F[0][0] = det(pmvs::TMat4<T>(p01, p02, p11, p12));
	F[0][1] = det(pmvs::TMat4<T>(p01, p02, p12, p10));
	F[0][2] = det(pmvs::TMat4<T>(p01, p02, p10, p11));

	F[1][0] = det(pmvs::TMat4<T>(p02, p00, p11, p12));
	F[1][1] = det(pmvs::TMat4<T>(p02, p00, p12, p10));
	F[1][2] = det(pmvs::TMat4<T>(p02, p00, p10, p11));

	F[2][0] = det(pmvs::TMat4<T>(p00, p01, p11, p12));
	F[2][1] = det(pmvs::TMat4<T>(p00, p01, p12, p10));
	F[2][2] = det(pmvs::TMat4<T>(p00, p01, p10, p11));
 };
 
}; // namespace image

#endif // CAMERA_H
