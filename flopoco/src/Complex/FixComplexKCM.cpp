#include <iostream>
#include <sstream>
#include <sollya.h>


#include "ConstMult/FixRealKCM.hpp"
#include "FixComplexKCM.hpp"

// TODO bug when signedIn=false, not sure it is in the operator or in emulate.
// Let's investigate only if there is a customer...
// Meanwhile the option signedIn is forced to true

using namespace std;
namespace flopoco {

	void FixComplexKCM::init()
	{
		if(lsb_in>msb_in) 
		{
			throw string("FixComplexKCM: Error, lsbIn>msbIn");
		}

		// definition of the source file name, used for info and error reporting
		// using REPORT 
		srcFileName="FixComplexKCM";

		// definition of the name of the operator
		ostringstream name;
		name << "FixComplexKCM_" << vhdlize(msb_in) <<"_" << vhdlize(lsb_in) 
			<< "_" << vhdlize(lsb_out) << "_" << vhdlize(constant_re) << "_" <<
			vhdlize(constant_im) << "_" << ((signedIn) ? "" : "un") <<
			"signed" ;

		setNameWithFreqAndUID(name.str());
		
		// Copyright 
		setCopyrightString("3IF 2015 dev team (2015-2019)");

		input_width = 1 + msb_in - lsb_in;
		
		// declaring inputs
		addInput ("ReIn" , input_width);
		addInput ("ImIn" , input_width);

		//Computing constants for testBench and in order to know constant width
		sollya_obj_t nodeIm, nodeRe;	
		nodeRe = sollya_lib_parse_string(constant_re.c_str());
		
		if(sollya_lib_obj_is_error(nodeRe))
		{
			ostringstream error;
			error << srcFileName <<" : Unable to parse string \""  <<
				constant_re << "\" as a numeric constant" << endl;
			throw error.str();
		}
		
		nodeIm = sollya_lib_parse_string(constant_im.c_str());
		if(sollya_lib_obj_is_error(nodeIm))
		{
			ostringstream error;
			error << srcFileName <<" : Unable to parse string \""  <<
				constant_im << "\" as a numeric constant" << endl;
			throw error.str();
		}

		mpfr_inits2(10000, mpfr_constant_re, mpfr_constant_im, NULL);

		sollya_lib_get_constant(mpfr_constant_re, nodeRe);
		sollya_lib_get_constant(mpfr_constant_im, nodeIm);

		constantReNeg = (mpfr_sgn(mpfr_constant_re) < 0);
		constantImNeg = (mpfr_sgn(mpfr_constant_im) < 0);

		mpfr_t log2C;
		mpfr_init2(log2C, 100); 
		
		//Constant real part width
		mpfr_log2(log2C, mpfr_constant_re, GMP_RNDN);
		constantReMsb = mpfr_get_si(log2C, GMP_RNDU);

		//Constant imaginary part width
		mpfr_log2(log2C, mpfr_constant_im, GMP_RNDN);
		constantImMsb = mpfr_get_si(log2C, GMP_RNDU);

		mpfr_clear(log2C);

		int constantMaxMSB = max(constantReMsb, constantImMsb);	

		//Do we need an extra sign bit ?
		bool extraSignBitRe = !signedIn && (constantReNeg || !constantImNeg);
		bool extraSignBitIm = !signedIn && (constantReNeg || constantImNeg);

		int msbout_re, msbout_im; 
		
		//Do we need an extra bit to prevent overflow in complex addition?
		if(extrabit)
			msbout_re = msbout_im = msb_in + constantMaxMSB +1;
		else
			msbout_re = msbout_im = msb_in + constantMaxMSB;


		if(extraSignBitRe)
		{
			msbout_re++;
		}
		if(extraSignBitIm)
		{
			msbout_im++;
		}

		REPORT(INFO, "Computed MSBout for real and im parts: " << msbout_re);	
		
		outputre_width = msbout_re - lsb_out + 1;
		outputim_width = msbout_im - lsb_out + 1;

		if(outputre_width < 0 || outputim_width < 0)
		{
			THROWERROR("Computed msb will be lower than asked lsb."
					" Result would always be zero ");
		}

	}

	
	int 	FixComplexKCM::get_MSBout(){
		return msbout_re;
	}



