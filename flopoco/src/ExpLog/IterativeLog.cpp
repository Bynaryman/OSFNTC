/*
  An FP logarithm for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

*/

// TODO List:
//  * test cases for boundary cases pfinal etc
//  * finetune pipeline
//  * Port back the Arith paper
#include <fstream>
#include <sstream>
#include <math.h>	// for NaN
#include "IterativeLog.hpp"
#include "TestBenches/FPNumber.hpp"
#include "utils.hpp"
#include "IntMult/IntSquarer.hpp"
#include "ConstMult/IntIntKCM.hpp"
#include "UserInterface.hpp"


using namespace std;


namespace flopoco{



	IterativeLog::FirstInvTable::FirstInvTable(Target* target, int wIn, int wOut) :
		Table(target, wIn, wOut)
	{
		ostringstream name;
		name <<"InvTable_0_"<<wIn<<"_"<<wOut;
		setNameWithFreqAndUID(name.str());

	}

	IterativeLog::FirstInvTable::~FirstInvTable() {}


	int    IterativeLog::FirstInvTable::double2input(double x){
		int result;
		cerr << "??? IterativeLog::FirstInvTable::double2input not yet implemented ";
		exit(1);
		return result;
	}


	double IterativeLog::FirstInvTable::input2double(int x) {
		double y;
		if(x>>(wIn-1)) //MSB of input
			y= ((double)(x+(1<<wIn))) //      11xxxx (binary)
				/  ((double)(1<<(wIn+1))); // 0.11xxxxx (binary)
		else
			y= ((double)(x+(1<<wIn))) //   10xxxx (binary)
				/  ((double)(1<<(wIn))); // 1.0xxxxx (binary)
		return(y);
	}

	mpz_class IterativeLog::FirstInvTable::double2output(double x){
		mpz_class result;
		result =  mpz_class(floor(x*((double)(1<<(wOut-1)))));
		return result;
	}

	double IterativeLog::FirstInvTable::output2double(mpz_class x) {
		double y=((double)x.get_si()) /  ((double)(1<<(wOut-1)));
		return(y);
	}


	mpz_class IterativeLog::FirstInvTable::function(int x)
	{
		double d;
		mpz_class r;
		d=input2double(x);
		r =  ceil( ((double)(1<<(wOut-1))) / d); // double rounding, but who cares really
		// The following line allows us to prove that the case a0=5 is lucky enough to need one bit less than the general case
		//cout << "FirstInvTable> x=" << x<< "  r=" <<r << "  error=" << ceil( ((double)(1<<(wOut-1))) / d)  - ( ((double)(1<<(wOut-1))) / d) << endl;
		return r;
	}



#if 0
	int IterativeLog::FirstInvTable::check_accuracy(int wF) {
		int i;
		mpz_class j;
		double x1,x2,y,e1,e2;
		double maxerror=0.0;
		double prod=0.0;

		maxMulOut=0;
		minMulOut=2;

		for (i=minIn; i<=maxIn; i++) {
			// x1 and x2 are respectively the smallest and largest FP possible
			// values leading to input i
			x1=input2double(i);
			if(i>>(wIn-1)) //MSB of input
				x2= - negPowOf2(wF)          // <--wF --->
					+ ((double)(i+1+(1<<wIn))) //   11 11...11 (binary)
					/ ((double)(1<<(wIn+1))); // 0.11 11...11 (binary)
			else
				x2= - negPowOf2(wF-1)
					+ ((double)(i+1+(1<<wIn))) //  10 11...11 (binary)
					/ ((double)(1<<(wIn))); // 1.0 11...11 (binary)
			j=function(i);
			y=output2double(j);
			if(UserInterface::verbose)
				cout << "i="<<i<< " ("<<input2double(i)<<") j="<<j
					  <<" min="<< x1*y <<" max="<< x2*y<< endl;
			prod=x1*y; if (prod<minMulOut) minMulOut=prod;
			prod=x2*y; if (prod>maxMulOut) maxMulOut=prod;
			e1= fabs(x1*y-1); if (e1>maxerror) maxerror=e1;
			e2= fabs(x2*y-1); if (e2>maxerror) maxerror=e2;
		}
		cout << "FirstInvTable: Max error=" <<maxerror << "  log2=" << log2(maxerror) <<endl;
		cout << "               minMulOut=" <<minMulOut << " maxMulOut=" <<maxMulOut  <<endl;

		printf("%1.30e\n", log2(maxerror));

		return (int) (ceil(log2(maxerror)));
	}

#endif







	IterativeLog::FirstLogTable::FirstLogTable(Target *target, int wIn, int wOut, FirstInvTable* fit, IterativeLog* op_) :
		Table(target, wIn, wOut), fit(fit), op(op_)
	{
		ostringstream name;
		name <<"LogTable_0_"<<wIn<<"_"<<wOut;
		setNameWithFreqAndUID(name.str());

		minIn = 0;
		maxIn = (1<<wIn) -1;
		if (wIn!=fit->wIn) {
			cerr<< "IterativeLog::FirstLogTable::FirstLogTable should use same wIn as FirstInvTable"<<endl;
			exit(1);
		}
	}

	IterativeLog::FirstLogTable::~FirstLogTable() {}


	int    IterativeLog::FirstLogTable::double2input(double x){
		int result;
		cerr << "??? IterativeLog::FirstLogTable::double2input not yet implemented ";
		exit(1);
		//   result = (int) floor(x*((double)(1<<(wIn-1))));
		//   if( result < minIn || result > maxIn) {
		//     cerr << "??? IterativeLog::FirstLogTable::double2input:  Input "<< result <<" out of range ["<<minIn<<","<<maxIn<<"]";
		//     exit(1);
		//  }
		return result;
	}

	double IterativeLog::FirstLogTable::input2double(int x) {
		return(fit->input2double(x));
	}

	mpz_class    IterativeLog::FirstLogTable::double2output(double y){
		//Here y is between -0.5 and 0.5 strictly, whatever wIn.  Therefore
		//we multiply y by 2 before storing it, so the table actually holds
		//2*log(1/m)
		double z = floor(2*y*((double)(1<<(wOut-1))));

		// otherwise, signed arithmetic on wOut bits
		if(z>=0)
			return (mpz_class) z;
		else
			return (z + (double)(1<<wOut));

	}

	double IterativeLog::FirstLogTable::output2double(mpz_class x) {
		cerr<<" IterativeLog::FirstLogTable::output2double TODO"; exit(1);
		//  double y=((double)x) /  ((double)(1<<(wOut-1)));
		//return(y);
	}


	mpz_class IterativeLog::FirstLogTable::function(int x)
	{
		mpz_class result;
		double apprinv;
		mpfr_t i,l;
		mpz_t r;

		mpfr_init(i);
		mpfr_init2(l,wOut);
		mpz_init2(r,400);
		apprinv = fit->output2double(fit->function(x));;
		// result = double2output(log(apprinv));
		mpfr_set_d(i, apprinv, GMP_RNDN);
		mpfr_log(l, i, GMP_RNDN);
		mpfr_neg(l, l, GMP_RNDN);

		// Remove the sum of small offsets that are added to the other log tables
		for(int j=1; j<=op->stages; j++){
			mpfr_set_d(i, 1.0, GMP_RNDN);
			int pi=op->p[j];
			mpfr_mul_2si(i, i, -2*pi, GMP_RNDN);
			mpfr_sub(l, l, i,  GMP_RNDN);
		}

		// code the log in 2's compliment
		mpfr_mul_2si(l, l, wOut, GMP_RNDN);
		mpfr_get_z(r, l, GMP_RNDN);
		result = mpz_class(r); // signed

		// This is a very inefficient way of converting
		mpz_class t = mpz_class(1) << wOut;
		result = t+result;
		if(result>t) result-=t;

		//  cout << "x="<<x<<" apprinv="<<apprinv<<" logapprinv="<<log(apprinv)<<" result="<<result<<endl;
		mpfr_clear(i);
		mpfr_clear(l);
		mpz_clear(r);
		return  result;
	}



