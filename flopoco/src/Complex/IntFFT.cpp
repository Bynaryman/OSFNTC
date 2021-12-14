#include <fstream>
#include <sstream>
#include "IntFFT.hpp"

using namespace std;

namespace flopoco{

	extern vector<Operator *> oplist;

	IntFFT::IntFFT(Target* target, int wI_, int wF_, int n_, bool signedOperator_)
		: Operator(target), wI(wI_), wF(wF_), n(n_), signedOperator(signedOperator_)
	{
		signedOperator ? w = 1 + wI + wF : w = wI + wF;
		
		ostringstream name;

		setCopyrightString ( "Matei Istoan, Florent de Dinechin (2008-2012)" );
		if(getTarget()->isPipelined())
			name << "IntFFT_" << w << "_f"<< getTarget()->frequencyMHz() << "_uid" << getNewUId();
		else
			name << "IntFFT_" << w << "_uid" << getNewUId();
		setNameWithFreqAndUID( name.str() );
		
		stages = log2(n);
		
		for(int i=0; i<n; i++){
			addInput(join("X", i, "i"), w, true);
			addInput(join("X", i, "r"), w, true);
		}
		for(int i=0; i<n; i++){
			addOutput(join("Z", i, "i"),   w, 2);
			addOutput(join("Z", i, "r"),   w, 2);
		}
		
		for(int i=0; i<stages; i++){
			IntFFTLevelDIT2* fftLevel = new IntFFTLevelDIT2(target, wI, wF, i, n, signedOperator);
			oplist.push_back(fftLevel);
			
			for(int j=0; j<n; j++){
				if(i!=0){
					vhdl << tab << declare(join("shortX_level_", i, "_", j, "_i"), w) << " <= " << join("X_level_", i, "_", j, "_i") << "(" << 2*w-1 << " downto " << w << ");" << endl;
					vhdl << tab << declare(join("shortX_level_", i, "_", j, "_r"), w) << " <= " << join("X_level_", i, "_", j, "_r") << "(" << 2*w-1 << " downto " << w << ");" << endl;
				}else{
					vhdl << tab << declare(join("shortX_level_", i, "_", j, "_i"), w) << " <= " << join("X", i, "i") << ";" << endl;
					vhdl << tab << declare(join("shortX_level_", i, "_", j, "_r"), w) << " <= " << join("X", i, "r") << ";" << endl;
				}
			}
			
			for(int j=0; j<n; j++){
				inPortMap (fftLevel, join("X", j, "i"), join("shortX_level_", i, "_", j, "_i"));
				inPortMap (fftLevel, join("X", j, "r"), join("shortX_level_", i, "_", j, "_r"));
			}
			for(int j=0; j<n; j++){
				outPortMap (fftLevel, join("Z", j, "i"), join("X_level_", i+1, "_", j, "_i"));
				outPortMap (fftLevel, join("Z", j, "r"), join("X_level_", i+1, "_", j, "_r"));
			}
			
			vhdl << instance(fftLevel, join("FFTLevel_", i));
		}
		
		for(int i=0; i<n; i++){
			vhdl << tab << join("Z", i, "i") << " <= " << join("X_level_", stages, "_", i, "_i") << "(" << 2*w-1 << " downto " << w << ");" << endl;
			vhdl << tab << join("Z", i, "r") << " <= " << join("X_level_", stages, "_", i, "_r") << "(" << 2*w-1 << " downto " << w << ");" << endl;
		}
	
	}	


	IntFFT::~IntFFT()
	{
	}
	