	FixComplexKCM::FixComplexKCM(
			OperatorPtr parentOp, 
			Target* target,
			bool signedIn,
			int msb_in, 
			int lsb_in, 
			int lsb_out,
			string constant_re,
			string constant_im,
			bool extrabit
		): 	
		Operator(parentOp, target),
			signedIn(signedIn),
			msb_in(msb_in),
			lsb_in(lsb_in),
			lsb_out(lsb_out),
			constant_re(constant_re),
			constant_im(constant_im),
			extrabit(extrabit)
	{
		init();

		// declaring output
		addOutput("ReOut", outputre_width);
		addOutput("ImOut", outputim_width);

		if(mpfr_zero_p(mpfr_constant_im) != 0 && mpfr_zero_p(mpfr_constant_re) != 0)
		{
			vhdl << tab << "ReOut" << " <= " << zg(outputre_width, 0) << ";" <<
				endl;
			vhdl << tab << "ImOut" << " <= " << zg(outputim_width, 0) << ";" <<
				endl;
		}
		else
		{

			FixRealKCM* kcmInReConstRe = new FixRealKCM( this, "ReIn", signedIn, msb_in, lsb_in, lsb_out, constant_re,true  );
			FixRealKCM* kcmInReConstIm = new FixRealKCM( this, "ReIn", signedIn, msb_in, lsb_in, lsb_out, constant_im,false );
			FixRealKCM* kcmInImConstRe = new FixRealKCM( this, "ImIn", signedIn, msb_in, lsb_in, lsb_out, constant_re,true  );
			FixRealKCM* kcmInImConstIm = new FixRealKCM( this, "ImIn", signedIn, msb_in, lsb_in, lsb_out, "-("+constant_im+")", false );
			
			double errorInUlpsRe = kcmInReConstRe->getErrorInUlps() + kcmInImConstIm->getErrorInUlps();
			double errorInUlpsIm = kcmInReConstIm->getErrorInUlps() + kcmInImConstRe->getErrorInUlps();
			int guardBits_re = intlog2(errorInUlpsRe)+1;
			int guardBits_im = intlog2(errorInUlpsIm)+1;
//			int guardBits_re = ceil(log2(errorInUlpsRe))+1;
//			int guardBits_im = ceil(log2(errorInUlpsIm))+1;

			BitHeap* bitheapRe = new BitHeap(
					this,
					 outputre_width + guardBits_re
				);
			BitHeap* bitheapIm = new BitHeap(
					this, 
					outputim_width + guardBits_im 
				);


			kcmInReConstRe->addToBitHeap(bitheapRe, guardBits_re);
			kcmInImConstIm->addToBitHeap(bitheapRe, guardBits_re);
			kcmInReConstIm->addToBitHeap(bitheapIm, guardBits_im);
			kcmInImConstRe->addToBitHeap(bitheapIm, guardBits_im);

			bitheapIm->startCompression();
			bitheapRe->startCompression();

			vhdl << "ImOut" << " <= " << 
				bitheapIm->getSumName(
						outputim_width+guardBits_im -1,
						guardBits_im
					) << ";" << endl;

			vhdl << "ReOut" << " <= " << 
				bitheapRe->getSumName(
						outputre_width + guardBits_re - 1,
						guardBits_re
					) << ";" << endl;
		}

	};

	FixComplexKCM::~FixComplexKCM()
	{
		mpfr_clears(mpfr_constant_im, mpfr_constant_re, NULL);
	}
	