	 // A table of log
	 // -log(1-x) where x < 2^-p and x on a+1 bits.
	 // the log is then smaller than 2^-p+1
	 //  outputPrecision is the weight of the last bit in the real domain
	 IterativeLog::OtherLogTable::OtherLogTable(Target* target, int wIn, int outputPrecision, int which, int ai, int pi) :
		 Table(target, wIn, outputPrecision, 0, -1, 1),  which(which), ai(ai), pi(pi)
		// TODO this forces a logic-based table

	 {
		 ostringstream name;
		 name <<"LogTable_"<<which<<"_"<<wIn<<"_"<<wOut;
		 setNameWithFreqAndUID(name.str());
	 }

	 IterativeLog::OtherLogTable::~OtherLogTable() {}


	 int    IterativeLog::OtherLogTable::double2input(double x){
		 int result;
		 cerr << "??? IterativeLog::OtherLogTable::double2input not yet implemented ";
		 exit(1);
		 return result;
	 }


	 double IterativeLog::OtherLogTable::input2double(int x) {
		 double d;
		 // function uses log1p, so we prepare d for that
		 // computation in double is exact as long as we don't want a quad
		 // operator...

		 double Ei;
		 if((which>1) || (which==1 && (1==(x>>(wIn-1)))) )
			 Ei = 1.0 / ((double) (((uint64_t) 1)<<(2*pi)));
		 else
			 Ei = 1.0 / ((double) (((uint64_t) 1)<<(2*pi+1)));

		 d = ((double) (-x))   /   ((double) (((uint64_t) 1)<<(pi+wIn)));

		 //cout << endl << d << " " << Ei << "   " ;
		 d += Ei;
		 //cout << d;
		 return d;
	 }





	 mpz_class    IterativeLog::OtherLogTable::double2output(double y){
		 mpz_class z;
		 z = (mpz_class) (  y *  ((double)(((int64_t)1)<< outputPrecision)) );
		 // TODO fix for more than 64-bit
		 //  z = (mpz_class(1)<< outputPrecision)  (  y *  ((double)(((int64_t)1)) );
		 if (0 != z>>wOut) {
			 cerr<<"IterativeLog::OtherLogTable::double2output: output does not fit output format"<<endl;
		 }
		 return z;
	 }



	 double IterativeLog::OtherLogTable::output2double(mpz_class x) {
		 cerr<<" IterativeLog::OtherLogTable::output2double TODO"; exit(1);
		 //double y=((long double)x) /  ((long double)(1<<outputPrecision));
		 //return(y);
	 }





	 mpz_class IterativeLog::OtherLogTable::function(int x) {
		 mpz_class result;
		 double apprinv;
		 mpfr_t i,l;
		 mpz_t r;

		 mpfr_init(i);
		 mpfr_init2(l,wOut);
		 mpz_init2(r,400);


		 apprinv = input2double(x);
		 mpfr_set_d(i, apprinv, GMP_RNDN);
		 mpfr_log1p(l, i, GMP_RNDN);
		 mpfr_neg(l, l, GMP_RNDN);
		 // cout << "which=" << which <<  " div" << (pi+wIn+1) << "  x=" << x << "  apprinv=" << apprinv << "  l=" << mpfr_get_d(l, GMP_RNDN) << endl;
		 // Add the small offset that ensures that it never gets negative (the sum of these offsets will be subtracted to L0)
		 mpfr_set_d(i, 1.0, GMP_RNDN);
		 mpfr_mul_2si(i, i, -2*pi, GMP_RNDN);
		 mpfr_add(l, l, i,  GMP_RNDN);

		 mpfr_mul_2si(l, l, pi+wOut, GMP_RNDN);
		 mpfr_get_z(r, l, GMP_RNDN);
		 result=mpz_class(r);
		 mpfr_clear(i);
		 mpfr_clear(l);
		 mpz_clear(r);
		 return  result;
	 }









