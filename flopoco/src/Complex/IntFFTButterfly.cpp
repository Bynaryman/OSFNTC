#include <fstream>
#include <sstream>
#include "IntFFTButterfly.hpp"

using namespace std;

namespace flopoco{

	extern vector<Operator *> oplist;

	IntFFTButterfly::IntFFTButterfly(Target* target, int wI_, int wF_, int twiddleExponent_, int n_, bool signedOperator_)
		: Operator(target), wI(wI_), wF(wF_), twiddleExponent(twiddleExponent_), n(n_), signedOperator(signedOperator_)
	{
		signedOperator ? w = 1 + wI + wF : w = wI + wF;
		
		ostringstream name;

		setCopyrightString ( "Matei Istoan, Florent de Dinechin (2008-2012)" );
		if(getTarget()->isPipelined())
			name << "IntFFTButterfly_" << w << "_w_exp_" << twiddleExponent << "_f"<< getTarget()->frequencyMHz() << "_uid" << getNewUId();
		else
			name << "IntFFTButterfly_" << w << "_w_exp_" << twiddleExponent << "_uid" << getNewUId();
		setNameWithFreqAndUID( name.str() );

		addInput("Xi", 		   w, true);
		addInput("Xr", 		   w, true);
		addInput("Yi", 		   w, true);
		addInput("Yr", 		   w, true);
		addOutput("ZAddi",   2*w, 2);
		addOutput("ZAddr",   2*w, 2);
		addOutput("ZSubi",   2*w, 2);
		addOutput("ZSubr",   2*w, 2);
		
		IntTwiddleMultiplier* twiddleMultiplier = new IntTwiddleMultiplier(target, wI, wF, twiddleExponent, n, signedOperator);
		oplist.push_back(twiddleMultiplier);
		
		IntComplexAdder* addOperator =  new IntComplexAdder(target, 2*wI+1, 2*wF, signedOperator, inDelayMap("X",getCriticalPath()));
		oplist.push_back(addOperator);
		
		inPortMap (twiddleMultiplier, "Xr", "Yr");
		inPortMap (twiddleMultiplier, "Xi", "Yi");
		outPortMap(twiddleMultiplier, "Zr", "wYr");
		outPortMap(twiddleMultiplier, "Zi", "wYi");
		vhdl << instance(twiddleMultiplier, "TwiddleMUL_Y");
		
		vhdl << tab << declare("extendedXr", 2*w) << " <= (" << w-1 << " downto 0 => Xr(" << w-1 << ")) & Xr;" << endl;
		vhdl << tab << declare("extendedXi", 2*w) << " <= (" << w-1 << " downto 0 => Xi(" << w-1 << ")) & Xi;" << endl;
		
		vhdl << tab << declare("neg_wYr", 2*w) << " <= wYr xor (" << 2*w-1 << " downto 0 => \'1\');" << endl;
		vhdl << tab << declare("neg_wYi", 2*w) << " <= wYi xor (" << 2*w-1 << " downto 0 => \'1\');" << endl;
		
		inPortMap 	(addOperator, "Xr",	  "extendedXr");
		inPortMap 	(addOperator, "Xi",	  "extendedXi");
		inPortMap   (addOperator, "Yr",	  "wYr");
		inPortMap   (addOperator, "Yi",	  "wYi");
		inPortMapCst(addOperator, "Cini", "\'0\'");
		inPortMapCst(addOperator, "Cinr", "\'0\'");
		outPortMap	(addOperator, "Zr",	  "ZAddr", false);
		outPortMap	(addOperator, "Zi",	  "ZAddi", false);
		vhdl << instance(addOperator, "ADD_XwY");
		
		inPortMap 	(addOperator, "Xr",	  "extendedXr");
		inPortMap 	(addOperator, "Xi",	  "extendedXi");
		inPortMap   (addOperator, "Yr",	  "neg_wYr");
		inPortMap   (addOperator, "Yi",	  "neg_wYi");
		inPortMapCst(addOperator, "Cini", "\'1\'");
		inPortMapCst(addOperator, "Cinr", "\'1\'");
		outPortMap	(addOperator, "Zr",	  "ZSubr", false);
		outPortMap	(addOperator, "Zi",	  "ZSubi", false);
		vhdl << instance(addOperator, "SUB_XwY");
	
	}	


	IntFFTButterfly::~IntFFTButterfly()
	{
	}
	
	//FIXME: correct the emulate function
	void IntFFTButterfly::emulate ( TestCase* tc ) {
		mpz_class svXi = tc->getInputValue("Xi");
		mpz_class svXr = tc->getInputValue("Xr");
		mpz_class svYi = tc->getInputValue("Yi");
		mpz_class svYr = tc->getInputValue("Yr");
		mpz_class svWi = getTwiddleConstant(TWIDDLEIM);
		mpz_class svWr = getTwiddleConstant(TWIDDLERE);
		
		
		if(twiddleExponent == 0){
			mpz_class svZi = svXi;
			mpz_class svZr = svXr;
		}else if((double)twiddleExponent == (double)n/4.0){
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

				tc->addExpectedOutput("ZAddi", svZAddi);
				tc->addExpectedOutput("ZAddr", svZAddr);
				tc->addExpectedOutput("ZSubi", svZSubi);
				tc->addExpectedOutput("ZSubr", svZSubr);
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
				
				tc->addExpectedOutput("ZAddi", svZAddi);
				tc->addExpectedOutput("ZAddr", svZAddr);
				tc->addExpectedOutput("ZSubi", svZSubi);
				tc->addExpectedOutput("ZSubr", svZSubr);
			}
		}
	}
	
	mpz_class IntFFTButterfly::getTwiddleConstant(int constantType){
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




