	void FixComplexKCM::emulate(TestCase * tc) {

		/* first we are going to format the entries */
		mpz_class reIn = tc->getInputValue("ReIn");
		mpz_class imIn = tc->getInputValue("ImIn");


		/* Sign handling */
		// Msb index counting from one
		bool reInNeg = (
				signedIn &&
				(mpz_tstbit(reIn.get_mpz_t(), input_width - 1) == 1)
			);

		bool imInNeg = (
				signedIn && 
				(mpz_tstbit(imIn.get_mpz_t(), input_width - 1) == 1)
			);

		// 2's complement -> absolute value unsigned representation
		if(reInNeg)
		{
			reIn = (mpz_class(1) << input_width) - reIn;
		}

		if(imInNeg)
		{
			imIn = (mpz_class(1) << input_width) - imIn;
		}

		//Cast to mp floating point number
		mpfr_t reIn_mpfr, imIn_mpfr;
		mpfr_init2(reIn_mpfr, input_width + 1);
		mpfr_init2(imIn_mpfr, input_width + 1);

		//Exact
		mpfr_set_z(reIn_mpfr, reIn.get_mpz_t(), GMP_RNDN); 
		mpfr_set_z(imIn_mpfr, imIn.get_mpz_t(), GMP_RNDN);

		//Scaling : Exact
		mpfr_mul_2si(reIn_mpfr, reIn_mpfr, lsb_in, GMP_RNDN);
		mpfr_mul_2si(imIn_mpfr, imIn_mpfr, lsb_in, GMP_RNDN);

		mpfr_t re_prod, im_prod, crexim_prod, xrecim_prod;
		mpfr_t reOut, imOut;

		mpfr_inits2(
				2 * input_width + 1, 
				re_prod, 
				im_prod, 
				crexim_prod, 
				xrecim_prod, 
				NULL
			);

		mpfr_inits2(5 * max(outputim_width, outputre_width) + 1, reOut, imOut, NULL);

		// c_r * x_r -> re_prod
		mpfr_mul(re_prod, reIn_mpfr, mpfr_constant_re, GMP_RNDN);

		// c_i * x_i -> im_prod
		mpfr_mul(im_prod, imIn_mpfr, mpfr_constant_im, GMP_RNDN);

		// c_r * x_i -> crexim_prod
		mpfr_mul(crexim_prod, mpfr_constant_re, imIn_mpfr, GMP_RNDN);

		// x_r * c_im -> xrecim_prod
		mpfr_mul(xrecim_prod, reIn_mpfr, mpfr_constant_im, GMP_RNDN);

		/* Input sign correction */
		if(reInNeg)
		{
			//Exact
			mpfr_neg(re_prod, re_prod, GMP_RNDN);
			mpfr_neg(xrecim_prod, xrecim_prod, GMP_RNDN);
		}

		if(imInNeg)
		{
			//Exact
			mpfr_neg(im_prod, im_prod, GMP_RNDN);
			mpfr_neg(crexim_prod, crexim_prod, GMP_RNDN);
		}

		mpfr_sub(reOut, re_prod, im_prod, GMP_RNDN);
		mpfr_add(imOut, crexim_prod, xrecim_prod, GMP_RNDN);

		bool reOutNeg = (mpfr_sgn(reOut) < 0);
		bool imOutNeg = (mpfr_sgn(imOut) < 0);

		if(reOutNeg)
		{
			//Exact
			mpfr_abs(reOut, reOut, GMP_RNDN);
		}

		if(imOutNeg)
		{
			//Exact
			mpfr_abs(imOut, imOut, GMP_RNDN);
		}

		//Scale back (Exact)
		mpfr_mul_2si(reOut, reOut, -lsb_out,  GMP_RNDN);
		mpfr_mul_2si(imOut, imOut, -lsb_out, GMP_RNDN);

		//Get bits vector
		mpz_class reUp, reDown, imUp, imDown, carry;

		mpfr_get_z(reUp.get_mpz_t(), reOut, GMP_RNDU);
		mpfr_get_z(reDown.get_mpz_t(), reOut, GMP_RNDD);
		mpfr_get_z(imDown.get_mpz_t(), imOut, GMP_RNDD);
		mpfr_get_z(imUp.get_mpz_t(), imOut, GMP_RNDU);
		carry = 0;

		//If result was negative, compute 2's complement
		if(reOutNeg)
		{
			reUp = (mpz_class(1) << outputre_width) - reUp;
			reDown = (mpz_class(1) << outputre_width) - reDown;
		}

		if(imOutNeg)
		{
			imUp = (mpz_class(1) << outputim_width) - imUp;
			imDown = (mpz_class(1) << outputim_width) - imDown;
		}

		//Handle border cases
		if(imUp > (mpz_class(1) << outputim_width) - 1 )
		{
			imUp = 0;
		}

		if(reUp > (mpz_class(1) << outputre_width) - 1)
		{
			reUp = 0;
		}

		if(imDown > (mpz_class(1) << outputim_width) - 1 )
		{
			imDown = 0;
		}

		if(reDown > (mpz_class(1) << outputre_width) - 1)
		{
			reDown = 0;
		}

				//cout << reUp << " " << reDown << " " << imUp << " " << imDown << endl;
		//Add expected results to corresponding outputs
		tc->addExpectedOutput("ReOut", reUp);	
		tc->addExpectedOutput("ReOut", reDown);	
		tc->addExpectedOutput("ImOut", imUp);	
		tc->addExpectedOutput("ImOut", imDown);	

		mpfr_clears(
				reOut,
				imOut, 
				re_prod, 
				im_prod, 
				crexim_prod, 
				xrecim_prod, 
				reIn_mpfr, 
				imIn_mpfr,
				NULL
			);
	}


