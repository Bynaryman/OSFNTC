#include "find_roots_gsl.hpp"
#include "minimise_gsl.hpp"

#define BOOST_TEST_MODULE BaseTests
#include <boost/test/unit_test.hpp>

struct poly1_t
{
	typedef double real_t;
	
	unsigned arity() const
	{ return 1; }
	
	void DefaultGuess(double *params) const
	{params[0]=0;}

	double metric(const double *params) const
	{ return pow(params[0]-3.14159265358979,2); }
	
	double metric_and_gradient(const double *params, double *G) const
	{
		G[0]=2*params[0]-2*3.14159265358979;
		return metric(params);
	}
};

BOOST_AUTO_TEST_CASE(MinimiseGSLTests_poly1)
{
	poly1_t target;
	std::pair<std::vector<double >,double> m=flopoco::random::MinimiseGSL(target);
	std::cerr<<"poly1 : root="<<m.first[0]<<", error="<<m.first[0]-3.14159265358979<<", metric="<<m.second<<"\n";
	BOOST_CHECK(abs(m.first[0]-3.14159265358979) < 1e-8);
}


struct test2_t
{
	typedef double real_t;
	
	unsigned arity() const
	{ return 2; }
	
	void DefaultGuess(double *params) const
	{params[0]=-1; params[1]=-1;}

	double metric(const double *params) const
	{
		double x=params[0], y=params[1];
		return pow(x-sin(y+0.25),2) + pow(y-cos(x-0.25),2);
	}
	
	double metric_and_gradient(const double *params, double *G) const
	{
		double x=params[0], y=params[1];
		G[0]=2*(y - cos(x - 0.25))*sin(x - 0.25) + 2*x - 2*sin(y +0.25);
		G[1]=-2*(x - sin(y + 0.25))*cos(y + 0.25) + 2*y - 2*cos(x - 0.25);
		return metric(params);
	}
	
	void roots(const double *params, double *roots) const
	{
		double x=params[0], y=params[1];
		roots[0]=x-sin(y+0.25);
		roots[1]=y-cos(x-0.25);
	}
	
	void roots_and_jacobian(const double *params, double *roots, double *jacobian) const
	{
		double x=params[0], y=params[1];
		this->roots(params, roots);
		jacobian[0]=1;
		jacobian[1]=-cos(y+0.25);
		jacobian[2]=sin(x-0.25);
		jacobian[3]=1;
	}
};

BOOST_AUTO_TEST_CASE(MinimiseGSLTests_test2)
{
	test2_t target;
	double R0=0.873308054655, R1=0.811951910877;
	for(int i=0;i<10;i++){
		double guess[2]={drand48()*2-1, drand48()*2-1};
		std::pair<std::vector<double >,double> m=flopoco::random::MinimiseGSL(target, guess);
		std::cerr<<"test2 : root=("<<m.first[0]<<", "<<m.first[1]<<"), error=("<<m.first[0]-R0<<", "<<m.first[1]-R1<<"), metric="<<m.second<<"\n";
		BOOST_CHECK(fabs(m.first[0]-R0) < 1e-8);
		BOOST_CHECK(fabs(m.first[1]-R1) < 1e-8);
	}
	
}

BOOST_AUTO_TEST_CASE(FindRootsGSLTests_test2)
{
	test2_t target;
	double R0=0.873308054655, R1=0.811951910877;
	
	for(int i=0;i<10;i++){
		double guess[2]={drand48()*10-5, drand48()*10-5};
		std::pair<std::vector<double >,std::vector<double> > m=flopoco::random::FindRootsGSL(target, guess);
		std::cerr<<"test2 : root=("<<m.first[0]<<", "<<m.first[1]<<"), error=("<<m.first[0]-R0<<", "<<m.first[1]-R1<<")\n";
		BOOST_CHECK(fabs(m.first[0]-R0) < 1e-8);
		BOOST_CHECK(fabs(m.first[1]-R1) < 1e-8);
	}
	
}
