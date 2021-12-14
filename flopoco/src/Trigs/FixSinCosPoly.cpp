
// TODOs
// Sorry , this code is all parameterized by $w=-\ell_r+1$
// Move FixSinPoly here!
// Compare not-ing Y and negating it properly
// FixSinCosPolyTable has constant sign bit as soon as there is argument reduction. As we just hit 36 output bits, this is important
// The upper test for order-one seems wrong, I get a 38-Kbit table for ./flopoco -pipeline=no  -verbose=2 FixSinCosPoly 15 TestBenchFile 10

// One optim for 24 bits would be to compute z² for free by a table using the second unused port of the blockram

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ConstMult/FixRealKCM.hpp"
#include "IntMult//IntMultiplier.hpp"
#include "FixFunctions/FixFunctionByTable.hpp"
#include "BitHeap/BitHeap.hpp"
#include "FixSinPoly.hpp"
#include "FixSinCosPoly.hpp"
#include "Table.hpp"

#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers */
#include "mpfr.h"


using namespace std;
namespace flopoco{


#define USEBITHEAP 1





	////////////////////////////////////// SinCosTable ///////////////////
	// argRedCase=1 for full table, 4 for quadrant, 8 for octant
	// 
	vector<mpz_class> FixSinCosPoly::buildSinCosTable(int wA, int lsbOut, int g, int argRedCase){
		vector<mpz_class> result;
		for (int32_t x=0; x<(1<<wA); x++) {
			mpz_class sin,cos;
			mpfr_t a, c, s;
			
			mpfr_init2(c, -10*lsb); // cosine
			mpfr_init2(s, -10*lsb); // sine

			// 2's complement convertion of x into xs
			int xs=x;
			if ( (xs>>(wA-1)) && (argRedCase==1) )
				xs-=(1<<wA);

			// a will be used for the value in xs
			mpfr_init2(a,10*wA-1); 
			mpfr_set_si(a, xs, GMP_RNDN);

			//REPORT(0,"Evaluating function on point x="<<x<<" positive value xs="<<xs<<" converted value a="<<printMPFR(a, 10));

			//divide by 2^w then we get a true fixpoint number between -1 and 1.
			if (argRedCase==1){
				if (xs>>(wA-1))
					xs-=(1<<wA);
				mpfr_div_2si(a,a,wA-1, GMP_RNDN);
				//REPORT(0,"a divided by 2^"<<wA<<" a="<<printMPFR(a,10));
			}

			else if (argRedCase==4)	{ //quadrant
				mpfr_div_2si(a,a,wA+1, GMP_RNDN);
				//REPORT(0,"a divided by 2^"<<wA<<" a="<<printMPFR(a,10));
			}

			else if (argRedCase==8)	{ // octant 
				mpfr_div_2si(a,a,wA+2, GMP_RNDN);
			}
			else 
				THROWERROR("Bad value for argRedCase in FixSinCosPoly::SinCosTable: " << argRedCase);

			mpfr_mul(a, a, constPi, GMP_RNDN);

			mpfr_sin_cos(s, c, a, GMP_RNDN); //function evaluation on point x

			mpfr_mul(s, s, scale, GMP_RNDN); //rescale the sine
			mpfr_mul(c, c, scale, GMP_RNDN); //rescale the cosine

			//REPORT(0," s="<<printMPFR(s,10)<<"; c="<<printMPFR(c,10));

			mpfr_mul_2si(s, s, lsbOut+g, GMP_RNDN); //scale to int
			mpfr_mul_2si(c, c, lsbOut+g, GMP_RNDN); 

			mpfr_get_z(sin.get_mpz_t(), s, GMP_RNDN); //rounding into mpz
			mpfr_get_z(cos.get_mpz_t(), c, GMP_RNDN);

			if(g){ // add the round bit
				sin += (mpz_class(1)<<(g-1));
				cos += (mpz_class(1)<<(g-1));
			}
			//REPORT(0,"Calculated values before 2's complement test: sin="<<sin.get_mpz_t()<<"; cos="<<cos.get_mpz_t());

			// no more need intermediates a, c, and s
			mpfr_clears(a, c, s, NULL);

			if(argRedCase==1) { // Full table: need to manage two's complement

				// check if negative, then 2's complement
				if(sin<0){
					sin+=mpz_class(1)<<(lsbOut+g+1); 
				}
		
				if (cos<0){
					cos+=mpz_class(1)<<(lsbOut+g+1); 
				}
			}

			// REPORT(0," function() returns. Value: "<<(sin+(cos<<wA))<<" ( sin=" << sin<<" , cos="<<cos<<  " )");
			result.push_back( cos  + ( sin << (lsbOut+g + (argRedCase==1?1:0)) ) ); 
		}
		return result;
	}






