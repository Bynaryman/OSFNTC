#include <iostream>
#include <sstream>
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
#include "FixFFTFullyPA.hpp"

using namespace std;


namespace flopoco{

const int TWIDDLERE = 1;
const int TWIDDLEIM = -1;

		

//	extern vector<Operator *> oplist;

	FixFFTFullyPA::FixFFTFullyPA(OperatorPtr parentOp,Target* target, int msbin_, int lsbin_, int msbout_, int lsbout_, int N_, int radix_, bool signedIn_, bool decimation_, bool revbitorder_)
		: Operator(parentOp, target), msbin(msbin_), lsbin(lsbin_), msbout(msbout_), lsbout(lsbout_), N(N_), radix(radix_), signedIn(signedIn_), decimation(decimation_), revbitorder(revbitorder_)
	{
//		signedOperator ? w = 1 + wI + wF : w = wI + wF;
		
//		const double myconstPi = 3.141592653589793238460;
		int n = log2(N); 		//number of stages

		int extrabitno = 0, msbinextrabit = 0; 		//number of extra bits to increase msb in FFT stages to prevent overflow
		bool bypassmultp = false;
		//cout << endl << n;
		int NoButinSta = N / radix;	//number of butterflies in every stage
		string tw_re="", tw_im="", bypassmultpstr="false", laststagestr="false";
		int coeff= N/2;
		bool extrabit = true;
		
		ostringstream name;
		string extrabitstr;

		guardbits = computeGuardBits(N);

//		cout << endl << "Gaurdbits=============" << guardbits << endl;

		/////TODO For the first step, I set the lsbcomp=lsbout-guardbits for the lsbout of the butterflies in FFT computation
		int lsbcomp = lsbout - guardbits;
			

		int input_width = msbin-lsbin+1;
		int output_width = msbout-lsbout+1;
//		int output_width = input_width+n+1;

		setCopyrightString ( "Hatam Abdoli, Florent de Dinechin (2019)" );
/*		if(getTarget()->isPipelined())
			name << "FixFFTFullyPA_" << input_width << "_w_exp_" << twiddleExponent << "_f"<< getTarget()->frequencyMHz() << "_uid" << getNewUId();
		else
			name << "FixFFTFullyPA_" << input_width << "_w_exp_" << twiddleExponent << "_uid" << getNewUId();
*/
		name << "FixFFTFullyPA_" << N << "_Points_" << abs(msbin) << "_" << abs(lsbin);
		setNameWithFreqAndUID( name.str() );

		for (int i=0; i < N; i++)
		{
			addInput(join("Xi_S0_",i), input_width);
			addInput(join("Xr_S0_",i), input_width);
			addOutput(join("Yi",i), output_width);
			addOutput(join("Yr",i), output_width);
		}

		//addInput("clk");
		//addInput("reset");
		addOutput("out_ready");


		std::ostringstream inportmap;
		std::ostringstream outportmap;
		



		int k=N, stageNo=0; //, varstageno;


	/*	while (k>1)
		{
			n=k;
			k = k>>1;
			for (i=0; i < N; i++)
			{
				vhdl << tab << declare(join("Xr", stageNo+1, i), input_width+stageNo+1) << endl;
				vhdl << tab << declare(join("Xi", stageNo+1, i), input_width+stageNo+1) << endl;
			}
			
			for (int l=0; l<k; l++)
			{
				for (int a=l; a<N; a =a+n)
				{
					tw_re = join("cos(pi/", coeff*a) + ")";
					tw_im = join("sin(pi/", coeff*a) + ")";

					int b = a + k;
					inportmap << "X0r => Xr" << stageNo << a+b << " X0i => Xi" << stageNo << a+b " X1r => Xr" << stageNo << a-b << " X1i => Xi" << stageNo << a-b;

					if (k ==1)  //last stage
						outportmap << "Y0r => Yr" << a+b << " Y0i => Yi" << a+b " Y1r => Yr" << a-b << " Y1i => Yi" << a-b;
					else
						outportmap << "Y0r => Xr" << stageNo+1 << a+b << " Y0i => Xi" << stageNo+1 << a+b " Y1r => Xr" << stageNo+1 << a-b << " Y1i => Xi" << stageNo+1 << a-b;


					newInstance("FixComplexR2Butterfly", join("BF",i,j), "msbin=" + std::to_string(msbin+stageNo) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbout+stageNo+1) + " lsbout=" + std::to_string(lsbout) + " Twiddle_re=" + tw_re + " Twiddle_im=" + tw_im, inportmap, outportmap);				

					
				}
				//coeff = coeff*
				//tw_re = join("cos(pi/", coeff) + ")";
				//tw_im = join("sin(pi/", coeff) + ")";

			}
			stageNo++;
		}*/

/*
		for(i=0; i<n; i++)
		{
			for(j=0; j<NoButinSta; j++)
			{
				inportmap << "X0r => Xr(" << (i*n+j)+(2*input_width-1) << " dowonto " << (i*n+j)+input_width << ") X0i => Xi(" << (i*n+j)+(input_width-1) << " dowonto " << (i*n+j);
				inportmap << "X1r => Xr(" << (i*n+j)+(2*input_width-1) << " dowonto " << (i*n+j)+input_width << ") X1i => Xi(" << (i*n+j)+(input_width-1) << " dowonto " << (i*n+j);
				outportmap <<

				newInstance("FixComplexR2Butterfly", join("BF",i,j), "msbin=" + std::to_string(msbin) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbout) + " lsbout=" + std::to_string(lsbout), inportmap, outportmap);				
			}
		}*/

		//vhdl << tab << declare("TwX1r", output_width)  << ";" << endl;
		//vhdl << tab << declare("TwX1i", output_width)  << ";" << endl;

		int a, b, bfno=0, uindex=0, lindex=0, submatrix=0, stride= N>>1;
		unsigned int uindex_rev=0, lindex_rev=0;
		if(decimation)
		{

			while (stride > 0)
			{
				//coeff = 1/stride;
//				coeff = stride;
				coeff = N/2;

				if (stageNo == 0)		// Increase the msbout of Fixcomplexkcm one bit, only in the first stage of FFT (to prevent overflow in its addition)
				{
					extrabit = true;
					extrabitstr = "true";
					extrabitno = 1;
					msbinextrabit = 0;
				}
				else
				{
					extrabit = false;
					extrabitstr = "false";
					extrabitno = 0;
					msbinextrabit = 1;
				}


				//varstageno = stageNo+10;
				//cout << endl << stageNo << tab << n << endl << endl;
				
				/*if (stageNo != n-1)  //is not last stage
					for (int i=0; i < N; i++)
					{
						vhdl << tab << declare(join("Xr", varstageno+1, i), input_width+stageNo+1) << ";" << endl;
						vhdl << tab << declare(join("Xi", varstageno+1, i), input_width+stageNo+1) << ";" << endl;
					}*/
				//cout << endl << stride << tab << coeff << endl;

				
				for (submatrix=0; submatrix < ((N>>1)/stride); submatrix++)
				{
					//cout << endl << "for submatrix" << tab << submatrix << endl;
					for (bfno=0; bfno<stride; bfno++)
					{
						//cout << endl << "for bfno" << tab << bfno << endl;
						//tw_re = join("cos(pi*", bfno) + join("/",coeff) + ")";
						//tw_im = join("sin(pi*", bfno) + join("/",coeff) + ")";
						//tw_re = join("cos(2*", myconstPi, "*", bfno) + join("/",coeff) + ")";
						//tw_im = join("-sin(2*", myconstPi, "*", bfno) + join("/",coeff) + ")";
						

						int anglecoeff = bfno * (uint32_t(1) << stageNo);
						if (anglecoeff == N/4)
						{
							tw_re = "0";
							tw_im = "-1";
						}
						else
						{
							tw_re = join("cos(pi*", anglecoeff) + join("/",coeff) + ")";
							tw_im = join("-sin(pi*", anglecoeff) + join("/",coeff) + ")";
//							tw_re = join("cos(pi*", bfno) + join("/",coeff) + ")";
//							tw_im = join("-sin(pi*", bfno) + join("/",coeff) + ")";
						}
						lindex = uindex + stride;

						
						inportmap << "X0r=>Xr_S" << stageNo << "_" << uindex << ", X0i=>Xi_S" << stageNo << "_" << uindex << ", X1r=>Xr_S" << stageNo << "_" << lindex << ", 								X1i=>Xi_S" << stageNo << "_" << lindex;

						//cout << endl << "for bfno" << tab << bfno << "point1" << endl;

						//cout << inportmap.str();
						if (stageNo == n-1)  //last stage
						{
							laststagestr = "true";
							bypassmultp = true;	// The coefficients in the last stage is equal to one and the multiplication can be bypassed
							//cout << endl << revbitorder << endl;
							if(revbitorder)
							{
								uindex_rev = bitrev(uindex, n); // make and return a bit reversed integer
								lindex_rev = bitrev(lindex, n);

//								outportmap << "Y0r=>Xr_S" << stageNo+1 << "_" << uindex_rev << ", Y0i=>Xi_S" << stageNo+1 << "_" << uindex_rev << ", Y1r=>Xr_S" << stageNo+1 << "_" << lindex_rev << ", Y1i=>Xi_S" << stageNo+1 << "_" << lindex_rev;
								outportmap << "Y0r=>Yr" << uindex_rev << ", Y0i=>Yi" << uindex_rev << ", Y1r=>Yr" << lindex_rev << ", Y1i=>Yi" << lindex_rev;
							}
							else
//								outportmap << "Y0r=>Xr_S" << stageNo+1 << "_" << uindex << ", Y0i=>Xi_S" << stageNo+1 << "_" << uindex << ", Y1r=>Xr_S" << stageNo+1 << "_" << lindex << ", Y1i=>Xi_S" << stageNo+1 << "_" << lindex;
								outportmap << "Y0r=>Yr" << uindex << ", Y0i=>Yi" << uindex << ", Y1r=>Yr" << lindex << ", Y1i=>Yi" << lindex;

							//cout << endl << uindex << endl;
							//cout << endl << uindex_rev << endl;
						}
						else
						{
							laststagestr = "false";

							if (bfno == 0)	// The coefficient of the first butterfly in every submatrix is equal to one and the multiplication can be bypassed
								bypassmultp = true; //TODO temp set to true
							else
								bypassmultp = false;

							outportmap << "Y0r=>Xr_S" << stageNo+1 << "_" << uindex << ", Y0i=>Xi_S" << stageNo+1 << "_" << uindex << ", Y1r=>Xr_S" << stageNo+1 << "_" << 									lindex << ", Y1i=>Xi_S" << stageNo+1 << "_" << lindex;
						}
						//cout << outportmap.str();
						//cout << endl << "for bfno" << tab << bfno << "point2" << endl;

						if(bypassmultp)
							bypassmultpstr = "true";
						else
							bypassmultpstr = "false";
						
						//////////cout << endl << "for bfno" << tab << bfno <<  tab << tw_re << tab << tw_im << tab << bypassmultpstr << tab << inportmap.str() << tab << outportmap.str() << endl;


//						newInstance("FixComplexR2Butterfly", join("BF_",stageNo, "_", submatrix, "_", bfno), "msbin=" + std::to_string(msbin+stageNo) + " lsbin=" + std::to_string(lsbin) + 
//							" msbout=" + std::to_string(msbin+stageNo+1) + " lsbout=" + std::to_string(lsbout) + " Twiddle_re=\"" + tw_re + "\" Twiddle_im=\"" + tw_im + 
//							"\" bypassmult=" + bypassmultpstr, inportmap.str(), outportmap.str());				
////							" bypassmult=" + std::to_string(bypassmultp), inportmap.str(), outportmap.str());				
//				cout << endl << "msbin=" << std::to_string(msbin+stageNo) << " lsbin=" + std::to_string(lsbin) << 
//							" msbout=" << std::to_string(msbin+stageNo+1) << " lsbout=" << std::to_string(lsbout) << " Twiddle_re=\"" << tw_re << "\" Twiddle_im=\"" << tw_im << 
//							"\" bypassmult=" << bypassmultpstr << " extrabit=" << extrabitstr << "laststage=" << laststagestr << " inportpam=" << inportmap.str() << " outportmap= " << outportmap.str() << endl;

//cout << endl << "laststage============================= " << laststagestr << "  stage no=====" << stageNo << endl;

						if (stageNo == 0)  //first stage
						{
							newInstance("FixComplexR2Butterfly", join("BF_",stageNo, "_", submatrix, "_", bfno), "msbin=" + std::to_string(msbin) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbin+stageNo+1+1) + " lsbout=" + std::to_string(lsbcomp) + " Twiddle_re=" + tw_re + " Twiddle_im=" + tw_im + " bypassmult=" + bypassmultpstr + " extrabit=" + extrabitstr + " laststage=" + laststagestr, inportmap.str(), outportmap.str());	
			
							//cout << endl << "stagno= " << stageNo << " msbin=" << std::to_string(msbin) << " lsbin=" + std::to_string(lsbin) << 							" msbout=" << std::to_string(msbin+stageNo+1+1) << " lsbout=" << std::to_string(lsbcomp) << " Twiddle_re=\"" << tw_re << "\" Twiddle_im=\"" << tw_im 								<< "\" bypassmult=" << bypassmultpstr << " extrabit=" << extrabitstr << "laststage=" << laststagestr << " inportpam=" << inportmap.str() << " 								outportmap= " << outportmap.str() << endl;
						}
						else if (stageNo == n-1)  //last stage
						{
							if(submatrix == 0)
								newInstance("FixComplexR2Butterfly", join("BF_",stageNo, "_", submatrix, "_", bfno), "msbin=" + std::to_string(msbin+stageNo+msbinextrabit) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbin+stageNo+1+1) + " lsbout=" + std::to_string(lsbout) + " Twiddle_re=" + tw_re + " Twiddle_im=" + tw_im + " bypassmult=" + bypassmultpstr + " extrabit=" + extrabitstr + " laststage=" + laststagestr, inportmap.str(), outportmap.str());	
							else
								newInstance("FixComplexR2Butterfly", join("BF_",stageNo, "_", submatrix, "_", bfno), "msbin=" + std::to_string(msbin+stageNo+msbinextrabit) + " lsbin=" + std::to_string(lsbcomp) + " msbout=" + std::to_string(msbin+stageNo+1+1) + " lsbout=" + std::to_string(lsbout) + " Twiddle_re=" + tw_re + " Twiddle_im=" + tw_im + " bypassmult=" + bypassmultpstr + " extrabit=" + extrabitstr + " laststage=" + laststagestr, inportmap.str(), outportmap.str());	

							//cout << endl << "stagno= " << stageNo << " msbin=" << std::to_string(msbin+stageNo+msbinextrabit) << " lsbin=" + std::to_string(lsbin) << 							" msbout=" << std::to_string(msbin+stageNo+1+1) << " lsbout=" << std::to_string(lsbout) << " Twiddle_re=\"" << tw_re << "\" Twiddle_im=\"" << tw_im 								<< "\" bypassmult=" << bypassmultpstr << " extrabit=" << extrabitstr << "laststage=" << laststagestr << " inportpam=" << inportmap.str() << " 								outportmap= " << outportmap.str() << endl;
						}
						else
						{
							if(submatrix == 0)
								newInstance("FixComplexR2Butterfly", join("BF_",stageNo, "_", submatrix, "_", bfno), "msbin=" + std::to_string(msbin+stageNo+msbinextrabit) + " lsbin=" + std::to_string(lsbin) + " msbout=" + std::to_string(msbin+stageNo+1+1) + " lsbout=" + std::to_string(lsbcomp) + " Twiddle_re=" + tw_re + " Twiddle_im=" + tw_im + " bypassmult=" + bypassmultpstr + " extrabit=" + extrabitstr + " laststage=" + laststagestr, inportmap.str(), outportmap.str());	

							else
								newInstance("FixComplexR2Butterfly", join("BF_",stageNo, "_", submatrix, "_", bfno), "msbin=" + std::to_string(msbin+stageNo+msbinextrabit) + " lsbin=" + std::to_string(lsbcomp) + " msbout=" + std::to_string(msbin+stageNo+1+1) + " lsbout=" + std::to_string(lsbcomp) + " Twiddle_re=" + tw_re + " Twiddle_im=" + tw_im + " bypassmult=" + bypassmultpstr + " extrabit=" + extrabitstr + " laststage=" + laststagestr, inportmap.str(), outportmap.str());	


							//cout << endl << "stagno= " << stageNo << " msbin=" << std::to_string(msbin+stageNo+msbinextrabit) << " lsbin=" + std::to_string(lsbcomp) << 							" msbout=" << std::to_string(msbin+stageNo+1+1) << " lsbout=" << std::to_string(lsbcomp) << " Twiddle_re=\"" << tw_re << "\" Twiddle_im=\"" << tw_im 								<< "\" bypassmult=" << bypassmultpstr << " extrabit=" << extrabitstr << "laststage=" << laststagestr << " inportpam=" << inportmap.str() << " 								outportmap= " << outportmap.str() << endl;
						}

			
				//cout << endl << "msbin=" << std::to_string(msbin+stageNo) << " lsbin=" + std::to_string(lsbin) << 
				//			" msbout=" << std::to_string(msbin+stageNo+1) << " lsbout=" << std::to_string(lsbout) << " Twiddle_re=" << tw_re << " Twiddle_im=" << tw_im << 
				//			" bypassmult=" << bypassmultpstr << " inportpam=" << inportmap.str() << " outportmap= " << outportmap.str() << endl;

						//cout << endl << "for bfno" << tab << bfno << "point4" << endl;
						uindex++;
						inportmap.str("");
						outportmap.str("");
						//cout << endl << "for bfno" << tab << bfno << "point5" << endl;

					}
					//coeff = coeff*
					//tw_re = join("cos(pi/", coeff) + ")";
					//tw_im = join("sin(pi/", coeff) + ")";
					uindex = (uindex+stride) & (N-1);
				}
				stageNo++;
				stride = stride>>1;
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

/*			newInstance("FixComplexKCM",
					"FixTwiddleKCM",
					"msbIn=" + std::to_string(msbin) + " lsbIn=" + std::to_string(lsbin) + " lsbOut=" + std::to_string(lsbin) + " constantRe=" + Twiddle_re + " constantIm=" +  	Twiddle_im, "ReIn=>X1r,ImIn=>X1i", "ReOut=>TwX1r,ImOut=>TwX1i");

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

	*/	}


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

