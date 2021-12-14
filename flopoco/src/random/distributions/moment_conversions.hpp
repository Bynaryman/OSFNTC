#ifndef random_base_moment_conversions_hpp
#define random_base_moment_conversions_hpp

#include <boost/math/special_functions/binomial.hpp>

template<class T>
T RawMomentsToCentralMoment(
	unsigned k,
	const T *raw
){
	if(k==0)
		return 1;
	if(k==1)
		return 0;
	if(raw[1]==0)
		return raw[k];	// no conversion if mean is zero
	//fprintf(stderr, "mom[%d]= raw[%d]", k, k);
	T acc=raw[k];
	T mu1=raw[1], mu1pow=1;
	int dir=+1;
	for(int i=k-1;i>=0;i--){
		mu1pow*=mu1;
		dir=-dir;
		T coeff=boost::math::binomial_coefficient<T>(k,i);
		acc += dir * raw[i] *coeff * mu1pow;
		//fprintf(stderr, " %s raw[i] 
	}
	return acc;
}

template<class T>
T CentralMomentsToRawMoment(
	unsigned k,
	double mean,
	const T *central
){
	if(k==0)
		return 1;
	if(k==1)
		return mean;
	if(mean==0)
		return central[k];
	
	// raw[k] = bin(k,k) * central[k] * mean^0 + bin(k,k-1) * central[k-1] * mean + bin(k,k-2) * central[k-2] * mean^2 + ... + bin(k,1) * central[1] * mean^(k-1) + bin(k,0) * central[0] * mean^k
	//  = central[k] + bin(k,k-1)*centra[k-1] * mean + ... + bin(k,2) * central[2] * mean^(k-2) + mean^k
	
	T acc=central[k];
	T mu1pow=1;	
	for(int i=k-1;i>1;i--){
		mu1pow*=mean;
		T coeff=boost::math::binomial_coefficient<T>(k,i);
		acc += central[i] *coeff * mu1pow;
		//fprintf(stderr, " %s raw[i] 
	}
	return acc + mu1pow * mean*mean;
}

#endif
