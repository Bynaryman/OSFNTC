#include <fstream>
#include <sstream>
#include "IntTwiddleMultiplier.hpp"

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
	IntTwiddleMultiplier::IntTwiddleMultiplier(Target* target, int wI_, int wF_, int twiddleExponent_, int n_, bool signedOperator_, bool reducedMultiplications, int multiplierMode_)
		: Operator(target), wI(wI_), wF(wF_), twiddleExponent(twiddleExponent_), n(n_), signedOperator(signedOperator_), multiplierMode(multiplierMode_)
	{
		bool completeExecutionPath = true;
		
		signedOperator ? w = 1 + wI + wF : w = wI + wF;
		
		ostringstream name;

		setCopyrightString ( "Matei Istoan, Florent de Dinechin (2008-2012)" );
		if(getTarget()->isPipelined())
			name << "IntTwiddleMultiplier_" << w << "_w_exp_" << twiddleExponent << "_f"<< getTarget()->frequencyMHz() << "_uid" << getNewUId();
		else
			name << "IntTwiddleMultiplier_" << w << "_w_exp_" << twiddleExponent << "_uid" << getNewUId();
		setNameWithFreqAndUID( name.str() );

		addInput("Xi", 		w, true);
		addInput("Xr", 		w, true);
		if((twiddleExponent==0) || ((double)twiddleExponent == (double)n/4.0))
			addOutput("Zi",   w, 2);
		else
			addOutput("Zi",   2*w, 2);
		if((twiddleExponent==0) || ((double)twiddleExponent == (double)n/4.0))
			addOutput("Zr",   w, 2);
		else
			addOutput("Zr",   2*w, 2);
		
		
		if(twiddleExponent == 0){
						
			vhdl << tab << "Zi <= (" << w-1 << " downto 0 => Xi(" << w-1 << ")) & Xi;" << endl;
			vhdl << tab << "Zr <= (" << w-1 << " downto 0 => Xr(" << w-1 << ")) & Xr;" << endl;
			
			completeExecutionPath = false;
		} else if((double)twiddleExponent == (double)n/4.0){
			
			vhdl << tab << declare("neg_Xr", w) << " <= Xr xor (" << w-1 << " downto 0 => \'1\');" << endl;
			
			IntAdder* addOperator =  new IntAdder(target, w, inDelayMap("X",getCriticalPath()));
			oplist.push_back(addOperator);
			
			inPortMap 	(addOperator, "X", 	 "neg_Xr");
			inPortMapCst(addOperator, "Y", 	 zg(w, 0));
			inPortMapCst(addOperator, "Cin", "\'1\'");
			outPortMap	(addOperator, "R", 	 "intZi", false);
			vhdl << instance(addOperator, "ADD_negXr");
			
			vhdl << tab << "Zi <= (" << w-1 << " downto 0 => intZi(" << w-1 << ")) & intZi;" << endl;
			vhdl << tab << "Zr <= (" << w-1 << " downto 0 => Xi(" << w-1 << ")) & Xi;" << endl;
			
			completeExecutionPath = false;
		}
	
		if(!reducedMultiplications && completeExecutionPath){
			mpz_class twRe, twIm;
			int wOutIm, wOutRe;
			
			twRe = getTwiddleConstant(TWIDDLERE);
			twIm = getTwiddleConstant(TWIDDLEIM);
			
			Operator *multiplyOperatorRe, *multiplyOperatorIm;
			
			if(multiplierMode == 0){
				multiplyOperatorRe = new FixRealKCM(target, true, wI-1, -wF, -2*wF, getTwiddleConstantString(TWIDDLERE));
				oplist.push_back(multiplyOperatorRe);
				
				if(abs(twIm.get_si()) == abs(twRe.get_si())){
					multiplyOperatorIm = multiplyOperatorRe;
				}else{
					multiplyOperatorIm = new FixRealKCM(target, true, wI-1, -wF, -2*wF, getTwiddleConstantString(TWIDDLEIM));
					oplist.push_back(multiplyOperatorIm);
				}
			}else{
				multiplyOperatorRe = new IntConstMult(target, w, (twRe<0 ? (-1)*twRe : twRe));
				oplist.push_back(multiplyOperatorRe);
				
				if(abs(twIm.get_si()) == abs(twRe.get_si())){
					multiplyOperatorIm = multiplyOperatorRe;
				}else{
					multiplyOperatorIm = new IntConstMult(target, w, (twIm<0 ? (-1)*twIm : twIm));
					oplist.push_back(multiplyOperatorIm);
				}
			}
			
			IntAdder* addOperator =  new IntAdder(target, 2*w, inDelayMap("X",getCriticalPath()));
			oplist.push_back(addOperator);
			
			inPortMap (multiplyOperatorIm, "X", "Xi");
			outPortMap(multiplyOperatorIm, "R", "intXiYi");
			vhdl << instance(multiplyOperatorIm, "MUL_XiYi");
			
			inPortMap (multiplyOperatorRe, "X", "Xi");
			outPortMap(multiplyOperatorRe, "R", "intXiYr");
			vhdl << instance(multiplyOperatorRe, "MUL_XiYr");
						
			inPortMap (multiplyOperatorRe, "X", "Xr");
			outPortMap(multiplyOperatorRe, "R", "intXrYr");
			vhdl << instance(multiplyOperatorRe, "MUL_XrYr");
						
			inPortMap (multiplyOperatorIm, "X", "Xr");
			outPortMap(multiplyOperatorIm, "R", "intXrYi");
			vhdl << instance(multiplyOperatorIm, "MUL_XrYi");
			
			if(multiplierMode == 0){
				wOutIm = 1 + wI + 2*wF + ceil(log2(abs(getTwiddleConstant(TWIDDLEIM).get_si())));
				wOutRe = 1 + wI + 2*wF + ceil(log2(abs(getTwiddleConstant(TWIDDLERE).get_si())));
			}else{
				wOutIm = ceil(log2((double)(twIm.get_si()) * ((pow(2.0, (double)w))-1.0)));
				wOutRe = ceil(log2((double)(twRe.get_si()) * ((pow(2.0, (double)w))-1.0)));
			}
			
			vhdl << tab << declare("XrYr", 2*w) << " <= (" << 2*w-1-wOutRe << " downto 0 => intXrYr(" << wOutRe-1 << ")) & intXrYr;" << endl;
			vhdl << tab << declare("XiYi", 2*w) << " <= (" << 2*w-1-wOutIm << " downto 0 => intXiYi(" << wOutIm-1 << ")) & intXiYi;" << endl;
			vhdl << tab << declare("XrYi", 2*w) << " <= (" << 2*w-1-wOutIm << " downto 0 => intXrYi(" << wOutIm-1 << ")) & intXrYi;" << endl;
			vhdl << tab << declare("XiYr", 2*w) << " <= (" << 2*w-1-wOutRe << " downto 0 => intXiYr(" << wOutRe-1 << ")) & intXiYr;" << endl;
			
			syncCycleFromSignal("XiYr", false);
			
			std::string strXrYr, strXiYi, strNegXrYi, strXiYr, strCinRe, strCinIm;
			bool negateZr = false, negateZi = false;
			
			if((twIm<0) && (twRe<0)){
				vhdl << tab << declare("neg_XiYr", 2*w) << " <= XiYr xor (" << 2*w-1 << " downto 0 => \'1\');" << endl;
				
				strXrYr 	= "XrYr";
				strXiYi 	= "XiYi";
				strCinRe	= "\'0\'";
				negateZr	= true;
				strNegXrYi	= "XrYi";
				strXiYr		= "neg_XiYr";
				strCinIm	= "\'1\'";
				negateZi	= false;
				
				syncCycleFromSignal("neg_XiYr", false);
				nextCycle();
			}else if((twIm<0) && (twRe>=0)){
				vhdl << tab << declare("neg_XiYi", 2*w) << " <= XiYi xor (" << 2*w-1 << " downto 0 => \'1\');" << endl;
				
				strXrYr 	= "XrYr";
				strXiYi 	= "neg_XiYi";
				strCinRe	= "\'1\'";
				negateZr	= false;
				strNegXrYi	= "XrYi";
				strXiYr		= "XiYr";
				strCinIm	= "\'0\'";
				negateZi	= false;
				
				syncCycleFromSignal("neg_XiYi", false);
				nextCycle();
			}else if((twIm>=0) && (twRe<0)){
				vhdl << tab << declare("neg_XrYr", 2*w) << " <= XrYr xor (" << 2*w-1 << " downto 0 => \'1\');" << endl;
				
				strXrYr 	= "neg_XrYr";
				strXiYi 	= "XiYi";
				strCinRe	= "\'1\'";
				negateZr	= false;
				strNegXrYi	= "XrYi";
				strXiYr		= "XiYr";
				strCinIm	= "\'0\'";
				negateZi	= true;
				
				syncCycleFromSignal("neg_XrYr", false);
				nextCycle();
			}else if((twIm>=0) && (twRe>=0)){
				vhdl << tab << declare("neg_XiYi", 2*w) << " <= XiYi xor (" << 2*w-1 << " downto 0 => \'1\');" << endl;
				
				strXrYr 	= "XrYr";
				strXiYi 	= "XiYi";
				strCinRe	= "\'0\'";
				negateZr	= false;
				strNegXrYi	= "neg_XrYi";
				strXiYr		= "XiYr";
				strCinIm	= "\'1\'";
				negateZi	= false;
				
				syncCycleFromSignal("neg_XiYi", false);
				nextCycle();
			}
			
			inPortMap 	(addOperator, "X", 	 strXrYr);
			inPortMap 	(addOperator, "Y", 	 strXiYi);
			inPortMapCst(addOperator, "Cin", strCinRe);
			if(negateZr)
				outPortMap	(addOperator, "R", 	 "intZr");
			else
				outPortMap	(addOperator, "R", 	 "Zr", false);
			vhdl << instance(addOperator, "ADD_XrYrSubXiYi");
			
			inPortMap 	(addOperator, "X", 	 strNegXrYi);
			inPortMap 	(addOperator, "Y", 	 strXiYr);
			inPortMapCst(addOperator, "Cin", strCinIm);
			if(negateZi)
				outPortMap	(addOperator, "R", 	 "intZi");
			else
				outPortMap	(addOperator, "R", 	 "Zi", false);
			vhdl << instance(addOperator, "ADD_XrYiAddXiYr");
			
			if(negateZr){
				inPortMap 	(addOperator, "X", 	 "intZr");
				inPortMapCst(addOperator, "Y", 	 zg(2*w, 0));
				inPortMapCst(addOperator, "Cin", "\'1\'");
				outPortMap	(addOperator, "R", 	 "Zr", false);
				vhdl << instance(addOperator, "ADD_negZr");
			}
			if(negateZi){
				inPortMap 	(addOperator, "X", 	 "intZi");
				inPortMapCst(addOperator, "Y", 	 zg(2*w, 0));
				inPortMapCst(addOperator, "Cin", "\'1\'");
				outPortMap	(addOperator, "R", 	 "Zi", false);
				vhdl << instance(addOperator, "ADD_negZi");
			}
		}else if(reducedMultiplications && completeExecutionPath){
			try{
			mpz_class twRe, twReSubIm, twNegReAddIm;
			int wOutRe, wOutReSubIm, wOutNegReAddIm;
			
			twRe = getTwiddleConstant(TWIDDLERE);
			twReSubIm = getTwiddleConstant(TWIDDLERESUBIM);
			twNegReAddIm = getTwiddleConstant(TWIDDLENEGREADDIM);
			
			if(multiplierMode == 0){
				wOutRe = 1 + wI + 2*wF + ceil(log2(abs(getTwiddleConstant(TWIDDLERE).get_si())));
				wOutReSubIm = 1 + wI + 2*wF + ceil(log2(abs(getTwiddleConstant(TWIDDLERESUBIM).get_si())));
				wOutNegReAddIm = 1 + wI + 2*wF + ceil(log2(abs(getTwiddleConstant(TWIDDLENEGREADDIM).get_si())));
			}else{
				wOutRe = ceil(log2((double)(twRe.get_si()) * ((pow(2.0, (double)w))-1.0)));
				wOutReSubIm = ceil(log2((double)(twReSubIm.get_si()) * ((pow(2.0, (double)w))-1.0)));
				wOutNegReAddIm = ceil(log2((double)(twNegReAddIm.get_si()) * ((pow(2.0, (double)w))-1.0)));
			}
			
			Operator *multiplyOperatorRe, *multiplyOperatorReSubIm, *multiplyOperatorNegReAddIm;
			
			if(multiplierMode == 0){
				multiplyOperatorRe = new FixRealKCM(target, true, wI-1, -wF, -2*wF, getTwiddleConstantString(TWIDDLERE));
				oplist.push_back(multiplyOperatorRe);
				
				if(abs(twReSubIm.get_si()) == abs(twNegReAddIm.get_si())){
					multiplyOperatorReSubIm = multiplyOperatorRe;
					multiplyOperatorNegReAddIm = multiplyOperatorRe;
				}else if(twReSubIm.get_si() == 0){
					multiplyOperatorNegReAddIm = new FixRealKCM(target, true,  wI-1, -wF, -2*wF, getTwiddleConstantString(TWIDDLENEGREADDIM));
					oplist.push_back(multiplyOperatorNegReAddIm);
				}else if(twNegReAddIm.get_si() == 0){
					multiplyOperatorReSubIm = new FixRealKCM(target, true, wI-1, -wF, -2*wF, getTwiddleConstantString(TWIDDLERESUBIM));
					oplist.push_back(multiplyOperatorReSubIm);
				}else{
					multiplyOperatorReSubIm = new FixRealKCM(target, true,  wI-1, -wF, -2*wF, getTwiddleConstantString(TWIDDLERESUBIM));
					oplist.push_back(multiplyOperatorReSubIm);
					multiplyOperatorNegReAddIm = new FixRealKCM(target, true, wI-1, -wF, -2*wF, getTwiddleConstantString(TWIDDLENEGREADDIM));
					oplist.push_back(multiplyOperatorNegReAddIm);
				}
			}else{
				multiplyOperatorRe = new IntConstMult(target, w, (twRe<0 ? (-1)*twRe : twRe));
				oplist.push_back(multiplyOperatorRe);
				
				if(abs(twReSubIm.get_si()) == abs(twNegReAddIm.get_si())){
					multiplyOperatorReSubIm = multiplyOperatorRe;
					multiplyOperatorNegReAddIm = multiplyOperatorRe;
				}else if(twReSubIm.get_si() == 0){
					multiplyOperatorNegReAddIm = new IntConstMult(target, w, (twNegReAddIm<0 ? (-1)*twNegReAddIm : twNegReAddIm));
					oplist.push_back(multiplyOperatorNegReAddIm);
				}else if(twNegReAddIm.get_si() == 0){
					multiplyOperatorReSubIm = new IntConstMult(target, w, (twReSubIm<0 ? (-1)*twReSubIm : twReSubIm));
					oplist.push_back(multiplyOperatorReSubIm);
				}else{
					multiplyOperatorReSubIm = new IntConstMult(target, w, (twReSubIm<0 ? (-1)*twReSubIm : twReSubIm));
					oplist.push_back(multiplyOperatorReSubIm);
					multiplyOperatorNegReAddIm = new IntConstMult(target, w, (twNegReAddIm<0 ? (-1)*twNegReAddIm : twNegReAddIm));
					oplist.push_back(multiplyOperatorNegReAddIm);
				}
			}
			
			IntAdder* addOperator =  new IntAdder(target, w, inDelayMap("X",getCriticalPath()));
			oplist.push_back(addOperator);	
				
			inPortMap 	(addOperator, "X", 	 "Xr");
			inPortMap 	(addOperator, "Y",   "Xi");
			inPortMapCst(addOperator, "Cin", "\'0\'");
			outPortMap	(addOperator, "R", 	 "XrAddXi");
			vhdl << instance(addOperator, "ADD_XrXi");
			
			syncCycleFromSignal("YrAddYi", false);
			//nextCycle(); 
			
			inPortMap (multiplyOperatorRe, "X", "XrAddXi");
			outPortMap(multiplyOperatorRe, "R", "intK1");
			vhdl << instance(multiplyOperatorRe, "MUL_K1");
			
			if(twNegReAddIm == 0){
				vhdl << tab << declare("intK2", wOutNegReAddIm) << " <= " << zg(wOutNegReAddIm, 0) << ";" << endl;
				
				inPortMap (multiplyOperatorReSubIm, "X", "Xi");
				outPortMap(multiplyOperatorReSubIm, "R", "intintK3");
				vhdl << instance(multiplyOperatorReSubIm, "MUL_K3");
				
				vhdl << tab << declare("intK3", wOutReSubIm+1) << " <= intintK3 & \'0\';" << endl;
			}else if(twReSubIm == 0){
				vhdl << tab << declare("intK3", wOutReSubIm) << " <= " << zg(wOutReSubIm, 0) << ";" << endl;
				
				inPortMap (multiplyOperatorNegReAddIm, "X", "Xr");
				outPortMap(multiplyOperatorNegReAddIm, "R", "intintK2");
				vhdl << instance(multiplyOperatorNegReAddIm, "MUL_K2");
				
				vhdl << tab << declare("intK2", wOutNegReAddIm+1) << " <= intintK2 & \'0\';" << endl;
			}else{
				inPortMap (multiplyOperatorNegReAddIm, "X", "Xr");
				outPortMap(multiplyOperatorNegReAddIm, "R", "intK2");
				vhdl << instance(multiplyOperatorNegReAddIm, "MUL_K2");
				
				inPortMap (multiplyOperatorReSubIm, "X", "Xi");
				outPortMap(multiplyOperatorReSubIm, "R", "intK3");
				vhdl << instance(multiplyOperatorReSubIm, "MUL_K3");
			}
			
			vhdl << tab << declare("K1", 2*w) << " <= (" << 2*w-1-wOutRe << " downto 0 => intK1(" << wOutRe-1 << ")) & intK1;" << endl;
			if(twReSubIm == 0)
				vhdl << tab << declare("K2", 2*w) << " <= (" << 2*w-2-wOutNegReAddIm << " downto 0 => intK2(" << wOutNegReAddIm << ")) & intK2;" << endl;
			else
				vhdl << tab << declare("K2", 2*w) << " <= (" << 2*w-1-wOutNegReAddIm << " downto 0 => intK2(" << wOutNegReAddIm-1 << ")) & intK2;" << endl;
			if(twNegReAddIm == 0)
				vhdl << tab << declare("K3", 2*w) << " <= (" << 2*w-2-wOutReSubIm << " downto 0 => intK3(" << wOutReSubIm << ")) & intK2;" << endl;
			else
				vhdl << tab << declare("K3", 2*w) << " <= (" << 2*w-1-wOutReSubIm << " downto 0 => intK3(" << wOutReSubIm-1 << ")) & intK3;" << endl;
			
			syncCycleFromSignal("K1", false);
			//nextCycle(); 
			
			if(twRe<0){
				vhdl << tab << declare("neg_K1", 2*w) << " <= K1 xor (" << 2*w-1 << " downto 0 => \'1\');" << endl;
			}
			if(twNegReAddIm<0){
				vhdl << tab << declare("neg_K2", 2*w) << " <= K2 xor (" << 2*w-1 << " downto 0 => \'1\');" << endl;
			}
			vhdl << tab << declare("neg_K3", 2*w) << " <= K3 xor (" << 2*w-1 << " downto 0 => \'1\');" << endl;
			
			syncCycleFromSignal("neg_K3", false);
			//nextCycle();
			
			std::string strK1Im, strK1Re, strK2, strNegK3, strCinRe, strCinIm;
			bool negateZr = false, negateZi = false;
			
			if((twRe<0) && (twReSubIm<0) && (twNegReAddIm<0)){
				strK1Im = "K1";
				strK2 = "K2";
				strCinIm = "\'0\'";
				negateZi = true;
				strK1Re = "neg_K1";
				strNegK3 = "K3";
				strCinRe = "\'1\'";
				negateZr = false;
			}else if((twRe<0) && (twReSubIm<0) && (twNegReAddIm>0)){
				strK1Im = "neg_K1";
				strK2 = "K2";
				strCinIm = "\'1\'";
				negateZi = false;
				strK1Re = "neg_K1";
				strNegK3 = "K3";
				strCinRe = "\'1\'";
				negateZr = false;
			}else if((twRe<0) && (twReSubIm>0) && (twNegReAddIm<0)){
				strK1Im = "K1";
				strK2 = "K2";
				strCinIm = "\'0\'";
				negateZi = true;
				strK1Re = "K1";
				strNegK3 = "K3";
				strCinRe = "\'0\'";
				negateZr = true;
			}else if((twRe<0) && (twReSubIm>0) && (twNegReAddIm>0)){
				strK1Im = "neg_K1";
				strK2 = "K2";
				strCinIm = "\'1\'";
				negateZi = false;
				strK1Re = "K1";
				strNegK3 = "K3";
				strCinRe = "\'0\'";
				negateZr = true;
			}else if((twRe>0) && (twReSubIm<0) && (twNegReAddIm<0)){
				strK1Im = "K1";
				strK2 = "neg_K2";
				strCinIm = "\'1\'";
				negateZi = false;
				strK1Re = "K1";
				strNegK3 = "K3";
				strCinRe = "\'0\'";
				negateZr = false;
			}else if((twRe>0) && (twReSubIm<0) && (twNegReAddIm>0)){
				strK1Im = "K1";
				strK2 = "K2";
				strCinIm = "\'0\'";
				negateZi = false;
				strK1Re = "K1";
				strNegK3 = "K3";
				strCinRe = "\'0\'";
				negateZr = false;
			}else if((twRe>0) && (twReSubIm>0) && (twNegReAddIm<0)){
				strK1Im = "K1";
				strK2 = "neg_K2";
				strCinIm = "\'1\'";
				negateZi = false;
				strK1Re = "K1";
				strNegK3 = "neg_K3";
				strCinRe = "\'1\'";
				negateZr = false;
			}else if((twRe>0) && (twReSubIm>0) && (twNegReAddIm>0)){
				strK1Im = "K1";
				strK2 = "K2";
				strCinIm = "\'0\'";
				negateZi = false;
				strK1Re = "K1";
				strNegK3 = "neg_K3";
				strCinRe = "\'1\'";
				negateZr = false;
			}
			
			IntAdder *addOperator2 =  new IntAdder(target, 2*w, inDelayMap("X",getCriticalPath()));
			oplist.push_back(addOperator2);
			
			inPortMap 	(addOperator2, "X",   strK1Re);
			inPortMap 	(addOperator2, "Y",   strNegK3);
			inPortMapCst(addOperator2, "Cin", strCinRe);
			if(negateZr)
				outPortMap	(addOperator2, "R", 	 "intZr");
			else
				outPortMap	(addOperator2, "R", 	 "Zr", false);
			vhdl << instance(addOperator2, "ADD_K1MinK3");
			
			inPortMap 	(addOperator2, "X",   strK1Im);
			inPortMap 	(addOperator2, "Y",   strK2);
			inPortMapCst(addOperator2, "Cin", strCinIm);
			if(negateZr)
				outPortMap	(addOperator2, "R", 	 "intZi");
			else
				outPortMap	(addOperator2, "R", 	 "Zi", false);
			vhdl << instance(addOperator2, "ADD_K1AddK2");
			
			if(negateZr){
				inPortMap 	(addOperator2, "X", 	 "intZr");
				inPortMapCst(addOperator2, "Y", 	 zg(2*w, 0));
				inPortMapCst(addOperator2, "Cin", 	 "\'1\'");
				outPortMap	(addOperator2, "R", 	 "Zr", false);
				vhdl << instance(addOperator2, "ADD_negZr");
			}
			if(negateZi){
				inPortMap 	(addOperator2, "X", 	 "intZi");
				inPortMapCst(addOperator2, "Y", 	 zg(2*w, 0));
				inPortMapCst(addOperator2, "Cin", 	 "\'1\'");
				outPortMap	(addOperator2, "R", 	 "Zi", false);
				vhdl << instance(addOperator2, "ADD_negZi");
			}
			
			}catch(std::string str){
				cout << "execution interrupted: " << str << endl;
				exit(1);
			}
		}
	
	}	


	IntTwiddleMultiplier::~IntTwiddleMultiplier()
	{
	}
	
	//FIXME: correct the emulate function
	void IntTwiddleMultiplier::emulate ( TestCase* tc ) {
		mpz_class svXi = tc->getInputValue("Xi");
		mpz_class svXr = tc->getInputValue("Xr");
		mpz_class svYi = getTwiddleConstant(TWIDDLEIM);
		mpz_class svYr = getTwiddleConstant(TWIDDLERE);
		
		
		if(twiddleExponent == 0){
			mpz_class svZi = svXi;
			mpz_class svZr = svXr;
		}else if((double)twiddleExponent == (double)n/4.0){
			mpz_class svZi = svXr;
			mpz_class svZr = (-1)*svXi;
		}else{
			if (! signedOperator){

				mpz_class svZi = svXi*svYr + svXr*svYi;
				mpz_class svZr = svXr*svYr + svXi*svYi;
				
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
				
				mpz_class svZi = svXiYr - svXrYi;
				mpz_class svZr = svXrYr + svXiYi;
				 
				// Don't allow overflow
				mpz_clrbit ( svZi.get_mpz_t(), 2*w );
				mpz_clrbit ( svZr.get_mpz_t(), 2*w );
				
				tc->addExpectedOutput("Zi", svZi);
				tc->addExpectedOutput("Zr", svZr);
			}
		}
	}
	
	
	mpz_class IntTwiddleMultiplier::getTwiddleConstant(int constantType){
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
			case TWIDDLERESUBIM:
							mpfr_set(temp, twiddleRe, GMP_RNDN);
							mpfr_sub(temp, temp, twiddleIm, GMP_RNDN);
							break;
			case TWIDDLENEGREADDIM:
							mpfr_set(temp, twiddleRe, GMP_RNDN);
							mpfr_add(temp, temp, twiddleIm, GMP_RNDN);
							mpfr_neg(temp, temp, GMP_RNDN);
							break;
		}
		
		mpfr_mul_2si(temp, temp, wF, GMP_RNDN);
		mpfr_get_z(intTemp.get_mpz_t(), temp,  GMP_RNDN);
		
		mpfr_free_cache();
		mpfr_clears (twiddleExp, twiddleIm, twiddleRe, constPi, NULL);
		
		return intTemp;
	}
	
	std::string IntTwiddleMultiplier::getTwiddleConstantString(int constantType){
		std::ostringstream result;
		mpz_class temp;
		
		temp = getTwiddleConstant(constantType);
		if(temp<0)
			temp *= (-1);
		result << temp;
		
		return result.str();
	}
	
	int IntTwiddleMultiplier::getGCD(int x, int y){
		int temp;
		
		if(x==0)
			return y;
		else if(y==0)
			return x;
		else if(x<y){
			temp = x;
			x = y;
			y = temp;
		}
		
		return getGCD(y, x % y);
	}

}




















