#include <sstream>
#include <iostream>
#include "PIFAdd.hpp"
#include <Operator.hpp>
#include <utils.hpp>

#include "ShiftersEtc/LZOCShifterSticky.hpp"
#include "ShiftersEtc/Shifters.hpp"

using namespace std;
namespace flopoco{


  PIFAdd::PIFAdd(Target* target, Operator* parentOp, int wE, int wF): Operator(parentOp, target), wE_(wE), wF_(wF) {
    srcFileName="PIFAdd";
    
    ostringstream name;
    
    name << "PIFAdd_" <<wE<<"_"<<wF;
    setNameWithFreqAndUID(name.str());
    setCopyrightString("test");
    
    int maxshiftsize = intlog2(wF+4);
    int width = wE + wF + 5;
    addInput ("X" , width);
    addInput ("Y" , width);
    addOutput("R" , width);
    
    addFullComment("Start of vhdl generation");
    
    
    vhdl << declare(.0, "X_is_NAR", 1, false) << "<= X" << of(width - 1) << ";" << endl;
    vhdl << declare(.0, "X_s", 1, false) << "<= X" << of(width - 2) << ";" << endl;
    vhdl << declare(.0, "X_exponent", wE) << "<= X" << range(width - 3, wF + 3) << ";" << endl;
    vhdl << declare(.0, "X_fraction", wF + 1) << "<= X" << range(wF +2, 2) << ";" << endl; //fraction avec bit implicite et round
    
    vhdl << declare(.0, "Y_is_NAR", 1, false) << "<= Y" << of(width - 1) << ";" << endl;
    vhdl << declare(.0, "Y_s", 1, false) << "<= Y" << of(width - 2) << ";" << endl;
    vhdl << declare(.0, "Y_exponent", wE) << "<= Y" << range(width - 3, wF + 3) << ";" << endl;
    vhdl << declare(.0, "Y_fraction", wF + 1) << "<= Y" << range(wF +2, 2) << ";" << endl;
    
   double delayAdderIsLargeExp = target->adderDelay(wE);
   cerr << "AdderIsLargeExp delay : " << delayAdderIsLargeExp << endl; 
    vhdl << declare(delayAdderIsLargeExp, "is_larger_exp", 1, false) << "<= '1' when X_exponent > Y_exponent else '0';" << endl;
    
    vhdl << "with is_larger_exp select " << declare(target->logicDelay(1), "larger_exp", wE) << "<= " << endl <<
      tab << "X_exponent when '1'," << endl <<
      tab << "Y_exponent when '0'," << endl <<
      tab << "\"" << string(wE, '-') << "\" when others;" << endl;
    
    vhdl << "with is_larger_exp select " << declare(target->logicDelay(1), "smaller_exp", wE) << "<= " << endl <<
      tab << "Y_exponent when '1'," << endl <<
      tab << "X_exponent when '0'," << endl <<
      tab << "\"" << string(wE, '-') << "\" when others;" << endl;
    
    vhdl << "with is_larger_exp select " << declare(target->logicDelay(1), "larger_mantissa", wF+2) << "<= " << endl <<
      tab << "X_s & X_fraction when '1'," << endl <<
      tab << "Y_s & Y_fraction when '0'," << endl <<
      tab << "\"" << string(wF+2, '-') << "\" when others;" << endl;
    
    vhdl << "with is_larger_exp select " << declare(target->logicDelay(1), "smaller_mantissa", wF+2) << "<= " << endl <<
      tab << "Y_s & Y_fraction when '1'," << endl <<
      tab << "X_s & X_fraction when '0'," << endl <<
      tab << "\"" << string(wF+2, '-') << "\" when others;" << endl;

    vhdl << "with is_larger_exp select " << declare(target->logicDelay(1), "larger_sign", 1, false) << "<= " << endl <<
      tab << "X_s when '1'," << endl <<
      tab << "Y_s when '0'," << endl <<
      tab << "'-' when others;" << endl;
    
    vhdl << "with is_larger_exp select " << declare(target->logicDelay(1), "pad", 1, false) << "<= " << endl <<
      tab << "Y_s when '1'," << endl <<
      tab << "X_s when '0'," << endl <<
      tab << "'-' when others;" << endl;

    vhdl << declare(target->adderDelay(wE+1), "offset", wE+1) << " <= ('0' & larger_exp) - ('0' & smaller_exp);" << endl;

    vhdl << declare(.0, "sup_offset", wE+1-maxshiftsize) << " <= offset" << range(wE, maxshiftsize) << ";" << endl;

    vhdl << declare(target->adderDelay(wE+1-maxshiftsize), "saturate", 1, false) << " <= '0' when sup_offset = \"" << string(wE+1-maxshiftsize, '0') << "\" else '1';"<<endl;

    vhdl << "with saturate select " << declare(target->logicDelay(1), "inf_offset", maxshiftsize) << "<=" << endl <<
      tab << "\"" << string(maxshiftsize, '1') << "\" when '1'," << endl <<
      tab << "offset" << range(maxshiftsize-1, 0) << " when '0'," << endl <<
      tab << "\"" << string(maxshiftsize, '-') << "\" when others;" << endl;
    
    vhdl << declare(.0, "input_shifter", wF+4) << " <= smaller_mantissa & \"00\";" << endl;
    
    ostringstream param1, inmap1, outmap1;
    param1 << "wIn=" << wF + 4;
    param1 << " maxShift=" << wF+4;
    param1 << " wOut=" << wF + 4;
    param1 << " dir=" << Shifter::Right;
    param1 << " computeSticky=true";
    param1 << " inputPadBit=true";
    
    inmap1 << "X=>input_shifter,S=>inf_offset,padBit=>pad";
    
    outmap1 << "R=>shifted_frac,Sticky=>sticky";
    
    newInstance("Shifter", "mantissa_shift", param1.str(), inmap1.str(), outmap1.str());
    
    vhdl << declare(.0, "shifted_frac_trunc", wF+3) << "<= shifted_frac" << range(wF+3, 1) << ";" << endl;

    vhdl << "with larger_sign select " << declare(target->logicDelay(1), "padded_larger_mantissa", wF+4) << "<= " << endl << //ça fait trop paddé y'a un pb 
      tab << "\"0\" & larger_mantissa & '0' when '0'," << endl <<
      tab << "\"1\" & larger_mantissa & '0' when '1'," << endl <<
      tab << "\"" << string(wF+4, '-') << "\" when others;" << endl;
    
    vhdl << declare(target->adderDelay(wF+6), "add_mantissa", wF + 6) <<  "<= ((padded_larger_mantissa) + (pad & shifted_frac_trunc)) & shifted_frac"<< of(0) << " & sticky;" << endl;
    
    
    vhdl << declare(.0, "count_type", 1, false) << "<= add_mantissa" << of(wF+5) << ";" << endl;
    
    ostringstream param2, inmap2, outmap2;
    int wCount = intlog2(wF+6); 
    param2 << "wIn=" << wF + 6;
    param2 << " wOut=" << wF + 6;
    param2 << " wCount=" << wCount; 
    
    inmap2 << "I=>add_mantissa,OZb=>count_type";
    
    outmap2 << "Count=>lzCount,O=>significand";
    
    newInstance("LZOCShifterSticky", "align_mantissa", param2.str(), inmap2.str(), outmap2.str());
    
    vhdl << declare(target->adderDelay(wE), "exponent", wE) << " <= larger_exp + 2 - lzCount;" << endl;
    
    vhdl << declare(.0, "round", 1, false) << "<= significand"<< of(4) << ";" << endl;
    
    vhdl << declare(target->adderDelay(4), "not_sticky", 1, false) << "<= '1' when significand" << range(3, 0) << " = \"0000\" else '0';" << endl;
    vhdl << declare(target->logicDelay(2), "pre_is_zero", 1, false) << "<= (not round) or (not_sticky and round);" << endl;
    vhdl << declare (.0, "fraction", wF+1) << "<= significand" << range(wF+5, 5) << ";" << endl;
    vhdl << declare(target->adderDelay(wF+1), "fraction_is_zero", 1, false) << "<= '1' when fraction = \"" <<  string(wF+1, '0') << "\" else '0';" << endl;
    vhdl << declare(target->logicDelay(3), "is_zero", 1, false) << "<= not count_type and fraction_is_zero and pre_is_zero;" << endl;
    
    vhdl << "with is_zero select " << declare(target->logicDelay(wF), "s", 1, false) << "<= " << endl <<
      tab << "'0' when '1'," << endl <<
      tab << "not fraction" << of(wF) << " when '0'," << endl <<
      tab << "'-' when others;" << endl;
    
    vhdl << "with is_zero select " << declare(target->logicDelay(1), "final_exponent", wE) << "<= " << endl <<
      tab << "\"" << string(wE, '0') << "\" when '1'," << endl <<
      tab << "exponent when '0'," << endl <<
      tab << "\"" << string(wE, '0') << "\" when others;" << endl;
    
    vhdl << declare(target->logicDelay(2), "is_NAR", 1, false) << " <= X" << of(width-1) << " or Y" << of(width-1) << ";" << endl;
    vhdl << "R <= is_NAR & s & final_exponent & fraction & round & (not not_sticky);" << endl;
  }
  
  void PIFAdd::emulate(TestCase * tc)
  {
  }


	
  OperatorPtr PIFAdd::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
    int wE, wF;
    UserInterface::parseStrictlyPositiveInt(args, "wE", &wE); 
    UserInterface::parseStrictlyPositiveInt(args, "wF", &wF);
    return new PIFAdd(target, parentOp, wE, wF);
  }
  
  void PIFAdd::registerFactory(){
    UserInterface::add("PIFAdd", // name
		       "A correctly rounded posit intermediate format adder.",
		       "BasicPIF",
		       "", //seeAlso
		       "wF(int): mantissa size in bits; \
                         wE(int): exponent size in bits;",
		       "",
		       PIFAdd::parseArguments
		       ) ;
  }
}
