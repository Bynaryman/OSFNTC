#include "distribution.hpp"
#include "table_distribution.hpp"
#include "gaussian_distribution.hpp"

#include "moment_conversions.hpp"

#define BOOST_TEST_MODULE BaseTests
#include <boost/test/unit_test.hpp>

double ReferenceRawMoment(unsigned n, const double *v, unsigned k)
{
	double acc=0;
	for(unsigned i=0;i<n;i++){
		acc += pow(v[i],(int)k);
	}
	return acc / n;
}

double ReferenceCentralMoment(unsigned n, const double *v, unsigned k)
{
	double mean=std::accumulate(v, v+n, 0.0)/n;
	double acc=0;
	for(unsigned i=0;i<n;i++){
		acc += pow(v[i]-mean,(int)k);
	}
	return acc / n;
}

BOOST_AUTO_TEST_CASE(MomentConversionTests)
{
	// Standardised normal moments, come through unscathed
	double normal[]={1,0,1,0,3,0,15,0,105};
	for(unsigned i=0;i<=8;i++){
		BOOST_CHECK(RawMomentsToCentralMoment(i, normal)==normal[i]);
	}
	
	int K=8;
	for(int i=0;i<10;i++){
		int n=1+(lrand48()%10000);
		std::vector<double> values(n);
		for(int j=0;j<n;j++){
			values[j]=drand48()*10;
		}
		double mean=std::accumulate(values.begin(), values.end(), 0.0)/n;
		
		std::vector<double> raw(K+1, 0), ref(K+1,0);
		for(int j=0;j<=K;j++){
			raw[j]=ReferenceRawMoment(n, &values[0], j);
			ref[j]=ReferenceCentralMoment(n, &values[0], j);
		}
		
		BOOST_CHECK_CLOSE(raw[0], 1, 1e-10);
		BOOST_CHECK_CLOSE(raw[1], mean, 1e-10);
		for(int j=2;j<=K;j++){
			BOOST_CHECK(abs(RawMomentsToCentralMoment(j, &raw[0])-ref[j]) < 1e-10);
			BOOST_CHECK(abs(CentralMomentsToRawMoment(j, raw[1], &ref[0])-raw[j]) < 1e-10);
		}
		
		TableDistribution<double> tab(&values[0], &values[n]);
		
		for(int j=1;j<=K;j++){
			BOOST_CHECK(abs(raw[j]-tab.RawMoment(j)) < 1e-10);
			BOOST_CHECK(abs(ref[j] - tab.CentralMoment(j)) < 1e-10);
		}
	}
}

BOOST_AUTO_TEST_CASE(TableDistributionTests)
{
	typedef TableDistribution<double>::TypePtr TableDistributionPtr;
	
	double data[4]={-1 , +1, 0};
	
	TableDistributionPtr ptr=boost::make_shared<TableDistribution<double> >(data, data+3);
	
	BOOST_CHECK(ptr->ElementCount()==3);
	
	BOOST_CHECK_CLOSE(ptr->Pmf(-1), 1.0/3, 1e-10);
	BOOST_CHECK(ptr->Pmf(-0.5)==0.0);
	BOOST_CHECK_CLOSE(ptr->Pmf(0), 1.0/3, 1e-10);
	BOOST_CHECK(ptr->Pmf(0.5)==0.0);
	BOOST_CHECK_CLOSE(ptr->Pmf(+1), 1.0/3, 1e-10);
	
	BOOST_CHECK(ptr->Cdf(-1.1)==0.0);
	BOOST_CHECK_CLOSE(ptr->Cdf(-1), 1.0/3, 1e-10);
	BOOST_CHECK_CLOSE(ptr->Cdf(-0.5), 1.0/3, 1e-10);
	BOOST_CHECK_CLOSE(ptr->Cdf(0), 2.0/3, 1e-10);
	BOOST_CHECK_CLOSE(ptr->Cdf(0.5), 2.0/3.0, 1e-10);
	BOOST_CHECK(ptr->Cdf(+1)==1.0);
	BOOST_CHECK(ptr->Cdf(1.1)==1.0);
	
	BOOST_CHECK(ptr->StandardMoment(0)==1.0);
	BOOST_CHECK(ptr->StandardMoment(1)==0.0);
	BOOST_CHECK_CLOSE(ptr->StandardMoment(2), 2.0/3.0, 1e-10);
	BOOST_CHECK(ptr->StandardMoment(3)==0.0);
	BOOST_CHECK_CLOSE(ptr->StandardMoment(4), 1.5, 1e-10);
	BOOST_CHECK(ptr->StandardMoment(5)==0.0);
	BOOST_CHECK_CLOSE(ptr->StandardMoment(6), 2.25, 1e-10);
}


