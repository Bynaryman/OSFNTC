// general c++ library for manipulating streams
#include <iostream>
#include <sstream>
#include <stdexcept>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "Altera_LCELL.hpp"

using namespace std;
namespace flopoco {

Altera_LCELL::Altera_LCELL(Operator *parentOp, Target *target, const string &lut_mask, const bool &shared_arith, const bool &dont_touch) : Primitive(parentOp, target), _hasSharedArith(false), _hasDontTouch(false), _hasLUT7(false) {
    srcFileName = "Altera_LCELL";
    //setNameWithFreqAndUID("Altera_LCELL");
    if(target->getVendor() == "Altera"){
        std::stringstream o;
        if( target->getID() == "CycloneII" ){
            o << "cycloneii";
        }else if( target->getID() == "StratixII" ){
            _hasSharedArith = true;
            _hasLUT7 = true;
            o << "stratixii";
        }else{
            _hasDontTouch = true;
            if( target->getID() == "CycloneIII" ){
                o << "cycloneiii";
            }else if( target->getID() == "CycloneIV" ){
                o << "cycloneiv";
            }else {
                _hasSharedArith = true;
                _hasLUT7 = true;
                if( target->getID() == "CycloneV" ){
                    o << "cyclonev";
                }else if( target->getID() == "StratixIII" ){
                    o << "stratixiii";
                }else if( target->getID() == "StratixIV" ){
                    o << "stratixiv";
                }else if( target->getID() == "StratixV" ){
                    o << "stratixv";
                }else{
                    throw std::runtime_error("Target not supported for Altera_LCELL");
                }
            }
            o << "lcell_comb";
            setNameWithFreqAndUID(o.str());
        }


        setGeneric("lut_mask",lut_mask);
        addInput("dataa",1,false);
        addInput("datab",1,false);
        addInput("datac",1,false);
        addInput("datad",1,false);
        addInput("cin",1,false);
        addOutput("combout",1,2,false);
        addOutput("cout",1,2,false);
        if( hasDontTouch() ){
            if(dont_touch) setGeneric("dont_touch","\"on\"");
        }
        if( hasLUT7() ){
            addInput("datae",1,false);
            addInput("dataf",1,false);
            addInput("datag",1,false);
        }
        if( hasSharedArith() ){
            if(shared_arith) setGeneric("shared_arith","\"on\"");
            addInput("sharein",1,false);
            addOutput("shareout",1,2,false);
            addOutput("sumout",1,2,false);
        }
    }else{
        throw std::runtime_error("Target not supported for Altera_LCELL");
    }
}

}//namespace
