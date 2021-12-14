#include <sstream>
#include <iostream>
#include "PositAdd.hpp"
#include <Operator.hpp>
#include <utils.hpp>

#include "PIFAdd.hpp"
#include "Conversions/Posit2PIF.hpp"
#include "Conversions/PIF2Posit.hpp"
#include "TestBenches/PositNumber.hpp"

using namespace std;
namespace flopoco{


  PositAdd::PositAdd(Target* target, Operator* parentOp, int width, int wES): Operator(parentOp, target), width_(width), wES_(wES) {
    srcFileName="PositAdd";
    
    ostringstream name;
    
    name << "PositAdd_" <<width_<<"_"<<wES_;
    setNameWithFreqAndUID(name.str());
    setCopyrightString("test");


    
    int wE = intlog2(width_) + 1 + wES_;
    int wF = width_ - (wES_ + 3);
    addInput ("X" , width_);
    addInput ("Y" , width_);
    addOutput("R" , width_);
    
    addFullComment("Start of vhdl generation");
    
    
    ostringstream paramX, inmapX, outmapX;
    paramX << "width=" << width_;
    paramX << " wES=" << wES_;
    
    inmapX << "I=>X";
    
    outmapX << "O=>X_PIF";
    
    newInstance("Posit2PIF", "X_conversion", paramX.str(), inmapX.str(), outmapX.str());

    ostringstream paramY, inmapY, outmapY;
    paramY << "width=" << width_;
    paramY << " wES=" << wES_;
    
    inmapY << "I=>Y";
    
    outmapY << "O=>Y_PIF";
    
    newInstance("Posit2PIF", "Y_conversion", paramY.str(), inmapY.str(), outmapY.str());

    ostringstream paramAdd, inmapAdd, outmapAdd;
    paramAdd << "wF=" << wF;
    paramAdd << " wE=" << wE;
    
    inmapAdd << "X=>X_PIF,Y=>Y_PIF";
    
    outmapAdd << "R=>R_PIF";
    
    newInstance("PIFAdd", "Addition", paramAdd.str(), inmapAdd.str(), outmapAdd.str());

    ostringstream paramR, inmapR, outmapR;
    paramR << "width=" << width_;
    paramR << " wES=" << wES_;
    
    inmapR << "I=>R_PIF";
    
    outmapR << "O=>R";
    
    newInstance("PIF2Posit", "R_conversion", paramR.str(), inmapR.str(), outmapR.str());



    addFullComment("End of vhdl generation");
    
  }
  
  void PositAdd::emulate(TestCase * tc)
  {	  
    /* Get I/O values */
    mpz_class svX = tc->getInputValue("X");
    mpz_class svY = tc->getInputValue("Y");
    
    /* Compute correct value */
    PositNumber posx(width_, wES_, svX);
    PositNumber posy(width_, wES_, svY);
    mpfr_t x, y, r;
    mpfr_init2(x, 1000*width_ -2);
    mpfr_init2(y, 1000*width_ -2);
    mpfr_init2(r, 1000*width_ -2);
    posx.getMPFR(x);
    posy.getMPFR(y);
    mpfr_add(r, x, y, GMP_RNDN);
    
    // Set outputs
    PositNumber posr(width_, wES_, r);
    mpz_class svR = posr.getSignalValue();
    tc->addExpectedOutput("R", svR);
    
    // clean up
    mpfr_clears(x, y, r, NULL);
  }


	
  OperatorPtr PositAdd::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
    int width, wES;
    UserInterface::parseStrictlyPositiveInt(args, "width", &width); 
    UserInterface::parsePositiveInt(args, "wES", &wES);
    return new PositAdd(target, parentOp, width, wES);
  }
  
  void PositAdd::registerFactory(){
    UserInterface::add("PositAdd", // name
		       "A correctly rounded posit adder.",
		       "Posit",
		       "", //seeAlso
		       "width(int): posit size in bits; \
                         wES(int): exponent size in bits;",
		       "",
		       PositAdd::parseArguments
		       ) ;
  }
}
