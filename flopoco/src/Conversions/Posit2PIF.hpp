#ifndef POSIT2PIF_HPP
#define POSIT2PIF_HPP


/* Each Operator declared within the flopoco framework has 
   to inherit the class Operator and overload some functions listed below*/
#include "Operator.hpp"

/* This file contains a lot of useful functions to manipulate vhdl */
#include "utils.hpp"

/*  All flopoco operators and utility functions are declared within
    the flopoco namespace.
    You have to use flopoco:: or using namespace flopoco in order to access these
    functions.
*/

namespace flopoco {
  
  // new operator class declaration
  class Posit2PIF : public Operator {
    
  public:        
    Posit2PIF(Target* target, Operator* parentOp, int widthI, int wES);
    
    // destructor
    // ~Posit2PIF();
    
    void emulate(TestCase * tc);
    static void computePIFWidths(int const widthI, int const wES, int* wE, int* wF);
    
    
    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
    
    
    static void registerFactory();
    
  private:
    int wE_;
    int wF_;
    int widthI_;
    int wES_;
    
  };
  
  
}//namespace

#endif
