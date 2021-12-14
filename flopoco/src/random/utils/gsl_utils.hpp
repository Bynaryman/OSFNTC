#ifndef random_utils_gsl_utils_hpp
#define random_utils_gsl_utils_hpp

#include "gsl/gsl_vector.h"
#include "gsl/gsl_matrix.h"
#include <vector>
#include <stdexcept>
#include <boost/smart_ptr.hpp>

namespace flopoco
{
namespace random
{
namespace detail
{
	template<class T>
	std::vector<T> FromGSL(const gsl_vector *vector)
	{
		if(vector->stride==1){
			return std::vector<T>(vector->data, vector->data+vector->size);
		}else{
			std::vector<T> res(vector->size);
			for(unsigned i=0;i<vector->size;i++){
				res[i]=vector->data[i*vector->stride];
			}
			return res;
		}
	}

	template<class T>
	boost::shared_ptr<gsl_vector> ToGSL(const std::vector<T> &x)
	{
		boost::shared_ptr<gsl_vector> res(
			gsl_vector_alloc(x.size()),
			gsl_vector_free
		);
		std::copy(x.begin(), x.end(), res->data);
		return res;
	}
	
	template<class T>
	boost::shared_ptr<gsl_vector> ToGSL(const T *begin, const T *end)
	{
		boost::shared_ptr<gsl_vector> res(
			gsl_vector_alloc(end-begin),
			gsl_vector_free
		);
		std::copy(begin, end, res->data);
		return res;
	}
	
	template<class T>
	void ToGSL(gsl_vector *dst, T begin, T end)
	{
		if(dst->size!=(size_t)std::distance(begin,end))
			throw std::logic_error("ToGSL - Destination vector has wrong size.");
		if(dst->stride==1){
			std::copy(begin, end, dst->data);
		}else{
			double *d=dst->data;
			while(begin!=end){
				*d=*begin;
				d+=dst->stride;
				begin++;
			}
		}
	}
	
	template<class T>
	void ToGSL(gsl_vector *dst, const T &container)
	{
		ToGSL(dst, container.begin(), container.end());
	}
	
	template<class T>
	void ToGSL(gsl_matrix *dst, T begin, T end)
	{
		if(dst->size1*dst->size2 != (size_t)std::distance(begin,end))
			throw std::logic_error("ToGSL - Destination matrix has wrong size.");
		if(dst->tda==dst->size2){
			std::copy(begin, end, dst->data);
		}else{
			for(unsigned i=0;i<dst->size1;i++){
				std::copy(begin, begin+dst->size2, dst->data+i*dst->size2);
				begin = begin+dst->size2;
			}
		}
	}
	
	template<class T>
	void ToGSL(gsl_matrix *dst, const T &x)
	{
		ToGSL(dst, x.begin(), x.end());
	}
}; //detail
}; // random
}; // flopoco

#endif
