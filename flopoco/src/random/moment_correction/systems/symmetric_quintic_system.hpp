#ifndef random_systems_quintic_symmetric_system_hpp
#define random_systems_quintic_symmetric_system_hpp

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
class SymmetricQuinticSystem{
private:
  T ss[31];
  T tmom[7];
  static T square(const T &x) { return x*x; }
public:
  typedef T real_t;

  int arity() const { return 3; };

  SymmetricQuinticSystem(  const T *_ss ,   const T *_tmom )
  {
    std::copy(_ss, _ss+31, ss);
    std::copy(_tmom, _tmom+7, tmom);
  }

  void DefaultGuess(T *params) const{
    params[0]=1; // c1
    params[1]=0; // c3
    params[2]=0; // c5
  }

T metric(const T *parameters) const {
  T c1p1 = parameters[0];
  T c3p1 = parameters[1];
  T c5p1 = parameters[2];
  T c1p2 = c1p1*c1p1;
  T c3p2 = c3p1*c3p1;
  T c5p2 = c5p1*c5p1;
  TACC root_0 = 2*c1p1*c3p1*ss[4];
  root_0 += 2*c1p1*c5p1*ss[6];
  root_0 += 2*c3p1*c5p1*ss[8];
  root_0 += c1p2*ss[2];
  root_0 += c3p2*ss[6];
  root_0 += c5p2*ss[10];
  root_0 += -tmom[2];
  T c1p3 = c1p2*c1p1;
  T c1p4 = c1p3*c1p1;
  T c3p3 = c3p2*c3p1;
  T c3p4 = c3p3*c3p1;
  T c5p3 = c5p2*c5p1;
  T c5p4 = c5p3*c5p1;
  TACC root_1 = 12*c1p1*c3p1*c5p2*ss[14];
  root_1 += 12*c1p1*c3p2*c5p1*ss[12];
  root_1 += 12*c1p2*c3p1*c5p1*ss[10];
  root_1 += 4*c1p1*c3p3*ss[10];
  root_1 += 4*c1p1*c5p3*ss[16];
  root_1 += 6*c1p2*c3p2*ss[8];
  root_1 += 6*c1p2*c5p2*ss[12];
  root_1 += 4*c1p3*c3p1*ss[6];
  root_1 += 4*c1p3*c5p1*ss[8];
  root_1 += 4*c3p1*c5p3*ss[18];
  root_1 += 6*c3p2*c5p2*ss[16];
  root_1 += 4*c3p3*c5p1*ss[14];
  root_1 += c1p4*ss[4];
  root_1 += c3p4*ss[12];
  root_1 += c5p4*ss[20];
  root_1 += -tmom[4];
  T c1p5 = c1p4*c1p1;
  T c1p6 = c1p5*c1p1;
  T c3p5 = c3p4*c3p1;
  T c3p6 = c3p5*c3p1;
  T c5p5 = c5p4*c5p1;
  T c5p6 = c5p5*c5p1;
  TACC root_2 = 30*c1p1*c3p1*c5p4*ss[24];
  root_2 += 60*c1p1*c3p2*c5p3*ss[22];
  root_2 += 60*c1p1*c3p3*c5p2*ss[20];
  root_2 += 30*c1p1*c3p4*c5p1*ss[18];
  root_2 += 60*c1p2*c3p1*c5p3*ss[20];
  root_2 += 90*c1p2*c3p2*c5p2*ss[18];
  root_2 += 60*c1p2*c3p3*c5p1*ss[16];
  root_2 += 60*c1p3*c3p1*c5p2*ss[16];
  root_2 += 60*c1p3*c3p2*c5p1*ss[14];
  root_2 += 30*c1p4*c3p1*c5p1*ss[12];
  root_2 += 6*c1p1*c3p5*ss[16];
  root_2 += 6*c1p1*c5p5*ss[26];
  root_2 += 15*c1p2*c3p4*ss[14];
  root_2 += 15*c1p2*c5p4*ss[22];
  root_2 += 20*c1p3*c3p3*ss[12];
  root_2 += 20*c1p3*c5p3*ss[18];
  root_2 += 15*c1p4*c3p2*ss[10];
  root_2 += 15*c1p4*c5p2*ss[14];
  root_2 += 6*c1p5*c3p1*ss[8];
  root_2 += 6*c1p5*c5p1*ss[10];
  root_2 += 6*c3p1*c5p5*ss[28];
  root_2 += 15*c3p2*c5p4*ss[26];
  root_2 += 20*c3p3*c5p3*ss[24];
  root_2 += 15*c3p4*c5p2*ss[22];
  root_2 += 6*c3p5*c5p1*ss[20];
  root_2 += c1p6*ss[6];
  root_2 += c3p6*ss[18];
  root_2 += c5p6*ss[30];
  root_2 += -tmom[6];
  return sqrt(root_0*root_0+root_1*root_1+root_2*root_2);
}
T metric_and_gradient(const T *parameters, T *gradient) const {
  T c1p1 = parameters[0];
  T c3p1 = parameters[1];
  T c5p1 = parameters[2];
  T c1p2 = c1p1*c1p1;
  T c3p2 = c3p1*c3p1;
  T c5p2 = c5p1*c5p1;
  TACC sys_part_0 = 2*c1p1*c3p1*ss[4];
  sys_part_0 += 2*c1p1*c5p1*ss[6];
  sys_part_0 += 2*c3p1*c5p1*ss[8];
  sys_part_0 += c1p2*ss[2];
  sys_part_0 += c3p2*ss[6];
  sys_part_0 += c5p2*ss[10];
  sys_part_0 += -tmom[2];
  T c1p3 = c1p2*c1p1;
  T c1p4 = c1p3*c1p1;
  T c3p3 = c3p2*c3p1;
  T c3p4 = c3p3*c3p1;
  T c5p3 = c5p2*c5p1;
  T c5p4 = c5p3*c5p1;
  TACC sys_part_1 = 12*c1p1*c3p1*c5p2*ss[14];
  sys_part_1 += 12*c1p1*c3p2*c5p1*ss[12];
  sys_part_1 += 12*c1p2*c3p1*c5p1*ss[10];
  sys_part_1 += 4*c1p1*c3p3*ss[10];
  sys_part_1 += 4*c1p1*c5p3*ss[16];
  sys_part_1 += 6*c1p2*c3p2*ss[8];
  sys_part_1 += 6*c1p2*c5p2*ss[12];
  sys_part_1 += 4*c1p3*c3p1*ss[6];
  sys_part_1 += 4*c1p3*c5p1*ss[8];
  sys_part_1 += 4*c3p1*c5p3*ss[18];
  sys_part_1 += 6*c3p2*c5p2*ss[16];
  sys_part_1 += 4*c3p3*c5p1*ss[14];
  sys_part_1 += c1p4*ss[4];
  sys_part_1 += c3p4*ss[12];
  sys_part_1 += c5p4*ss[20];
  sys_part_1 += -tmom[4];
  T c1p5 = c1p4*c1p1;
  T c1p6 = c1p5*c1p1;
  T c3p5 = c3p4*c3p1;
  T c3p6 = c3p5*c3p1;
  T c5p5 = c5p4*c5p1;
  T c5p6 = c5p5*c5p1;
  TACC sys_part_2 = 30*c1p1*c3p1*c5p4*ss[24];
  sys_part_2 += 60*c1p1*c3p2*c5p3*ss[22];
  sys_part_2 += 60*c1p1*c3p3*c5p2*ss[20];
  sys_part_2 += 30*c1p1*c3p4*c5p1*ss[18];
  sys_part_2 += 60*c1p2*c3p1*c5p3*ss[20];
  sys_part_2 += 90*c1p2*c3p2*c5p2*ss[18];
  sys_part_2 += 60*c1p2*c3p3*c5p1*ss[16];
  sys_part_2 += 60*c1p3*c3p1*c5p2*ss[16];
  sys_part_2 += 60*c1p3*c3p2*c5p1*ss[14];
  sys_part_2 += 30*c1p4*c3p1*c5p1*ss[12];
  sys_part_2 += 6*c1p1*c3p5*ss[16];
  sys_part_2 += 6*c1p1*c5p5*ss[26];
  sys_part_2 += 15*c1p2*c3p4*ss[14];
  sys_part_2 += 15*c1p2*c5p4*ss[22];
  sys_part_2 += 20*c1p3*c3p3*ss[12];
  sys_part_2 += 20*c1p3*c5p3*ss[18];
  sys_part_2 += 15*c1p4*c3p2*ss[10];
  sys_part_2 += 15*c1p4*c5p2*ss[14];
  sys_part_2 += 6*c1p5*c3p1*ss[8];
  sys_part_2 += 6*c1p5*c5p1*ss[10];
  sys_part_2 += 6*c3p1*c5p5*ss[28];
  sys_part_2 += 15*c3p2*c5p4*ss[26];
  sys_part_2 += 20*c3p3*c5p3*ss[24];
  sys_part_2 += 15*c3p4*c5p2*ss[22];
  sys_part_2 += 6*c3p5*c5p1*ss[20];
  sys_part_2 += c1p6*ss[6];
  sys_part_2 += c3p6*ss[18];
  sys_part_2 += c5p6*ss[30];
  sys_part_2 += -tmom[6];
  TACC grad_0_num = 2*(c1p1*ss[2] + c3p1*ss[4] + c5p1*ss[6])*sys_part_0;
  grad_0_num += 4*(6*c1p1*c3p1*c5p1*ss[10] + 3*c1p1*c3p2*ss[8] + 3*c1p1*c5p2*ss[12] + 3*c1p2*c3p1*ss[6] + 3*c1p2*c5p1*ss[8] + 3*c3p1*c5p2*ss[14] + 3*c3p2*c5p1*ss[12] + c1p3*ss[4] + c3p3*ss[10] + c5p3*ss[16])*sys_part_1;
  grad_0_num += 6*(20*c1p1*c3p1*c5p3*ss[20] + 30*c1p1*c3p2*c5p2*ss[18] + 20*c1p1*c3p3*c5p1*ss[16] + 30*c1p2*c3p1*c5p2*ss[16] + 30*c1p2*c3p2*c5p1*ss[14] + 20*c1p3*c3p1*c5p1*ss[12] + 5*c1p1*c3p4*ss[14] + 5*c1p1*c5p4*ss[22] + 10*c1p2*c3p3*ss[12] + 10*c1p2*c5p3*ss[18] + 10*c1p3*c3p2*ss[10] + 10*c1p3*c5p2*ss[14] + 5*c1p4*c3p1*ss[8] + 5*c1p4*c5p1*ss[10] + 5*c3p1*c5p4*ss[24] + 10*c3p2*c5p3*ss[22] + 10*c3p3*c5p2*ss[20] + 5*c3p4*c5p1*ss[18] + c1p5*ss[6] + c3p5*ss[16] + c5p5*ss[26])*sys_part_2;
  T grad_0_den = sqrt(square(sys_part_0) + square(sys_part_1) + square(sys_part_2));
  TACC grad_1_num = 2*(c1p1*ss[4] + c3p1*ss[6] + c5p1*ss[8])*sys_part_0;
  grad_1_num += 4*(6*c1p1*c3p1*c5p1*ss[12] + 3*c1p1*c3p2*ss[10] + 3*c1p1*c5p2*ss[14] + 3*c1p2*c3p1*ss[8] + 3*c1p2*c5p1*ss[10] + 3*c3p1*c5p2*ss[16] + 3*c3p2*c5p1*ss[14] + c1p3*ss[6] + c3p3*ss[12] + c5p3*ss[18])*sys_part_1;
  grad_1_num += 6*(20*c1p1*c3p1*c5p3*ss[22] + 30*c1p1*c3p2*c5p2*ss[20] + 20*c1p1*c3p3*c5p1*ss[18] + 30*c1p2*c3p1*c5p2*ss[18] + 30*c1p2*c3p2*c5p1*ss[16] + 20*c1p3*c3p1*c5p1*ss[14] + 5*c1p1*c3p4*ss[16] + 5*c1p1*c5p4*ss[24] + 10*c1p2*c3p3*ss[14] + 10*c1p2*c5p3*ss[20] + 10*c1p3*c3p2*ss[12] + 10*c1p3*c5p2*ss[16] + 5*c1p4*c3p1*ss[10] + 5*c1p4*c5p1*ss[12] + 5*c3p1*c5p4*ss[26] + 10*c3p2*c5p3*ss[24] + 10*c3p3*c5p2*ss[22] + 5*c3p4*c5p1*ss[20] + c1p5*ss[8] + c3p5*ss[18] + c5p5*ss[28])*sys_part_2;
  T grad_1_den = sqrt(square(sys_part_0) + square(sys_part_1) + square(sys_part_2));
  TACC grad_2_num = 2*(c1p1*ss[6] + c3p1*ss[8] + c5p1*ss[10])*sys_part_0;
  grad_2_num += 4*(6*c1p1*c3p1*c5p1*ss[14] + 3*c1p1*c3p2*ss[12] + 3*c1p1*c5p2*ss[16] + 3*c1p2*c3p1*ss[10] + 3*c1p2*c5p1*ss[12] + 3*c3p1*c5p2*ss[18] + 3*c3p2*c5p1*ss[16] + c1p3*ss[8] + c3p3*ss[14] + c5p3*ss[20])*sys_part_1;
  grad_2_num += 6*(20*c1p1*c3p1*c5p3*ss[24] + 30*c1p1*c3p2*c5p2*ss[22] + 20*c1p1*c3p3*c5p1*ss[20] + 30*c1p2*c3p1*c5p2*ss[20] + 30*c1p2*c3p2*c5p1*ss[18] + 20*c1p3*c3p1*c5p1*ss[16] + 5*c1p1*c3p4*ss[18] + 5*c1p1*c5p4*ss[26] + 10*c1p2*c3p3*ss[16] + 10*c1p2*c5p3*ss[22] + 10*c1p3*c3p2*ss[14] + 10*c1p3*c5p2*ss[18] + 5*c1p4*c3p1*ss[12] + 5*c1p4*c5p1*ss[14] + 5*c3p1*c5p4*ss[28] + 10*c3p2*c5p3*ss[26] + 10*c3p3*c5p2*ss[24] + 5*c3p4*c5p1*ss[22] + c1p5*ss[10] + c3p5*ss[20] + c5p5*ss[30])*sys_part_2;
  T grad_2_den = sqrt(square(sys_part_0) + square(sys_part_1) + square(sys_part_2));
  TACC metric_sqr = square(sys_part_0);
  metric_sqr += square(sys_part_1);
  metric_sqr += square(sys_part_2);
  gradient[0]=grad_0_num / grad_0_den;
  gradient[1]=grad_1_num / grad_1_den;
  gradient[2]=grad_2_num / grad_2_den;
  return sqrt(metric_sqr);
}
void roots(const T *parameters, T *roots) const {
  T c1p1 = parameters[0];
  T c3p1 = parameters[1];
  T c5p1 = parameters[2];
  T c1p2 = c1p1*c1p1;
  T c3p2 = c3p1*c3p1;
  T c5p2 = c5p1*c5p1;
  TACC root_0 = 2*c1p1*c3p1*ss[4];
  root_0 += 2*c1p1*c5p1*ss[6];
  root_0 += 2*c3p1*c5p1*ss[8];
  root_0 += c1p2*ss[2];
  root_0 += c3p2*ss[6];
  root_0 += c5p2*ss[10];
  root_0 += -tmom[2];
  T c1p3 = c1p2*c1p1;
  T c1p4 = c1p3*c1p1;
  T c3p3 = c3p2*c3p1;
  T c3p4 = c3p3*c3p1;
  T c5p3 = c5p2*c5p1;
  T c5p4 = c5p3*c5p1;
  TACC root_1 = 12*c1p1*c3p1*c5p2*ss[14];
  root_1 += 12*c1p1*c3p2*c5p1*ss[12];
  root_1 += 12*c1p2*c3p1*c5p1*ss[10];
  root_1 += 4*c1p1*c3p3*ss[10];
  root_1 += 4*c1p1*c5p3*ss[16];
  root_1 += 6*c1p2*c3p2*ss[8];
  root_1 += 6*c1p2*c5p2*ss[12];
  root_1 += 4*c1p3*c3p1*ss[6];
  root_1 += 4*c1p3*c5p1*ss[8];
  root_1 += 4*c3p1*c5p3*ss[18];
  root_1 += 6*c3p2*c5p2*ss[16];
  root_1 += 4*c3p3*c5p1*ss[14];
  root_1 += c1p4*ss[4];
  root_1 += c3p4*ss[12];
  root_1 += c5p4*ss[20];
  root_1 += -tmom[4];
  T c1p5 = c1p4*c1p1;
  T c1p6 = c1p5*c1p1;
  T c3p5 = c3p4*c3p1;
  T c3p6 = c3p5*c3p1;
  T c5p5 = c5p4*c5p1;
  T c5p6 = c5p5*c5p1;
  TACC root_2 = 30*c1p1*c3p1*c5p4*ss[24];
  root_2 += 60*c1p1*c3p2*c5p3*ss[22];
  root_2 += 60*c1p1*c3p3*c5p2*ss[20];
  root_2 += 30*c1p1*c3p4*c5p1*ss[18];
  root_2 += 60*c1p2*c3p1*c5p3*ss[20];
  root_2 += 90*c1p2*c3p2*c5p2*ss[18];
  root_2 += 60*c1p2*c3p3*c5p1*ss[16];
  root_2 += 60*c1p3*c3p1*c5p2*ss[16];
  root_2 += 60*c1p3*c3p2*c5p1*ss[14];
  root_2 += 30*c1p4*c3p1*c5p1*ss[12];
  root_2 += 6*c1p1*c3p5*ss[16];
  root_2 += 6*c1p1*c5p5*ss[26];
  root_2 += 15*c1p2*c3p4*ss[14];
  root_2 += 15*c1p2*c5p4*ss[22];
  root_2 += 20*c1p3*c3p3*ss[12];
  root_2 += 20*c1p3*c5p3*ss[18];
  root_2 += 15*c1p4*c3p2*ss[10];
  root_2 += 15*c1p4*c5p2*ss[14];
  root_2 += 6*c1p5*c3p1*ss[8];
  root_2 += 6*c1p5*c5p1*ss[10];
  root_2 += 6*c3p1*c5p5*ss[28];
  root_2 += 15*c3p2*c5p4*ss[26];
  root_2 += 20*c3p3*c5p3*ss[24];
  root_2 += 15*c3p4*c5p2*ss[22];
  root_2 += 6*c3p5*c5p1*ss[20];
  root_2 += c1p6*ss[6];
  root_2 += c3p6*ss[18];
  root_2 += c5p6*ss[30];
  root_2 += -tmom[6];
  roots[0]=root_0;
  roots[1]=root_1;
  roots[2]=root_2;
}
void roots_and_jacobian(const T *parameters, T *roots, T *jacobian) const {
  T c1p1 = parameters[0];
  T c3p1 = parameters[1];
  T c5p1 = parameters[2];
  T c1p2 = c1p1*c1p1;
  T c3p2 = c3p1*c3p1;
  T c5p2 = c5p1*c5p1;
  TACC root_0 = 2*c1p1*c3p1*ss[4];
  root_0 += 2*c1p1*c5p1*ss[6];
  root_0 += 2*c3p1*c5p1*ss[8];
  root_0 += c1p2*ss[2];
  root_0 += c3p2*ss[6];
  root_0 += c5p2*ss[10];
  root_0 += -tmom[2];
  T c1p3 = c1p2*c1p1;
  T c1p4 = c1p3*c1p1;
  T c3p3 = c3p2*c3p1;
  T c3p4 = c3p3*c3p1;
  T c5p3 = c5p2*c5p1;
  T c5p4 = c5p3*c5p1;
  TACC root_1 = 12*c1p1*c3p1*c5p2*ss[14];
  root_1 += 12*c1p1*c3p2*c5p1*ss[12];
  root_1 += 12*c1p2*c3p1*c5p1*ss[10];
  root_1 += 4*c1p1*c3p3*ss[10];
  root_1 += 4*c1p1*c5p3*ss[16];
  root_1 += 6*c1p2*c3p2*ss[8];
  root_1 += 6*c1p2*c5p2*ss[12];
  root_1 += 4*c1p3*c3p1*ss[6];
  root_1 += 4*c1p3*c5p1*ss[8];
  root_1 += 4*c3p1*c5p3*ss[18];
  root_1 += 6*c3p2*c5p2*ss[16];
  root_1 += 4*c3p3*c5p1*ss[14];
  root_1 += c1p4*ss[4];
  root_1 += c3p4*ss[12];
  root_1 += c5p4*ss[20];
  root_1 += -tmom[4];
  T c1p5 = c1p4*c1p1;
  T c1p6 = c1p5*c1p1;
  T c3p5 = c3p4*c3p1;
  T c3p6 = c3p5*c3p1;
  T c5p5 = c5p4*c5p1;
  T c5p6 = c5p5*c5p1;
  TACC root_2 = 30*c1p1*c3p1*c5p4*ss[24];
  root_2 += 60*c1p1*c3p2*c5p3*ss[22];
  root_2 += 60*c1p1*c3p3*c5p2*ss[20];
  root_2 += 30*c1p1*c3p4*c5p1*ss[18];
  root_2 += 60*c1p2*c3p1*c5p3*ss[20];
  root_2 += 90*c1p2*c3p2*c5p2*ss[18];
  root_2 += 60*c1p2*c3p3*c5p1*ss[16];
  root_2 += 60*c1p3*c3p1*c5p2*ss[16];
  root_2 += 60*c1p3*c3p2*c5p1*ss[14];
  root_2 += 30*c1p4*c3p1*c5p1*ss[12];
  root_2 += 6*c1p1*c3p5*ss[16];
  root_2 += 6*c1p1*c5p5*ss[26];
  root_2 += 15*c1p2*c3p4*ss[14];
  root_2 += 15*c1p2*c5p4*ss[22];
  root_2 += 20*c1p3*c3p3*ss[12];
  root_2 += 20*c1p3*c5p3*ss[18];
  root_2 += 15*c1p4*c3p2*ss[10];
  root_2 += 15*c1p4*c5p2*ss[14];
  root_2 += 6*c1p5*c3p1*ss[8];
  root_2 += 6*c1p5*c5p1*ss[10];
  root_2 += 6*c3p1*c5p5*ss[28];
  root_2 += 15*c3p2*c5p4*ss[26];
  root_2 += 20*c3p3*c5p3*ss[24];
  root_2 += 15*c3p4*c5p2*ss[22];
  root_2 += 6*c3p5*c5p1*ss[20];
  root_2 += c1p6*ss[6];
  root_2 += c3p6*ss[18];
  root_2 += c5p6*ss[30];
  root_2 += -tmom[6];
  TACC jac_0_0 = 2*c1p1*ss[2];
  jac_0_0 += 2*c3p1*ss[4];
  jac_0_0 += 2*c5p1*ss[6];
  TACC jac_0_1 = 2*c1p1*ss[4];
  jac_0_1 += 2*c3p1*ss[6];
  jac_0_1 += 2*c5p1*ss[8];
  TACC jac_0_2 = 2*c1p1*ss[6];
  jac_0_2 += 2*c3p1*ss[8];
  jac_0_2 += 2*c5p1*ss[10];
  TACC jac_1_0 = 24*c1p1*c3p1*c5p1*ss[10];
  jac_1_0 += 12*c1p1*c3p2*ss[8];
  jac_1_0 += 12*c1p1*c5p2*ss[12];
  jac_1_0 += 12*c1p2*c3p1*ss[6];
  jac_1_0 += 12*c1p2*c5p1*ss[8];
  jac_1_0 += 12*c3p1*c5p2*ss[14];
  jac_1_0 += 12*c3p2*c5p1*ss[12];
  jac_1_0 += 4*c1p3*ss[4];
  jac_1_0 += 4*c3p3*ss[10];
  jac_1_0 += 4*c5p3*ss[16];
  TACC jac_1_1 = 24*c1p1*c3p1*c5p1*ss[12];
  jac_1_1 += 12*c1p1*c3p2*ss[10];
  jac_1_1 += 12*c1p1*c5p2*ss[14];
  jac_1_1 += 12*c1p2*c3p1*ss[8];
  jac_1_1 += 12*c1p2*c5p1*ss[10];
  jac_1_1 += 12*c3p1*c5p2*ss[16];
  jac_1_1 += 12*c3p2*c5p1*ss[14];
  jac_1_1 += 4*c1p3*ss[6];
  jac_1_1 += 4*c3p3*ss[12];
  jac_1_1 += 4*c5p3*ss[18];
  TACC jac_1_2 = 24*c1p1*c3p1*c5p1*ss[14];
  jac_1_2 += 12*c1p1*c3p2*ss[12];
  jac_1_2 += 12*c1p1*c5p2*ss[16];
  jac_1_2 += 12*c1p2*c3p1*ss[10];
  jac_1_2 += 12*c1p2*c5p1*ss[12];
  jac_1_2 += 12*c3p1*c5p2*ss[18];
  jac_1_2 += 12*c3p2*c5p1*ss[16];
  jac_1_2 += 4*c1p3*ss[8];
  jac_1_2 += 4*c3p3*ss[14];
  jac_1_2 += 4*c5p3*ss[20];
  TACC jac_2_0 = 120*c1p1*c3p1*c5p3*ss[20];
  jac_2_0 += 180*c1p1*c3p2*c5p2*ss[18];
  jac_2_0 += 120*c1p1*c3p3*c5p1*ss[16];
  jac_2_0 += 180*c1p2*c3p1*c5p2*ss[16];
  jac_2_0 += 180*c1p2*c3p2*c5p1*ss[14];
  jac_2_0 += 120*c1p3*c3p1*c5p1*ss[12];
  jac_2_0 += 30*c1p1*c3p4*ss[14];
  jac_2_0 += 30*c1p1*c5p4*ss[22];
  jac_2_0 += 60*c1p2*c3p3*ss[12];
  jac_2_0 += 60*c1p2*c5p3*ss[18];
  jac_2_0 += 60*c1p3*c3p2*ss[10];
  jac_2_0 += 60*c1p3*c5p2*ss[14];
  jac_2_0 += 30*c1p4*c3p1*ss[8];
  jac_2_0 += 30*c1p4*c5p1*ss[10];
  jac_2_0 += 30*c3p1*c5p4*ss[24];
  jac_2_0 += 60*c3p2*c5p3*ss[22];
  jac_2_0 += 60*c3p3*c5p2*ss[20];
  jac_2_0 += 30*c3p4*c5p1*ss[18];
  jac_2_0 += 6*c1p5*ss[6];
  jac_2_0 += 6*c3p5*ss[16];
  jac_2_0 += 6*c5p5*ss[26];
  TACC jac_2_1 = 120*c1p1*c3p1*c5p3*ss[22];
  jac_2_1 += 180*c1p1*c3p2*c5p2*ss[20];
  jac_2_1 += 120*c1p1*c3p3*c5p1*ss[18];
  jac_2_1 += 180*c1p2*c3p1*c5p2*ss[18];
  jac_2_1 += 180*c1p2*c3p2*c5p1*ss[16];
  jac_2_1 += 120*c1p3*c3p1*c5p1*ss[14];
  jac_2_1 += 30*c1p1*c3p4*ss[16];
  jac_2_1 += 30*c1p1*c5p4*ss[24];
  jac_2_1 += 60*c1p2*c3p3*ss[14];
  jac_2_1 += 60*c1p2*c5p3*ss[20];
  jac_2_1 += 60*c1p3*c3p2*ss[12];
  jac_2_1 += 60*c1p3*c5p2*ss[16];
  jac_2_1 += 30*c1p4*c3p1*ss[10];
  jac_2_1 += 30*c1p4*c5p1*ss[12];
  jac_2_1 += 30*c3p1*c5p4*ss[26];
  jac_2_1 += 60*c3p2*c5p3*ss[24];
  jac_2_1 += 60*c3p3*c5p2*ss[22];
  jac_2_1 += 30*c3p4*c5p1*ss[20];
  jac_2_1 += 6*c1p5*ss[8];
  jac_2_1 += 6*c3p5*ss[18];
  jac_2_1 += 6*c5p5*ss[28];
  TACC jac_2_2 = 120*c1p1*c3p1*c5p3*ss[24];
  jac_2_2 += 180*c1p1*c3p2*c5p2*ss[22];
  jac_2_2 += 120*c1p1*c3p3*c5p1*ss[20];
  jac_2_2 += 180*c1p2*c3p1*c5p2*ss[20];
  jac_2_2 += 180*c1p2*c3p2*c5p1*ss[18];
  jac_2_2 += 120*c1p3*c3p1*c5p1*ss[16];
  jac_2_2 += 30*c1p1*c3p4*ss[18];
  jac_2_2 += 30*c1p1*c5p4*ss[26];
  jac_2_2 += 60*c1p2*c3p3*ss[16];
  jac_2_2 += 60*c1p2*c5p3*ss[22];
  jac_2_2 += 60*c1p3*c3p2*ss[14];
  jac_2_2 += 60*c1p3*c5p2*ss[18];
  jac_2_2 += 30*c1p4*c3p1*ss[12];
  jac_2_2 += 30*c1p4*c5p1*ss[14];
  jac_2_2 += 30*c3p1*c5p4*ss[28];
  jac_2_2 += 60*c3p2*c5p3*ss[26];
  jac_2_2 += 60*c3p3*c5p2*ss[24];
  jac_2_2 += 30*c3p4*c5p1*ss[22];
  jac_2_2 += 6*c1p5*ss[10];
  jac_2_2 += 6*c3p5*ss[20];
  jac_2_2 += 6*c5p5*ss[30];
  roots[0]=root_0;
  roots[1]=root_1;
  roots[2]=root_2;
  jacobian[0] = jac_0_0;
  jacobian[1] = jac_0_1;
  jacobian[2] = jac_0_2;
  jacobian[3] = jac_1_0;
  jacobian[4] = jac_1_1;
  jacobian[5] = jac_1_2;
  jacobian[6] = jac_2_0;
  jacobian[7] = jac_2_1;
  jacobian[8] = jac_2_2;
}
}; // SymmetricQuinticSystem

}; // systems
}; // random
}; // flopoco

#endif