	////////////////////////////////////// FixSinCosPoly ///////////////////

	FixSinCosPoly::FixSinCosPoly(OperatorPtr parentOp, Target * target, int lsb_):
		FixSinCos(parentOp,target, lsb_){ 
		int g=-42; // silly value from the start, because so many different paths may assign it (or forget to do so) 
		srcFileName="FixSinCosPoly";

		w=-lsb; //sorry, old code.
		// definition of the name of the operator
		ostringstream name;
		name << "FixSinCosPoly_LSBm" << -lsb;
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Florent de Dinechin, Antoine Martinet, Guillaume Sergent, (2013-2019)");



		// These are limits between small-precision cases for which we generate simpler architectures
		// Rememember that w=-lsb hence the size of inputs and outputs is actually w+1. It is a bug but Won't fix

		// plain tabulation fits LUTs
		bool wSmallerThanBorder1 = ( w <= getTarget()->lutInputs() );    

		//  table with quadrant reduction fits LUTs
		bool wSmallerThanBorder2 = ( w-2 <= getTarget()->lutInputs() );   

		// plain tabulation fits BlockRAM
		bool wSmallerThanBorder3 = ( (w+1)*(mpz_class(2)<<(w+1)) <= getTarget()->sizeOfMemoryBlock() );

		//  table with quadrant reduction fits BlockRAM
		bool wSmallerThanBorder4 = ( (w+1)*(mpz_class(2)<<(w+1-3)) <= getTarget()->sizeOfMemoryBlock() );

		bool usePlainTable = wSmallerThanBorder1 || (!wSmallerThanBorder2 && wSmallerThanBorder3);
		bool usePlainTableWithQuadrantReduction = wSmallerThanBorder2 || (!wSmallerThanBorder3 && wSmallerThanBorder4);


		// order-1 architecture: sinZ \approx Z, cosZ \approx 1
		int gOrder1Arch = w<14? 3 : 4; // exhaustive tests show that g=3 is enough for small sizes...
		// order-2 architecture: sinZ \approx Z, cosZ \approx 1-Z^2/2
		int gOrder2Arch = w<17? 3 : 4;  // exhaustive tests show that g=3 is enough for small sizes...
		// generic case:
		int gGeneric=4;

		// For all the other methods we need to address a table of sincos with wA bits of the input
		// Let us compute wA such that these bits fit in a blockRAM
		// but it depends on g, so we compute the various cases
		int wAtemp=3;
		while((mpz_class(2)<<wAtemp)*(w+1+gOrder1Arch) < getTarget()->sizeOfMemoryBlock()) wAtemp++; 
		int wAOrder1Arch = wAtemp--;
		wAtemp=3;
		while((mpz_class(2)<<wAtemp)*(w+1+gOrder2Arch) < getTarget()->sizeOfMemoryBlock()) wAtemp++; 
		int wAOrder2Arch = wAtemp--;
		wAtemp=3;
		while((mpz_class(2)<<wAtemp)*(w+1+gGeneric) < getTarget()->sizeOfMemoryBlock()) wAtemp++; 
		int wAGeneric = wAtemp--;


		// Now we may compute the borders on the simplified cases
		// Y will be smaller than 1/4 => Z will be smaller than pi*2^(-wA-2), or Z<2^-wA 
		// for sinZ we neglect something in the order of Z^2/2 : we want 2^(-2wA-1) < 2^(lsb-g)    2wA+1>w+g
		bool wSmallerThanBorderFirstOrderTaylor = (w-1 + gOrder1Arch <= 2*wAOrder1Arch + 1);

		// now we neglect something in the order of Z^3/6 (smaller than Z^3/4): we want 2^(-3wA-2) < 2^(lsb-g)
		bool wSmallerThanBorderSecondOrderTaylor = (w-1 + gOrder2Arch <= 3*wAOrder2Arch + 2);



		REPORT(0*DEBUG, "Boundaries on the various cases for w="<<w << ": " << wSmallerThanBorder1 << wSmallerThanBorder2 << wSmallerThanBorder3 << wSmallerThanBorder4 << wSmallerThanBorderFirstOrderTaylor<< wSmallerThanBorderSecondOrderTaylor);





		if (usePlainTable)	{
			REPORT(INFO, "Simpler architecture: Using plain table" );
			//int sinCosSize = 2*(w_+1); // size of output (sine plus cosine in a same number, sine on high weight bits)
			vhdl << tab << declare("sinCosTabIn", w+1) << " <= X;" << endl;// signal declaration
			vector<mpz_class> tableContent = buildSinCosTable(w+1, w, 0, 1);
			Table::newUniqueInstance(this,
															 "sinCosTabIn", "SC",
															 tableContent, "sincosTable",
															 w+1, 2*(w+1) );
														 
			vhdl << tab << declare("Sine", w+1) << " <= SC" << range(2*(w+1)-1, w+1) << ";" << endl;// signal declaration
			vhdl << tab << declare("Cosine", w+1) << " <= SC" << range(w, 0) << ";" << endl;// signal declaration
			vhdl << tab<< "S <= Sine;"<<endl;
			vhdl << tab<< "C <= Cosine;"<<endl;

		}

		else if (usePlainTableWithQuadrantReduction) 	{
			REPORT(INFO, "Simpler architecture: Using plain table with quadrant reduction");
			/*********************************** RANGE REDUCTION **************************************/
			// the argument is reduced into (0,1/2) because one sin/cos
			// computation in this range can always compute the right sin/cos
			// 2's complement: 0 is always positive
			vhdl << tab << declare ("X_sgn") << " <= X" << of (w) << ";" << endl;
			vhdl << tab << declare ("Q") << " <= X" << of (w-1) << ";" << endl;
			vhdl << tab << declare ("sinCosTabIn", w-1) << " <= X " << range (w-2,0) << ";" << endl;

			vhdl << tab << declare("C_sgn")
					 << " <= Q xor X_sgn;" << endl; //sign of cosin

			/*********************************** REDUCED TABLE **************************************/
			vector<mpz_class> tableContent = buildSinCosTable(w-1, w, 0, 4);
			Table::newUniqueInstance(this,
															 "sinCosTabIn", "SC_red",
															 tableContent, "sincosTable",
															 w-1, 2*w );

			vhdl << tab << declare("S_out", w) << " <= SC_red " << range( 2*w-1 , w ) << ";" << endl;// signal declaration
			vhdl << tab << declare("C_out", w) << " <= SC_red " << range( w-1, 0 ) << ";" << endl;// signal declaration

			/*********************************** Reconstruction of both sine and cosine **************************************/

			vhdl << tab << declare ("S_wo_sgn", w)
					 << " <= C_out when Q = '1' else S_out;" << endl; //swap sine and cosine if q.
			vhdl << tab << declare ("C_wo_sgn", w)
					 << " <= S_out when Q = '1' else C_out;" << endl;


			vhdl << tab << declare ("S_wo_sgn_ext", w+1)
					 << " <= '0' & S_wo_sgn;" << endl
					 << tab << declare ("C_wo_sgn_ext", w+1)
					 << " <= '0' & C_wo_sgn;" << endl; //S_wo_sgn_ext and C_wo_sgn are the positive versions of the sine and cosine

			vhdl << tab << declare ("S_wo_sgn_neg", w+1)
					 << " <= (not S_wo_sgn_ext) + 1;" << endl;
			vhdl << tab << declare ("C_wo_sgn_neg", w+1)
					 << " <= (not C_wo_sgn_ext) + 1;" << endl; //2's complement. We have now the negative version of the sine and cosine results


			vhdl << tab << "S <= S_wo_sgn_ext when X_sgn = '0'"
					 << " else S_wo_sgn_neg;" << endl
					 << tab << "C <= C_wo_sgn_ext when C_sgn = '0'"
					 << " else C_wo_sgn_neg;" << endl; //set the correspondant value to C and S according to input sign
		}



		else { // From now on we will have a table-based argument reduction
			REPORT(INFO, "Using a table-based argument reduction");
		// We must set g here (early) in order to be able to factor out code that uses g
		g=0; 
		int wA=0;
		if (!wSmallerThanBorder4 && wSmallerThanBorderFirstOrderTaylor) {
			g=gOrder1Arch;
			wA = wAOrder1Arch;
			REPORT(INFO, "Using order-1 arch with wA="<<wA << " and g=" << g);
		}
		else if(wSmallerThanBorderSecondOrderTaylor) {
			g = gOrder2Arch;
			wA = wAOrder2Arch;
			REPORT(INFO, "Using order-2 arch with wA="<<wA << " and g=" << g);
		}
		else{ // generic case
			g=gGeneric;
			wA=wAGeneric;
			// In the generic case we neglect order-4 term, Z^4/24 < Z^4/16
			// Let us check that our current wA allows that, otherwise increase it. 
			while (w > 4*wA-4-g) {
				wA++;
			}
			REPORT(INFO, "Using order-3 arch with wA="<<wA << " and g=" << g);
		}
			
			/*********************************** RANGE REDUCTION **************************************/ 
			addComment("The argument is reduced into (0,1/4)");
			vhdl << tab << declare ("X_sgn") << " <= X" << of (w) << ";  -- sign" << endl;
			vhdl << tab << declare ("Q") << " <= X" << of (w-1) << ";  -- quadrant" << endl;
			vhdl << tab << declare ("O") << " <= X" << of (w-2) << ";  -- octant" << endl;
			vhdl << tab << declare ("Y",w-2) << " <= X " << range (w-3,0) << ";" << endl;

			// now X -> X_sgn + Q*.5 + O*.25 + Y where Q,O \in {0,1} and Y \in {0,.25}
			
			int wYIn=w-2+g;
		
			addComment("Computing .25-Y :  we do a logic NOT, at a cost of 1 ulp");
			vhdl << tab << declare (getTarget()->logicDelay(), "Yneg", wYIn) << " <= ((not Y) & " << '"' << std::string (g, '1') << '"' << ") when O='1' "
					 << "else (Y & " << '"' << std::string (g, '0') << '"' << ");" << endl;

			int wY = wYIn-wA; // size of Y_red
		
			vhdl << tab << declare ( "A", wA) << " <= Yneg " << range(wYIn-1, wYIn-wA) << ";" << endl;
			vhdl << tab << declare ("Y_red", wY) << " <= Yneg" << range (wYIn-wA-1,0) << ";" << endl; // wYin-wA=wY: OK

			//------------------------------------SinCosTable building for A -------------------------------------
			vector<mpz_class> tableValues = buildSinCosTable(wA, w, g, 8);
			Table::newUniqueInstance(this, "A", "SCA", 
															 tableValues,
															 "sinCosPiATable", //name
															 wA, // wIn
															 2*(w+g) //wOut
															 );
		
			vhdl << tab << declare("SinPiA", w+g) << " <= SCA " << range( 2*(w+g)-1 , w+g ) << ";" << endl;
			vhdl << tab << declare("CosPiA", w+g) << " <= SCA " << range( w+g-1, 0 ) << ";" << endl;
			//-------------------------------- MULTIPLIER BY PI ---------------------------------------
		
			int wZ=w-wA+g; // see alignment below. Actually w-2-wA+2  (-2 because Q&O bits, +2 because mult by Pi)

			newInstance("FixRealKCM", "MultByPi",
									join("msbIn=", -2-wA-1) + join(" lsbIn=",-w-g) + join(" lsbOut=",-w-g)  + " constant=pi signedIn=0",
									"X=>Y_red",
									"R=>Z");

			int wZz=getSignalByName("Z")->width();
			REPORT(DEBUG, "wZ=" <<wZ<<";"<<" wZz="<<wZz<<";");



			if (wSmallerThanBorderFirstOrderTaylor) {
				// TODO bitheapize
				REPORT(INFO,"Simpler architecture: Using only first order Taylor");

				//---------------------------- Sine computation ------------------------
				vhdl << tab <<  declare("SinPiACosZ",w+g) << " <= SinPiA; -- For these sizes  CosZ approx 1"<<endl; // msb is -1; 
				vhdl << tab << declare("CosPiAtrunc", wZ ) << " <= CosPiA" << range( w+g-1, w+g-wZ ) <<";" <<endl; // 
				vhdl << tab << declare(getTarget()->DSPMultiplierDelay(), "CosPiASinZ", 2*wZ)
						 << " <= CosPiAtrunc*Z;  -- For these sizes  SinZ approx Z" <<endl; //
				// msb of CosPiASinZ is that of Z, plus 2 (due to multiplication by Pi)
				//   for g=2 and wA=4:          :  .QOAAAAYYYYgg
				//                                      ZZZZZZZZ
				// to align with sinACosZ:         .XXXXXXXXXXgg we need to add wA+2-2 zeroes. 
				// and truncate cosAsinZ to the size of Z, too
				vhdl << tab << declare(getTarget()->adderDelay(w+g), "PreSinX", w+g)
						 << " <= SinPiACosZ + ( " << zg(wA) << " & (CosPiASinZ" << range( 2*wZ-1, 2*wZ - (w+g - wA) ) << ") );"<<endl;

				//---------------------------- Cosine computation -------------------------------
				vhdl << tab << declare("CosPiACosZ", w+g ) << " <= CosPiA; -- For these sizes  CosZ approx 1" << endl;
				vhdl << tab << declare("SinPiAtrunc", wZ ) << " <= SinPiA" << range( w+g-1, w+g-wZ ) <<";" <<endl; // 
				vhdl << tab << declare("SinPiASinZ", 2*wZ ) << " <= SinPiAtrunc*Z;  -- For these sizes  SinZ approx Z" <<endl; //
				vhdl << tab << declare(getTarget()->adderDelay(w+g), "PreCosX", w+g)
						 << " <= CosPiACosZ - ( " << zg(wA) << " & (SinPiASinZ" << range( 2*wZ-1, 2*wZ - (w+g - wA) )<< ") );" << endl;

				// Reconstruction expects a positive C_out and S_out, without their sign bits
				vhdl << tab << declare ("C_out", w) << " <= PreCosX" << range (w+g-1, g) << ';' << endl;
				vhdl << tab << declare ("S_out", w) << " <= PreSinX" << range (w+g-1, g) << ';' << endl;
			}



			else if (wSmallerThanBorderSecondOrderTaylor) {

				REPORT(DETAILED,"Using first-order Taylor for sine and second-order for cosine");
				// TODO bitheapize
				//--------------------------- SQUARER --------------------------------
		
				// Z < 2^-wA  :
				//   for g=2 and wA=4:          :  .QOAAAAYYYYgg
				//                                 .0000ZZZZZZZZ
				// Z^2/2 <  2^(-2wA-1):            .000000000SSS
				int wZ2o2 = w+g-(2*wA+1); // according to figure above
				vhdl << tab << declare("Z_trunc_for_square", wZ2o2) << "<= Z "<<range(wZ-1, wZ-wZ2o2) << ";" << endl;
				//				cerr << "*********** " << 	"f=x*x msbOut=-1 signedIn=0 " + join("lsbIn=",-wZ2o2) + join(" lsbOut=",-wZ2o2);
				newInstance("FixFunctionByTable", "ZSquarer",
										"f=x*x msbOut=-1 signedIn=0 " + join("lsbIn=",-wZ2o2) + join(" lsbOut=",-wZ2o2),
									"X=>Z_trunc_for_square",
									"Y=>Z2o2");
							
				vhdl << tab << declare("CosPiA_trunc_to_z2o2", wZ2o2) << " <= CosPiA" << range(w+g-1, w+g-wZ2o2) << ";" << endl;
				vhdl << tab << declare(getTarget()->DSPMultiplierDelay(), "Z2o2CosPiA", 2*wZ2o2)
						 << " <=  CosPiA_trunc_to_z2o2 * Z2o2;" << endl;
				vhdl << tab << declare("Z2o2CosPiA_aligned", w+g)<< " <= " << zg(2*wA+1);
				if(wZ2o2 >= 1)
					vhdl << " & Z2o2CosPiA" << range(2*wZ2o2-1, 2*wZ2o2- wZ2o2);
				vhdl << ";" << endl;
				vhdl << tab << declare(getTarget()->adderDelay(w+g), "CosPiACosZ", w+g) << "<= CosPiA - Z2o2CosPiA_aligned;" << endl;

				vhdl << tab << declare("SinPiA_trunc_to_z2o2", wZ2o2) << " <= SinPiA" << range(w+g-1, w+g-wZ2o2) << ";" << endl;
				vhdl << tab << declare(getTarget()->DSPMultiplierDelay(), "Z2o2SinPiA", 2*wZ2o2)
						 << " <=  SinPiA_trunc_to_z2o2 * Z2o2;" << endl;
				vhdl << tab << declare("Z2o2SinPiA_aligned", w+g)<< " <= " << zg(2*wA+1);
				if(2*wZ2o2-1 >= 2*wZ2o2- wZ2o2)
					vhdl << " & Z2o2SinPiA" << range(2*wZ2o2-1, 2*wZ2o2- wZ2o2);
				vhdl << ";" << endl;
				vhdl << tab << declare(getTarget()->adderDelay(w+g), "SinPiACosZ", w+g)
						 << "<= SinPiA - Z2o2SinPiA_aligned;" << endl;

				vhdl << tab << declare(getTarget()->DSPMultiplierDelay(),  "CosPiAZ", w+g +  wZ)
						 << " <= CosPiA*Z;  -- TODO check it fits DSP" <<endl;
				vhdl << tab << declare("CosPiASinZ", w+g) << " <= " << zg(wA) << " & CosPiAZ"  << range(w+g+wZ-1, w+g+wZ- (w+g-wA)) << ";" <<endl; // alignment according to figure above
				vhdl << tab << declare(getTarget()->DSPMultiplierDelay(),  "SinPiAZ", w+g +  wZ)
						 << " <= SinPiA*Z;  -- TODO check it fits DSP" <<endl;
				vhdl << tab << declare("SinPiASinZ", w+g) << " <= " << zg(wA) << " & SinPiAZ"  << range(w+g+wZ-1, w+g+wZ- (w+g-wA)) << ";" <<endl; // alignment according to figure above

				vhdl << tab << declare(getTarget()->adderDelay(w+g), "PreSinX", w+g)
						 << " <= SinPiACosZ + CosPiASinZ;"<<endl;
				vhdl << tab << declare(getTarget()->adderDelay(w+g), "PreCosX", w+g)
						 << " <= CosPiACosZ - SinPiASinZ;"<<endl;

				// Reconstruction expects a positive C_out and S_out, without their sign bits
				vhdl << tab << declare ("C_out", w) << " <= PreCosX" << range (w+g-1, g) << ';' << endl;
				vhdl << tab << declare ("S_out", w) << " <= PreSinX" << range (w+g-1, g) << ';' << endl;
			}
		
			else	{
				REPORT(DETAILED, "Using generic architecture with 3rd-order Taylor");
				/*********************************** THE SQUARER **************************************/
			

				// vhdl:sqr (Z -> Z2o2)
				// we have no truncated squarer as of now
				/*IntSquarer *sqr_z;
					sqr_z = new IntSquarer (target, wZ);
					addSubComponent (sqr_z);
					inPortMap (sqr_z, "X", "Z");
					outPortMap (sqr_z, "R", "Z2o2_ext");
					vhdl << instance (sqr_z, "sqr_z");
					// so now we truncate unnecessarily calculated bits of Z2o2_ext
					int wZ2o2 = 2*wZ - (w+g);
					vhdl << declare ("Z2o2",wZ2o2) << " <= Z2o2_ext"
					<< range (wZ-1,wZ-wZ2o2) << ";" << endl;*/
				// so we use a truncated multiplier instead
				int wZ2o2 = 2*wZ - (w+g)-1;
				if (wZ2o2 < 2)
					wZ2o2 = 2; //for sanity
				vhdl << tab << "-- truncate the inputs of the multiplier to the precision of the output" << endl;
				vhdl << tab << declare("Z_truncToZ2", wZ2o2) << " <= Z" << range(wZ-1, wZ-wZ2o2) << ";" << endl;

				// TODO a squarer ! For 40 bits, z2o2=19 
				newInstance("IntMultiplier", "sqr_z",
										"wX=" + to_string(wZ2o2) + " wY=" + to_string(wZ2o2) + " wOut=" + to_string(wZ2o2) + " signedIO=false",
										"X=>Z_truncToZ2,Y=>Z_truncToZ2",
										"R=>Z2o2");

			
				/*********************************** Z-Z^3/6 **************************************/
			
				//	int wZ3 = 3*wZ - 2*(w+g) -1; // -1 for the div by 2
				int wZ3o6 = 3*wZ - 2*(w+g) -2;
				if (wZ3o6 < 2)
					wZ3o6 = 2; //using 1 will generate bad vhdl
			
				if(wZ3o6<=12) {
					REPORT(DETAILED, "Tabulating Z^3/6 as we need it only on " << wZ3o6 << " bits"); 
					vhdl << tab << "-- First truncate Z to its few MSBs relevant to the computation of Z^3/6" << endl;
					vhdl << tab << declare("Z_truncToZ3o6", wZ3o6) << " <= Z" << range(wZ-1, wZ-wZ3o6) << ";" << endl;
					newInstance("FixFunctionByTable",
											"Z3o6Table",
											"f=x^3/6 signedIn=false lsbIn=" + to_string(-wZ3o6) + " msbOut=-3" + " lsbOut=" + to_string(-wZ3o6-2),
											"X=>Z_truncToZ3o6",
											"Y=>Z3o6");
					
					vhdl << tab << declare (getTarget()->adderDelay(wZ), "SinZ", wZ) << " <= Z - Z3o6;" << endl;				
				}
				else {
					REPORT(DETAILED, "Using a bit-heap based computation of Z-Z^3/6,  we need it on " << wZ3o6 << " bits"); 
					// This component is for internal use only, it has no user interface: using its constructor 
					schedule();
					inPortMap ("X", "Z");
					outPortMap("R", "SinZ");
					FixSinPoly *fsp =new FixSinPoly(parentOp,
																					target, 
																					-wA-1, //msbin
																					-w-g, // lsbin
																					true, // truncated
																					-wA-1, // msbOut_ = 0,
																					-w-g, // lsbout
																					false);
					vhdl << instance (fsp, "ZminusZ3o6");
				}


				// vhdl:sub (Z, Z3o6 -> SinZ)
			


				// and now, evaluate Sin Yneg and Cos Yneg
				// Cos Yneg:
				// vhdl:slr (Z2o2 -> Z2o2)
			
			



				vhdl << tab << "-- ********************************** Reconstruction of cosine ************************************" << endl;

		
#if 1 		// No bit heap
				// // vhdl:id (CosPiA -> C_out_1)
				// vhdl:mul (Z2o2, CosPiA -> Z2o2CosPiA)
			
				vhdl << tab << "--  truncate the larger input of each multiplier to the precision of its output" << endl;
				vhdl << tab << declare("CosPiA_truncToZ2o2", wZ2o2) << " <= CosPiA" << range(w+g-1, w+g-wZ2o2) << ";" << endl;

				newInstance("IntMultiplier", "c_out_2_compute",
										"wX=" + to_string(wZ2o2) + " wY=" + to_string(wZ2o2) + " wOut=" + to_string(wZ2o2) + " signedIO=false",
										"X=>Z2o2,Y=>CosPiA_truncToZ2o2",
										"R=>Z2o2CosPiA");


				vhdl << tab << "--  truncate the larger input of each multiplier to the precision of its output" << endl;
				vhdl << tab << declare("SinPiA_truncToZ", wZ) << " <= SinPiA" << range(w+g-1, w+g-wZ) << ";" << endl;


				// vhdl:mul (SinZ, SinPiA -> SinZSinPiA)
				newInstance("IntMultiplier", "c_out_3_compute",
										"wX=" + to_string(wZ) + " wY=" + to_string(wZ) + " wOut=" + to_string(wZ) + " signedIO=false",
										"X=>SinZ,Y=>SinPiA_truncToZ",
										"R=>SinZSinPiA");
			
				vhdl << tab << declare (getTarget()->adderDelay(w+g), "CosZCosPiA_plus_rnd", w+g)
						 << " <= CosPiA - Z2o2CosPiA;" << endl;
				vhdl << tab << declare (getTarget()->adderDelay(w+g), "C_out_rnd_aux", w+g)
						 << " <= CosZCosPiA_plus_rnd - SinZSinPiA;" << endl;

				vhdl << tab << declare ("C_out", w)
						 << " <= C_out_rnd_aux" << range (w+g-1, g) << ';' << endl;


				// ---------------------------------------------
#else //Bit heap computing Cos Z  ~   CosPiA - Z2o2*cosPiA - sinZ*SinPiA
			//
			// cosPiA   xxxxxxxxxxxxxxxxxxggggg
			//
			
#define TINKERCOS 1
#if TINKERCOS
				int gMult=0;
#else
				int g1 = IntMultiplier::neededGuardBits(wZ, wZ, wZ);
				int g2 = IntMultiplier::neededGuardBits(wZ2o2, wZ2o2, wZ2o2);
				int gMult=max(g1,g2);
#endif

				REPORT(0, "wZ2o2=" << wZ2o2 << "    wZ=" << wZ << "    g=" << g << "    gMult=" << gMult);
				BitHeap* bitHeapCos = new BitHeap(this, w+g+gMult, "Sin"); 
			
				// Add CosPiA to the bit heap
				bitHeapCos -> addUnsignedBitVector(gMult, "CosPiA", w+g);
			
				vhdl << tab << "--  truncate the larger input of each multiplier to the precision of its output" << endl;
				vhdl << tab << declare("CosPiA_truncToZ2o2", wZ2o2) << " <= CosPiA" << range(w+g-1, w+g-wZ2o2) << ";" << endl;
				vhdl << tab << "--  truncate the larger input of each multiplier to the precision of its output" << endl;
				vhdl << tab << declare("SinPiA_truncToZ", wZ) << " <= SinPiA" << range(w+g-1, w+g-wZ) << ";" << endl;
			

#if TINKERCOS
				vhdl <<  tab << declare("Z2o2CosPiA", 2*wZ2o2) << " <= Z2o2 * CosPiA_truncToZ2o2" << ";" << endl;
				// add it to the bit heap

				for (int i=0; i<wZ2o2-1; i++)
					bitHeapCos->addBit(i, "not Z2o2CosPiA"+of(i+wZ2o2));
				bitHeapCos->addBit(wZ2o2-1, "Z2o2CosPiA"+of(wZ2o2+wZ2o2-1));
				for (int i=wZ2o2-1; i<w+g+gMult; i++)
					bitHeapCos->addConstantOneBit(i);
				bitHeapCos->addConstantOneBit(0);
			

				vhdl <<  tab << declare("SinZSinPiA", 2*wZ) << " <=  SinZ *SinPiA_truncToZ" << ";" << endl;
			
				// add it to the bit heap	
				for (int i=0; i<wZ-1; i++)
					bitHeapCos->addBit(i, "not SinZSinPiA"+of(i+wZ));
				bitHeapCos->addBit(wZ-1, "SinZSinPiA"+of(wZ+wZ-1));
				for (int i=wZ-1; i<w+g+gMult; i++)
					bitHeapCos->addConstantOneBit(i);
				bitHeapCos->addConstantOneBit(0);
			
				//	vhdl <<  tab << declare("Z2o2CosPiA", 2*wZ) << " <= " << range(w+g-1, w+g-wZ) << ";" << endl;
#else
				// First virtual multiplier
				new IntMultiplier (this,
													 bitHeapCos,
													 getSignalByName("Z2o2"),
													 getSignalByName("CosPiA_truncToZ2o2"),
													 wZ2o2, wZ2o2, wZ2o2,
													 gMult,
													 true, // negate
													 false // signed inputs
													 );



				// Second virtual multiplier
				new IntMultiplier (this,
													 bitHeapCos,
													 getSignalByName("SinZ"),
													 getSignalByName("SinPiA_truncToZ"),
													 wZ, wZ, wZ,
													 gMult,
													 true, // negate
													 false // signed inputs
													 );
#endif
			
				// The round bit is in the table already
				bitHeapCos -> generateCompressorVHDL();	
				vhdl << tab << declare ("C_out", w) << " <= " << bitHeapCos -> getSumName() << range (w+g+gMult-1, g+gMult) << ';' << endl;
			
#endif  // closes the else Bit heap computing Cos Z  ~   CosPiA - Z2o2*cosPiA - sinZ*SinPiA



				//Bit heap computing   SinPiA - Z2o2*sinPiA + sinZ*CosPiA
				vhdl << tab << "-- ********************************** Reconstruction of sine ************************************" << endl;
				vhdl << tab << "-- truncate the larger input of the multiplier to the precision of the output" << endl;
				vhdl << tab << declare("SinPiA_truncToZ2o2", wZ2o2) << " <= SinPiA" << range(w+g-1, w+g-wZ2o2) << ";" << endl;

				newInstance("IntMultiplier", "s_out_2_compute",
										"wX=" + to_string(wZ2o2) + " wY=" + to_string(wZ2o2) + " wOut=" + to_string(wZ2o2) + " signedIO=false",
										"X=>Z2o2,Y=>SinPiA_truncToZ2o2",
										"R=>Z2o2SinPiA");
			
				vhdl << tab << "-- truncate the larger input of the multiplier to the precision of the output" << endl;
				vhdl << tab << declare("CosPiA_truncToSinZ", wZ) << " <= CosPiA" << range(w+g-1, w+g-wZ) << ";" << endl;
				newInstance("IntMultiplier", "s_out_3_compute",
										"wX=" + to_string(wZ) + " wY=" + to_string(wZ) + " wOut=" + to_string(wZ) + " signedIO=false",
										"X=>SinZ,Y=>CosPiA_truncToSinZ",
										"R=>SinZCosPiA");

				vhdl << tab << declare (getTarget()->adderDelay(w+g), "CosZSinPiA_plus_rnd", w+g)
						 << " <= SinPiA - Z2o2SinPiA;" << endl;
				vhdl << tab << declare (getTarget()->adderDelay(w+g), "S_out_rnd_aux", w+g)
						 << " <= CosZSinPiA_plus_rnd + SinZCosPiA;" << endl;


				vhdl << tab << declare ("S_out", w)
						 << " <= S_out_rnd_aux" << range (w+g-1, g) << ';' << endl;
			
				REPORT(DETAILED, " wA=" << wA <<" wZ=" << wZ <<" wZ2=" << wZ2o2 <<" wZ3o6=" << wZ3o6 );

				// For LateX in the paper
				//	cout << "     " << w <<  "   &   "  << wA << "   &   " << wZ << "   &   " << wZ2o2 << "   &   " << wZ3o6 << "   \\\\ \n \\hline" <<  endl;
			} // closes if for generic case



			// When we arrive here we should have two signals C_out and S_out, each of size w 
			addComment("--- Final reconstruction of both sine and cosine ---");

			vhdl << tab << declare("C_sgn") << " <= X_sgn xor Q;" << endl;
			vhdl << tab << declare ("Exch") << " <= Q xor O;" << endl;

			vhdl << tab << declare ("S_wo_sgn", w)
					 << " <= C_out when Exch = '1' else S_out;" << endl; //swap sine and cosine if q xor o
			vhdl << tab << declare ("C_wo_sgn", w)
					 << " <= S_out when Exch = '1' else C_out;" << endl;
		
		
			vhdl << tab << declare ("S_wo_sgn_ext", w+1)
					 << " <= '0' & S_wo_sgn;" << endl
					 << tab << declare ("C_wo_sgn_ext", w+1)
					 << " <= '0' & C_wo_sgn;" << endl; //S_wo_sgn_ext and C_wo_sgn are the positive versions of the sine and cosine
		
			vhdl << tab << declare ("S_wo_sgn_neg", w+1)
					 << " <= (not S_wo_sgn_ext) + 1;" << endl;
			vhdl << tab << declare ("C_wo_sgn_neg", w+1)
					 << " <= (not C_wo_sgn_ext) + 1;" << endl; //2's complement. We have now the negative version of the sine and cosine results
		
			vhdl << tab << "S <= S_wo_sgn_ext when X_sgn = '0'"
					 << " else S_wo_sgn_neg;" << endl;

			vhdl << tab << "C <= C_wo_sgn_ext when C_sgn = '0'"
					 << " else C_wo_sgn_neg;" << endl; //set the correspondant value to C and S according to input sign
		}
	};




	FixSinCosPoly::~FixSinCosPoly() {
	};






}
