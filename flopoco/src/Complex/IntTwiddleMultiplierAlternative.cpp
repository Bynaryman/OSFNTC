#include <fstream>
#include <sstream>
#include "IntTwiddleMultiplierAlternative.hpp"

using namespace std;

namespace flopoco{

	extern vector<Operator *> oplist;

	//TODO: explore implementation using multiply-accumulate operators
	//TODO: explore the use of both KCM and Shift-And-Add techniques ->
	//		or just a simple signed multiplier	
	//FIXME: correct timing of the circuit
	//FIXME: verify behaviour in case of negative re/im twiddle parts
	//FIXME: correct the size of the output and intermediary computations
	//		 for now it's fixed at 2*w, achieved through padding (and with 1 all around)
	//FIXME: correct the emulate function
	//FIXME: fix the sizes of the operations -> additions and multiplications 
	//		 and the necessary padding
	IntTwiddleMultiplierAlternative::IntTwiddleMultiplierAlternative(Target* target, int wI_, int wF_, int twiddleExponent_, int n_, bool signedOperator_, int multiplierMode_)
		: Operator(target), wI(wI_), wF(wF_), twiddleExponent(twiddleExponent_), n(n_), signedOperator(signedOperator_), multiplierMode(multiplierMode_)
	{
		bool completeExecutionPath = true;
		
		signedOperator ? w = 1 + wI + wF : w = wI + wF;
		
		ostringstream name;

		setCopyrightString ( "Matei Istoan, Florent de Dinechin (2008-2012)" );
		if(getTarget()->isPipelined())
			name << "IntTwiddleMultiplierAlternative_" << w << "_w_exp_" << twiddleExponent << "_f"<< getTarget()->frequencyMHz() << "_uid" << getNewUId();
		else
			name << "IntTwiddleMultiplierAlternative_" << w << "_w_exp_" << twiddleExponent << "_uid" << getNewUId();
		setNameWithFreqAndUID( name.str() );

		addInput("Xi", 		w, true);
		addInput("Xr", 		w, true);
		addOutput("Zi",   2*w, 2);
		addOutput("Zr",   2*w, 2);
		
		
		if(twiddleExponent == 0){
						
			vhdl << tab << "Zi <= (" << w-1 << " downto 0 => Xi(" << w-1 << ")) & Xi;" << endl;
			vhdl << tab << "Zr <= (" << w-1 << " downto 0 => Xr(" << w-1 << ")) & Xr;" << endl;
			
			completeExecutionPath = false;
		} else if((double)twiddleExponent == (double)n/4.0){
			
			vhdl << tab << declare("neg_Xi", w) << " <= Xi xor (" << w-1 << " downto 0 => \'1\');" << endl;
			
			IntAdder* addOperator =  new IntAdder(target, w, inDelayMap("X",getCriticalPath()));
			oplist.push_back(addOperator);
			
			inPortMap 	(addOperator, "X", 	 "neg_Xi");
			inPortMapCst(addOperator, "Y", 	 zg(w, 0));
			inPortMapCst(addOperator, "Cin", "\'1\'");
			outPortMap	(addOperator, "R", 	 "intZr", false);
			vhdl << instance(addOperator, "ADD_negXi");
			
			vhdl << tab << "Zr <= (" << w-1 << " downto 0 => intZr(" << w-1 << ")) & intZr;" << endl;
			vhdl << tab << "Zi <= (" << w-1 << " downto 0 => Xr(" << w-1 << ")) & Xr;" << endl;
			
			completeExecutionPath = false;
		}
	
		if(completeExecutionPath){
			mpz_class twCp, twS;
			int wOutAlpha, wOutGamma, wOutBeta, wOutZiMulCp;
			
			twCp = getTwiddleConstant(TWIDDLECP);
			twS = getTwiddleConstant(TWIDDLES);
			
			Operator *multiplyOperatorCp, *multiplyOperatorS, *multiplyOperatorCp2;
			IntAdder *addOperatorGamma, *addOperatorZi, *addOperatorZr;
			
			if(multiplierMode == 0){
				wOutAlpha   = 1 + wI + 2*wF + ceil(log2(abs(getTwiddleConstant(TWIDDLECP).get_si())));
				wOutGamma   = wOutAlpha;
				wOutBeta    = 1 + (wI + (wOutGamma-w-wF)) + 4*wF + ceil(log2(abs(getTwiddleConstant(TWIDDLES).get_si())));
				wOutZiMulCp = 1 + (wI + (wOutBeta-w-3*wF)) + 8*wF + ceil(log2(abs(getTwiddleConstant(TWIDDLES).get_si())));
			}else{
				wOutAlpha 	= ceil(log2((double)(twCp.get_si()) * ((pow(2.0, (double)w))-1.0)));
				wOutGamma 	= wOutAlpha;
				wOutBeta  	= ceil(log2((double)(twS.get_si()) * ((pow(2.0, (double)wOutGamma))-1.0)));
				wOutZiMulCp = ceil(log2((double)(twCp.get_si()) * ((pow(2.0, (double)wOutBeta))-1.0)));
			}
			
			if(multiplierMode == 0){
				multiplyOperatorCp = new FixRealKCM(target, true, wI-1, -wF,1, -2*wF, getTwiddleConstantString(TWIDDLECP));
				oplist.push_back(multiplyOperatorCp);
				
				multiplyOperatorS = new FixRealKCM(target, true, wI+(wOutGamma-w-wF)-1, -2*wF,  -4*wF, getTwiddleConstantString(TWIDDLES));
				oplist.push_back(multiplyOperatorS);
				
				multiplyOperatorCp2 = new FixRealKCM(target, true, wI+(wOutBeta-w-3*wF)-1, -4*wF, -8*wF, getTwiddleConstantString(TWIDDLECP));
				oplist.push_back(multiplyOperatorCp2);
			}else{
				multiplyOperatorCp = new IntConstMult(target, w, (twCp<0 ? (-1)*twCp : twCp));
				oplist.push_back(multiplyOperatorCp);
				
				multiplyOperatorS = new IntConstMult(target, wOutGamma, (twS<0 ? (-1)*twS : twS));
				oplist.push_back(multiplyOperatorS);
				
				multiplyOperatorCp2 = new IntConstMult(target, wOutBeta, (twCp<0 ? (-1)*twCp : twCp));
				oplist.push_back(multiplyOperatorCp2);
			}
			
			addOperatorGamma =  new IntAdder(target, wOutAlpha, inDelayMap("X",getCriticalPath()));
			oplist.push_back(addOperatorGamma);
			
			addOperatorZi =  new IntAdder(target, wOutBeta, inDelayMap("X",getCriticalPath()));
			oplist.push_back(addOperatorZi);
			
			addOperatorZr =  new IntAdder(target, wOutZiMulCp, inDelayMap("X",getCriticalPath()));
			oplist.push_back(addOperatorZr);
			
			inPortMap( multiplyOperatorCp, "X", "Xi");
			outPortMap(multiplyOperatorCp, "R", "Alpha");
			vhdl << instance(multiplyOperatorCp, "MUL_Alpha");
			
			vhdl << tab << declare("Xr_pad_to_wOutAlpha", wOutAlpha) << " <= (" << wOutAlpha-w-1 << " downto 0 => Xr(" << w-1 << ")) & Xr;" << endl;
			
			inPortMap(	 addOperatorGamma, "X", "Xr_pad_to_wOutAlpha");
			inPortMap(	 addOperatorGamma, "Y", "Alpha");
			inPortMapCst(addOperatorGamma, "Cin", "\'0\'");
			outPortMap(	 addOperatorGamma, "R", "Gamma");
			vhdl << instance(addOperatorGamma, "ADD_Gamma");
			
			inPortMap( multiplyOperatorS, "X", "Gamma");
			outPortMap(multiplyOperatorS, "R", "Beta");
			vhdl << instance(multiplyOperatorS, "MUL_Beta");
			
			vhdl << tab << declare("Xi_pad_to_wOutBeta", wOutBeta) << " <= (" << wOutBeta-w-1 << " downto 0 => Xi(" << w-1 << ")) & Xi;" << endl;
			vhdl << tab << declare("neg_Beta", wOutBeta) << " <= (" << wOutBeta-1 << " downto 0 => \'1\') xor Beta;" << endl;
			
			inPortMap(	 addOperatorZi, "X", "Xi_pad_to_wOutBeta");
			inPortMap(	 addOperatorZi, "Y", "neg_Beta");
			inPortMapCst(addOperatorZi, "Cin", "\'1\'");
			outPortMap(	 addOperatorZi, "R", "intZi");
			vhdl << instance(addOperatorZi, "ADD_Zi");
			
			inPortMap( multiplyOperatorCp2, "X", "intZi");
			outPortMap(multiplyOperatorCp2, "R", "intZiMulCp");
			vhdl << instance(multiplyOperatorCp2, "MUL_ZiMulCp");
			
			vhdl << tab << declare("Gamma_pad_to_wOutZiMulCp", wOutZiMulCp) << " <= (" << wOutZiMulCp-wOutGamma-1 << " downto 0 => Gamma(" << wOutGamma-1 << ")) & Gamma;" << endl;
			
			inPortMap(	 addOperatorZr, "X", "intZiMulCp");
			inPortMap(	 addOperatorZr, "Y", "Gamma_pad_to_wOutZiMulCp");
			inPortMapCst(addOperatorZr, "Cin", "\'0\'");
			outPortMap(	 addOperatorZr, "R", "intZr");
			vhdl << instance(addOperatorZr, "ADD_Zr");
			
			if(2*w>wOutBeta){
				vhdl << tab << "Zi <= (" << 2*w-wOutBeta-1 << " downto 0 => intZi(" << wOutBeta-1 << ")) & intZi;" << endl;
			}else{
				vhdl << tab << "Zi <= intZi(" << wOutBeta-1 << " downto " << wOutBeta-2*w << ");" << endl;
			}
			if(2*w>wOutZiMulCp){
				vhdl << tab << "Zr <= (" << 2*w-wOutZiMulCp-1 << " downto 0 => intZr(" << wOutZiMulCp << ")) & intZr;" << endl;
			}else{
				vhdl << tab << "Zr <= intZr(" << wOutZiMulCp-1 << " downto " << wOutZiMulCp-2*w << ");" << endl;
			}
		}
	}

