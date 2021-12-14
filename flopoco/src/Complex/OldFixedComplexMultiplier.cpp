#include <fstream>
#include <sstream>
#include "OldFixedComplexMultiplier.hpp"

using namespace std;

namespace flopoco{

	extern vector<Operator *> oplist;

	//TODO: explore implementation using multiply-accumulate operators
	//FIXME: correct timing of the circuit
	OldFixedComplexMultiplier::OldFixedComplexMultiplier(Target* target, int wI_, int wO_, float ratio_, bool signedOperator_, bool threeMultiplications, bool enableSuperTiles_)
		: Operator(target), wI(wI_), wO(wO_), signedOperator(signedOperator_), ratio(ratio_), enableSuperTiles(enableSuperTiles_)
	{
		
		ostringstream name;

		setCopyrightString ( "Matei Istoan, Florent de Dinechin (2008-2012)" );

		if(signedOperator)
			useStdLogicSigned();
		else
			useStdLogicUnsigned();

		if(getTarget()->isPipelined())
			name << "OldFixedComplexMultiplier_" << wI << "_" << wO << "_f"<< getTarget()->frequencyMHz() << "_uid" << getNewUId();
		else
			name << "OldFixedComplexMultiplier_" << wI << "_" << wO << "_uid" << getNewUId();
		setNameWithFreqAndUID( name.str() );

		addInput("Xr", 		wI, true);
		addInput("Xi", 		wI, true);
		addInput("Yr", 		wI, true);
		addInput("Yi", 		wI, true);

		addOutput("Zi",   wO, 2);
		addOutput("Zr",   wO, 2);

		if(!threeMultiplications){
			IntMultiplier* multiplyOperator = new IntMultiplier(target, wI, wI, wO, signedOperator, ratio, inDelayMap("Xi",getCriticalPath()), enableSuperTiles);
			oplist.push_back(multiplyOperator);
						
			inPortMap (multiplyOperator, "X", "Xi");
			inPortMap (multiplyOperator, "Y", "Yi");
			outPortMap(multiplyOperator, "R", "XiYi");
			vhdl << instance(multiplyOperator, "MUL_XiYi");
			
			inPortMap (multiplyOperator, "X", "Xr");
			inPortMap (multiplyOperator, "Y", "Yr");
			outPortMap(multiplyOperator, "R", "XrYr");
			vhdl << instance(multiplyOperator, "MUL_XrYr");
			
			inPortMap (multiplyOperator, "X", "Xr");
			inPortMap (multiplyOperator, "Y", "Yi");
			outPortMap(multiplyOperator, "R", "XrYi");
			vhdl << instance(multiplyOperator, "MUL_XrYi");
			
			inPortMap (multiplyOperator, "X", "Xi");
			inPortMap (multiplyOperator, "Y", "Yr");
			outPortMap(multiplyOperator, "R", "XiYr");
			vhdl << instance(multiplyOperator, "MUL_XiYr");
			
			setCycleFromSignal("XiYi", true);
			syncCycleFromSignal("XrYr", true);
			syncCycleFromSignal("XrYi", true);
			syncCycleFromSignal("XiYr", true);
			
			// invert the sign of XiYi to obtain a subtraction
			manageCriticalPath(getTarget()->localWireDelay(wO) + getTarget()->lutDelay());
			
			vhdl << tab << declare("neg_XiYi", wO) << " <= XiYi xor (" << wO-1 << " downto 0 => \'1\');" << endl;
			
			syncCycleFromSignal("neg_XiYi", true);
			
			IntAdder* addOperator =  new IntAdder(target, wO, inDelayMap("XrYr",getCriticalPath()));
			oplist.push_back(addOperator);
			
			inPortMap 	(addOperator, "X", 	 "XrYr");
			inPortMap 	(addOperator, "Y", 	 "neg_XiYi");
			inPortMapCst(addOperator, "Cin", "\'1\'");
			outPortMap	(addOperator, "R", 	 "Zr", false);
			vhdl << instance(addOperator, "ADD_XrYrMinXiYi");
			
			inPortMap 	(addOperator, "X", 	 "XrYi");
			inPortMap 	(addOperator, "Y", 	 "XiYr");
			inPortMapCst(addOperator, "Cin", "\'0\'");
			outPortMap	(addOperator, "R", 	 "Zi", false);
			vhdl << instance(addOperator, "ADD_XrYiAddXiYr");
		}
		else{
			try{
				IntAdder* addOperator =  new IntAdder(target, wI, inDelayMap("Xr",getCriticalPath()));
				oplist.push_back(addOperator);	
				
				vhdl << tab << declare("neg_Yr", wI) << " <= Yr xor (" << wI-1 << " downto 0 => \'1\');" << endl;
				
				syncCycleFromSignal("neg_Yr", true);
				
				inPortMap 	(addOperator, "X", 	 "Xr");
				inPortMap 	(addOperator, "Y",   "Xi");
				inPortMapCst(addOperator, "Cin", "\'0\'");
				outPortMap	(addOperator, "R", 	 "XrAddXi");
				vhdl << instance(addOperator, "ADD_XrXi");
			
				inPortMap 	(addOperator, "X", 	 "Yi");
				inPortMap 	(addOperator, "Y",   "neg_Yr");
				inPortMapCst(addOperator, "Cin", "\'1\'");
				outPortMap	(addOperator, "R",   "YiMinYr");
				vhdl << instance(addOperator, "ADD_YiMinYr");
			
				inPortMap 	(addOperator, "X",   "Yi");
				inPortMap 	(addOperator, "Y",   "Yr");
				inPortMapCst(addOperator, "Cin", "\'0\'");
				outPortMap	(addOperator, "R",   "YrAddYi");
				vhdl << instance(addOperator, "ADD_YrAddYi");
			
				setCycleFromSignal("XrAddXi", true);
				syncCycleFromSignal("YiMinYr", true);
				syncCycleFromSignal("YrAddYi", true);
				
				IntMultiplier* multiplyOperator = new IntMultiplier(target, wI, wI, wO, signedOperator, ratio, inDelayMap("XrAddXi",getCriticalPath()), enableSuperTiles);
				oplist.push_back(multiplyOperator);
			
				inPortMap (multiplyOperator, "X", "Yr");
				inPortMap (multiplyOperator, "Y", "XrAddXi");
				outPortMap(multiplyOperator, "R", "K1");
				vhdl << instance(multiplyOperator, "MUL_K1");
			
				inPortMap (multiplyOperator, "X", "Xr");
				inPortMap (multiplyOperator, "Y", "YiMinYr");
				outPortMap(multiplyOperator, "R", "K2");
				vhdl << instance(multiplyOperator, "MUL_K2");
			
				inPortMap (multiplyOperator, "X", "Xi");
				inPortMap (multiplyOperator, "Y", "YrAddYi");
				outPortMap(multiplyOperator, "R", "K3");
				vhdl << instance(multiplyOperator, "MUL_K3");
			
				setCycleFromSignal("K1", false);
				syncCycleFromSignal("K2", false);
				syncCycleFromSignal("K3", false);
				
				manageCriticalPath(getTarget()->localWireDelay(wO) + getTarget()->lutDelay());
			
				vhdl << tab << declare("neg_K3", wO) << " <= K3 xor (" << wO-1 << " downto 0 => \'1\');" << endl;
			
				syncCycleFromSignal("neg_K3", false);
			
				IntAdder *addOperator2 =  new IntAdder(target, wO, inDelayMap("neg_K3",getCriticalPath()));
				oplist.push_back(addOperator2);
			
				inPortMap 	(addOperator2, "X",   "K1");
				inPortMap 	(addOperator2, "Y",   "neg_K3");
				inPortMapCst(addOperator2, "Cin", "\'1\'");
				outPortMap	(addOperator2, "R",   "Zr", false);
				vhdl << instance(addOperator2, "ADD_K1MinK3");
			
				inPortMap 	(addOperator2, "X",   "K1");
				inPortMap 	(addOperator2, "Y",   "K2");
				inPortMapCst(addOperator2, "Cin", "\'0\'");
				outPortMap	(addOperator2, "R",   "Zi", false);
				vhdl << instance(addOperator2, "ADD_K1AddK2");
				
			}catch(std::string str){
				cout << "execution interrupted: " << str << endl;
				exit(1);
			}
		}
	
	}	


