#ifndef random__utils_find_roots_gsl_hpp
#define random__utils_find_roots_gsl_hpp

#include "gsl_utils.hpp"

#include "gsl/gsl_multiroots.h"

#include <stdexcept>

namespace flopoco
{
namespace random
{

namespace detail
{
	
	template<class TF,class T=typename TF::real_t>
	struct GSLFindRootsWrapper
	{
		typedef T real_t;
		
		const TF &m_f;
		unsigned m_arity;
		
		gsl_multiroot_function_fdf m_fdf;
		
		GSLFindRootsWrapper(const TF &f)
			: m_f(f)
			, m_arity(f.arity())
		{
			m_fdf.f=StaticRoots;
			m_fdf.df=StaticJacobian;
			m_fdf.fdf=StaticRootsAndJacobian;
			m_fdf.n=m_arity;
			m_fdf.params=this;
		}
		
		static GSLFindRootsWrapper *ToThis(void *params)
		{ return (GSLFindRootsWrapper*)params; }
		
		void Roots(const gsl_vector *X,  gsl_vector *R) const
		{
			std::vector<real_t> vX(FromGSL<real_t>(X));
			std::vector<real_t> vR(m_arity);
			m_f.roots(&vX[0], &vR[0]);
			ToGSL(R, vR);
		}
		
		static int StaticRoots(const gsl_vector *X, void *params, gsl_vector *R)
		{ ToThis(params)->Roots(X,R); return 0; }
		
		void RootsAndJacobian(const gsl_vector *X, gsl_vector *R, gsl_matrix *J) const
		{
			std::vector<real_t> vX(FromGSL<real_t>(X));
			std::vector<real_t> vR(m_arity), vJ(m_arity*m_arity);
			m_f.roots_and_jacobian(&vX[0], &vR[0], &vJ[0]);
			ToGSL(R, vR);
			ToGSL(J, vJ);
		}
		
		void Jacobian(const gsl_vector *X, gsl_matrix *J) const
		{
			std::vector<real_t> vX(m_arity);
			std::vector<real_t> vR(m_arity), vJ(m_arity*m_arity);
			m_f.roots_and_jacobian(&vX[0], &vR[0], &vJ[0]);
			ToGSL(J, vJ);
		}
		
		static int StaticRootsAndJacobian(const gsl_vector *X, void *params, gsl_vector *R, gsl_matrix *J)
		{ ToThis(params)->RootsAndJacobian(X, R, J); return 0;}
		
		static int StaticJacobian(const gsl_vector *X, void *params, gsl_matrix *J)
		{ ToThis(params)->Jacobian(X, J); return 0;}
		
		
	};
};

template<class TF,class T>
std::pair<std::vector<T>,std::vector<T> > FindRootsGSL(
	const TF &f,
	const T *guess,
	const gsl_multiroot_fdfsolver_type *solveType= gsl_multiroot_fdfsolver_hybridsj
){
	boost::shared_ptr<gsl_multiroot_fdfsolver> fsolve(
		gsl_multiroot_fdfsolver_alloc(solveType, f.arity()),
		gsl_multiroot_fdfsolver_free
	);
	if(fsolve==0)
		throw std::logic_error("Couldn't allocate GSL solver.");
	
	boost::shared_ptr<gsl_vector> gslGuess=detail::ToGSL(guess, guess+f.arity());

	detail::GSLFindRootsWrapper<TF> wrapper(f);
	
	gsl_multiroot_fdfsolver_set(fsolve.get(), &wrapper.m_fdf, gslGuess.get());
	
	while(true){
		int code=gsl_multiroot_fdfsolver_iterate(fsolve.get());
		if((code==GSL_ENOPROG) || (code==GSL_ENOPROGJ))
			break;
		if(code!=0)
			throw std::logic_error("Error while trying to find function roots using GSL.");
	}
	
	return std::make_pair(
		detail::FromGSL<T>(gsl_multiroot_fdfsolver_root(fsolve.get())),
		detail::FromGSL<T>(gsl_multiroot_fdfsolver_f(fsolve.get()))
	);
}

template<class TF>
std::pair<std::vector<typename TF::real_t>,typename TF::real_t> FindRootsGSL(
	const TF &f
){
	std::vector<typename TF::real_t> guess(f.arity());
	f.DefaultGuess(&guess[0]);
	return FindRootsGSL(f, &guess[0]);
}

}; // random
}; // flopoco


#endif