	IntTwiddleMultiplierAlternative::~IntTwiddleMultiplierAlternative()
	{
	}
	
	//FIXME: correct the emulate function
	void IntTwiddleMultiplierAlternative::emulate ( TestCase* tc ) {
		mpz_class svXi = tc->getInputValue("Xi");
		mpz_class svXr = tc->getInputValue("Xr");
		mpz_class svYi = getTwiddleConstant(TWIDDLES);
		mpz_class svYr = getTwiddleConstant(TWIDDLEC);
		
		
		if(twiddleExponent == 0){
			mpz_class svZi = svXi;
			mpz_class svZr = svXr;
		}else if((double)twiddleExponent == (double)n/4.0){
			mpz_class svZi = svXr;
			mpz_class svZr = (-1)*svXi;
		}else{
			if (! signedOperator){

				mpz_class svZi = svXr*svYi + svXi*svYr;
				mpz_class svZr = svXr*svYr - svXi*svYi;
				
				// Don't allow overflow
				mpz_clrbit ( svZi.get_mpz_t(), 2*w );
				mpz_clrbit ( svZr.get_mpz_t(), 2*w );

				tc->addExpectedOutput("Zi", svZi);
				tc->addExpectedOutput("Zr", svZr);
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
				
				mpz_class svXrYr = svXr*svYr;
				mpz_class svXiYi = svXi*svYi;
				mpz_class svXrYi = svXr*svYi;
				mpz_class svXiYr = svXi*svYr;
				
				if ( svXrYr < 0){
					mpz_class tmpSUB = (mpz_class(1) << (2*w));
					svXrYr = tmpSUB + svXrYr; 
				}
				if ( svXiYi < 0){
					mpz_class tmpSUB = (mpz_class(1) << (2*w));
					svXiYi = tmpSUB + svXiYi; 
				}
				if ( svXrYi < 0){
					mpz_class tmpSUB = (mpz_class(1) << (2*w));
					svXrYi = tmpSUB + svXrYi; 
				}
				if ( svXiYr < 0){
					mpz_class tmpSUB = (mpz_class(1) << (2*w));
					svXiYr = tmpSUB + svXiYr; 
				}
				
				mpz_class svZi = svXrYi + svXiYr;
				mpz_class svZr = svXrYr - svXiYi;
				 
				// Don't allow overflow
				mpz_clrbit ( svZi.get_mpz_t(), 2*w );
				mpz_clrbit ( svZr.get_mpz_t(), 2*w );
				
				tc->addExpectedOutput("Zi", svZi);
				tc->addExpectedOutput("Zr", svZr);
			}
		}
	}
	
	
	mpz_class IntTwiddleMultiplierAlternative::getTwiddleConstant(int constantType){
		mpfr_t twiddleExp, twiddleS, twiddleC, constPi, temp;
		mpz_class intTemp;
		
		mpfr_init2(twiddleS, 	10*w);
		mpfr_init2(twiddleC, 	10*w);
		mpfr_init2(twiddleExp, 	10*w);
		mpfr_init2(constPi, 	10*w);
		mpfr_init2(temp, 		10*w);
		
		mpfr_const_pi(	constPi, GMP_RNDN);
		
		mpfr_set_d(		twiddleExp, twiddleExponent, 			GMP_RNDN);
		mpfr_mul_2si(	twiddleExp, twiddleExp, 	1, 			GMP_RNDN);
		mpfr_mul(		twiddleExp, twiddleExp, 	constPi, 	GMP_RNDN);
		mpfr_div_d(		twiddleExp, twiddleExp, 	n, 			GMP_RNDN);
		
		mpfr_sin_cos(	twiddleS, 	twiddleC, 		twiddleExp, GMP_RNDN);
		
		switch(constantType){
			case TWIDDLES:
							mpfr_set(temp, twiddleC, GMP_RNDN);
							break;
			case TWIDDLEC:
							mpfr_set(temp, twiddleS, GMP_RNDN);
							break;
			case TWIDDLECP:
							mpfr_set_d(	temp, 1.0, 			  GMP_RNDN);
							mpfr_sub(	temp, temp, twiddleC, GMP_RNDN);
							mpfr_sub(	temp, temp, twiddleS, GMP_RNDN);
							break;
		}
		
		mpfr_mul_2si(temp, temp, wF, GMP_RNDN);
		mpfr_get_z(intTemp.get_mpz_t(), temp,  GMP_RNDN);
		
		mpfr_free_cache();
		mpfr_clears (twiddleExp, twiddleS, twiddleC, constPi, NULL);
		
		return intTemp;
	}
	
	std::string IntTwiddleMultiplierAlternative::getTwiddleConstantString(int constantType){
		std::ostringstream result;
		mpz_class temp;
		
		temp = getTwiddleConstant(constantType);
		if(temp<0)
			temp *= (-1);
		result << temp;
		
		return result.str();
	}

}




















