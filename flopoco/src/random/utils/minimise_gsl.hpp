#ifndef random__utils_minimise_gsl_hpp
#define random__utils_minimise_gsl_hpp

#include "gsl_utils.hpp"

#include "gsl/gsl_multimin.h"

#include <stdexcept>
#include <iostream>

namespace flopoco
{
namespace random
{

namespace detail
{
	
	template<class TF,class T=typename TF::real_t>
	struct GSLMinimiseWrapper
	{
		typedef T real_t;
		
		const TF &m_f;
		unsigned m_arity;
		
		gsl_multimin_function_fdf m_fdf;
		
		GSLMinimiseWrapper(const TF &f)
			: m_f(f)
			, m_arity(f.arity())
		{
			m_fdf.f=StaticMetric;
			m_fdf.df=StaticGradient;
			m_fdf.fdf=StaticMetricAndGradient;
			m_fdf.n=m_arity;
			m_fdf.params=this;
		}
		
		static GSLMinimiseWrapper *ToThis(void *params)
		{ return (GSLMinimiseWrapper*)params; }
		
		double Metric(const gsl_vector *X)
		{
			std::vector<real_t> vX(FromGSL<real_t>(X));
			return m_f.metric(&vX[0]);
		}
		
		static double StaticMetric(const gsl_vector *X, void *params)
		{ return ToThis(params)->Metric(X); }
		
		double MetricAndGradient(const gsl_vector *X, gsl_vector *G)
		{
			std::vector<real_t> vX(FromGSL<real_t>(X));
			std::vector<real_t> vG(m_arity);
			real_t res=m_f.metric_and_gradient(&vX[0], &vG[0]);
			std::copy(&vG[0], &vG[m_arity], G->data);
			return res;
		}
		
		static void StaticMetricAndGradient(const gsl_vector *X, void *params, double *M, gsl_vector *G)
		{ *M=ToThis(params)->MetricAndGradient(X, G); }
		
		static void StaticGradient(const gsl_vector *X, void *params, gsl_vector *G)
		{ ToThis(params)->MetricAndGradient(X, G); }
		
		
	};
};

template<class TF,class T>
std::pair<std::vector<T>,T> MinimiseGSL(
	const TF &f,
	const T *guess,
	const gsl_multimin_fdfminimizer_type *minType=gsl_multimin_fdfminimizer_vector_bfgs,
	double stepSize=0.1,
	double tol=0.01
){
	boost::shared_ptr<gsl_multimin_fdfminimizer> fmin(
		gsl_multimin_fdfminimizer_alloc(minType, f.arity()),
		gsl_multimin_fdfminimizer_free
	);
	if(fmin==0)
		throw std::logic_error("Couldn't allocate GSL minimiser.");
	
	boost::shared_ptr<gsl_vector> gslGuess=detail::ToGSL(guess, guess+f.arity());

	detail::GSLMinimiseWrapper<TF> wrapper(f);
	
	gsl_multimin_fdfminimizer_set(fmin.get(), &wrapper.m_fdf, gslGuess.get(), stepSize, tol);
	
	while(true){
		int code=gsl_multimin_fdfminimizer_iterate(fmin.get());
		if(code==GSL_ENOPROG)
			break;
		if(code!=0)
			throw std::logic_error("Error while trying to minimise function using GSL.");
	}
	
	return std::make_pair(
		detail::FromGSL<T>(gsl_multimin_fdfminimizer_x(fmin.get())),
		gsl_multimin_fdfminimizer_minimum(fmin.get())
	);
}

template<class TF>
std::pair<std::vector<typename TF::real_t>,typename TF::real_t> MinimiseGSL(
	const TF &f
){
	std::vector<typename TF::real_t> guess(f.arity());
	f.DefaultGuess(&guess[0]);
	return MinimiseGSL(f, &guess[0]);
}

}; // random
}; // flopoco


#endif