	OldFixedComplexMultiplier::~OldFixedComplexMultiplier()
	{
	}
	


	void OldFixedComplexMultiplier::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		// first few cases to check emulate()
		tc = new TestCase(this); 
		tc->addInput("Xr", mpz_class(0) );
		tc->addInput("Xi", mpz_class(0) );
		tc->addInput("Yr", mpz_class(0) );
		tc->addInput("Yi", mpz_class(0) );
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addInput("Xr", mpz_class(1) );
		tc->addInput("Xi", mpz_class(0) );
		tc->addInput("Yr", mpz_class(1) );
		tc->addInput("Yi", mpz_class(0) );
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addInput("Xr", mpz_class(0) );
		tc->addInput("Xi", mpz_class(1) );
		tc->addInput("Yr", mpz_class(0) );
		tc->addInput("Yi", mpz_class(1) );
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addInput("Xr", mpz_class(3) );
		tc->addInput("Xi", mpz_class(3));
		tc->addInput("Yr", mpz_class(3) );
		tc->addInput("Yi", mpz_class(3) );
		emulate(tc);
		tcl->add(tc);

		mpz_class neg = (mpz_class(1)<<wI);
		tc = new TestCase(this); 
		tc->addInput("Xr", mpz_class(3) );
		tc->addInput("Xi", neg -3);
		tc->addInput("Yr", mpz_class(3) );
		tc->addInput("Yi", mpz_class(3) );
		emulate(tc);
		tcl->add(tc);

	}

	
	void OldFixedComplexMultiplier::emulate ( TestCase* tc ) {
		mpz_class svXr = tc->getInputValue("Xr");
		mpz_class svXi = tc->getInputValue("Xi");
		mpz_class svYr = tc->getInputValue("Yr");
		mpz_class svYi = tc->getInputValue("Yi");
		
		
		if (! signedOperator){

			// mpz_class svZi = svXr*svYi + svXi*svYr;
			// mpz_class svZr = svXr*svYr - svXi*svYi;
			
			// // Don't allow overflow
			// mpz_clrbit ( svZi.get_mpz_t(), 2*wI );
			// mpz_clrbit ( svZr.get_mpz_t(), 2*w );

			// tc->addExpectedOutput("Zi", svZi);
			// tc->addExpectedOutput("Zr", svZr);
		}
		else{
			mpz_class big1I = (mpz_class(1) << (wI));
			mpz_class big1PI = (mpz_class(1) << (wI-1));
			mpz_class tmpSUB = (mpz_class(1) << (2*wI+1));

			if ( svXi >= big1PI)
				svXi = svXi - big1I;
			if ( svXr >= big1PI)
				svXr = svXr - big1I;

			if ( svYi >= big1PI)
				svYi = svYi - big1I;
			if ( svYr >= big1PI)
				svYr = svYr - big1I;
			
			mpz_class svZr = svXr*svYr - svXi*svYi;
			mpz_class svZi = svXr*svYi + svXi*svYr;
			
			if ( svZr < 0){
				svZr += tmpSUB; 
			}
			if ( svZi < 0){
				svZi += tmpSUB; 
			}
			
			// now truncate to wO bits
			if (wO<2*wI+1){
				svZr = svZr >> (2*wI+1-wO);
				svZi = svZi >> (2*wI+1-wO);
			}

			if (wO>2*wI+1){
				svZr = svZr << (-2*wI+1+wO);
				svZi = svZi << (-2*wI+1+wO);
			}
			tc->addExpectedOutput("Zi", svZi);
			tc->addExpectedOutput("Zr", svZr);
			
			svZr++;
			svZi++;
			mpz_clrbit ( svZr.get_mpz_t(), wO );			// no overflow
			mpz_clrbit ( svZi.get_mpz_t(), wO );			// no overflow
			tc->addExpectedOutput("Zi", svZi);
			tc->addExpectedOutput("Zr", svZr);
			
			
		}
		

	}

}