	IterativeLog::IterativeLog(Target* target,
	             int wE, int wF,
	             int inTableSize,
               map<string, double> inputDelays)
		: Operator(target), wE(wE), wF(wF)
	{

		setCopyrightString("F. de Dinechin, C. Klein  (2008-2011)");

		ostringstream o;
		srcFileName = "IterativeLog";

		o << "IterativeLog_" << wE << "_" << wF << "_" << inTableSize << "_";
		if(getTarget()->isPipelined())
			o << getTarget()->frequencyMHz() ;
		else
			o << "comb";
		setNameWithFreqAndUID(o.str());

		addFPInput("X", wE, wF);
		addFPOutput("R", wE, wF, 2); // 2 because faithfully rounded

		int i, bitsPerStage;

		if (inTableSize==0) {
			if (wF>=20)
				bitsPerStage=12;
			else
				bitsPerStage=8; // somehow arbitrary
		}
		else
			bitsPerStage=inTableSize;

		if(bitsPerStage <6)
			throw string("IterativeLog error: tables need at least 6 input bits");



		// First compute the parameters of each iteration

		// stage 0, needs a specific inverter table
		p[0] = 0;
		a[0] = bitsPerStage;
		// How many bits have been zeroed?  a0=5 is a lucky case but it messes up the following
		p[1] = a[0]-2; // if a0==5, would be a0-1

		// The number of guard bit
		// For each stage: 0.5 ulp for rounding the log table
		// For each stage after the first one:
		//                 1 ulp for truncating Z
		//                 1 ulp for truncating P
		//                 1 ulp for truncating EiY
		//        total 3.5 bit/stage
		// Plus for the rest of the computation:
		//                 1 ulp for the approximation error of neglecting the 3rd-order term
		//                 1 ulp for truncating Z before input to the square
		//                 1 ulp for truncating Z^2

		// the iterations i=0 and i=1 lead to no truncation

		i=1;
		gLog=4;
		while(3*p[i]+1 <= p[i]+wF+gLog){ // while the third-order term of log(1+Zi) is not negligible
			if(i==1) { 	// stage 1 cannot be more accurate than 2p1-1
				a[1] = p[1];
				p[2] = 2*p[1]-1;
			}
			else {
				a[i] = bitsPerStage;
				p[i+1] = p[i] + a[i] - 1; // and we zero out a[i]-1 bits
			}
			i++;
			gLog=max(4, intlog2(3+0.5+0.5+3*i-1));
		}

		// The number of stages, not counting stage 0
		stages = i-1;
		gLog=max(4, intlog2(3+0.5+0.5+3*stages));

		if(UserInterface::verbose>=2) {
			cerr << "> IterativeLog\t Initial parameters:" << endl;
			for(i=0; i<=stages; i++) {
				cerr << "> IterativeLog\t";
				cerr<<"\tp"<<i<<"=" << p[i];
				cerr<<"\ta"<<i<<"=" << a[i];
				cerr <<endl;
			}
		}
		// Now we probably have reduced too far
		pfinal = p[stages+1];
		int extraBits = pfinal - ((wF+gLog-2)>>1);
		int extraBitsperstage =  floor(((double)extraBits) / ((double)(stages+1)));
		if(UserInterface::verbose)
			cerr << "> IterativeLog\t before optimization:  pfinal=" << pfinal << "--- extraBits=" << extraBits << "---extraBitsperstage=" << extraBitsperstage << endl;
		if(bitsPerStage>6) {
			for (i=0; i<= stages; i++)
				a[i] -= extraBitsperstage;
		}
		else  {
			for (i=2; i<= stages; i++)
				a[i] -= extraBitsperstage;
		}
		// Recompute the pi
		p[1] = a[0]-2;

		for(i=1; i<=stages; i++){ // for faithful rounding
			if(i==1)   	// stage 1 cannot be more accurate than 2p1-1
				p[2] = p[1] + a[1] -1; // 2p[1]-1
			else
				p[i+1] = p[i] + a[i] - 1;
		}
		pfinal = p[stages+1];
		extraBits = pfinal -  ((wF+gLog-2)>>1);
		if(stages>=2) { // remove extra bits from stages 2 to stages
			extraBitsperstage =  floor(((double)extraBits) / ((double)(stages-1)));
			int extraBitsStage2 = extraBits - (stages-1)*extraBitsperstage;
			a[2] -= extraBitsperstage + extraBitsStage2;
			p[2+1] = p[2] + a[2] - 1;
			for(i=3; i<= stages; i++) {
				a[i] -= extraBitsperstage;
				p[i+1] = p[i] + a[i] - 1;
			}
			pfinal = p[stages+1];
		}

		extraBits = pfinal -  ((wF+gLog-2)>>1);

		if(UserInterface::verbose>=2)
			cerr << "> IterativeLog\t after optimization:   pfinal=" << pfinal << "--- extraBits=" << extraBits << endl;


		if(UserInterface::verbose)
			cerr << "> IterativeLog"<<tab<<"Guard bits: " << gLog << endl;


		target_prec = wF + pfinal +gLog;
		if(UserInterface::verbose==2)
			cerr << "> IterativeLog"<<tab<<"Target precision: " << target_prec << endl;

		s[0] = wF+2;
		psize[0] = s[0] + a[0]+1;
		//	sfullZ[0] = wF+2;
		sbt[1] = psize[0] - p[1] -2; // -2 because P0 begins with 01   was: wF+2 ;
		s[1] = sbt[1];
		t[1] = 0;
		//	sfullZ[1] = sfullZ[0]      +    a[0] + 1;
		//         size of Y0          size of approx inv of A0

		for(i=1; i<=stages; i++) {
			// size of Z before truncation; Zi = 0Bi - 0AiZi + EiYi ;
			// AiZi has weight -2*p[i], and size a[i]+s[i]
			// EiYi has weight -2*p[i]-1 and size 1+p[i]+s[i]
			// except i=1: same weight but size may be 1+p[i]+s[i]+1
			if(i==1)
				sbt[i+1] =    max( a[i]+s[i]+1, 1+p[i]+sbt[i]+1);
			else
				sbt[i+1] =    max( a[i]+s[i]+1, 1+p[i]+sbt[i]);

			if(p[i+1]+sbt[i+1] <= target_prec)
				{ // No truncation at all
					psize[i] = s[i];
					s[i+1] = sbt[i+1];
					t[i+1] = 0;
				}
			else
				{ // Truncate everybody to targetprec :
					// we compute Z[i+1] = B[i] - A[i]Z[i] + (1+Z[i])>>Ei[i]
					// Product  A[i]Z[i] has MSB 2*p[i], LSB target_prec, therefore size target_prec - 2*p[i]
					// We need only this many bits of Z[i] to compute it.
					psize[i] = target_prec - 2*p[i];
					if (psize[i]>s[i]) // in the first iterations
						psize[i]=s[i];
					s[i+1] = target_prec - p[i+1];
					t[i+1] = sbt[i+1] - s[i+1];
				}
		}

		sfinal =  s[stages+1];

		// MSB of squarer input is p[stages+1];
		// LSB will be target_prec
		// size will be target_prec - p[stages+1]


		if(UserInterface::verbose)
			cerr<<"> IterativeLog\t needs 1+"<<stages<<" range reduction stages"<<endl;
		if(UserInterface::verbose>=2) {
			for(i=0; i<=stages; i++) {
				cerr << "> IterativeLog\t";
				cerr<<"\tp"<<i<<"=" << p[i];
				cerr<<"\ta"<<i<<"=" << a[i];
				cerr<<"\ts"<<i<<"=" << s[i];
				cerr<<"\tpsize"<<i<<"=" << psize[i];
				cerr <<endl;
			}
			cerr << "> IterativeLog\t\tsfinal=" << sfinal << "\tpfinal=" << pfinal << endl;

		}


		// On we go with the vhdl


		addConstant("g",  "positive",          gLog);
		addConstant("wE", "positive",          wE);
		addConstant("wF", "positive",          wF);
		addConstant("log2wF", "positive",     intlog2(wF));
		addConstant("targetprec", "positive", target_prec);
		addConstant("sfinal", "positive",     s[stages+1]);
		addConstant("pfinal", "positive",     p[stages+1]);

		setCriticalPath( getMaxInputDelays(inputDelays) );

		vhdl << tab << declare("XExnSgn", 3) << " <=  X(wE+wF+2 downto wE+wF);" << endl;
		vhdl << tab << declare("FirstBit") << " <=  X(wF-1);" << endl;

		manageCriticalPath( getTarget()->eqComparatorDelay(wE) + getTarget()->lutDelay() );

		// if(isSequential()) vhdl << tab << "-- Rem: the Y0 input is registered inside the RangeRed box" << endl;
		vhdl << tab << declare("Y0", wF+2) << " <= \"1\" & X(wF-1 downto 0) & \"0\" when FirstBit = '0' else \"01\" & X(wF-1 downto 0);" << endl;
		vhdl << tab << declare("Y0h", wF) << " <= Y0(wF downto 1);" << endl;
		vhdl << tab << "-- Sign of the result;" << endl;
		vhdl << tab << 	declare("sR") << " <= '0'   when  (X(wE+wF-1 downto wF) = ('0' & (wE-2 downto 0 => '1')))  -- binade [1..2)" << endl
		                              << "     else not X(wE+wF-1);                -- MSB of exponent" << endl;
		double cpY0 = getCriticalPath();

		manageCriticalPath( getTarget()->adderDelay(wF-pfinal+2) + getTarget()->lutDelay() );
		vhdl << tab << declare("absZ0", wF-pfinal+2) << " <=   Y0(wF-pfinal+1 downto 0)          when (sR='0') else" << endl
			  << "             ((wF-pfinal+1 downto 0 => '0') - Y0(wF-pfinal+1 downto 0));" << endl;
		vhdl << tab << declare("E", wE) << " <= (X(wE+wF-1 downto wF)) - (\"0\" & (wE-2 downto 1 => '1') & (not FirstBit));" << endl;

		manageCriticalPath( getTarget()->adderDelay(wE) + getTarget()->lutDelay() );
		vhdl << tab << declare("absE", wE) << " <= ((wE-1 downto 0 => '0') - E)   when sR = '1' else E;" << endl;
		vhdl << tab << declare("EeqZero") << " <= '1' when E=(wE-1 downto 0 => '0') else '0';" << endl;


		setCycleFromSignal("Y0h"); //get back to fisrt steps
		setCriticalPath(cpY0);     //set the corresponding delay

		lzoc = new LZOC(target, wF, inDelayMap("I", getCriticalPath()) );

		inPortMap(lzoc, "I", "Y0h");
		inPortMap(lzoc, "OZB", "FirstBit");
		outPortMap(lzoc, "O", "lzo");
		vhdl << instance(lzoc, "lzoc1");
		setCycleFromSignal("lzo");
		setCriticalPath( lzoc->getOutputDelay("O") );

		manageCriticalPath( getTarget()->localWireDelay() + getTarget()->adderDelay(intlog2(wF)+1) );
		double cpshiftval = getCriticalPath();
		vhdl << tab << declare("pfinal_s", intlog2(wF)) << " <= \"" << unsignedBinary(mpz_class(pfinal), intlog2(wF)) << "\";"<<endl;
		vhdl << tab << declare("shiftval", intlog2(wF)+1) << " <= ('0' & lzo) - ('0' & pfinal_s); " << endl;
		vhdl << tab << declare("shiftvalinL", intlog2(wF-pfinal+2))     << " <= shiftval(" << intlog2(wF-pfinal+2)-1 << " downto 0);" << endl;
		vhdl << tab << declare("shiftvalinR", intlog2(sfinal-pfinal+1)) << " <= shiftval(" << intlog2(sfinal-pfinal+1)-1 << " downto 0);" << endl;
		vhdl << tab << declare("doRR") << " <= shiftval(log2wF); -- sign of the result" << endl;

		manageCriticalPath( getTarget()->localWireDelay() + getTarget()->lutDelay() );
		//done in parallel with the shifter
		vhdl << tab << declare("small") << " <= EeqZero and not(doRR);" << endl;


		setCycleFromSignal("shiftvalinL");
		setCriticalPath(cpshiftval);

		// ao stands for "almost one"
		vhdl << tab << "-- The left shifter for the 'small' case" <<endl;
		ao_lshift = new Shifter(target, wF-pfinal+2,  wF-pfinal+2, Shifter::Left );

		inPortMap(ao_lshift, "X", "absZ0");
		inPortMap(ao_lshift, "S", "shiftvalinL");
		outPortMap(ao_lshift, "R", "small_absZ0_normd_full");
		vhdl << instance(ao_lshift, "small_lshift");

		syncCycleFromSignal("small_absZ0_normd_full");
		setCriticalPath( getOutputDelay("R") );
		double cpsmall_absZ0_normd = getCriticalPath();

		int small_absZ0_normd_size = getSignalByName("small_absZ0_normd_full")->width() - (wF-pfinal+2);
		vhdl << tab << declare("small_absZ0_normd", small_absZ0_normd_size) << " <= small_absZ0_normd_full" << range(small_absZ0_normd_size -1, 0) << "; -- get rid of leading zeroes" << endl;

		//////////////////////////////////////////////
		setCycle(0); //back to the beggining
		setCriticalPath( getMaxInputDelays(inputDelays) );
		vhdl << tab << "---------------- The range reduction box ---------------" << endl;

		vhdl << tab << declare("A0", a[0]) << " <= X" << range(wF-1,  wF-a[0]) << ";" << endl;

		nextCycle(); //buffer input to get synthetized into BRAM: TODO
		vhdl << tab << "-- First inv table" << endl;
		FirstInvTable* it0 = new FirstInvTable(target, a[0], a[0]+1);
		inPortMap       (it0, "X", "A0");
		outPortMap      (it0, "Y", "InvA0");
		useHardRAM(it0);
		vhdl << instance(it0, "itO");
		// nextCycle(); //this gets absorbed into the bram

		// TODO: somehow arbitrary
		// TODO: unplugged
		if(false && getTarget()->isPipelined() && a[0]+1>=9) {
			IntMultiplier* p0 = new IntMultiplier(target, a[0]+1, wF+2, 0, false /*unsigned*/);
			inPortMap  (p0, "X", "InvA0");
			inPortMap  (p0, "Y", "Y0");
			outPortMap (p0, "R", "P0");
			vhdl << instance(p0, "p0_mult");
			syncCycleFromSignal("P0");
			setCriticalPath( p0->getOutputDelay("R") );
		}
		else {
			REPORT(DETAILED, "unpipelined multiplier for P0, implemented as * in VHDL");
			nextCycle();
			vhdl << tab << declare("P0",  psize[0]) << " <= InvA0 * Y0;" <<endl <<endl;
			nextCycle();
			setCriticalPath( 0 ); //FIXME
		}

		double cpzi;
		vhdl << tab << declare("Z1", s[1]) << " <= P0" << range (psize[0] - p[1]-3,  0) << ";" << endl;

		for (i=1; i<= stages; i++) {

			vhdl <<endl;
			//computation
			vhdl << tab << declare(join("A",i), a[i]) <<      " <= " << join("Z",i) << range(s[i]-1,      s[i]-a[i]) << ";"<<endl;
			vhdl << tab << declare(join("B",i), s[i]-a[i]) << " <= " << join("Z",i) << range(s[i]-a[i]-1, 0        ) << ";"<<endl;

			cpzi = getCriticalPath();
			vhdl << tab << declare(join("ZM",i), psize[i]) << " <= " << join("Z",i) ;
			if(psize[i] == s[i])
				vhdl << ";"<<endl;
			else
				vhdl << range(s[i]-1, s[i]-psize[i])  << ";" << endl;

#if 0 // TODO unplugged
			// TODO experiment with logic-based by setting the ratio to 0
			IntMultiplier* pi = new IntMultiplier(target, a[i], psize[i], 0, false, 1.0, inDelayMap("X", getCriticalPath()) );
			inPortMap  (pi, "X", join("A",i));
			inPortMap  (pi, "Y", join("ZM",i));
			outPortMap (pi, "R", join("P",i));
			vhdl << instance(pi, join("p",i)+"_mult") << endl;

			syncCycleFromSignal( join("P",i) );
			setCriticalPath( pi->getOutputDelay("R") );

#else
			if(UserInterface::verbose) cerr << "> IterativeLog: unpipelined multiplier for P"<<i<<", implemented as * in VHDL" << endl;
			nextCycle();//
			vhdl << tab << declare(join("P",i),  psize[i] + a[i]) << " <= " << join("A",i) << "*" << join("ZM",i) << ";" << endl;
			nextCycle();
			setCriticalPath( 0 ); //FIXME
#endif
			double cppi = getCriticalPath();
			vhdl << tab << " -- delay at multiplier output is " << getCriticalPath() << endl;

			int yisize = s[i]+p[i]+1; // +1 because implicit 1

			// While the multiplication takes place, we may prepare the other term
			setCycleFromSignal(join("Z",i));
			setCriticalPath(cpzi);

			string Yi= join("Y",i);
			string Zi= join("Z",i);
			vhdl << tab << declare(Yi, yisize) << " <= \"1\" & " << rangeAssign(p[i]-1, 0, "'0'") << " & " << Zi <<";"<<endl;

			// We truncate EiY to a position well above target_prec
			if(i==1) {
				manageCriticalPath( getTarget()->lutDelay());
				vhdl << tab << declare(join("EiY",i), s[i+1]) << " <= " << Yi ;
				// now we may need to truncate or to pad Yi
				if(yisize > s[i+1]) // truncate Yi
					vhdl << range(yisize-1, yisize-s[i+1]);
				else if (yisize < s[i+1]) // pad Yi
					vhdl << " & " << rangeAssign(s[i+1] - yisize -1, 0, "'0'");
				vhdl  << "  when " << join("A",i) << of(a[i]-1) << " = '1'" << endl
						<< tab << "  else  \"0\" & " << Yi;
				if(yisize > s[i+1]-1) // truncate Yi
					vhdl << range(yisize-1, yisize-s[i+1]+1);
				vhdl 	  << ";" << endl ;
			}
			else { // i>1, general case
				vhdl << tab << declare(join("EiY",i), s[i+1]) << " <= " ;
				vhdl << rangeAssign(2*p[i] -p[i+1] -2,  0,  "'0'")  << " & " << Yi << range(yisize-1,  yisize - (s[i+1] - (2*p[i]-p[i+1]) )-1) << ";" << endl ;
			}

			IntAdder* addCycleI1 = new IntAdder ( target, s[i+1]);

			vhdl << tab << declare( join("addXIter",i), s[i+1] ) << " <= \"0\" & " << join("B",i);
			if (s[i+1] > 1+(s[i]-a[i]))  // need to padd Bi
				vhdl << " & " << rangeAssign(s[i+1] - 1-(s[i]-a[i]) -1,  0 , "'0'");
			vhdl <<";"<<endl;

			inPortMap   ( addCycleI1, "X"  , join("addXIter",i) );
			inPortMap   ( addCycleI1, "Y"  , join("EiY",i) );
			inPortMapCst( addCycleI1, "Cin", " '0'" );
			outPortMap  ( addCycleI1, "R"  , join("EiYPB",i) );
			vhdl << instance ( addCycleI1, join("addIter1_",i) ) << endl;
			syncCycleFromSignal ( join("EiYPB",i) );
			setCriticalPath( addCycleI1->getOutputDelay("R") );

			if(syncCycleFromSignal(join("P",i)))
				setCriticalPath( cppi );

			vhdl << tab << declare(join("Pp", i), s[i+1])  << " <= " << rangeAssign(p[i]-a[i],  0,  "'1'") << " & not(" << join("P", i);
			// either pad, or truncate P
			if(p[i]-a[i]+1  + psize[i]+a[i]  < s[i+1]) // size of leading 0s + size of p
				vhdl << " & "<< rangeAssign(s[i+1] - (p[i]-a[i]+1  + psize[i]+a[i]) - 1,    0,  "'0'");  // Pad
			if(p[i]-a[i]+1  + psize[i]+a[i]  > s[i+1])
				//truncate
				vhdl << range(psize[i]+a[i]-1,    p[i]-a[i]+1  + psize[i]+a[i] - s[i+1]);
			vhdl << ");"<<endl;


#if 0
			double ctperiod;
			ctperiod = 1.0 / getTarget()->frequency();
			getTarget()->setFrequency( 1.0 / (ctperiod - getTarget()->LogicToDSPWireDelay() ) );
			IntAdder* addCycleI2 = new IntAdder ( target, s[i+1]);
			getTarget()->setFrequency( 1.0 / ctperiod );
#else
			IntAdder* addCycleI2 = new IntAdder ( target, s[i+1]);
#endif


			inPortMap( addCycleI2, "X" , join("EiYPB",i) );
			inPortMap( addCycleI2, "Y" , join("Pp", i) );
			inPortMapCst( addCycleI2, "Cin", " '1'" );
			outPortMap( addCycleI2, "R", join("Z", i+1) );
			vhdl << instance ( addCycleI2, join("addIter2_",i) ) << endl;
			syncCycleFromSignal ( join("Z", i+1) );
			setCriticalPath( addCycleI2->getOutputDelay("R") );
			vhdl << " -- the critical path at the adder output = " << getCriticalPath() << endl;

		}

		vhdl << tab << declare("Zfinal", s[stages+1]) << " <= " << join("Z", stages+1) << ";" << endl;


		vhdl << tab << "--  Synchro between RR box and case almost 1" << endl;

		if (syncCycleFromSignal("small_absZ0_normd"))
			setCriticalPath(cpsmall_absZ0_normd);




		// In the small path we need Z2O2 accurate to  (wF+gLog+2) - pfinal
		// In the RR path we need Z2O2 accurate to sfinal-pfinal
		// Take the max. This is useful  for small precs only
		int squarerInSize;
		if((wF+gLog+2) - pfinal > sfinal-pfinal)
			squarerInSize = (wF+gLog+2) - pfinal;
		else
			squarerInSize = sfinal-pfinal;

		manageCriticalPath( getTarget()->localWireDelay() +  getTarget()->lutDelay() );
		vhdl << tab << declare("squarerIn", squarerInSize) << " <= "
			 << "Zfinal(sfinal-1 downto sfinal-"<< squarerInSize << ") when doRR='1'" << endl;
		if(squarerInSize>small_absZ0_normd_size)
			vhdl << tab << "                 else (small_absZ0_normd & " << rangeAssign(squarerInSize-small_absZ0_normd_size-1, 0, "'0'") << ");  " << endl;
		else  // sfinal-pfinal <= small_absZ0_normd_size
			vhdl << tab << "                 else small_absZ0_normd" << range(small_absZ0_normd_size-1, small_absZ0_normd_size - squarerInSize) << ";  " << endl<< endl;

		IntSquarer* sq = new IntSquarer(target, squarerInSize, inDelayMap( "X", getTarget()->LogicToDSPWireDelay() + getCriticalPath() ) );

		inPortMap  (sq, "X", "squarerIn");
		outPortMap (sq, "R", "Z2o2_full");
		vhdl << instance(sq, "squarer");
		syncCycleFromSignal("Z2o2_full", true);
		setCriticalPath( sq->getOutputDelay("R")  );

		manageCriticalPath(getTarget()->DSPToLogicWireDelay());
		double cpZ2o2_full_dummy = getCriticalPath();
		vhdl << tab << declare("Z2o2_full_dummy", 2*squarerInSize) << " <= Z2o2_full;" << endl;

		vhdl << tab << declare("Z2o2_normal", sfinal-pfinal-1) << " <= Z2o2_full_dummy ("<< 2*squarerInSize-1 << "  downto " << 2*squarerInSize - (sfinal-pfinal-1) << ");" << endl;

		IntAdder* addFinalLog1p_normal = new IntAdder( target, sfinal);

		vhdl << tab << declare( "addFinalLog1pY", sfinal) << " <= (pfinal downto 0  => '1') & not(Z2o2_normal);" <<endl;

		inPortMap( addFinalLog1p_normal, "X" , "Zfinal" );
		inPortMap( addFinalLog1p_normal, "Y" , "addFinalLog1pY" );
		inPortMapCst( addFinalLog1p_normal, "Cin", " '1'" );
		outPortMap( addFinalLog1p_normal, "R", "Log1p_normal" );
		vhdl << instance(addFinalLog1p_normal, "addFinalLog1p_normalAdder");

		syncCycleFromSignal("Log1p_normal");
		setCriticalPath( addFinalLog1p_normal->getOutputDelay("R") );



		vhdl << endl << tab << "-- Now the log tables, as late as possible" << endl;

		// the log tables have small input and large outputs, so we had rather register the inputs.
		// We have to compute the sum of the outputs of the log tables, and we want this sum (AlmostLog) to be synchronized to Log1pNormal, to which it will be added.
		// To get this synchro right, we have to do a first dummy evaluation of the pipeline to profile its depth
		// We first do it in a dummy way, starting from cycle 0, to measure the depth of this sub-pipeline

		FirstLogTable* lt0 = new FirstLogTable(target, a[0], target_prec, it0, this);

		int profilingDepth;
		setCycle(0);
		setCriticalPath(0.0);
		for (i=1; i<= stages; i++) {
			manageCriticalPath( getTarget()->LogicToRAMWireDelay() + getTarget()->RAMDelay(), false );
			if( getTarget()->normalizedFrequency() > 0.5  ) { // we targetting a deeply pipelined operator
					nextCycle(); nextCycle();// gets absorbed in the BRams, and reinits the critical paths, and we have plenty of cycles to live in
			}
			IntAdder * adderS = new IntAdder( target, lt0->wOut);
			setCycle(getCurrentCycle()+adderS->getPipelineDepth());
			setCriticalPath( adderS->getOutputDelay("R") );
		}
		profilingDepth=getCurrentCycle();

		// End of the dummy pipeline construction. Now go to the real one

		setCycleFromSignal("Log1p_normal", false);
		setCycle(getCurrentCycle() - profilingDepth , true);
		setCriticalPath(0.0);
		vhdl << tab << "-- First log table" << endl;
		inPortMap       (lt0, "X", "A0");
		outPortMap      (lt0, "Y", "L0");
		vhdl << instance(lt0, "ltO");
		syncCycleFromSignal("L0");
		vhdl << tab << declare("S1", lt0->wOut) << " <= L0;"<<endl;

		for (i=1; i<= stages; i++) {
#if 0
			if (i==1){
				setCycleFromSignal("Log1p_normal", false);
				setCycle(getCurrentCycle() - profilingDepth , true);
				setCriticalPath(0.0);
				manageCriticalPath(getTarget()->LogicToRAMWireDelay() + getTarget()->RAMDelay());
			}else{
				manageCriticalPath( getTarget()->LogicToDSPWireDelay() + getTarget()->RAMDelay() );
			}
#else
			if (i>1){
				manageCriticalPath( getTarget()->LogicToDSPWireDelay() + getTarget()->RAMDelay() );
			}

#endif

			OtherLogTable* lti = new OtherLogTable(target, a[i], target_prec - p[i], i, a[i], p[i]);
			inPortMap       (lti, "X", join("A", i));
			outPortMap      (lti, "Y", join("L", i));
			vhdl << instance(lti, join("lt",i));
			syncCycleFromSignal(join("L", i));

			vhdl << tab << declare(join("sopX",i), lt0->wOut) << " <= (" << rangeAssign(lt0->wOut-1, lti->wOut,  "'0'") << " & " << join("L",i) <<");"<<endl;

			if( getTarget()->normalizedFrequency() > 0.5  ) { // we targetting a deeply pipelined operator
				nextCycle(); nextCycle();// gets absorbed in the BRams, and reinits the critical paths, and we have plenty of cycles to live in
			}
			IntAdder * adderS = new IntAdder( target, lt0->wOut);

			inPortMap( adderS, "X", join("S",i) );
			inPortMap( adderS, "Y", join("sopX",i) );
			inPortMapCst ( adderS, "Cin", " '0' ");
			outPortMap( adderS, "R", join("S",i+1) );

			vhdl << instance( adderS, join("adderS",i) ) << endl;
			syncCycleFromSignal( join("S",i+1) );
			setCriticalPath( adderS->getOutputDelay("R") );
		}

		nextCycle();
		vhdl << tab << declare("almostLog", lt0->wOut) << " <= " << join("S",stages+1) << ";" << endl;

		IntAdder* adderLogF_normal = new IntAdder( target, target_prec );

		vhdl << tab << declare( "adderLogF_normalY", target_prec ) << " <= ((targetprec-1 downto sfinal => '0') & Log1p_normal);" << endl;

		inPortMap( adderLogF_normal, "X", "almostLog" );
		inPortMap( adderLogF_normal, "Y", "adderLogF_normalY");
		inPortMapCst( adderLogF_normal, "Cin", "'0'");
		outPortMap( adderLogF_normal, "R", "LogF_normal" );
		vhdl << instance( adderLogF_normal, "adderLogF_normal");

		syncCycleFromSignal("LogF_normal");

		// The log2 constant
		mpfr_t two, log2;
		mpz_t zlog2;
		mpfr_init2(two, 2);
		mpfr_set_d(two, 2.0, GMP_RNDN);
		mpfr_init2(log2, wF+gLog);
		mpfr_log(log2, two, GMP_RNDN);
		mpfr_mul_2si(log2, log2, wF+gLog, GMP_RNDN); // shift left
		mpz_init2(zlog2, wF+gLog);
		mpfr_get_z(zlog2, log2, GMP_RNDN);




		IntIntKCM* kcm=new IntIntKCM(target, wE, mpz_class(zlog2), false);
		// get back enough cycles to synchronize it with LogF_normal
		setCycle(getCurrentCycle() - kcm->getPipelineDepth());
		inPortMap       (kcm, "X", "absE");
		outPortMap      (kcm, "R", "absELog2");
		vhdl << instance(kcm, "Log2KCM");
		setCycle(getCurrentCycle() + kcm->getPipelineDepth());

		vhdl << tab << declare("absELog2_pad", wE+target_prec) << " <=   "
			 << "absELog2 & (targetprec-wF-g-1 downto 0 => '0');       " << endl;

		vhdl << tab << declare("LogF_normal_pad", wE+target_prec) << " <= (wE-1  downto 0 => LogF_normal(targetprec-1))  & LogF_normal;" << endl;

		vhdl << tab << declare("lnaddX", wE+target_prec) << " <= absELog2_pad;"<<endl;


		setCriticalPath( adderLogF_normal->getOutputDelay("R") );
		if (!manageCriticalPath(getTarget()->lutDelay()))
			setCriticalPath( max( getCriticalPath(), kcm->getOutputDelay("R")) );
		vhdl << tab << declare("lnaddY", wE+target_prec) << " <= LogF_normal_pad when sR='0' else not(LogF_normal_pad); "<<endl;


		IntAdder* lnadder = new IntAdder( target, wE+target_prec);

		inPortMap( lnadder, "X", "lnaddX");
		inPortMap( lnadder, "Y", "lnaddY");
		inPortMap( lnadder, "Cin", "sR");
		outPortMap( lnadder, "R", "Log_normal");
		vhdl << instance( lnadder, "lnadder") << endl;

		syncCycleFromSignal("Log_normal");
		setCriticalPath( lnadder->getOutputDelay("R") );

		final_norm = new LZOCShifterSticky(target, wE+target_prec, target_prec, intlog2(wE+(wF>>1))+1, false, 0);
		inPortMap(final_norm, "I", "Log_normal");
		outPortMap(final_norm, "Count", "E_normal");
		outPortMap(final_norm, "O", "Log_normal_normd");
		vhdl << instance(final_norm, "final_norm");
		setCriticalPath( final_norm->getOutputDelay("O"));
		double cpE_normal = getCriticalPath();

		// back to the squarer output
		// 		setCycleFromSignal("Z2o2_full");

		int Z2o2_small_size=(wF+gLog+2) - pfinal; // we need   (wF+gLog+2) - pfinal bits of Z2O2

		if (syncCycleFromSignal("Z2o2_full_dummy"))
			setCriticalPath(cpZ2o2_full_dummy);

		vhdl << tab << declare("Z2o2_small_bs", Z2o2_small_size)  << " <= Z2o2_full_dummy" << range(2*squarerInSize -1, 2*squarerInSize -Z2o2_small_size) << ";" << endl;

		ao_rshift = new Shifter(target, Z2o2_small_size, sfinal-pfinal+1, Shifter::Right ) ;
		inPortMap(ao_rshift, "X", "Z2o2_small_bs");
		inPortMap(ao_rshift, "S", "shiftvalinR");
		outPortMap(ao_rshift, "R", "Z2o2_small_s");
		vhdl << instance(ao_rshift, "ao_rshift");

		setCycleFromSignal("Z2o2_small_s");
		setCriticalPath(ao_rshift->getOutputDelay("R") );
		vhdl << tab << "-- output delay at shifter output is " << getCriticalPath() << endl;


		vhdl << tab << "  -- send the MSB to position pfinal" << endl;
		int Z2o2_small_sSize = getSignalByName("Z2o2_small_s")->width();
		vhdl << tab << declare("Z2o2_small", wF+gLog+2) << " <=  (pfinal-1 downto 0  => '0') & Z2o2_small_s"
			  << range(Z2o2_small_sSize-1,  Z2o2_small_sSize - (wF+gLog+2) + pfinal) << ";" << endl;

		vhdl << tab << "-- mantissa will be either Y0-z^2/2  or  -Y0+z^2/2,  depending on sR  " << endl;
		vhdl << tab << declare("Z_small", wF+gLog+2) << " <= small_absZ0_normd & " << rangeAssign((wF+gLog+2)-small_absZ0_normd_size-1, 0, "'0'") << ";" << endl;

		manageCriticalPath( getTarget()->localWireDelay() + getTarget()->lutDelay() );
		vhdl << tab << declare("Log_smallY", wF+gLog+2) << " <= Z2o2_small when sR='1' else not(Z2o2_small);" << endl;
		vhdl << tab << declare("nsRCin",1, false) << " <= not ( sR );" << endl;

		IntAdder* log_small_adder = new IntAdder(target,wF+gLog+2 );

		inPortMap( log_small_adder, "X", "Z_small" );
		inPortMap (log_small_adder, "Y", "Log_smallY");
		inPortMap (log_small_adder, "Cin", "nsRCin");
		outPortMap(log_small_adder, "R", "Log_small");
		vhdl << instance(log_small_adder, "log_small_adder") << endl;
		////////////////////////////////////////////////////////////////////////-> start the other path
		syncCycleFromSignal("Log_small");
		setCriticalPath(log_small_adder->getOutputDelay("R") );
		vhdl << " -- critical path here is " << getCriticalPath() << endl;
		manageCriticalPath( getTarget()->localWireDelay() + getTarget()->lutDelay() );
		vhdl << tab << "-- Possibly subtract 1 or 2 to the exponent, depending on the LZC of Log_small" << endl;
		vhdl << tab << declare("E0_sub", 2) << " <=   \"11\" when Log_small(wF+g+1) = '1'" << endl
			  << "          else \"10\" when Log_small(wF+g+1 downto wF+g) = \"01\"" << endl
			  << "          else \"01\" ;" << endl;

		// Is underflow possible?
		vhdl << tab <<	"-- The smallest log will be log(1+2^{-wF}) \\approx 2^{-wF}  = 2^" << -wF <<  endl
			  << tab << "-- The smallest representable number is 2^{1-2^(wE-1)} = 2^" << 1 -(1<<(wE-1))<< endl;
		double cpE_small;
		if(1 -(1<<(wE-1)) < -wF) {
			vhdl << tab <<	"-- No underflow possible" <<  endl;
			vhdl << tab << declare("ufl") << " <= '0';" << endl;
			manageCriticalPath( getTarget()->adderDelay(wE) );
			cpE_small = getCriticalPath();
			vhdl << tab << declare("E_small", wE) << " <=  (\"0\" & (wE-2 downto 2 => '1') & E0_sub)  -  ";
			if(wE>getSignalByName("lzo")->width())
				vhdl << "((wE-1 downto " << getSignalByName("lzo")->width() << " => '0') & lzo) ;" << endl;
			else
				vhdl << "lzo;" << endl;
		}
		else{
			vhdl << tab <<	"-- Underflow may happen" <<  endl;
			manageCriticalPath( getTarget()->adderDelay(wE) );
			cpE_small = getCriticalPath();
			vhdl << tab << declare("E_small", wE+1) << " <=  (\"00\" & (wE-2 downto 2 => '1') & E0_sub)  -  (";
			vhdl << "'0' & lzo);" << endl;
			vhdl << tab << declare("ufl") << " <= E_small(wE);" << endl;
		}

		double cpLog_small_normd;
		manageCriticalPath( getTarget()->lutDelay() );
		cpLog_small_normd = getCriticalPath();
		vhdl << tab << declare("Log_small_normd", wF+gLog) << " <= Log_small(wF+g+1 downto 2) when Log_small(wF+g+1)='1'" << endl
			  << "           else Log_small(wF+g downto 1)  when Log_small(wF+g)='1'  -- remove the first zero" << endl
			  << "           else Log_small(wF+g-1 downto 0)  ; -- remove two zeroes (extremely rare, 001000000 only)" << endl ;


//		//////////////////////////////////////// get back to Log_normal_normd
		if ( syncCycleFromSignal("E_small") )
			setCriticalPath( cpE_small );
		if ( syncCycleFromSignal("E_normal") )
			setCriticalPath( cpE_normal );


		int E_normalSize = getSignalByName("E_normal")->width();

		manageCriticalPath(getTarget()->lutDelay() + getTarget()->adderDelay(wE));
		double cpER = getCriticalPath();

		vhdl << tab << declare("E0offset", wE) << " <= \"" << unsignedBinary((mpz_class(1)<<(wE-1)) -2 + wE , wE) << "\"; -- E0 + wE "<<endl;
		vhdl << tab << declare("ER", wE) << " <= E_small" << range(wE-1,0) << " when small='1'" << endl;
		if(wE>E_normalSize)
			vhdl << "      else E0offset - (" << rangeAssign(wE-1,  E_normalSize, "'0'") << " & E_normal);" << endl;
		else
			vhdl << "      else E0offset - E_normal;" << endl;

		//////////////////////////////////////////// now back to log
		setCycleFromSignal("E_normal");
		setCriticalPath( cpE_normal );
		if ( syncCycleFromSignal("Log_small_normd"))
			setCriticalPath( cpLog_small_normd );

		manageCriticalPath( getTarget()->lutDelay() );
		vhdl << tab << declare("Log_g", wF+gLog) << " <=  Log_small_normd(wF+g-2 downto 0) & \"0\" when small='1'           -- remove implicit 1" << endl
			  << "      else Log_normal_normd(targetprec-2 downto targetprec-wF-g-1 );  -- remove implicit 1" << endl ;

		// Sticky is always 1 for a transcendental function !
		// vhdl << tab << declare("sticky") << " <= '0' when Log_g(g-2 downto 0) = (g-2 downto 0 => '0')    else '1';" << endl;
		vhdl << tab << declare("round") << " <= Log_g(g-1) ; -- sticky is always 1 for a transcendental function " << endl;


		vhdl << tab << "-- if round leads to a change of binade, the carry propagation magically updates both mantissa and exponent" << endl;

		if (syncCycleFromSignal("ER") )
			setCriticalPath( cpER );

		vhdl << tab << declare("fraX", wE+wF) << " <= (ER & Log_g(wF+g-1 downto g)) ; " << endl;
		vhdl << tab << declare("fraY", wE+wF) << " <= ((wE+wF-1 downto 1 => '0') & round); " << endl;
		IntAdder* finalRoundAdder = new IntAdder(target, wE+wF);
		inPortMap(finalRoundAdder, "X", "fraX");
		inPortMap(finalRoundAdder, "Y", "fraY");
		inPortMapCst(finalRoundAdder, "Cin", "'0'");
		outPortMap(finalRoundAdder, "R", "EFR");
		vhdl << instance(finalRoundAdder, "finalRoundAdder");
		syncCycleFromSignal("EFR");

		setCriticalPath(finalRoundAdder->getOutputDelay("R"));

		manageCriticalPath(getTarget()->lutDelay() );
		vhdl << tab << "R(wE+wF+2 downto wE+wF) <= \"110\" when ((XExnSgn(2) and (XExnSgn(1) or XExnSgn(0))) or (XExnSgn(1) and XExnSgn(0))) = '1' else" << endl
			  << "                              \"101\" when XExnSgn(2 downto 1) = \"00\"  else" << endl
			  << "                              \"100\" when XExnSgn(2 downto 1) = \"10\"  else" << endl
			  << "                              \"00\" & sR when (((Log_normal_normd(targetprec-1)='0') and (small='0')) or ( (Log_small_normd (wF+g-1)='0') and (small='1'))) or (ufl = '1') else" << endl
			  << "                               \"01\" & sR;" << endl;
		vhdl << tab << "R(wE+wF-1 downto 0) <=  EFR;" << endl;

		getOutDelayMap()["R"]=getCriticalPath();
	}

