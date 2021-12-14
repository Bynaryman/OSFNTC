#ifndef random_systems_cubic_symmetric_system_hpp
#define random_systems_cubic_symmetric_system_hpp

#include <utility>
#include <iterator>
#include <math.h>

namespace flopoco
{
namespace random
{
namespace systems
 {

template<class T,class TACC=T>
class SymmetricCubicSystem{
private:
  T ss[13];
  T tmom[5];
  template<class TT>
  static TT square(const TT &x) { return x*x; }
public:
  typedef T real_t;

  int arity() const { return 2; };

  SymmetricCubicSystem(  const T *_ss ,   const T *_tmom )
  {
    std::copy(_ss, _ss+13, ss);
    std::copy(_tmom, _tmom+5, tmom);
  }

  void DefaultGuess(T *params) const{
    params[0]=1; // c1
    params[1]=0; // c3
  }

T metric(const T *parameters) const {
  T c1p1 = parameters[0];
  T c3p1 = parameters[1];
  T c1p2 = c1p1*c1p1;
  T c3p2 = c3p1*c3p1;
  TACC root_0 = convert<TACC>(2*c1p1*c3p1*ss[4]);
  root_0 += c1p2*ss[2];
  root_0 += c3p2*ss[6];
  root_0 += -tmom[2];
  T c1p3 = c1p2*c1p1;
  T c1p4 = c1p3*c1p1;
  T c3p3 = c3p2*c3p1;
  T c3p4 = c3p3*c3p1;
  TACC root_1 = convert<TACC>(4*c1p1*c3p3*ss[10]);
  root_1 += 6*c1p2*c3p2*ss[8];
  root_1 += 4*c1p3*c3p1*ss[6];
  root_1 += c1p4*ss[4];
  root_1 += c3p4*ss[12];
  root_1 += -tmom[4];
  return convert<T>(sqrt(root_0*root_0+root_1*root_1));
}
T metric_and_gradient(const T *parameters, T *gradient) const {
  T c1p1 = parameters[0];
  T c3p1 = parameters[1];
  T c1p2 = c1p1*c1p1;
  T c3p2 = c3p1*c3p1;
  TACC sys_part_0 = convert<TACC>(2*c1p1*c3p1*ss[4]);
  sys_part_0 += c1p2*ss[2];
  sys_part_0 += c3p2*ss[6];
  sys_part_0 += -tmom[2];
  T c1p3 = c1p2*c1p1;
  T c1p4 = c1p3*c1p1;
  T c3p3 = c3p2*c3p1;
  T c3p4 = c3p3*c3p1;
  TACC sys_part_1 = convert<TACC>(4*c1p1*c3p3*ss[10]);
  sys_part_1 += 6*c1p2*c3p2*ss[8];
  sys_part_1 += 4*c1p3*c3p1*ss[6];
  sys_part_1 += c1p4*ss[4];
  sys_part_1 += c3p4*ss[12];
  sys_part_1 += -tmom[4];
  TACC grad_0_num = convert<TACC>(2*(c1p1*ss[2] + c3p1*ss[4])*sys_part_0);
  grad_0_num += 4*(3*c1p1*c3p2*ss[8] + 3*c1p2*c3p1*ss[6] + c1p3*ss[4] + c3p3*ss[10])*sys_part_1;
  TACC grad_0_den = sqrt(square(sys_part_0) + square(sys_part_1));
  TACC grad_1_num = convert<TACC>(2*(c1p1*ss[4] + c3p1*ss[6])*sys_part_0);
  grad_1_num += 4*(3*c1p1*c3p2*ss[10] + 3*c1p2*c3p1*ss[8] + c1p3*ss[6] + c3p3*ss[12])*sys_part_1;
  TACC grad_1_den = sqrt(square(sys_part_0) + square(sys_part_1));
  TACC metric_sqr = convert<TACC>(square(sys_part_0));
  metric_sqr += square(sys_part_1);
  gradient[0]=convert<T>(grad_0_num / grad_0_den);
  gradient[1]=convert<T>(grad_1_num / grad_1_den);
  return convert<T>(sqrt(metric_sqr));
}
void roots(const T *parameters, T *roots) const {
  T c1p1 = parameters[0];
  T c3p1 = parameters[1];
  T c1p2 = c1p1*c1p1;
  T c3p2 = c3p1*c3p1;
  TACC root_0 = convert<TACC>(2*c1p1*c3p1*ss[4]);
  root_0 += c1p2*ss[2];
  root_0 += c3p2*ss[6];
  root_0 += -tmom[2];
  T c1p3 = c1p2*c1p1;
  T c1p4 = c1p3*c1p1;
  T c3p3 = c3p2*c3p1;
  T c3p4 = c3p3*c3p1;
  TACC root_1 = convert<TACC>(4*c1p1*c3p3*ss[10]);
  root_1 += 6*c1p2*c3p2*ss[8];
  root_1 += 4*c1p3*c3p1*ss[6];
  root_1 += c1p4*ss[4];
  root_1 += c3p4*ss[12];
  root_1 += -tmom[4];
  roots[0]=convert<T>(root_0);
  roots[1]=convert<T>(root_1);
}
void roots_and_jacobian(const T *parameters, T *roots, T *jacobian) const {
  T c1p1 = parameters[0];
  T c3p1 = parameters[1];
  T c1p2 = c1p1*c1p1;
  T c3p2 = c3p1*c3p1;
  TACC root_0 = convert<TACC>(2*c1p1*c3p1*ss[4]);
  root_0 += c1p2*ss[2];
  root_0 += c3p2*ss[6];
  root_0 += -tmom[2];
  T c1p3 = c1p2*c1p1;
  T c1p4 = c1p3*c1p1;
  T c3p3 = c3p2*c3p1;
  T c3p4 = c3p3*c3p1;
  TACC root_1 = convert<TACC>(4*c1p1*c3p3*ss[10]);
  root_1 += 6*c1p2*c3p2*ss[8];
  root_1 += 4*c1p3*c3p1*ss[6];
  root_1 += c1p4*ss[4];
  root_1 += c3p4*ss[12];
  root_1 += -tmom[4];
  TACC jac_0_0 = convert<TACC>(2*c1p1*ss[2]);
  jac_0_0 += 2*c3p1*ss[4];
  TACC jac_0_1 = convert<TACC>(2*c1p1*ss[4]);
  jac_0_1 += 2*c3p1*ss[6];
  TACC jac_1_0 = convert<TACC>(12*c1p1*c3p2*ss[8]);
  jac_1_0 += 12*c1p2*c3p1*ss[6];
  jac_1_0 += 4*c1p3*ss[4];
  jac_1_0 += 4*c3p3*ss[10];
  TACC jac_1_1 = convert<TACC>(12*c1p1*c3p2*ss[10]);
  jac_1_1 += 12*c1p2*c3p1*ss[8];
  jac_1_1 += 4*c1p3*ss[6];
  jac_1_1 += 4*c3p3*ss[12];
  roots[0]=convert<T>(root_0);
  roots[1]=convert<T>(root_1);
  jacobian[0] = convert<T>(jac_0_0);
  jacobian[1] = convert<T>(jac_0_1);
  jacobian[2] = convert<T>(jac_1_0);
  jacobian[3] = convert<T>(jac_1_1);
}
}; // SymmetricCubicSystem

}; // systems
}; // random
}; // flopoco

#endif
