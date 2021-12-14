#ifndef random_distributions_make_table_approximation_hpp
#define random_distributions_make_table_approximation_hpp

#include "table_distribution.hpp"
#include "moment_conversions.hpp"

namespace flopoco
{
namespace random
{

template<class T>
typename TableDistribution<T>::TypePtr MakeTableApproximation(
	typename ContinuousDistribution<T>::TypePtr target,
	unsigned n
){
	std::vector<T> elts(n);
	double targetMean=target->StandardMoment(1);
	
	T half=0.5;
	if(target->IsSymmetric()){
		for(unsigned i=0;i<n/2;i++){
			T e=target->InvCdf((i+half)/n);
			elts[i]=e;
			elts[n-i-1]=2*targetMean-e;
		}
		if(n%2)
			elts[n/2]=target->InvCdf(0.5);
	}else{
		for(unsigned i=0;i<n/2;i++){
			T e=target->InvCdf((i+half)/n);
			elts[i]=e;
		}
	}
	
	return boost::make_shared<TableDistribution<T> >(&elts[0], &elts[n]);
}	

}; // random
}; // flopoco

#endif
