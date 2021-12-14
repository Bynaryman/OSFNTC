#include "correct_distribution.hpp"

#include "random/distributions/gaussian_distribution.hpp"
#include "random/distributions/table_distribution.hpp"

#define BOOST_TEST_MODULE MomentCorrectionTests
#include <boost/test/unit_test.hpp>

using namespace flopoco::random;

BOOST_AUTO_TEST_CASE(SymmetricCubicTest)
{
	ContinuousDistribution<double>::TypePtr target=boost::make_shared<GaussianDistribution<double> >();
	
	int n=64;
	std::vector<double> x(n);
	for(int i=0;i<n/2;i++){
		double u=(i+0.5)/n;
		x[i]=getTarget()->InvCdf(u);
		x[n-i-1]=-x[i];
	}
	
	TableDistribution<double>::TypePtr current=boost::make_shared<TableDistribution<double> >(&x[0], &x[n]);
	
	std::vector<double> poly=FindSymmetricCubicPolynomialCorrection<double>(
		current,
		target
	);
	if(poly.size()==0){
		std::cerr<<"No solution\n";
	}else{
		std::cerr<<"Poly = "<<poly[0];
		for(unsigned i=1;i<poly.size();i++){
			std::cerr<<" + "<<poly[i]<<"*x^"<<i;
		}
		std::cerr<<"\n";
	}
}

BOOST_AUTO_TEST_CASE(CubicTest)
{
	ContinuousDistribution<double>::TypePtr target=boost::make_shared<GaussianDistribution<double> >(1.0,2.0);
	
	int n=64;
	std::vector<double> x(n);
	for(int i=0;i<n/2;i++){
		double u=(i+0.5)/n;
		x[i]=getTarget()->InvCdf(u);
		x[n-i-1]=-x[i];
	}
	
	TableDistribution<double>::TypePtr current=boost::make_shared<TableDistribution<double> >(&x[0], &x[n]);
	
	std::vector<double> poly=FindCubicPolynomialCorrection<double>(
		current,
		target
	);
	if(poly.size()==0){
		std::cerr<<"No solution\n";
	}else{
		std::cerr<<"Poly = "<<poly[0];
		for(unsigned i=1;i<poly.size();i++){
			std::cerr<<" + "<<poly[i]<<"*x^"<<i;
		}
		std::cerr<<"\n";
	}
	
	TableDistribution<double>::TypePtr corrected=current->ApplyPolynomial(poly);
	
	for(int i=0;i<=6;i++){
		fprintf(stderr, "%12.10lg, %12.10lg, %12.10lg\n", getTarget()->RawMoment(i), current->RawMoment(i), corrected->RawMoment(i));
	}
}

BOOST_AUTO_TEST_CASE(QuinticTest)
{
	ContinuousDistribution<double>::TypePtr target=boost::make_shared<GaussianDistribution<double> >(1,1);
	
	int n=1024;
	std::vector<double> x(n);
	for(int i=0;i<n/2;i++){
		double u=(i+0.5)/n;
		x[i]=getTarget()->InvCdf(u);
		x[n-i-1]=-x[i];
	}
	
	TableDistribution<double>::TypePtr current=boost::make_shared<TableDistribution<double> >(&x[0], &x[n]);
	
	std::vector<double> poly=FindQuinticPolynomialCorrection<double>(
		current,
		target
	);
	if(poly.size()==0){
		std::cerr<<"No solution\n";
	}else{
		std::cerr<<"Poly = "<<poly[0];
		for(unsigned i=1;i<poly.size();i++){
			std::cerr<<" + "<<poly[i]<<"*x^"<<i;
		}
		std::cerr<<"\n";
	}
	
	TableDistribution<double>::TypePtr corrected=current->ApplyPolynomial(poly);
	
	for(int i=0;i<=10;i++){
		fprintf(stderr, "%12.10lg, %12.10lg, %12.10lg\n", getTarget()->RawMoment(i), current->RawMoment(i), corrected->RawMoment(i));
	}
}