	IterativeLog::~IterativeLog()
	{
#if 0 // probably to be performed on oplist
		delete lzoc;
		delete ao_rshift;
		delete ao_lshift;
		delete rrbox;
		delete final_norm;
#endif
	}

	void IterativeLog::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");

		/* Compute correct value */
		FPNumber fpx(wE, wF);
		fpx = svX;

		mpfr_t x, ru,rd;
		mpfr_init2(x,  1+wF);
		mpfr_init2(ru, 1+wF);
		mpfr_init2(rd, 1+wF);
		fpx.getMPFR(x);
		mpfr_log(rd, x, GMP_RNDD);
		mpfr_log(ru, x, GMP_RNDU);
#if 0
		mpfr_out_str (stderr, 10, 30, x, GMP_RNDN); cerr << " ";
		mpfr_out_str (stderr, 10, 30, rd, GMP_RNDN); cerr << " ";
		mpfr_out_str (stderr, 10, 30, ru, GMP_RNDN); cerr << " ";
		cerr << endl;
#endif
		FPNumber  fprd(wE, wF, rd);
		FPNumber  fpru(wE, wF, ru);
		mpz_class svRD = fprd.getSignalValue();
		mpz_class svRU = fpru.getSignalValue();
		tc->addExpectedOutput("R", svRD);
		tc->addExpectedOutput("R", svRU);
		mpfr_clears(x, ru, rd, NULL);
	}


	// TEST FUNCTIONS


	void IterativeLog::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;
		mpz_class x;


		tc = new TestCase(this);
		tc->addFPInput("X", 1.0);
		tc->addComment("1.0");
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addComment("The worst case of the error analysis: max cancellation, and full range reduction");
		x = (mpz_class(1) << wF) - (mpz_class(1) << (wF-pfinal+2)) // mantissa
			+ (((mpz_class(1) << (wE-1)) -2) << wF)  // exponent
			+ (mpz_class(1) << (wE+wF+1))	; // exn=010
		tc->addInput("X", x);
		emulate(tc);
		tcl->add(tc);



	}



	// One test out of 8 fully random (tests NaNs etc)
	// All the remaining ones test positive numbers.
	// with special treatment for exponents 0 and -1,
	// and for the range reduction worst case.

	TestCase* IterativeLog::buildRandomTestCase(int i){

		TestCase *tc;
		mpz_class a;
		mpz_class normalExn = mpz_class(1)<<(wE+wF+1);

		tc = new TestCase(this);
		/* Fill inputs */
		if ((i & 7) == 0)
			a = getLargeRandom(wE+wF+3);
		else if ((i & 7) == 1) // exponent of 1
			a  = getLargeRandom(wF) + ((((mpz_class(1)<<(wE-1))-1)) << wF) + normalExn;
		else if ((i & 7) == 2) // exponent of 0.5
			a  = getLargeRandom(wF) + ((((mpz_class(1)<<(wE-1))-2)) << wF) + normalExn;
		else if ((i & 7) == 3) { // worst case for range reduction
			tc->addComment("The worst case of the error analysis: max cancellation, and full range reduction");
			a = (mpz_class(1) << wF) - (mpz_class(1) << (wF-pfinal+2)) + getLargeRandom(wF-pfinal+2) // mantissa
				+ (((mpz_class(1) << (wE-1)) -2) << wF)  // exponent
				+ (mpz_class(1) << (wE+wF+1))	; // exn=010
		}
		else
			a  = getLargeRandom(wE+wF)  + normalExn; // 010xxxxxx

		tc->addInput("X", a);
		/* Get correct outputs */
		emulate(tc);
		// add to the test case list
		return tc;
	}

		OperatorPtr IterativeLog::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int wE;
		UserInterface::parseStrictlyPositiveInt(args, "wE", &wE);
		int wF;
		UserInterface::parseStrictlyPositiveInt(args, "wF", &wF);
		int inTableSize;
		UserInterface::parseStrictlyPositiveInt(args, "inTableSize", &inTableSize);
		return new IterativeLog(target, wE, wF, inTableSize);
	}

	void IterativeLog::registerFactory(){
		UserInterface::add("FPLog", // name
											 "Floating-point logarithm using an iterative method.",
											 "ElementaryFunctions", // categories
											 "",
											 "wE(int): exponent size in bits; \
                        wF(int): mantissa size in bits; \
                        inTableSize(int)=0: The input size to the tables, in bits, between 6 and 16. 0 choses a a sensible value",
											 "For details on the technique used, see <a href=\"bib/flopoco.html#DetDinPuj2007:Arith\">this article</a> and <a href=\"bib/flopoco.html#2010-RR-FPLog\">this research report</a>.",
											 IterativeLog::parseArguments
											 ) ;

	}

}
