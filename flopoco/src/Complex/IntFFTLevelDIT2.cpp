#include <fstream>
#include <sstream>
#include "IntFFTLevelDIT2.hpp"

using namespace std;

namespace flopoco{

	extern vector<Operator *> oplist;

	IntFFTLevelDIT2::IntFFTLevelDIT2(Target* target, int wI_, int wF_, int k_, int n_, bool signedOperator_)
		: Operator(target), wI(wI_), wF(wF_), k(k_), n(n_), signedOperator(signedOperator_)
	{
		signedOperator ? w = 1 + wI + wF : w = wI + wF;
		
		ostringstream name;

		setCopyrightString ( "Matei Istoan, Florent de Dinechin (2008-2012)" );
		if(getTarget()->isPipelined())
			name << "IntFFTLevelDIT2_" << w << "_level_" << k << "_f"<< getTarget()->frequencyMHz() << "_uid" << getNewUId();
		else
			name << "IntFFTLevelDIT2_" << w << "_level_" << k << "_uid" << getNewUId();
		setNameWithFreqAndUID( name.str() );
		
		for(int i=0; i<n; i++){
			addInput(join("X", i, "i"), w, true);
			addInput(join("X", i, "r"), w, true);
		}
		for(int i=0; i<n; i++){
			addOutput(join("Z", i, "i"),   2*w, 2);
			addOutput(join("Z", i, "r"),   2*w, 2);
		}

		nrGroups = pow(2, k);
		distance = n/(nrGroups*2);
		
		for(int i=0; i<nrGroups; i++){
			int first, last, twiddleIndex;
			
			first = 2 * i * distance;
			last  = first + distance - 1;
			twiddleIndex = bitReverse(i, n);
			
			for(int j=first; j<=last; j++){
				IntFFTButterfly* butterfly = new IntFFTButterfly(target, wI, wF, twiddleIndex, n, signedOperator);
				oplist.push_back(butterfly);
				
				inPortMap (butterfly, "Xr",    join("X", j, 		 "r"));
				inPortMap (butterfly, "Xi",    join("X", j, 		 "i"));
				inPortMap (butterfly, "Yr",    join("X", j+distance, "r"));
				inPortMap (butterfly, "Yi",    join("X", j+distance, "i"));
				outPortMap(butterfly, "ZAddr", join("Z", j, 		 "r"), false);
				outPortMap(butterfly, "ZAddi", join("Z", j, 		 "i"), false);
				outPortMap(butterfly, "ZSubr", join("Z", j+distance, "r"), false);
				outPortMap(butterfly, "ZSubi", join("Z", j+distance, "i"), false);
				vhdl << instance(butterfly, join("Butterfly_", j, "_", j+distance));
			}
		}
	
	}	


	IntFFTLevelDIT2::~IntFFTLevelDIT2()
	{
	}
	
