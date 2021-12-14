#ifndef flopoco_random_table_approx_gaussian_distribution_hpp
#define flopoco_random_table_approx_gaussian_distribution_hpp

#include <boost/math/distributions/normal.hpp>

#include "distribution.hpp"

namespace flopoco
{
namespace random
{

template<class T>
class GaussianDistribution
	: public ContinuousDistribution<T>
{
private:	
	boost::math::normal_distribution<T> m_dist;
public:
	GaussianDistribution(T mean=0.0, T stddev=1.0)
		: m_dist(mean, stddev)
	{}

	//! k==1 -> mean, k==2 -> stddev, k==3 -> skewness, k==4 -> kurtosis, etc.
	T StandardMoment(unsigned k) const
	{
		if(k==0) return 1.0;
		if(k==1) return boost::math::mean(m_dist);
		if(k==2) return boost::math::standard_deviation(m_dist);
		if(k%2) return 0.0;
		T acc;
		acc=1.0;
		for(unsigned i=4;i<=k;i+=2){
			acc=acc*(i-1);
		}
		return acc;
	}
	
	T CentralMoment(unsigned k) const
	{
		if(k==0) return 1.0;
		if(k==1) return 0.0;
		if(k==2) return boost::math::variance(m_dist);
		if(k%2) return 0.0;
		return StandardMoment(k) * pow(boost::math::standard_deviation(m_dist), k);
	}
	
	bool IsSymmetric() const
	{ return true; }
		
	std::pair<T,T> Support() const
	{
		return boost::math::support(m_dist);
	}

	T Pdf(const T &x) const
	{ return boost::math::pdf(m_dist,x); }
	
	T Cdf(const T &x) const
	{ return boost::math::cdf(m_dist,x); }
		
	T InvCdf(const T &x) const
	{ return boost::math::quantile(m_dist, x); }
		
	typedef boost::shared_ptr<GaussianDistribution> GaussianDistributionPtr;
};

}; // random
}; // flopoco

#endif