	void FixComplexKCM::buildStandardTestCases(TestCaseList * tcl) {
		TestCase* tc;

		int one = 1;
		if(lsb_in < 0 && msb_in >= 0)
		{
			//Real one
			one = one << -lsb_in;
		}

		tc = new TestCase(this);		
		tc->addInput("ReIn", 0);
		tc->addInput("ImIn", 0);
		emulate(tc);
		tcl->add(tc);
		
		tc = new TestCase(this);		
		tc->addInput("ReIn", one);
		tc->addInput("ImIn", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);		
		tc->addInput("ReIn", 0);
		tc->addInput("ImIn", one);
		emulate(tc);
		tcl->add(tc);
		
		tc = new TestCase(this);		
		tc->addInput("ReIn", one);
		tc->addInput("ImIn", one);
		emulate(tc);
		tcl->add(tc);

		if(signedIn)
		{
			tc = new TestCase(this);		
			tc->addInput("ReIn", -1 * one);
			tc->addInput("ImIn", -1 * one);
			emulate(tc);
			tcl->add(tc);
		}
#if 0
		tc = new TestCase(this);		
		tc->addInput("ReIn", 2 * one);
		tc->addInput("ImIn", 0);
		emulate(tc);
		tcl->add(tc);
#endif
	}

	OperatorPtr FixComplexKCM::parseArguments(OperatorPtr parentOp, Target* target, std::vector<std::string> &args)
	{
		int lsbIn, lsbOut, msbIn;
		//		bool signedIn;
		string constantIm, constantRe;
		bool extrabit;
		UserInterface::parseInt(args, "lsbIn", &lsbIn);
		UserInterface::parseString(args, "constantIm", &constantIm);
		UserInterface::parseString(args, "constantRe", &constantRe);
		UserInterface::parseInt(args, "lsbOut", &lsbOut);
		UserInterface::parseInt(args, "msbIn", &msbIn);
		UserInterface::parseBoolean(args, "extrabit", &extrabit);

		// UserInterface::parseBoolean(args, "signedIn", &signedIn);
		return new FixComplexKCM(
														 parentOp,
				target, 
				true, //signedIn,
				msbIn,
				lsbIn,
				lsbOut,
				constantRe, 
				constantIm,
				extrabit
			);
	}

	void FixComplexKCM::registerFactory()
	{
		UserInterface::add(
				"FixComplexKCM",
				"Table-based complex multiplier. Inputs are two's complement. Output size is computed",
				"ConstMultDiv",
				"",
				//				"signedIn(bool)=true: 0=unsigned, 1=signed;
				"msbIn(int): weight associated to most significant bit (including sign bit);\
				lsbIn(int): weight associated to least significant bit;\
				lsbOut(int): weight associated to output least significant bit; \
				constantRe(string): real part of the constant, given as a Sollya expression, e.g \"log(2)\"; \
				constantIm(string): imaginary part of the constant, given as a Sollya expression, e.g \"log(2)\"; \
				extrabit(bool)=true: do we need extra bit for addition",
				"",
				FixComplexKCM::parseArguments
		);
	}



}//namespace