	//FIXME: correct the emulate function
	void IntFFTLevelDIT2::emulate ( TestCase* tc ) {
		int nrGroups = pow(2, k);
		int distance = n/(nrGroups*2);
		
		for(int i=0; i<nrGroups; i++){
			int first, last, twiddleIndex;
			
			first = 2 * i * distance;
			last  = first + distance - 1;
			twiddleIndex = bitReverse(i, n);
			
			for(int j=first; j<=last; j++){
				mpz_class svXi = tc->getInputValue(join("X", j, 		 "r"));
				mpz_class svXr = tc->getInputValue(join("X", j, 		 "i"));
				mpz_class svYi = tc->getInputValue(join("X", j+distance, "r"));
				mpz_class svYr = tc->getInputValue(join("X", j+distance, "i"));
				mpz_class svWi = getTwiddleConstant(TWIDDLEIM, twiddleIndex);
				mpz_class svWr = getTwiddleConstant(TWIDDLERE, twiddleIndex);
				
				
				if(twiddleIndex == 0){
					mpz_class svZi = svXi;
					mpz_class svZr = svXr;
				}else if((double)twiddleIndex == (double)n/4.0){
					mpz_class svZi = svXr;
					mpz_class svZr = (-1)*svXi;
				}else{
					if (! signedOperator){

						mpz_class svWYi = svYi*svWr - svYr*svWr;
						mpz_class svWYr = svYr*svWr + svYi*svWi;
						
						mpz_class svZAddi = svXi + svWYi;
						mpz_class svZAddr = svXr + svWYr;
						
						mpz_class svZSubi = svXi - svWYi;
						mpz_class svZSubr = svXr - svWYr;
						
						// Don't allow overflow
						mpz_clrbit ( svZAddi.get_mpz_t(), 2*w );
						mpz_clrbit ( svZAddr.get_mpz_t(), 2*w );
						mpz_clrbit ( svZSubi.get_mpz_t(), 2*w );
						mpz_clrbit ( svZSubr.get_mpz_t(), 2*w );

						tc->addExpectedOutput(join("Z", j, 		 "r"), svZAddr);
						tc->addExpectedOutput(join("Z", j, 		 "i"), svZAddi);
						tc->addExpectedOutput(join("Z", j+distance, "r"), svZSubr);
						tc->addExpectedOutput(join("Z", j+distance, "i"), svZSubi);
					}else{
						mpz_class big1 = (mpz_class(1) << (w));
						mpz_class big1P = (mpz_class(1) << (w-1));
						mpz_class big2 = (mpz_class(1) << (w));
						mpz_class big2P = (mpz_class(1) << (w-1));

						if ( svXi >= big1P)
							svXi = svXi - big1;
						if ( svXr >= big1P)
							svXr = svXi - big1;

						if ( svYi >= big2P)
							svYi = svYi - big2;
						if ( svYr >= big2P)
							svYr = svYr - big2;
						
						mpz_class svYrWr = svYr*svWr;
						mpz_class svYiWi = svYi*svWi;
						mpz_class svYrWi = svYr*svWi;
						mpz_class svYiWr = svYi*svWr;
						
						if ( svYrWr < 0){
							mpz_class tmpSUB = (mpz_class(1) << (2*w));
							svYrWr = tmpSUB + svYrWr; 
						}
						if ( svYiWi < 0){
							mpz_class tmpSUB = (mpz_class(1) << (2*w));
							svYiWi = tmpSUB + svYiWi; 
						}
						if ( svYiWr < 0){
							mpz_class tmpSUB = (mpz_class(1) << (2*w));
							svYiWr = tmpSUB + svYiWr; 
						}
						if ( svYrWi < 0){
							mpz_class tmpSUB = (mpz_class(1) << (2*w));
							svYrWi = tmpSUB + svYrWi; 
						}
						
						mpz_class svWYi = svYiWr - svYrWr;
						mpz_class svWYr = svYrWr + svYiWi;
						 
						// Don't allow overflow
						mpz_clrbit ( svWYi.get_mpz_t(), 2*w );
						mpz_clrbit ( svWYr.get_mpz_t(), 2*w );
						
						mpz_class svZAddi = svXi + svWYi;
						mpz_class svZAddr = svXr + svWYr;
						
						mpz_class svZSubi = svXi - svWYi;
						mpz_class svZSubr = svXr - svWYr;
						
						// Don't allow overflow
						mpz_clrbit ( svZAddi.get_mpz_t(), 2*w );
						mpz_clrbit ( svZAddr.get_mpz_t(), 2*w );
						mpz_clrbit ( svZSubi.get_mpz_t(), 2*w );
						mpz_clrbit ( svZSubr.get_mpz_t(), 2*w );
						
						tc->addExpectedOutput(join("Z", j, 		 "r"), svZAddr);
						tc->addExpectedOutput(join("Z", j, 		 "i"), svZAddi);
						tc->addExpectedOutput(join("Z", j+distance, "r"), svZSubr);
						tc->addExpectedOutput(join("Z", j+distance, "i"), svZSubi);
					}
				}
			}
		}
		
	}
	
	int IntFFTLevelDIT2::bitReverse(int x, int n){
		int count = log2((double)n)-1;
		int reversedX = 0, copyx;
		
		while(x>0){
			copyx = x&1;
			reversedX = reversedX | copyx;
			x = x >> 1;
			if(x!=0)
				reversedX = reversedX << 1;
			count--;
		}
		reversedX = reversedX << count;
		
		return reversedX;
	}
	
	mpz_class IntFFTLevelDIT2::getTwiddleConstant(int constantType, int twiddleExponent){
		mpfr_t twiddleExp, twiddleIm, twiddleRe, constPi, temp;
		mpz_class intTemp;
		
		mpfr_init2(twiddleIm, 	10*w);
		mpfr_init2(twiddleRe, 	10*w);
		mpfr_init2(twiddleExp, 	10*w);
		mpfr_init2(constPi, 	10*w);
		mpfr_init2(temp, 		10*w);
		
		mpfr_const_pi(	constPi, GMP_RNDN);
		
		mpfr_set_d(		twiddleExp, twiddleExponent, 			GMP_RNDN);
		mpfr_mul_2si(	twiddleExp, twiddleExp, 	1, 			GMP_RNDN);
		mpfr_mul(		twiddleExp, twiddleExp, 	constPi, 	GMP_RNDN);
		mpfr_div_d(		twiddleExp, twiddleExp, 	n, 			GMP_RNDN);
		
		mpfr_sin_cos(	twiddleIm, 	twiddleRe, 		twiddleExp, GMP_RNDN);
		
		switch(constantType){
			case TWIDDLERE:
							mpfr_set(temp, twiddleRe, GMP_RNDN);
							break;
			case TWIDDLEIM:
							mpfr_set(temp, twiddleIm, GMP_RNDN);
							break;
		}
		
		mpfr_mul_2si(temp, temp, wF, GMP_RNDN);
		mpfr_get_z(intTemp.get_mpz_t(), temp,  GMP_RNDN);
		
		mpfr_free_cache();
		mpfr_clears (twiddleExp, twiddleIm, twiddleRe, constPi, NULL);
		
		return intTemp;
	}

}




















