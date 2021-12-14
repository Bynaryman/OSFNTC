#include <iostream>
//#include <sstream>
#include <iomanip>


#include <vector>
#include <math.h>
#include <string>

#include <gmp.h>
#include <mpfr.h>

#include <gmpxx.h>
#include "sollya.h"
//#include <utils.hpp>
#include <Operator.hpp>

//#include <string.h>
//#include <utils.hpp>

//#include "IntFFTButterfly.hpp"
#include "FixComplexR2Butterfly.hpp"

using namespace std;



namespace flopoco{

//	extern vector<Operator *> oplist;

	FixComplexR2Butterfly::FixComplexR2Butterfly(OperatorPtr parentOp,Target* target, int msbin_, int lsbin_, int msbout_, int lsbout_, string Twiddle_re_, string Twiddle_im_, bool signedIn_, bool bypassmult_, bool decimation_, bool extrabit_, bool laststage_)
		: Operator(parentOp, target), msbin(msbin_), lsbin(lsbin_), msbout(msbout_), lsbout(lsbout_), Twiddle_re(Twiddle_re_), Twiddle_im(Twiddle_im_), signedIn(signedIn_), bypassmult(bypassmult_), decimation(decimation_), extrabit(extrabit_), laststage(laststage_)
	{
//		signedOperator ? w = 1 + wI + wF : w = wI + wF;
		int input_width = msbin-lsbin+1;
//		int output_width = msbout-lsbout+1;
		int output_width_add;
		int output_width_sub;


		if(laststage)
		{
			output_width_add = msbout-lsbout+1;
			output_width_sub = msbout-lsbout+1;
		}
		else
		{
			output_width_add = msbout-lsbin+1;
			output_width_sub = msbout-lsbout+1;
		}


/*		if(bypassmult)
		{
			output_width_add = msbout-lsbout+1;
			output_width_sub = msbout-lsbout+1;
		}
		else
		{
			output_width_add = msbout-lsbin+1;
			output_width_sub = msbout-lsbout+1;
		}
*/

		string extrabitstr;
		
		ostringstream name;
		std::ostringstream outportmap;

		setCopyrightString ( "Hatam Abdoli, Florent de Dinechin (2019)" );
/*		if(getTarget()->isPipelined())
			name << "FixComplexR2Butterfly_" << input_width << "_w_exp_" << twiddleExponent << "_f"<< getTarget()->frequencyMHz() << "_uid" << getNewUId();
		else
			name << "FixComplexR2Butterfly_" << input_width << "_w_exp_" << twiddleExponent << "_uid" << getNewUId();
*/
		name << "FixComplexR2Butterfly_" << abs(msbin) << "_" << abs(lsbin);
		setNameWithFreqAndUID( name.str() );


		if(extrabit)
		{
			extrabitstr = "true";
//			output_width += 1;
		}
		else
			extrabitstr = "false";


		addInput("X0i", input_width);
		addInput("X0r", input_width);
		addInput("X1i", input_width);
		addInput("X1r", input_width);
//		addOutput("Y0i", output_width);
//		addOutput("Y0r", output_width);
//		addOutput("Y1i", output_width);
//		addOutput("Y1r", output_width);
		addOutput("Y0i", output_width_add);
		addOutput("Y0r", output_width_add);
		addOutput("Y1i", output_width_sub);
		addOutput("Y1r", output_width_sub);

		//vhdl << tab << declare("TwX1r", output_width)  << ";" << endl;
		//vhdl << tab << declare("TwX1i", output_width)  << ";" << endl;

		
		if(decimation)
		{

			/*FixComplexAdder* FixComplexAdd = new FixComplexAdder(nullptr, getTarget(), msbin, lsbin, msbout, lsbout, true, true);
			//oplist.push_back(FixComplexAdd);

			inPortMap (FixComplexAdd, "Xr", "X0r");
			inPortMap (FixComplexAdd, "Xi", "X0i");
			inPortMap (FixComplexAdd, "Yr", "X1r");
			inPortMap (FixComplexAdd, "Yi", "X1i");
			outPortMap(FixComplexAdd, "Sr", "Y0r");
			outPortMap(FixComplexAdd, "Si", "Y0i");	
			vhdl << instance(FixComplexAdd, "FixComplexAdd_X0");


			FixComplexAdder* FixComplexSub = new FixComplexAdder(nullptr, getTarget(), msbin, lsbin, msbout, lsbout, true, false);
			//oplist.push_back(FixComplexSub);

			inPortMap (FixComplexSub, "Xr", "X0r");
			inPortMap (FixComplexSub, "Xi", "X0i");
			inPortMap (FixComplexSub, "Yr", "X1r");
			inPortMap (FixComplexSub, "Yi", "X1i");
			outPortMap(FixComplexSub, "Sr", "TwX1r");
			outPortMap(FixComplexSub, "Si", "TwX1i");
			vhdl << instance(FixComplexAdd, "FixComplexAdd_X1");


			FixComplexKCM* FixTwiddleKCM = new FixComplexKCM(target, true, msbin, lsbin, lsbin, Twiddle_re, Twiddle_im);
	//		oplist.push_back(FixTwiddleKCM);

			inPortMap (FixTwiddleKCM, "ReIn", "TwX1r");
			inPortMap (FixTwiddleKCM, "ImIn", "TwX1i");

			outPortMap(FixTwiddleKCM, "ReOut", "Y1r");
			outPortMap(FixTwiddleKCM, "ImOut", "Y1i");
			vhdl << instance(FixTwiddleKCM, "TwiddleKCMMUL_X1");*/

//				cout << endl << "mmmmmsbin=" << std::to_string(msbin) << " lsbin=" + std::to_string(lsbin) << 
//							" msbout=" << std::to_string(msbout) << " lsbout=" << std::to_string(lsbout) << " bypassmult=" << std::to_string(bypassmult) << " laststage=" << std::to_string(laststage) << endl; 

//			if (laststage)
//			{
//				cout << endl << "laststage = trueeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee" << endl;
//			}
//			else
			{

				if (bypassmult && laststage)  //bypass multiplication because the twiddle angle is zero and the coefficient is equal to one
				{
					newInstance("FixComplexAdder",
							"FixComplexAdd",
							"msbin=" + std::to_string(msbin) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbout) + " lsbout=" +
							std::to_string(lsbout) + " signedIn=true addorsub=true laststage=" + std::to_string(laststage),
//							std::to_string(lsbout) + " signedIn=true addorsub=true laststage=" + std::to_string(laststage),
							"Xr=>X0r,Xi=>X0i,Yr=>X1r,Yi=>X1i",
							"Sr=>Y0r,Si=>Y0i");

					newInstance("FixComplexAdder",
							"FixComplexSub",
							"msbin=" + std::to_string(msbin) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbout) + " lsbout=" +
					std::to_string(lsbout) + " signedIn=true addorsub=false laststage=" + std::to_string(laststage),
//					std::to_string(lsbout) + " signedIn=true addorsub=false laststage=" + std::to_string(laststage),
							"Xr=>X0r,Xi=>X0i,Yr=>X1r,Yi=>X1i",
							"Sr=>Y1r,Si=>Y1i");
				}
				else
				{
	//				outportmap << "ReOut(" << output_width-1 <<  "downto 0)=>Y1r,ImOut(" << output_width-1 << " downto 0)=>Y1i";

					newInstance("FixComplexAdder",
							"FixComplexAdd",
							"msbin=" + std::to_string(msbin) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbout) + " lsbout=" +
							std::to_string(lsbin) + " signedIn=true addorsub=true laststage=" + std::to_string(laststage),
//							std::to_string(lsbout) + " signedIn=true addorsub=true laststage=" + std::to_string(laststage),
							"Xr=>X0r,Xi=>X0i,Yr=>X1r,Yi=>X1i",
							"Sr=>Y0r,Si=>Y0i");


				//cout << endl << "mmmmmmmmmmmmmmmmmmmmsbin=" << std::to_string(msbin) << " lsbin=" + std::to_string(lsbin) << " msbout=" << std::to_string(msbout) << " lsbout=" << 						std::to_string(lsbout) << " Twiddle_re=" << Twiddle_re << " Twiddle_im=" << Twiddle_im << " extrabit=" << extrabitstr << endl;

					if(extrabit)
					{
						newInstance("FixComplexAdder",
								"FixComplexSub",
								"msbin=" + std::to_string(msbin) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbout-1) + " lsbout=" +
						std::to_string(lsbin) + " signedIn=true addorsub=false laststage=" + std::to_string(laststage),
								"Xr=>X0r,Xi=>X0i,Yr=>X1r,Yi=>X1i",
								"Sr=>TwX1r,Si=>TwX1i");

						newInstance("FixComplexKCM",
								"FixTwiddleKCM",
								"msbIn=" + std::to_string(msbout-1) + " lsbIn=" + std::to_string(lsbin) + " lsbOut=" + std::to_string(lsbout) + " constantRe=" + Twiddle_re +
								" constantIm=" + 	Twiddle_im + " extrabit=" + extrabitstr, "ReIn=>TwX1r,ImIn=>TwX1i", "ReOut=>Y1r,ImOut=>Y1i");
								//" constantIm=" + 	Twiddle_im, "ReIn=>TwX1r,ImIn=>TwX1i", outportmap.str());
								//" constantIm=" + 	Twiddle_im, "ReIn=>TwX1r,ImIn=>TwX1i", "ReOut=>tempY1r,ImOut=>tempY1i");
					}
					else
					{
						newInstance("FixComplexAdder",
								"FixComplexSub",
								"msbin=" + std::to_string(msbin) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbout) + " lsbout=" +
						std::to_string(lsbin) + " signedIn=true addorsub=false laststage=" + std::to_string(laststage),
								"Xr=>X0r,Xi=>X0i,Yr=>X1r,Yi=>X1i",
								"Sr=>TwX1r,Si=>TwX1i");

