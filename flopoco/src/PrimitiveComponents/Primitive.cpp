#include "Primitive.hpp"
// general c++ library for manipulating streams
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "gmp.h"
#include "mpfr.h"
#include "Target.hpp"

using namespace std;
namespace flopoco {
	Primitive::Primitive(Operator *parentOp, Target *target ) : Operator( parentOp, target ){
        setCopyrightString( "Marco Kleinlein" );
        setCombinatorial();
		setShared();
		setLibraryComponent();
    }

    Primitive::~Primitive() {}

    void Primitive::addPrimitiveLibrary(OperatorPtr op, Target *target ){
        std::stringstream o;
        o << "--------------------------------------------------------------------------------" << std::endl;
        if( target->getVendor() == "Xilinx" ){
			o << "library UNISIM;" << std::endl;
			o << "use UNISIM.Vcomponents.all;" << std::endl;
        }
        else if(target->getVendor() == "Altera")
        {
            o << "library wysiwyg;" << std::endl;
            o << "use wysiwyg.";
            if( target->getID() == "CycloneII" ){
                o << "cycloneii";
            }else if( target->getID() == "CycloneIII" ){
                o << "cycloneiii";
            }else if( target->getID() == "CycloneIV" ){
                o << "cycloneiv";
            }else if( target->getID() == "CycloneV" ){
                o << "cyclonev";
            }else if( target->getID() == "StratixII" ){
                o << "stratixii";
            }else if( target->getID() == "StratixIII" ){
                o << "stratixiii";
            }else if( target->getID() == "StratixIV" ){
                o << "stratixiv";
            }else if( target->getID() == "StratixV" ){
                o << "stratixv";
            }else{
                throw std::runtime_error("Target not supported for primitives");
            }
            o << "_components.all;" << std::endl;
        }else{
            throw std::runtime_error("Target not supported for primitives");
        }
        o << "--------------------------------------------------------------------------------" << std::endl;
        if( op->getAdditionalHeaderInformation().find( o.str() )==std::string::npos  ){
            op->addAdditionalHeaderInformation(o.str());
        }
    }
/*
    void Primitive::checkTargetCompatibility( Target *target ) {
        if( target->getVendor() != "Xilinx" || !(target->getID() == "Virtex6" || target->getID() == "Zynq7000" || target->getID() == "Kintex7") ) {
            throw std::runtime_error( "This component is only suitable for target Xilinx Virtex6, Zynq7000 and Kintex7." );
        }
    }
*/

    /* The new interface, similar to instance()*/
    string Primitive::primitiveInstance(string instanceName)
    {
        addPrimitiveLibrary(getParentOp(),getParentOp()->getTarget());
        return getParentOp()->instance(this, instanceName);
    }

}//namespace