	//FIXME: correct the emulate function
	void IntFFT::emulate ( TestCase* tc ) {
		int nrGroups = 1;
		int distance = n/2;
		mpz_class svXi[n];
		mpz_class svXr[n];
		
		for(int i=0; i<n; i++){
			svXi[i] = tc->getInputValue(join("X", i, "i"));
			svXr[i] = tc->getInputValue(join("X", i, "r"));
		}		
		
		for(int i=0; i<stages; i++){
			for(int j=0; j<nrGroups; j++){
				int first, last, twiddleIndex;
				
				first = 2 * j * distance;
				last  = first + distance - 1;
				twiddleIndex = bitReverse(j, n);
				
				for(int k=first; k<=last; k++){
					mpz_class svWi = getTwiddleConstant(TWIDDLEIM, twiddleIndex);
					mpz_class svWr = getTwiddleConstant(TWIDDLERE, twiddleIndex);
					
					if(twiddleIndex == 0){
						svXi[k] = svXi[k];
						svXr[k] = svXr[k];
					}else if((double)twiddleIndex == (double)n/4.0){
						mpz_class tempMPZ = svXi[k];
						svXi[k] = svXr[k];
						svXr[k] = (-1)*tempMPZ;
					}else{
						if (! signedOperator){

							mpz_class svWXi = svXi[k+distance]*svWr - svXr[k+distance]*svWr;
							mpz_class svWXr = svXr[k+distance]*svWr + svXi[k+distance]*svWi;
							
							mpz_class svZAddi = svXi[k] + svWXi;
							mpz_class svZAddr = svXr[k] + svWXr;
							
							mpz_class svZSubi = svXi[k] - svWXi;
							mpz_class svZSubr = svXr[k] - svWXr;
							
							// Don't allow overflow
							mpz_clrbit ( svZAddi.get_mpz_t(), 2*w );
							mpz_clrbit ( svZAddr.get_mpz_t(), 2*w );
							mpz_clrbit ( svZSubi.get_mpz_t(), 2*w );
							mpz_clrbit ( svZSubr.get_mpz_t(), 2*w );

							svXi[k] 		 = svZAddi;
							svXr[k] 		 = svZAddr;
							svXi[k+distance] = svZAddi;
							svXr[k+distance] = svZAddr;
						}else{
							mpz_class big1 = (mpz_class(1) << (w));
							mpz_class big1P = (mpz_class(1) << (w-1));
							mpz_class big2 = (mpz_class(1) << (w));
							mpz_class big2P = (mpz_class(1) << (w-1));

							if ( svXi[k] >= big1P)
								svXi[k] = svXi[k] - big1;
							if ( svXr[k] >= big1P)
								svXr[k] = svXi[k] - big1;

							if ( svXi[k+distance] >= big2P)
								svXi[k+distance] = svXi[k+distance] - big2;
							if ( svXr[k+distance] >= big2P)
								svXr[k+distance] = svXr[k+distance] - big2;
							
							mpz_class svXrWr = svXr[k+distance]*svWr;
							mpz_class svXiWi = svXi[k+distance]*svWi;
							mpz_class svXrWi = svXr[k+distance]*svWi;
							mpz_class svXiWr = svXi[k+distance]*svWr;
							
							if ( svXrWr < 0){
								mpz_class tmpSUB = (mpz_class(1) << (2*w));
								svXrWr = tmpSUB + svXrWr; 
							}
							if ( svXiWi < 0){
								mpz_class tmpSUB = (mpz_class(1) << (2*w));
								svXiWi = tmpSUB + svXiWi; 
							}
							if ( svXiWr < 0){
								mpz_class tmpSUB = (mpz_class(1) << (2*w));
								svXiWr = tmpSUB + svXiWr; 
							}
							if ( svXrWi < 0){
								mpz_class tmpSUB = (mpz_class(1) << (2*w));
								svXrWi = tmpSUB + svXrWi; 
							}
							
							mpz_class svWXi = svXiWr - svXrWr;
							mpz_class svWXr = svXrWr + svXiWi;
							 
							// Don't allow overflow
							mpz_clrbit ( svWXi.get_mpz_t(), 2*w );
							mpz_clrbit ( svWXr.get_mpz_t(), 2*w );
							
							mpz_class svZAddi = svXi[k] + svWXi;
							mpz_class svZAddr = svXr[k] + svWXr;
							
							mpz_class svZSubi = svXi[k] - svWXi;
							mpz_class svZSubr = svXr[k] - svWXr;
							
							// Don't allow overflow
							mpz_clrbit ( svZAddi.get_mpz_t(), 2*w );
							mpz_clrbit ( svZAddr.get_mpz_t(), 2*w );
							mpz_clrbit ( svZSubi.get_mpz_t(), 2*w );
							mpz_clrbit ( svZSubr.get_mpz_t(), 2*w );
							
							svXi[k] 		 = svZAddi;
							svXr[k] 		 = svZAddr;
							svXi[k+distance] = svZAddi;
							svXr[k+distance] = svZAddr;
						}
					}
				}
			}
		}
		
		for(int i=0; i<n; i++){
			tc->addExpectedOutput(join("Z", i, "r"), svXr[i]);
			tc->addExpectedOutput(join("Z", i, "i"), svXi[i]);
		}
		
		
	}
	
	int IntFFT::bitReverse(int x, int n){
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
	
	mpz_class IntFFT::getTwiddleConstant(int constantType, int twiddleExponent){
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




















