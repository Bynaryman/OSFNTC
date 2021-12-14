#ifndef random_base_distribution_hp
#define random_base_distribution_hp

#include <utility>
#include <stdexcept>
#include <vector>

#include <boost/smart_ptr.hpp>

#include "moment_conversions.hpp"

namespace flopoco
{
namespace random
{

template<class T>
class Distribution
{
public:
	virtual ~Distribution()
	{}
		
	//! k==1 -> mean, k==2 -> stddev, k==3 -> skewness, k==4 -> kurtosis, etc.
	virtual T StandardMoment(unsigned k) const=0;
		
	//! k==1 -> 0, k==2 -> variance, k==3 -> skewness * stddev^3, k==4 -> kurtosis * stddev^4
	virtual T CentralMoment(unsigned k) const
	{
		if(k==0) return 1;
		if(k==1) return 0;
		if(k==2) return pow(StandardMoment(2),2);
		if((k%2) && IsSymmetric()) return 0;
		return StandardMoment(k) * pow(StandardMoment(2),k);
	}
		
	virtual T RawMoment(unsigned k) const
	{
		// The default implementation isn't pretty, but it shouldn't be on the critical path
		std::vector<T> central(k+1);
		for(unsigned i=0;i<k+1;i++)
			central[i]=CentralMoment(i);
		return CentralMomentsToRawMoment(k, StandardMoment(1), &central[0]);
	}
		
	//! Return true if the distribution is definitely symmetric about the mean, false if it is not known.
	virtual bool IsSymmetric() const=0;
		
	virtual std::pair<T,T> Support() const=0;
		
	typedef boost::shared_ptr<Distribution> TypePtr;
};

template<class T>
class ContinuousDistribution
	: public Distribution<T>
{
public:
	virtual T Pdf(const T &x) const=0;
	virtual T Cdf(const T &x) const=0;
	virtual T InvCdf(const T &x) const=0;	

	typedef boost::shared_ptr<ContinuousDistribution> TypePtr;
};


template<class T>
class DiscreteDistribution
	: public Distribution<T>
{
public:
	virtual T Pmf(const T &x) const=0;
};

template<class T>
class EnumerableDistribution
	: public DiscreteDistribution<T>
{
public:
	virtual uint64_t ElementCount() const=0;

	// All legal distributions contain at least one element. Elements are returned as (x,p)
	virtual std::pair<T,T> GetElement(uint64_t index) const=0;

	void GetElements(uint64_t begin, uint64_t end, std::pair<T,T> *dest) const
	{
		if((end>begin) || (end>=ElementCount()))
			throw std::range_error("Requested elements are out of range.");
		while(begin!=end){
			dest[begin]=GetElement(begin);
			begin++;
		}
	}
};

}; // random
}; // flopoco

#endif