BOOST_AUTO_TEST_CASE(GaussianDistributionTests)
{
	typedef ContinuousDistribution<double>::ContinuousDistributionPtr ContinuousDistributionPtr;
	
	ContinuousDistributionPtr ptr=boost::make_shared<GaussianDistribution<double> >();
	
	BOOST_CHECK(ptr->StandardMoment(0)==1.0);
	BOOST_CHECK(ptr->StandardMoment(1)==0.0);
	BOOST_CHECK(ptr->StandardMoment(2)==1.0);
	BOOST_CHECK(ptr->StandardMoment(3)==0.0);
	BOOST_CHECK(ptr->StandardMoment(4)==3.0);
	BOOST_CHECK(ptr->StandardMoment(5)==0.0);
	BOOST_CHECK(ptr->StandardMoment(6)==15.0);
	BOOST_CHECK(ptr->StandardMoment(7)==0.0);
	BOOST_CHECK(ptr->StandardMoment(8)==105.0);
	
	BOOST_CHECK(ptr->RawMoment(0)==1.0);
	BOOST_CHECK(ptr->RawMoment(1)==0.0);
	BOOST_CHECK(ptr->RawMoment(2)==1.0);
	BOOST_CHECK(ptr->RawMoment(3)==0.0);
	BOOST_CHECK(ptr->RawMoment(4)==3.0);
	BOOST_CHECK(ptr->RawMoment(5)==0.0);
	BOOST_CHECK(ptr->RawMoment(6)==15.0);
	BOOST_CHECK(ptr->RawMoment(7)==0.0);
	BOOST_CHECK(ptr->RawMoment(8)==105.0);
	
	BOOST_CHECK(ptr->CentralMoment(0)==1.0);
	BOOST_CHECK(ptr->CentralMoment(1)==0.0);
	BOOST_CHECK(ptr->CentralMoment(2)==1.0);
	BOOST_CHECK(ptr->CentralMoment(3)==0.0);
	BOOST_CHECK(ptr->CentralMoment(4)==3.0);
	BOOST_CHECK(ptr->CentralMoment(5)==0.0);
	BOOST_CHECK(ptr->CentralMoment(6)==15.0);
	BOOST_CHECK(ptr->CentralMoment(7)==0.0);
	BOOST_CHECK(ptr->CentralMoment(8)==105.0);
	
	BOOST_CHECK_CLOSE(ptr->Pdf(0.0), 0.398942280401433, 1e-10);
	BOOST_CHECK_CLOSE(ptr->Pdf(1.0), 0.241970724519143, 1e-10);
	BOOST_CHECK_CLOSE(ptr->Pdf(-1.0), 0.241970724519143, 1e-10);
	
	BOOST_CHECK_CLOSE(ptr->Cdf(0.0), 0.5, 1e-10);
	BOOST_CHECK_CLOSE(ptr->Cdf(1.0), 0.841344746068543, 1e-10);
	BOOST_CHECK_CLOSE(ptr->Cdf(-1.0),  0.1586552539314573, 1e-10);
	
	BOOST_CHECK_CLOSE(ptr->InvCdf(0.5), 0.0, 1e-10);
	BOOST_CHECK_CLOSE(ptr->InvCdf(0.841344746068543), 1.0, 1e-10);
	BOOST_CHECK_CLOSE(ptr->InvCdf(0.1586552539314573), -1.0 , 1e-10);	
	
	
	ptr=boost::make_shared<GaussianDistribution<double> >(1.0,2.0);
	
	BOOST_CHECK(ptr->StandardMoment(0)==1.0);
	BOOST_CHECK(ptr->StandardMoment(1)==1.0);
	BOOST_CHECK(ptr->StandardMoment(2)==2.0);
	BOOST_CHECK(ptr->StandardMoment(3)==0.0);
	BOOST_CHECK(ptr->StandardMoment(4)==3.0);
	BOOST_CHECK(ptr->StandardMoment(5)==0.0);
	BOOST_CHECK(ptr->StandardMoment(6)==15.0);
	BOOST_CHECK(ptr->StandardMoment(7)==0.0);
	BOOST_CHECK(ptr->StandardMoment(8)==105.0);
	
	BOOST_CHECK(ptr->CentralMoment(0)==1.0);
	BOOST_CHECK(ptr->CentralMoment(1)==0.0);
	BOOST_CHECK(ptr->CentralMoment(2)==1.0*4);
	BOOST_CHECK(ptr->CentralMoment(3)==0.0);
	BOOST_CHECK(ptr->CentralMoment(4)==3.0*16);
	BOOST_CHECK(ptr->CentralMoment(5)==0.0);
	BOOST_CHECK(ptr->CentralMoment(6)==15.0*64);
	BOOST_CHECK(ptr->CentralMoment(7)==0.0);
	BOOST_CHECK(ptr->CentralMoment(8)==105.0*256);
	
	// These came from integration in sage
	BOOST_CHECK(ptr->RawMoment(0)== 1);
	BOOST_CHECK(ptr->RawMoment(1)== 1);
	BOOST_CHECK(ptr->RawMoment(2)== 5);
	BOOST_CHECK(ptr->RawMoment(3)== 13);
	BOOST_CHECK(ptr->RawMoment(4)== 73);
	BOOST_CHECK(ptr->RawMoment(5)==281);
	BOOST_CHECK(ptr->RawMoment(6)== 1741);
	BOOST_CHECK(ptr->RawMoment(7)== 8485);
	BOOST_CHECK(ptr->RawMoment(8)== 57233);

	
	BOOST_CHECK_CLOSE(ptr->Pdf(0.0*2.0+1.0), 0.398942280401433/2.0, 1e-10);
	BOOST_CHECK_CLOSE(ptr->Pdf(1.0*2.0+1.0), 0.241970724519143/2.0, 1e-10);
	BOOST_CHECK_CLOSE(ptr->Pdf(-1.0*2.0+1.0), 0.241970724519143/2.0, 1e-10);
	
	BOOST_CHECK_CLOSE(ptr->Cdf(0.0*2.0+1.0), 0.5, 1e-10);
	BOOST_CHECK_CLOSE(ptr->Cdf(1.0*2.0+1.0), 0.841344746068543, 1e-10);
	BOOST_CHECK_CLOSE(ptr->Cdf(-1.0*2.0+1.0),  0.1586552539314573, 1e-10);
	
	BOOST_CHECK_CLOSE(ptr->InvCdf(0.5), 0.0*2.0+1.0, 1e-10);
	BOOST_CHECK_CLOSE(ptr->InvCdf(0.841344746068543), 1.0*2.0+1.0, 1e-10);
	BOOST_CHECK_CLOSE(ptr->InvCdf(0.1586552539314573), -1.0*2.0+1.0 , 1e-10);	
}