						newInstance("FixComplexKCM",
								"FixTwiddleKCM",
								"msbIn=" + std::to_string(msbout) + " lsbIn=" + std::to_string(lsbin) + " lsbOut=" + std::to_string(lsbout) + " constantRe=" + Twiddle_re +
								" constantIm=" + 	Twiddle_im + " extrabit=" + extrabitstr, "ReIn=>TwX1r,ImIn=>TwX1i", "ReOut=>Y1r,ImOut=>Y1i");
			//					" constantIm=" + 	Twiddle_im, "ReIn=>TwX1r,ImIn=>TwX1i", outportmap.str());
			//					" constantIm=" + 	Twiddle_im, "ReIn=>TwX1r,ImIn=>TwX1i", "ReOut=>tempY1r,ImOut=>tempY1i");
					}


//					if (extrabit)
//					{
						
						//vhdl << tab << "Y1r <= tempY1r" << range(output_width-1, 0) << ";" << endl;
//					}
	//				vhdl << tab << "Y1r <= tempY1r" << range(output_width-1, 0) << ";" << endl;
	//				vhdl << tab << "Y1i <= tempY1i" << range(output_width-1, 0) << ";" << endl;


				}
			}
		}
		else
		{

			/*FixComplexKCM* FixTwiddleKCM = new FixComplexKCM(target, true, msbin, lsbin, lsbin, Twiddle_re, Twiddle_im);

			inPortMap (FixTwiddleKCM, "ReIn", "X1r");
			inPortMap (FixTwiddleKCM, "ImIn", "X1i");

			outPortMap(FixTwiddleKCM, "ReOut", "TwX1r");
			outPortMap(FixTwiddleKCM, "ImOut", "TwX1i");
			vhdl << instance(FixTwiddleKCM, "TwiddleKCMMUL_X1");

			FixComplexAdder* FixComplexAdd = new FixComplexAdder(nullptr, getTarget(), msbin, lsbin, msbout, lsbout, true, true);

			inPortMap (FixComplexAdd, "Xr", "X0r");
			inPortMap (FixComplexAdd, "Xi", "X0i");
			inPortMap (FixComplexAdd, "Yr", "TwX1r");
			inPortMap (FixComplexAdd, "Yi", "TwX1i");
			outPortMap(FixComplexAdd, "Sr", "Y0r");
			outPortMap(FixComplexAdd, "Si", "Y0i");
			vhdl << instance(FixComplexAdd, "FixComplexAdd_X0");

			FixComplexAdder* FixComplexSub = new FixComplexAdder(nullptr, getTarget(), msbin, lsbin, msbout, lsbout, true, false);

			inPortMap (FixComplexSub, "Xr", "X0r");
			inPortMap (FixComplexSub, "Xi", "X0i");
			inPortMap (FixComplexSub, "Yr", "TwX1r");
			inPortMap (FixComplexSub, "Yi", "TwX1i");
			outPortMap(FixComplexSub, "Sr", "Y0r");
			outPortMap(FixComplexSub, "Si", "Y0i");
			vhdl << instance(FixComplexAdd, "FixComplexAdd_X1");*/

			//string outportmap="";
			//outportmap += "ReOut=>TwX1r, " + "ImOut<=TwX1i" ;

			newInstance("FixComplexKCM",
					"FixTwiddleKCM",
					"msbIn=" + std::to_string(msbin) + " lsbIn=" + std::to_string(lsbin) + " lsbOut=" + std::to_string(lsbout) + " constantRe=" + Twiddle_re + " constantIm=" +  	Twiddle_im, "ReIn=>X1r,ImIn=>X1i", "ReOut=>TwX1r,ImOut=>TwX1i");

			newInstance("FixComplexAdder",
					"FixComplexAdd",
					"msbin=" + std::to_string(msbin) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbout) + " lsbout=" + std::to_string(lsbout) + " signedIn=true addorsub=true",
					"Xr=>X0r,Xi=>X0i,Yr=>TwX1r,Yi=>TwX1i",
					"Sr=>Y0r,Si=>Y0i");

			newInstance("FixComplexAdder",
					"FixComplexSub",
					"msbin=" + std::to_string(msbin) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbout) + " lsbout=" + std::to_string(lsbout) + " signedIn=true addorsub=false",
					"Xr=>X0r,Xi=>X0i,Yr=>TwX1r,Yi=>TwX1i",
					"Sr=>Y0r,Si=>Y0i");

		}


		/*IntTwiddleMultiplier* twiddleMultiplier = new IntTwiddleMultiplier(target, wI, wF, twiddleExponent, n, signedOperator);
		oplist.push_back(twiddleMultiplier);
		
		IntComplexAdder* addOperator =  new IntComplexAdder(target, 2*wI+1, 2*wF, signedOperator, inDelayMap("X",getCriticalPath()));
		oplist.push_back(addOperator);
		
		inPortMap (twiddleMultiplier, "X0r", "X1r");
		inPortMap (twiddleMultiplier, "X0i", "X1i");
		outPortMap(twiddleMultiplier, "Zr", "wX1r");
		outPortMap(twiddleMultiplier, "Zi", "wX1i");
		vhdl << instance(twiddleMultiplier, "TwiddleMUL_Y");
		
		vhdl << tab << declare("extendedX0r", 2*input_width) << " <= (" << input_width-1 << " downto 0 => X0r(" << input_width-1 << ")) & X0r;" << endl;
		vhdl << tab << declare("extendedX0i", 2*input_width) << " <= (" << input_width-1 << " downto 0 => X0i(" << input_width-1 << ")) & X0i;" << endl;
		
		vhdl << tab << declare("neg_wX1r", 2*input_width) << " <= wX1r xor (" << 2*input_width-1 << " downto 0 => \'1\');" << endl;
		vhdl << tab << declare("neg_wX1i", 2*input_width) << " <= wX1i xor (" << 2*input_width-1 << " downto 0 => \'1\');" << endl;
		
		inPortMap 	(addOperator, "X0r",	  "extendedX0r");
		inPortMap 	(addOperator, "X0i",	  "extendedX0i");
		inPortMap   (addOperator, "X1r",	  "wX1r");
		inPortMap   (addOperator, "X1i",	  "wX1i");
		inPortMapCst(addOperator, "Cini", "\'0\'");
		inPortMapCst(addOperator, "Cinr", "\'0\'");
		outPortMap	(addOperator, "Zr",	  "Y0r", false);
		outPortMap	(addOperator, "Zi",	  "Y0i", false);
		vhdl << instance(addOperator, "ADD_XwY");
		
		inPortMap 	(addOperator, "X0r",	  "extendedX0r");
		inPortMap 	(addOperator, "X0i",	  "extendedX0i");
		inPortMap   (addOperator, "X1r",	  "neg_wX1r");
		inPortMap   (addOperator, "X1i",	  "neg_wX1i");
		inPortMapCst(addOperator, "Cini", "\'1\'");
		inPortMapCst(addOperator, "Cinr", "\'1\'");
		outPortMap	(addOperator, "Zr",	  "Y1r", false);
		outPortMap	(addOperator, "Zi",	  "Y1i", false);
		vhdl << instance(addOperator, "SUB_XwY");*/
	
	}	


	FixComplexR2Butterfly::~FixComplexR2Butterfly()
	{
	}
	
	//FIXME: correct the emulate function
	void FixComplexR2Butterfly::emulate ( TestCase* tc ) {
/*		mpz_class svX0i = tc->getInputValue("X0i");
		mpz_class svX0r = tc->getInputValue("X0r");
		mpz_class svX1i = tc->getInputValue("X1i");
		mpz_class svX1r = tc->getInputValue("X1r");
		mpz_class svWi = getTwiddleConstant(TWIDDLEIM);
		mpz_class svWr = getTwiddleConstant(TWIDDLERE);
		
		
		if(twiddleExponent == 0){
			mpz_class svZi = svX0i;
			mpz_class svZr = svX0r;
		}else if((double)twiddleExponent == (double)n/4.0){
			mpz_class svZi = svX0r;
			mpz_class svZr = (-1)*svX0i;
		}else{
			if (! signedOperator){

				mpz_class svWX1i = svX1i*svWr - svX1r*svWr;
				mpz_class svWX1r = svX1r*svWr + svX1i*svWi;
				
				mpz_class svY0i = svX0i + svWX1i;
				mpz_class svY0r = svX0r + svWX1r;
				
				mpz_class svY1i = svX0i - svWX1i;
				mpz_class svY1r = svX0r - svWX1r;
				
				// Don't allow overflow
				mpz_clrbit ( svY0i.get_mpz_t(), 2*input_width );
				mpz_clrbit ( svY0r.get_mpz_t(), 2*input_width );
				mpz_clrbit ( svY1i.get_mpz_t(), 2*input_width );
				mpz_clrbit ( svY1r.get_mpz_t(), 2*input_width );

				tc->addExpectedOutput("Y0i", svY0i);
				tc->addExpectedOutput("Y0r", svY0r);
				tc->addExpectedOutput("Y1i", svY1i);
				tc->addExpectedOutput("Y1r", svY1r);
			}else{
				mpz_class big1 = (mpz_class(1) << (input_width));
				mpz_class big1P = (mpz_class(1) << (input_width-1));
				mpz_class big2 = (mpz_class(1) << (input_width));
				mpz_class big2P = (mpz_class(1) << (input_width-1));

				if ( svX0i >= big1P)
					svX0i = svX0i - big1;
				if ( svX0r >= big1P)
					svX0r = svX0i - big1;

				if ( svX1i >= big2P)
					svX1i = svX1i - big2;
				if ( svX1r >= big2P)
					svX1r = svX1r - big2;
				
				mpz_class svX1rWr = svX1r*svWr;
				mpz_class svX1iWi = svX1i*svWi;
				mpz_class svX1rWi = svX1r*svWi;
				mpz_class svX1iWr = svX1i*svWr;
				
				if ( svX1rWr < 0){
					mpz_class tmpSUB = (mpz_class(1) << (2*input_width));
					svX1rWr = tmpSUB + svX1rWr; 
				}
				if ( svX1iWi < 0){
					mpz_class tmpSUB = (mpz_class(1) << (2*input_width));
					svX1iWi = tmpSUB + svX1iWi; 
				}
				if ( svX1iWr < 0){
					mpz_class tmpSUB = (mpz_class(1) << (2*input_width));
					svX1iWr = tmpSUB + svX1iWr; 
				}
				if ( svX1rWi < 0){
					mpz_class tmpSUB = (mpz_class(1) << (2*input_width));
					svX1rWi = tmpSUB + svX1rWi; 
				}
				
				mpz_class svWX1i = svX1iWr - svX1rWr;
				mpz_class svWX1r = svX1rWr + svX1iWi;
				 
				// Don't allow overflow
				mpz_clrbit ( svWX1i.get_mpz_t(), 2*input_width );
				mpz_clrbit ( svWX1r.get_mpz_t(), 2*input_width );
				
				mpz_class svY0i = svX0i + svWX1i;
				mpz_class svY0r = svX0r + svWX1r;
				
				mpz_class svY1i = svX0i - svWX1i;
				mpz_class svY1r = svX0r - svWX1r;
				
				// Don't allow overflow
				mpz_clrbit ( svY0i.get_mpz_t(), 2*input_width );
				mpz_clrbit ( svY0r.get_mpz_t(), 2*input_width );
				mpz_clrbit ( svY1i.get_mpz_t(), 2*input_width );
				mpz_clrbit ( svY1r.get_mpz_t(), 2*input_width );
				
				tc->addExpectedOutput("Y0i", svY0i);
				tc->addExpectedOutput("Y0r", svY0r);
				tc->addExpectedOutput("Y1i", svY1i);
				tc->addExpectedOutput("Y1r", svY1r);
			}
		}*/
	}
	
	/*mpz_class FixComplexR2Butterfly::getTwiddleConstant(int constantType){
		mpfr_t twiddleExp, twiddleIm, twiddleRe, constPi, temp;
		mpz_class intTemp;
		
		mpfr_init2(twiddleIm, 	10*input_width);
		mpfr_init2(twiddleRe, 	10*input_width);
		mpfr_init2(twiddleExp, 	10*input_width);
		mpfr_init2(constPi, 	10*input_width);
		mpfr_init2(temp, 		10*input_width);
		
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
	} */

	OperatorPtr FixComplexR2Butterfly::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int msbin;
		UserInterface::parseInt(args, "msbin", &msbin);
		int lsbin;
		UserInterface::parseInt(args, "lsbin", &lsbin);
		int msbout;
		UserInterface::parseInt(args, "msbout", &msbout);
		int lsbout;
		UserInterface::parseInt(args, "lsbout", &lsbout);
		string Twiddle_re;
		UserInterface::parseString(args, "Twiddle_re", &Twiddle_re);
		string Twiddle_im;
		UserInterface::parseString(args, "Twiddle_im", &Twiddle_im);
		bool signedIn;
		UserInterface::parseBoolean(args, "signedIn", &signedIn);
		bool bypassmult;
		UserInterface::parseBoolean(args, "bypassmult", &bypassmult);
		bool decimation;
		UserInterface::parseBoolean(args, "decimation", &decimation);
		bool extrabit;
		UserInterface::parseBoolean(args, "extrabit", &extrabit);
		bool laststage;
		UserInterface::parseBoolean(args, "laststage", &laststage);

		return new FixComplexR2Butterfly(parentOp, target, msbin, lsbin, msbout, lsbout, Twiddle_re, Twiddle_im, signedIn, bypassmult, decimation, extrabit, laststage);
	}
	
	void FixComplexR2Butterfly::registerFactory(){
		UserInterface::add("FixComplexR2Butterfly", // name
											 "A Complex Radix-2 Butterfly.",
											 "Complex", // categories
											 "",
											 "msbin(int): integer input most significant bit;\
							lsbin(int): integer input least significant bit;\
							msbout(int): integer output most significant bit;\
							lsbout(int): integer output least significant bit;\
							Twiddle_re(string): string  Real part of twiddle factor;\
							Twiddle_im(string): string  imaginary part of twiddle factor;\
							signedIn(bool)=true: if false calculation will be unsigned ;\
							bypassmult(bool)=false: if true bypass coefficient multiplication ;\
							decimation(bool)=true: if true(default) DIF, else DIT FFT ;\
							extrabit(bool)=true: if true (default) add one extra bit in fixcomplexkcm msbout, else no extra bit ;\
							laststage(bool)=false: if false (default) is not last stage, if true, last stage of FFT",
						 	"See the developper manual in the doc/ directory of FloPoCo.", 
			 				FixComplexR2Butterfly::parseArguments
							) ;

	}

}




