	unsigned int FixFFTFullyPA::bitrev(int indexvar, int n)
	{
		unsigned int revindex = 0;
		
		int i;
		for (i = 0; i < n; i++)
		{
			revindex = (revindex << 1) | (indexvar & 1);
			indexvar = indexvar >> 1;
		}

		return revindex;
	}


	FixFFTFullyPA::~FixFFTFullyPA()
	{
	}
	

	int FixFFTFullyPA::computeGuardBits(int N)
	{
//		return (1+log2(N));
//		return (ceil(1+log2((N/2)-1))+1);
		return (10);
	}

	
	int FixFFTFullyPA::bitReverse(int x, int n)
	{
		int count = log2((double)n)-1;
		int reversedX = 0, copyx;
		
		while(x>0)
		{
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
	
	mpz_class FixFFTFullyPA::getTwiddleConstant(int constantType, int twiddleExponent)
	{
		mpfr_t twiddleExp, twiddleIm, twiddleRe, constPi, temp;
		mpz_class intTemp;
		int input_width = msbout-lsbout+2;
		
		mpfr_init2(twiddleIm, 	10*input_width);
		mpfr_init2(twiddleRe, 	10*input_width);
		mpfr_init2(twiddleExp, 	10*input_width);
		mpfr_init2(constPi, 	10*input_width);
		mpfr_init2(temp, 		10*input_width);
		
		mpfr_const_pi(	constPi, GMP_RNDN);
		
		mpfr_set_d(		twiddleExp, twiddleExponent, 			GMP_RNDN);
		mpfr_mul_2si(	twiddleExp, twiddleExp, 	1, 			GMP_RNDN);
		mpfr_mul(		twiddleExp, twiddleExp, 	constPi, 	GMP_RNDN);
		mpfr_div_d(		twiddleExp, twiddleExp, 	N, 			GMP_RNDN);
		
		mpfr_sin_cos(	twiddleIm, 	twiddleRe, 		twiddleExp, GMP_RNDN);
		
		switch(constantType)
		{
			case TWIDDLERE:
							mpfr_set(temp, twiddleRe, GMP_RNDN);
							break;
			case TWIDDLEIM:
							mpfr_set(temp, twiddleIm, GMP_RNDN);
							break;
		}
		
		mpfr_mul_2si(temp, temp, -lsbin, GMP_RNDN);
		mpfr_get_z(intTemp.get_mpz_t(), temp,  GMP_RNDN);
		
		mpfr_free_cache();
		mpfr_clears (twiddleExp, twiddleIm, twiddleRe, constPi, NULL);
		
		return intTemp;
	}

	
	void FixFFTFullyPA::emulate ( TestCase* tc )
	{
		int nrGroups = 1;
		int distance = N/2;
		int wIn=msbin-lsbin+1;
		int wOut=msbout-lsbout+1;
		bool negativeInputr = false;
		bool negativeInputi = false;
		bool negativeConstanti = false;
		bool negativeConstantr = false;
		
		mpz_class svXi[N];
		mpz_class svXr[N];
		
		int n = log2(N);
		
		// Get I/O values
		for(int i=0; i<N; i++)
		{
			svXi[i] = tc->getInputValue(join("Xi_S0_", i));
			svXr[i] = tc->getInputValue(join("Xr_S0_", i));

			// get rid of two's complement
			if(signedIn)	
			{
				if (1==(svXi[i] >> (wIn-1))) // sign bit
				{
					svXi[i] -= (mpz_class(1)<<wIn);
					negativeInputi = true;
				}

//				if ( svXi[i] > ( (mpz_class(1)<<(wIn-1))-1) )
//				{
//					cout << endl << "svXi before=" << svXi[i] << endl; 
//					cout << endl << "mpzshift=" << ((mpz_class(1)<<(wIn-1))-1) << "badi=" << (mpz_class(1)<<wIn) << endl; 
//					svXi[i] -= (mpz_class(1)<<wIn);
//					cout << endl << "svXi after=" << svXi[i] << endl; 
//					negativeInputi = true;
//				}
				if (1==(svXr[i] >> (wIn-1))) // sign bit
				{
					svXr[i] -= (mpz_class(1)<<wIn);
					negativeInputr = true;
				}
//				if ( svXr[i] > ( (mpz_class(1)<<(wIn-1))-1) )
//				{
//					svXr[i] -= (mpz_class(1)<<wIn);
//					negativeInputr = true;
//				}
			}

		}
		
		// Cast it to mpfr
		mpfr_t mpXi[N];
		mpfr_t mpXr[N];
		
		for(int i=0; i<N; i++)
		{	
			mpfr_init2(mpXi[i], msbin-lsbin+2);
			mpfr_init2(mpXr[i], msbin-lsbin+2);
			mpfr_set_z(mpXi[i], svXi[i].get_mpz_t(), GMP_RNDN); // should be exact
			mpfr_set_z(mpXr[i], svXr[i].get_mpz_t(), GMP_RNDN); // should be exact
			mpfr_mul_2si(mpXi[i], mpXi[i], lsbin, GMP_RNDN); //Exact
			mpfr_mul_2si(mpXr[i], mpXr[i], lsbin, GMP_RNDN); //Exact
		}

		
		/////////////////////////////////////////////////////////////////
		mpfr_t mpRi[N];  //Result array, Im
		mpfr_t mpRr[N];  //Result array, Re

		for(int i=0; i<N; i++)
		{
			mpfr_t sumre, sumim;
			mpfr_init2(sumre, 10*wOut);
			mpfr_init2(sumim, 10*wOut);
			
			mpfr_set_d(sumre, 0.0, GMP_RNDN); // initialize to 0
			mpfr_set_d(sumim, 0.0, GMP_RNDN); // initialize to 0
			for(int j=0; j<N; j++)
			{
				mpfr_t angle, twiddleIm, twiddleRe, constPi, temp;
				mpz_class intTemp;
				int input_width = msbin-lsbin+1;
				
				mpfr_init2(twiddleIm, 	10*input_width);
				mpfr_init2(twiddleRe, 	10*input_width);
				mpfr_init2(angle, 	10*input_width);
				mpfr_init2(constPi, 	10*input_width);
				mpfr_init2(temp, 	10*input_width);
				
				mpfr_const_pi(	constPi, GMP_RNDN);
				
				mpfr_set_d(	angle, 		i,			GMP_RNDN);
				mpfr_mul_si(	angle, angle, 	j, 			GMP_RNDN);
				mpfr_mul_si(	angle, angle, 	2, 			GMP_RNDN);
				mpfr_mul(	angle, angle, 	constPi, 		GMP_RNDN);
				mpfr_div_si(	angle, angle, 	N, 			GMP_RNDN);
				
				mpfr_sin_cos(	twiddleIm, 	twiddleRe, 		angle, GMP_RNDN);
				
				//if negative constant, then set negativeConstant
				negativeConstanti = (mpfr_cmp_si(twiddleIm, 0) < 0 ? true : false);
				negativeConstantr = (mpfr_cmp_si(twiddleRe, 0) < 0 ? true : false);

				mpfr_t mpXrTwr, mpXiTwi, mpXrTwi, mpXiTwr, tempr, tempi;
				mpfr_init2(mpXrTwr, 10*wOut);
				mpfr_init2(mpXiTwi, 10*wOut);
				mpfr_init2(mpXrTwi, 10*wOut);
				mpfr_init2(mpXiTwr, 10*wOut);
				mpfr_init2(tempr, 10*wOut);
				mpfr_init2(tempi, 10*wOut);
				
				mpfr_mul(mpXrTwr, mpXr[j], 	twiddleRe, 	GMP_RNDN);
				mpfr_mul(mpXiTwi, mpXi[j], 	twiddleIm, 	GMP_RNDN);
				mpfr_mul(mpXrTwi, mpXr[j], 	twiddleIm, 	GMP_RNDN);
				mpfr_neg(mpXrTwi, mpXrTwi,			GMP_RNDN);
				mpfr_mul(mpXiTwr, mpXi[j], 	twiddleRe, 	GMP_RNDN);
				
				mpfr_add(tempr, mpXrTwr, mpXiTwi, 	GMP_RNDN);
				mpfr_add(tempi, mpXrTwi, mpXiTwr, 	GMP_RNDN);

				mpfr_add(sumre, sumre, tempr, 	GMP_RNDN);
				mpfr_add(sumim, sumim, tempi, 	GMP_RNDN);
			}
			mpfr_init2(mpRr[i], 10*wOut);
			mpfr_init2(mpRi[i], 10*wOut);
			mpfr_set(mpRr[i], sumre, GMP_RNDN); 
			mpfr_set(mpRi[i], sumim, GMP_RNDN); 
		}
		
			
		for(int i=0; i<N; i++)
		{
			// scale back to an integer
			mpfr_mul_2si(mpRi[i], mpRi[i], -lsbout, GMP_RNDN); //Exact
			mpfr_mul_2si(mpRr[i], mpRr[i], -lsbout, GMP_RNDN); //Exact
			mpz_class svRui, svRdi, svRur, svRdr;

			mpfr_get_z(svRdi.get_mpz_t(), mpRi[i], GMP_RNDD);
//			svRdi=signedToBitVector(svRdi, 1+msbout-lsbout);
			mpfr_get_z(svRui.get_mpz_t(), mpRi[i], GMP_RNDU);
//			svRui=signedToBitVector(svRui, 1+msbout-lsbout);
			mpfr_get_z(svRdr.get_mpz_t(), mpRr[i], GMP_RNDD);
//			svRdr=signedToBitVector(svRdr, 1+msbout-lsbout);
			mpfr_get_z(svRur.get_mpz_t(), mpRr[i], GMP_RNDU);
//			svRur=signedToBitVector(svRur, 1+msbout-lsbout);

			//		cout << " emulate x="<< svX <<"  before=" << svRd;
/*			if(negativeInputi != negativeConstanti)
			{
				svRdi += (mpz_class(1) << wOut);
				svRui += (mpz_class(1) << wOut);
			}
			if(negativeInputr != negativeConstantr)
			{
				svRdr += (mpz_class(1) << wOut);
				svRur += (mpz_class(1) << wOut);
			}
			//		cout << " emulate after=" << svRd << endl;
*/
			//Border cases
			if(svRdi > (mpz_class(1) << wOut) - 1 )		
			{
				svRdi = 0;
			}

			if(svRui > (mpz_class(1) << wOut) - 1 )		
			{
				svRui = 0;
			}
		
			if(svRdr > (mpz_class(1) << wOut) - 1 )		
			{
				svRdr = 0;
			}

			if(svRur > (mpz_class(1) << wOut) - 1 )		
			{
				svRur = 0;
			}


			tc->addExpectedOutput(join("Yi", i), svRdi);
			tc->addExpectedOutput(join("Yi", i), svRui);
			tc->addExpectedOutput(join("Yr", i), svRdr);
			tc->addExpectedOutput(join("Yr", i), svRur);
		}
		


		mpfr_clears (*mpXi, *mpXr, NULL);
//		mpfr_clears (svXrWi, svXiWr, mpXi, mpXr, NULL);



	}
	
	//FIXME: correct the emulate function
	void FixFFTFullyPA::emulate2 ( TestCase* tc )
	{


		int nrGroups = 1;
		int distance = N/2;
		int wIn=msbin-lsbin+1;
		int wOut=msbout-lsbout+1;
		bool negativeInputr = false;
		bool negativeInputi = false;
		bool negativeConstanti = false;
		bool negativeConstantr = false;
		
		mpz_class svXi[N];
		mpz_class svXr[N];
		
		int n = log2(N);
		
		// Get I/O values
		for(int i=0; i<N; i++)
		{
			svXi[i] = tc->getInputValue(join("Xi_S0_", i));
			svXr[i] = tc->getInputValue(join("Xr_S0_", i));

			// get rid of two's complement
			if(signedIn)	{
				if ( svXi[i] > ( (mpz_class(1)<<(wIn-1))-1) )
				{
					svXi[i] -= (mpz_class(1)<<wIn);
					negativeInputi = true;
				}
				if ( svXr[i] > ( (mpz_class(1)<<(wIn-1))-1) )
				{
					svXr[i] -= (mpz_class(1)<<wIn);
					negativeInputr = true;
				}
			}

		}
		
		// Cast it to mpfr
		mpfr_t mpXi[N];
		mpfr_t mpXr[N];
		
		for(int i=0; i<N; i++)
		{	
			mpfr_init2(mpXi[i], msbin-lsbin+2);
			mpfr_init2(mpXr[i], msbin-lsbin+2);
			mpfr_set_z(mpXi[i], svXi[i].get_mpz_t(), GMP_RNDN); // should be exact
			mpfr_set_z(mpXr[i], svXr[i].get_mpz_t(), GMP_RNDN); // should be exact
			mpfr_mul_2si(mpXi[i], mpXi[i], lsbin, GMP_RNDN); //Exact
			mpfr_mul_2si(mpXr[i], mpXr[i], lsbin, GMP_RNDN); //Exact
		}

		for(int i=0; i<n; i++)
		{
			for(int j=0; j<nrGroups; j++)
			{
				int first, last, twiddleIndex;
				
				first = 2 * j * distance;
				last  = first + distance - 1;
				twiddleIndex = bitReverse(j, N);
				//twiddleIndex = j;
				
				for(int k=first; k<=last; k++)
				{
					mpz_class svWi = getTwiddleConstant(TWIDDLEIM, twiddleIndex);
					mpz_class svWr = getTwiddleConstant(TWIDDLERE, twiddleIndex);
					
					
					mpfr_t	mpsvWi, mpsvWr;

					mpfr_init2(mpsvWi, msbin-lsbin+2);
					mpfr_set_z(mpsvWi, svWi.get_mpz_t(), GMP_RNDN); // should be exact

					mpfr_init2(mpsvWr, msbin-lsbin+2);
					mpfr_set_z(mpsvWr, svWr.get_mpz_t(), GMP_RNDN); // should be exact

					//if negative constant, then set negativeConstant
					negativeConstanti = (mpfr_cmp_si(mpsvWi, 0) < 0 ? true : false);
					negativeConstantr = (mpfr_cmp_si(mpsvWr, 0) < 0 ? true : false);

					//getTwiddleConstant(TWIDDLEIM, twiddleIndex);
					//getTwiddleConstant(TWIDDLERE, twiddleIndex);
					
					if(twiddleIndex == 0)
					{
						//mpXi[k] = mpXi[k];
						//mpXr[k] = mpXr[k];
					}
					else if((double)twiddleIndex == (double)N/4.0)
					{
						mpfr_t tempMPZ;
						mpfr_init2(tempMPZ, msbin-lsbin+2);

						mpfr_set(tempMPZ, mpXi[k], GMP_RNDN); // should be exact

						//tempMPZ = mpXi[k];
						mpfr_set(mpXi[k], mpXr[k], GMP_RNDN); // should be exact
						//mpXi[k] = mpXr[k];
						mpfr_set(mpXr[k], tempMPZ, GMP_RNDN); // should be exact
						mpfr_neg (mpXr[k], mpXr[k], GMP_RNDN);
						//mpXr[k] = -tempMPZ;
//						mpXr[k] = (-1)*tempMPZ;
					}
					else
					{
						if ( signedIn)
						{
							// prepare the result
							mpfr_t mpRXiWr, mpRXrWi, mpRXrWr, mpRXiWi, svWXi, svWXr, svZAddi, svZAddr, svZSubi, svZSubr;

							mpfr_init2(mpRXiWr, 10*wOut);
							mpfr_mul(mpRXiWr, mpXi[k+distance], mpsvWr, GMP_RNDN);
							
							mpfr_init2(mpRXrWi, 10*wOut);
							mpfr_mul(mpRXrWi, mpXr[k+distance], mpsvWi, GMP_RNDN);
							
							mpfr_init2(mpRXrWr, 10*wOut);
							mpfr_mul(mpRXrWr, mpXr[k+distance], mpsvWr, GMP_RNDN);
							
							mpfr_init2(mpRXiWi, 10*wOut);
							mpfr_mul(mpRXiWi, mpXi[k+distance], mpsvWi, GMP_RNDN);
							
							mpfr_add (svWXi, mpRXiWr, mpRXrWi, GMP_RNDN);
							//mpfr_t svWXi = mpRXiWr + mpRXrWi;
							
							mpfr_sub (svWXr, mpRXrWr, mpRXiWi, GMP_RNDN);
							//mpfr_t svWXr = mpRXrWr - mpRXiWi;
							
//							mpfr_t svWXi = mpXi[k+distance]*svWr - mpXr[k+distance]*svWi;
//							mpfr_t svWXr = mpXr[k+distance]*svWr + mpXi[k+distance]*svWi;
							

							mpfr_add (svZAddi, mpXi[k], svWXi, GMP_RNDN);
							//mpfr_t svZAddi = mpXi[k] + svWXi;
							mpfr_add (svZAddr, mpXr[k], svWXr, GMP_RNDN);
							//mpfr_t svZAddr = mpXr[k] + svWXr;
							
							mpfr_sub (svZSubi, mpXi[k], svWXi, GMP_RNDN);
							//mpfr_t svZSubi = mpXi[k] - svWXi;
							mpfr_sub (svZSubr, mpXr[k], svWXr, GMP_RNDN);
							//mpfr_t svZSubr = mpXr[k] - svWXr;
							
							// Don't allow overflow
/*							mpz_clrbit ( svZAddi.get_mpz_t(), 2*w );
							mpz_clrbit ( svZAddr.get_mpz_t(), 2*w );
							mpz_clrbit ( svZSubi.get_mpz_t(), 2*w );
							mpz_clrbit ( svZSubr.get_mpz_t(), 2*w );
							*/

							mpfr_set(mpXi[k], svZAddi, GMP_RNDN); 
							//mpXi[k] 		 = svZAddi;
							mpfr_set(mpXr[k], svZAddr, GMP_RNDN); 
							//mpXr[k] 		 = svZAddr;
							mpfr_set(mpXi[k+distance], svZSubi, GMP_RNDN); 
							//mpXi[k+distance] = svZSubi;
							mpfr_set(mpXr[k+distance], svZSubr, GMP_RNDN); 
							//mpXr[k+distance] = svZSubr;
						}
						else
						{
/*							mpfr_t big1 = (mpz_class(1) << (wIn));
							mpfr_t big1P = (mpz_class(1) << (wIn-1));
							mpfr_t big2 = (mpz_class(1) << (wIn));
							mpfr_t big2P = (mpz_class(1) << (wIn-1));

							if ( mpXi[k] >= big1P)
								mpXi[k] = mpXi[k] - big1;
							if ( mpXr[k] >= big1P)
								mpXr[k] = mpXi[k] - big1;

							if ( mpXi[k+distance] >= big2P)
								mpXi[k+distance] = mpXi[k+distance] - big2;
							if ( mpXr[k+distance] >= big2P)
								mpXr[k+distance] = mpXr[k+distance] - big2;
							
							mpfr_init2(svXrWr, 10*wOut);
							mpfr_init2(svXiWi, 10*wOut);
							mpfr_init2(svXrWi, 10*wOut);
							mpfr_init2(svXiWr, 10*wOut);
							mpfr_mul(svXrWr, mpXr[k+distance], svWr, GMP_RNDN);
							mpfr_mul(svXiWi, mpXi[k+distance], svWi, GMP_RNDN);
							mpfr_mul(svXrWi, mpXr[k+distance], svWi, GMP_RNDN);
							mpfr_mul(svXiWr, mpXi[k+distance], svWr, GMP_RNDN);
							//mpfr_t svXrWr = mpXr[k+distance]*svWr;
							//mpfr_t svXiWi = mpXi[k+distance]*svWi;
							//mpfr_t svXrWi = mpXr[k+distance]*svWi;
							//mpfr_t svXiWr = mpXi[k+distance]*svWr;
							
							if ( svXrWr < 0)
							{
								mpfr_t tmpSUB = (mpz_class(1) << (2*wIn));
								svXrWr = tmpSUB + svXrWr; 
							}
							if ( svXiWi < 0)
							{
								mpfr_t tmpSUB = (mpz_class(1) << (2*wIn));
								svXiWi = tmpSUB + svXiWi; 
							}
							if ( svXiWr < 0)
							{
								mpfr_t tmpSUB = (mpz_class(1) << (2*wIn));
								svXiWr = tmpSUB + svXiWr; 
							}
							if ( svXrWi < 0)
							{
								mpfr_t tmpSUB = (mpz_class(1) << (2*wIn));
								svXrWi = tmpSUB + svXrWi; 
							}
							
							mpfr_t svWXi = svXiWr + svXrWi;
							mpfr_t svWXr = svXrWr - svXiWi;
							 
							// Don't allow overflow
//							mpz_clrbit ( svWXi.get_mpz_t(), 2*input_width );
//							mpz_clrbit ( svWXr.get_mpz_t(), 2*input_width );
							
							
							mpfr_t svZAddi = mpXi[k] + svWXi;
							mpfr_t svZAddr = mpXr[k] + svWXr;
							
							mpfr_t svZSubi = mpXi[k] - svWXi;
							mpfr_t svZSubr = mpXr[k] - svWXr;
							
							// Don't allow overflow
//							mpz_clrbit ( svZAddi.get_mpz_t(), 2*input_width );
//							mpz_clrbit ( svZAddr.get_mpz_t(), 2*input_width );
//							mpz_clrbit ( svZSubi.get_mpz_t(), 2*input_width );
//							mpz_clrbit ( svZSubr.get_mpz_t(), 2*input_width );
							
							
							mpXi[k] 		 = svZAddi;
							mpXr[k] 		 = svZAddr;
							mpXi[k+distance] = svZSubi;
							mpXr[k+distance] = svZSubr;
							*/
						}
					}
				}
			}
		}
		
		for(int i=0; i<N; i++)
		{
			// scale back to an integer
			mpfr_mul_2si(mpXi[i], mpXi[i], -lsbout, GMP_RNDN); //Exact
			mpfr_mul_2si(mpXr[i], mpXr[i], -lsbout, GMP_RNDN); //Exact
			mpz_class svRui, svRdi, svRur, svRdr;

			mpfr_get_z(svRdi.get_mpz_t(), mpXi[i], GMP_RNDD);
			mpfr_get_z(svRui.get_mpz_t(), mpXi[i], GMP_RNDU);
			mpfr_get_z(svRdr.get_mpz_t(), mpXr[i], GMP_RNDD);
			mpfr_get_z(svRur.get_mpz_t(), mpXr[i], GMP_RNDU);

			//		cout << " emulate x="<< svX <<"  before=" << svRd;
			if(negativeInputi != negativeConstanti)
			{
				svRdi += (mpz_class(1) << wOut);
				svRui += (mpz_class(1) << wOut);
			}
			if(negativeInputr != negativeConstantr)
			{
				svRdr += (mpz_class(1) << wOut);
				svRur += (mpz_class(1) << wOut);
			}
			//		cout << " emulate after=" << svRd << endl;

			//Border cases
			if(svRdi > (mpz_class(1) << wOut) - 1 )		
			{
				svRdi = 0;
			}

			if(svRui > (mpz_class(1) << wOut) - 1 )		
			{
				svRui = 0;
			}
		
			if(svRdr > (mpz_class(1) << wOut) - 1 )		
			{
				svRdr = 0;
			}

			if(svRur > (mpz_class(1) << wOut) - 1 )		
			{
				svRur = 0;
			}

			tc->addExpectedOutput(join("Yi", i), svRdi);
			tc->addExpectedOutput(join("Yi", i), svRui);
			tc->addExpectedOutput(join("Yr", i), svRdr);
			tc->addExpectedOutput(join("Yr", i), svRur);
		}
		


		mpfr_clears (*mpXi, *mpXr, NULL);
//		mpfr_clears (svXrWi, svXiWr, mpXi, mpXr, NULL);


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
	
	/*mpz_class FixFFTFullyPA::getTwiddleConstant(int constantType){
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


	void FixFFTFullyPA::buildStandardTestCases(TestCaseList * tcl) 
	{
		// Although standard test cases may be architecture-specific, it can't hurt to factor them here.
		TestCase* tc;

		int one = 1;
		if(lsbin < 0 && msbin >= 0)
		{
			//Real one
			one = one << -lsbin;
		}

		tc = new TestCase(this);

		for(int i=0; i<N; i++)
		{
			tc->addInput(join("Xi_S0_", i), 0);
			tc->addInput(join("Xr_S0_", i), 0);
		}
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);

		for(int i=0; i<N/2; i++)
		{
			tc->addInput(join("Xi_S0_", i), 0);
			tc->addInput(join("Xr_S0_", i), one);
		}
		for(int i=N/2; i<N; i++)
		{
			tc->addInput(join("Xi_S0_", i), 0);
			tc->addInput(join("Xr_S0_", i), 0);
		}
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);

		tc->addInput(join("Xi_S0_", 0), 0);
		tc->addInput(join("Xr_S0_", 0), one);
		for(int i=1; i<N; i++)
		{
			tc->addInput(join("Xi_S0_", i), 0);
			tc->addInput(join("Xr_S0_", i), 0);
		}
		emulate(tc);
		tcl->add(tc);
		
		tc = new TestCase(this);

		for(int i=0; i<N; i++)
		{
			tc->addInput(join("Xi_S0_", i), 0);
			tc->addInput(join("Xr_S0_", i), 0);
		}
		tc->addInput(join("Xi_S0_", 1), 0);
		tc->addInput(join("Xr_S0_", 1), one);
		emulate(tc);
		tcl->add(tc);
		
		tc = new TestCase(this);		
		for(int i=0; i<(N/2); i++)
		{
			tc->addInput(join("Xi_S0_", i), 0);
			tc->addInput(join("Xr_S0_", i), one);
		}
		for(int i=N/2; i<N; i++)
		{
			tc->addInput(join("Xi_S0_", i), 0);
			tc->addInput(join("Xr_S0_", i), 0);
		}
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);		
		for(int i=0; i<N; i++)
		{
			tc->addInput(join("Xi_S0_", i), one);
			tc->addInput(join("Xr_S0_", i), 0);
		}
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);	
		for(int i=0; i<N; i++)
		{
			tc->addInput(join("Xi_S0_", i), 0);
			tc->addInput(join("Xr_S0_", i), one);
		}
		emulate(tc);
		tcl->add(tc);
		
		tc = new TestCase(this);		
		for(int i=0; i<N; i++)
		{
			tc->addInput(join("Xi_S0_", i), one);
			tc->addInput(join("Xr_S0_", i), one);
		}
		emulate(tc);
		tcl->add(tc);

		if(signedIn)
		{
			tc = new TestCase(this);		
			for(int i=0; i<N; i++)
			{
				tc->addInput(join("Xi_S0_", i), -1 * one);
				tc->addInput(join("Xr_S0_", i), -1 * one);
			}
			emulate(tc);
			tcl->add(tc);
		}
	}




	OperatorPtr FixFFTFullyPA::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int msbin;
		UserInterface::parseInt(args, "msbin", &msbin);
		int lsbin;
		UserInterface::parseInt(args, "lsbin", &lsbin);
		int msbout;
		UserInterface::parseInt(args, "msbout", &msbout);
		int lsbout;
		UserInterface::parseInt(args, "lsbout", &lsbout);
		int N;
		UserInterface::parseInt(args, "N", &N);
		int radix;
		UserInterface::parseInt(args, "radix", &radix);
		bool signedIn;
		UserInterface::parseBoolean(args, "signedIn", &signedIn);
		bool decimation;
		UserInterface::parseBoolean(args, "decimation", &decimation);
		bool revbitorder;
		UserInterface::parseBoolean(args, "revbitorder", &revbitorder);

		return new FixFFTFullyPA(parentOp, target, msbin, lsbin, msbout, lsbout, N, radix, signedIn, decimation, revbitorder);
	}
	
	void FixFFTFullyPA::registerFactory(){
		UserInterface::add("FixFFTFullyPA", // name
											 "A Complex Radix-2 Butterfly.",
											 "Complex", // categories
											 "",
											 "msbin(int): integer input most significant bit;\
							lsbin(int): integer input least significant bit;\
							msbout(int): integer output most significant bit;\
							lsbout(int): integer output least significant bit;\
							N(int): integer size (points) of FFT;\
							radix(int): integer radix of FFT butterflies;\
							signedIn(bool)=true: if false calculation will be unsigned;\
							decimation(bool)=true: if true(default) DIF, else DIT FFT;\
							revbitorder(bool)=true: if true(default) reverse bit order output, else no change",

						 	"See the developper manual in the doc/ directory of FloPoCo.",
			 				FixFFTFullyPA::parseArguments
							) ;

	}

}




