BOOST_AUTO_TEST_CASE(SymmetricQuinticTest)
{
	ContinuousDistribution<double>::TypePtr target=boost::make_shared<GaussianDistribution<double> >(0,1);
	
	int n=1024;
	std::vector<double> x(n);
	for(int i=0;i<n/2;i++){
		double u=(i+0.5)/n;
		x[i]=getTarget()->InvCdf(u);
		x[n-i-1]=-x[i];
	}
	
	TableDistribution<double>::TypePtr current=boost::make_shared<TableDistribution<double> >(&x[0], &x[n]);
	
	std::vector<double> poly=FindSymmetricQuinticPolynomialCorrection<double>(
		current,
		target
	);
	if(poly.size()==0){
		std::cerr<<"No solution\n";
	}else{
		std::cerr<<"Poly = "<<poly[0];
		for(unsigned i=1;i<poly.size();i++){
			std::cerr<<" + "<<poly[i]<<"*x^"<<i;
		}
		std::cerr<<"\n";
	}
	
	TableDistribution<double>::TypePtr corrected=current->ApplyPolynomial(poly);
	
	for(int i=0;i<=10;i++){
		fprintf(stderr, "%12.10lg, %12.10lg, %12.10lg\n", getTarget()->RawMoment(i), current->RawMoment(i), corrected->RawMoment(i));
	}
}

BOOST_AUTO_TEST_CASE(SymmetricHepticTest)
{
	ContinuousDistribution<double>::TypePtr target=boost::make_shared<GaussianDistribution<double> >(0,1);
	
	int n=65536;
	std::vector<double> x(n);
	for(int i=0;i<n/2;i++){
		double u=(i+0.5)/n;
		x[i]=getTarget()->InvCdf(u);
		x[n-i-1]=-x[i];
	}
	
	TableDistribution<double>::TypePtr current=boost::make_shared<TableDistribution<double> >(&x[0], &x[n]);
	
	std::vector<double> poly=FindSymmetricHepticPolynomialCorrection<double>(
		current,
		target
	);
	if(poly.size()==0){
		std::cerr<<"No solution\n";
	}else{
		std::cerr<<"Poly = "<<poly[0];
		for(unsigned i=1;i<poly.size();i++){
			std::cerr<<" + "<<poly[i]<<"*x^"<<i;
		}
		std::cerr<<"\n";
	}
	
	TableDistribution<double>::TypePtr corrected=current->ApplyPolynomial(poly);
	
	for(int i=0;i<=14;i++){
		fprintf(stderr, "%12.10lg, %12.10lg, %12.10lg\n", getTarget()->RawMoment(i), current->RawMoment(i), corrected->RawMoment(i));
	}
}

BOOST_AUTO_TEST_CASE(SymmetricNonicTest)
{
	ContinuousDistribution<double>::TypePtr target=boost::make_shared<GaussianDistribution<double> >(0,1);
	
	int n=65536;
	std::vector<double> x(n);
	for(int i=0;i<n/2;i++){
		double u=(i+0.5)/n;
		x[i]=getTarget()->InvCdf(u);
		x[n-i-1]=-x[i];
	}
	
	TableDistribution<double>::TypePtr current=boost::make_shared<TableDistribution<double> >(&x[0], &x[n]);
	
	std::vector<double> poly=FindSymmetricNonicPolynomialCorrection<double>(
		current,
		target
	);
	if(poly.size()==0){
		std::cerr<<"No solution\n";
	}else{
		std::cerr<<"Poly = "<<poly[0];
		for(unsigned i=1;i<poly.size();i++){
			std::cerr<<" + "<<poly[i]<<"*x^"<<i;
		}
		std::cerr<<"\n";
	}
	
	TableDistribution<double>::TypePtr corrected=current->ApplyPolynomial(poly);
	
	for(int i=0;i<=16;i++){
		fprintf(stderr, "%12.10lg, %12.10lg, %12.10lg\n", getTarget()->RawMoment(i), current->RawMoment(i), corrected->RawMoment(i));
	}
}
