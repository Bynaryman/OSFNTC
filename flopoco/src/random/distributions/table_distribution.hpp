#ifndef random_base_table_distribution_hpp
#define random_base_table_distribution_hpp

#include "distribution.hpp"
#include "moment_conversions.hpp"

#include <vector>
#include <algorithm>
#include <numeric>

namespace flopoco
{
namespace random
{

template<class T>
class TableDistribution
	: public EnumerableDistribution<T>
{
public:	
	typedef boost::shared_ptr<TableDistribution> TypePtr;
private:
	bool m_isSymmetric;

	// Elements are sorted as (x,p). Repeated x is allowed. Must have 0<=p<=1, so zero probability elements are allowed.
	typedef std::vector<std::pair<T,T> > storage_t;
	storage_t m_elements;
	
	mutable std::vector<T> m_rawMoments;

	// First sort on x, then sort on decreasing p if x<0 or increasing p if x>0
	static bool EltLessThan(const std::pair<T,T> &a, const std::pair<T,T> &b)
	{
		if(a.first<b.first)
			return true;
		if(a.first>b.first)
			return false;
		if(a.first < 0)
			return a.second<b.second;
		else
			return a.second>b.second;
	};
	
	static bool ProbLessThan(const std::pair<T,T> &a, const std::pair<T,T> &b)
	{
		return a.second<b.second;
	};
	
	void CompleteInit(bool sorted)
	{
		unsigned n=m_elements.size();
		
		if(!sorted)
			std::sort(m_elements.begin(), m_elements.end(), EltLessThan);

		if((n%2) && m_elements[n/2].first!=0)
			m_isSymmetric=false;
		if(m_isSymmetric){
			for(unsigned i=0;i<n/2 && m_isSymmetric;i++){
				if(m_elements[i].first!=-m_elements[n-i-1].first)
					m_isSymmetric=false;
				if(m_elements[i].second!=m_elements[n-i-1].second)
					m_isSymmetric=false;
			}
		}
	}
public:

	template<class TC>
	TableDistribution(const TC &src)
		: m_isSymmetric(true)
	{
		if(src.size()==0)
			throw std::invalid_argument("TableDistribution - Table must contain at least one element.");
		
		m_elements.assign(src.begin(), src.end());
		std::sort(m_elements.begin(), m_elements.end(), ProbLessThan);
		T acc=0;
		for(unsigned i=0;i<src.size();i++){
			if(m_elements[i].second < 0)
				throw std::logic_error("TableDistribution - Element probability is less than 0.");
			acc += m_elements[i].second;
		}
		if(fabs(acc-1)>1e-12)
			throw std::logic_error("TableDistribution - Element probabilities are more than 1e-12 from one.");
		
		CompleteInit(false);
	}

	TableDistribution(const T *begin, const T *end)
		: m_isSymmetric(true)
	{
		if(end<=begin)
			throw std::invalid_argument("TableDistribution - Table must contain at least one element.");
		
		size_t n=end-begin;
		
		m_elements.reserve(n);
		T p=1.0;
		p=p/n;
		bool sorted=true;
		for(int i=0;i<(end-begin);i++){
			m_elements.push_back(std::make_pair(begin[i],p));
			if(i!=0)
				sorted=sorted && EltLessThan(*(m_elements.end()-1), m_elements.back());
		}

		CompleteInit(sorted);
	}
	
	T RawMoment(unsigned k) const
	{
		if(k>=m_rawMoments.size()){
			std::vector<T> tmp(m_elements.size());
			while(k>=m_rawMoments.size()){
				if((k%2) && m_isSymmetric){
					m_rawMoments.push_back(0.0);
				}else{
					for(unsigned i=0;i<m_elements.size();i++){
						tmp[i]=m_elements[i].second*pow(m_elements[i].first,m_rawMoments.size());
					}
					std::sort(tmp.begin(), tmp.end());
					m_rawMoments.push_back(std::accumulate(tmp.begin(),tmp.end(),(T)0.0));
				}
			}
		}
		return m_rawMoments[k];
	}

	virtual T StandardMoment(unsigned k) const
	{
		if(k==0)
			return 1.0;
		if(k==1)
			return RawMoment(k);
		if(k==2)
			return CentralMoment(k);
		return CentralMoment(k) / pow(CentralMoment(2), k/2.0);
	}
	
	T CentralMoment(unsigned k) const
	{
		RawMoment(k);	// force calculate if necessary
		return RawMomentsToCentralMoment(k, &m_rawMoments[0]);
	}
	
	virtual bool IsSymmetric() const
	{ return m_isSymmetric; } // this is more strict than symmetric about the mean, but still is valid
		
	virtual std::pair<T,T> Support() const
	{ return std::make_pair(m_elements.front().first, m_elements.back().second); }
		
	virtual T Pmf(const T &x) const
	{
		// Elements must have non-negative probability, so we always have (x-eps,p) < (x,-1) < (x,p)
		typename storage_t::const_iterator it=std::lower_bound(m_elements.begin(), m_elements.end(), std::pair<T,T>(x,-1.0));
		T acc=0.0;
		while( it!=m_elements.end() ? it->first==x : false ){
			acc+=it->second;
			++it;
		}
		return acc;
	}
	
	virtual T Cdf(const T &x) const
	{
		if(x>=m_elements.back().first)
			return 1.0;
		T acc=0.0;
		for(unsigned i=0;i<m_elements.size();i++){
			if(x<m_elements[i].first)
				return acc;
			acc+=m_elements[i].second;
		}
		assert(0);
	}

	virtual uint64_t ElementCount() const
	{ return m_elements.size(); }

	// All legal distributions contain at least one element.
	virtual std::pair<T,T> GetElement(uint64_t index) const
	{ return m_elements.at(index); }

	void GetElements(uint64_t begin, uint64_t end, std::pair<T,T> *dest) const
	{
		if((end>begin) || (end>=ElementCount()))
			throw std::range_error("Requested elements are out of range.");
		std::copy(m_elements.begin()+begin, m_elements.end()+end, dest);
	}
	
	TypePtr ApplyPolynomial(const std::vector<T> &poly) const
	{
		storage_t elts(m_elements.begin(), m_elements.end());
		for(unsigned i=0;i<elts.size();i++){
			T xx=elts[i].first;
			T acc=poly.back();
			for(int j=(int)poly.size()-2;j>=0;j--){
				acc=poly[j] + acc * xx;
			}
			elts[i].first=acc;
		}
		return boost::make_shared<TableDistribution>(elts);
	}
};

}; // random
}; // flopoco

#endif
