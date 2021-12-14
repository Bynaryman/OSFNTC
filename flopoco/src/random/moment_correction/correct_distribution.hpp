#ifndef random_moment_correction_correct_distribution_hpp
#define random_moment_correction_correct_distribution_hpp

#include "random/utils/gsl_utils.hpp"
#include "random/utils/minimise_gsl.hpp"
#include "random/utils/find_roots_gsl.hpp"
#include "random/distributions/distribution.hpp"

#include "random/utils/conversions.hpp"

#include "random/moment_correction/systems/cubic_system.hpp"
#include "random/moment_correction/systems/symmetric_cubic_system.hpp"
#include "random/moment_correction/systems/quintic_system.hpp"
#include "random/moment_correction/systems/symmetric_quintic_system.hpp"
#include "random/moment_correction/systems/symmetric_heptic_system.hpp"
#include "random/moment_correction/systems/symmetric_nonic_system.hpp"

#ifdef HAVE_NTL
#include "NTL/quad_float.h"
#endif

namespace flopoco
{
namespace random
{	

template<class T>
std::vector<T> FindLinearPolynomialCorrection(
	typename Distribution<T>::TypePtr current,
	typename Distribution<T>::TypePtr target
){
	T currMean=current->StandardMoment(1), targMean=target->StandardMoment(1);
	T currStd=current->StandardMoment(2), targStd=target->StandardMoment(2);
	
	// If we have (mu_c,std_c) and (mu_t,std_t),
	// a decent correction is:
	// ((x-mu_c)/std_c) * std_t + mu_t
	T scale=sqrt(targStd/currStd);
	std::vector<T> res(2);
	res[0]=targMean-scale*currMean;
	res[1]=scale;
	return res;
}	
	
template<class T>
std::vector<T> FindSymmetricCubicPolynomialCorrection(
	typename Distribution<T>::TypePtr current,
	typename Distribution<T>::TypePtr target
){
	if(!current->IsSymmetric())
		throw std::logic_error("FindSymmetricCubicPolynomialCorrection - Source is not symmetric");
	if(!target->IsSymmetric())
		throw std::logic_error("FindSymmetricCubicPolynomialCorrection - Target is not symmetric");
	
	std::vector<T> ssCurrent(13), ssTarget(5);
	for(int i=0;i<=12;i++)
		ssCurrent[i]=current->CentralMoment(i);
	for(int i=0;i<=4;i++)
		ssTarget[i]=target->CentralMoment(i);
	
	systems::SymmetricCubicSystem<T> sys(&ssCurrent[0], &ssTarget[0]);
	
	// Look for an initial guess, and give up if there is nothing good
	std::pair<std::vector<T>,T> mm=MinimiseGSL(sys);
	if(mm.second > 1e-8)
		return std::vector<T>();
	
	// Try to polish down
	std::pair<std::vector<T>,std::vector<T> > rr=FindRootsGSL(sys, &mm.first[0]);
	
	std::vector<T> res;
	res.push_back(current->RawMoment(1)-target->RawMoment(1));
	res.push_back(rr.first[0]);
	res.push_back(0);
	res.push_back(rr.first[1]);
	return res;
}

template<class T>
std::vector<T> FindSymmetricQuinticPolynomialCorrection(
	typename Distribution<T>::TypePtr current,
	typename Distribution<T>::TypePtr target
){
	if(!current->IsSymmetric())
		throw std::logic_error("FindSymmetricQuinticPolynomialCorrection - Source is not symmetric");
	if(!target->IsSymmetric())
		throw std::logic_error("FindSymmetricQuinticPolynomialCorrection - Target is not symmetric");
	
	std::vector<T> ssCurrent(31), ssTarget(7);
	for(int i=0;i<=30;i++)
		ssCurrent[i]=current->RawMoment(i);
	for(int i=0;i<=6;i++)
		ssTarget[i]=target->RawMoment(i);
	
	systems::SymmetricQuinticSystem<T> sys(&ssCurrent[0], &ssTarget[0]);
	
	std::pair<std::vector<T>,T> mm=MinimiseGSL(sys);
	
	std::pair<std::vector<T>,std::vector<T> > rr=FindRootsGSL(sys, &mm.first[0]);
	
	std::vector<T> res;
	res.push_back(current->RawMoment(1)-target->RawMoment(1));
	res.push_back(rr.first[0]);
	res.push_back(0);
	res.push_back(rr.first[1]);
	res.push_back(0);
	res.push_back(rr.first[2]);
	return res;
}

template<class T>
std::vector<T> FindSymmetricHepticPolynomialCorrection(
	typename Distribution<T>::TypePtr current,
	typename Distribution<T>::TypePtr target
){
	if(!current->IsSymmetric())
		throw std::logic_error("FindSymmetricHepticPolynomialCorrection - Source is not symmetric");
	if(!target->IsSymmetric())
		throw std::logic_error("FindSymmetricHepticPolynomialCorrection - Target is not symmetric");
	
	std::vector<T> ssCurrent(57), ssTarget(9);
	for(int i=0;i<=56;i++)
		ssCurrent[i]=current->RawMoment(i);
	for(int i=0;i<=8;i++)
		ssTarget[i]=target->RawMoment(i);
	
	systems::SymmetricHepticSystem<T,NTL::quad_float> sys(&ssCurrent[0], &ssTarget[0]);
	
	std::pair<std::vector<T>,T> mm=MinimiseGSL(sys);
	
	std::pair<std::vector<T>,std::vector<T> > rr=FindRootsGSL(sys, &mm.first[0]);
	
	std::vector<T> res;
	res.push_back(current->RawMoment(1)-target->RawMoment(1));
	res.push_back(rr.first[0]);
	res.push_back(0);
	res.push_back(rr.first[1]);
	res.push_back(0);
	res.push_back(rr.first[2]);
	res.push_back(0);
	res.push_back(rr.first[3]);
	return res;
}

template<class T>
std::vector<T> FindSymmetricNonicPolynomialCorrection(
	typename Distribution<T>::TypePtr current,
	typename Distribution<T>::TypePtr target
){
	if(!current->IsSymmetric())
		throw std::logic_error("FindSymmetricNonicPolynomialCorrection - Source is not symmetric");
	if(!target->IsSymmetric())
		throw std::logic_error("FindSymmetricNonicPolynomialCorrection - Target is not symmetric");
	
	std::vector<T> ssCurrent(91), ssTarget(11);
	for(int i=0;i<=90;i++)
		ssCurrent[i]=current->RawMoment(i);
	for(int i=0;i<=10;i++)
		ssTarget[i]=target->RawMoment(i);
	
	systems::SymmetricNonicSystem<T,NTL::quad_float> sys(&ssCurrent[0], &ssTarget[0]);
	
	std::pair<std::vector<T>,T> mm=MinimiseGSL(sys);
	
	std::pair<std::vector<T>,std::vector<T> > rr=FindRootsGSL(sys, &mm.first[0]);
	
	std::vector<T> res;
	res.push_back(current->RawMoment(1)-target->RawMoment(1));
	res.push_back(rr.first[0]);
	res.push_back(0);
	res.push_back(rr.first[1]);
	res.push_back(0);
	res.push_back(rr.first[2]);
	res.push_back(0);
	res.push_back(rr.first[3]);
	res.push_back(0);
	res.push_back(rr.first[4]);
	return res;
}

template<class T>
std::vector<T> FindCubicPolynomialCorrection(
	typename Distribution<T>::TypePtr current,
	typename Distribution<T>::TypePtr target
){
	std::vector<T> ssCurrent(13), ssTarget(5);
	for(int i=0;i<=12;i++)
		ssCurrent[i]=current->RawMoment(i);
	for(int i=0;i<=4;i++)
		ssTarget[i]=target->RawMoment(i);
	
	systems::CubicSystem<T> sys(&ssCurrent[0], &ssTarget[0]);
	
	std::pair<std::vector<T>,T> mm=MinimiseGSL(sys);
	if(mm.second > 1e-8)
		return std::vector<T>();
	
	return FindRootsGSL(sys, &mm.first[0]).first;
}

template<class T>
std::vector<T> FindQuinticPolynomialCorrection(
	typename Distribution<T>::TypePtr current,
	typename Distribution<T>::TypePtr target
){
	std::vector<T> ssCurrent(31), ssTarget(7);
	for(int i=0;i<=30;i++)
		ssCurrent[i]=current->RawMoment(i);
	for(int i=0;i<=6;i++)
		ssTarget[i]=target->RawMoment(i);
	
	systems::QuinticSystem<T,NTL::quad_float> sys(&ssCurrent[0], &ssTarget[0]);
	
	// Look for an initial point, and give up if there is nothing good
	
	// Use the difference in mean as an initial guess. So if we have (mu_c,std_c) and (mu_t,std_t),
	// a decent correction is:
	// ((x-mu_c)/std_c) * std_t + mu_t
	std::vector<T> guess(6,0);
	T scale=sqrt(ssTarget[2]/ssCurrent[2]);
	guess[0]=ssTarget[1]-scale*ssCurrent[0];
	guess[1]=scale;
	
	std::pair<std::vector<T>,T> mm=MinimiseGSL(sys, &guess[0]);
	
	// Try to polish down
	std::pair<std::vector<T>,std::vector<T> > rr=FindRootsGSL(sys, &mm.first[0]);
	
	return rr.first;
}

template<class T>
std::vector<T> FindPolynomialCorrection(
	typename Distribution<T>::TypePtr current,
	typename Distribution<T>::TypePtr target,
	unsigned degree
){
	if(degree<1)
		throw std::logic_error("FindPolynomialCorrection - Correction degree must be at least one.");
	
	if(degree==1)
		return FindLinearPolynomialCorrection<T>(current, target);
	
	if(current->IsSymmetric() && target->IsSymmetric()){
		if(degree==3){
			return FindSymmetricCubicPolynomialCorrection<T>(current, target);
		}else if(degree==5){
			return FindSymmetricQuinticPolynomialCorrection<T>(current, target);
		}else if(degree==7){
			return FindSymmetricHepticPolynomialCorrection<T>(current, target);
		}else if(degree==9){
			return FindSymmetricNonicPolynomialCorrection<T>(current, target);
		}else{
			throw std::logic_error("FindPolynomialCorrection - Only symmetric corrections with odd degree up to 9 are currently supported.");
		}
	}else{
		if(degree==3){
			return FindCubicPolynomialCorrection<T>(current, target);
		}else if(degree==5){
			return FindQuinticPolynomialCorrection<T>(current, target);
		}else{
			throw std::logic_error("FindPolynomialCorrection - Only asymmetric corrections with odd degree up to 5 are supported.");
		}
	}
}

}; // random
}; // flopoco

#endif
