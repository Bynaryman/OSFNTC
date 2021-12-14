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
#include "PrimitiveComponents/Xilinx/Xilinx_CFGLUTShadow.hpp"

#include "PrimitiveComponents/Primitive.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_Primitive.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_CFGLUT5.hpp"
#include "PrimitiveComponents/Xilinx/Xilinx_LUT_compute.h"
#include "PrimitiveComponents/Xilinx/Xilinx_LUT6.hpp"


using namespace std;
namespace flopoco {

    Xilinx_CFGLUTShadow::Xilinx_CFGLUTShadow(Target* target, bool cfgLutCompatibleInterface) : Operator(target)
	{
        srcFileName="Xilinx_CFGLUTShadow";
        setNameWithFreqAndUID("Xilinx_CFGLUTShadow");
        setCopyrightString("Martin Kumm");

		// declaring inputs

        if(cfgLutCompatibleInterface)
        {
            //generating the CFGLUT compatible interface
            //For that, a single control signal (CE) is used to controll output and configuration:
            //CE=0: outputs CFGLUT1 and enables configuration for CFGLUT2, CE=1: outputs CFGLUT2 and enables configuration for CFGLUT1
            //Note that this makes the timing of the configuration more critical and may also lead
            //to higher power consumption as one LUT is reconfiguring all the time
            addInput("ce"); //single CE input as the CFGLUT
        }
        else
        {
            addInput("ce1"); //enables configuration for CFGLUT1
            addInput("ce2"); //enables configuration for CFGLUT2
            addInput("sel"); //selects the output of the active CFGLUT
        }
        addInput("cdi");//configuration data in
		
        //LUT inputs
        for(int i=0; i <= 4; i++)
        {
            addInput("i" + std::to_string(i));
        }
		// declaring output
        addOutput("o5"); //LUT-4 output
        addOutput("o6"); //LUT-5 output
        if(cfgLutCompatibleInterface)
        {
            addOutput("cdo"); //configuration data out
            vhdl << "cdo" << " <= " << declare("cdo1") << ";" << std::endl;
            declare("cdo2"); //not used for the CFGLUT5 compatible interface
        }
        else
        {
            addOutput("cdo1"); //configuration data out of CFGLUT 1
            addOutput("cdo2"); //configuration data out of CFGLUT 2
        }

        Xilinx_CFGLUT5 *cfglut1 = new Xilinx_CFGLUT5(target);
        Xilinx_CFGLUT5 *cfglut2 = new Xilinx_CFGLUT5(target);
        addToGlobalOpList(cfglut1);
        addToGlobalOpList(cfglut2);

//        cfglut->setGeneric( "init", generateInitStringFor(defaultConstant,LUT_No) );

        inPortMapCst(cfglut1,"clk","clk");
        inPortMapCst(cfglut2,"clk","clk");

        if(cfgLutCompatibleInterface)
        {
            vhdl << declare("ce1") << " <= ce;" << std::endl;
            vhdl << declare("ce2") << " <= not ce;" << std::endl;
            vhdl << declare("sel") << " <= ce;" << std::endl;
        }

        inPortMapCst(cfglut1,"ce","ce1");
        inPortMapCst(cfglut2,"ce","ce2");

        inPortMapCst(cfglut1,"cdi","cdi");
        inPortMapCst(cfglut2,"cdi","cdi");

        for(int i=0; i <= 4; i++)
        {
            inPortMapCst(cfglut1,"i" + std::to_string(i),"i" + std::to_string(i));
            inPortMapCst(cfglut2,"i" + std::to_string(i),"i" + std::to_string(i));
        }

        //connect outputs:
        outPortMap(cfglut1,"o5","cfglut1_o5");
        outPortMap(cfglut1,"o6","cfglut1_o6");
        outPortMap(cfglut2,"o5","cfglut2_o5");
        outPortMap(cfglut2,"o6","cfglut2_o6");

        outPortMap(cfglut1,"cdo","cdo1", false);
        outPortMap(cfglut2,"cdo","cdo2", false);

        vhdl << cfglut1->primitiveInstance("cfglut1",this);
        vhdl << cfglut2->primitiveInstance("cfglut2",this);


        //construct the multiplexer:
        //LUT content of the LUTs:
        lut_op lutop_o5 = (~lut_in(0) & lut_in(1)) | (lut_in(0) & lut_in(2)); //2:1 MUX between in1 and in2 (select input is in0)
        lut_op lutop_o6 = (~lut_in(0) & lut_in(3)) | (lut_in(0) & lut_in(4)); //2:1 MUX between in3 and in4 (select input is in0)
        lut_init lutop( lutop_o5, lutop_o6 );

		Xilinx_LUT6_2 *muxlut = new Xilinx_LUT6_2( parentOp,target );
        addToGlobalOpList(muxlut);
        muxlut->setGeneric( "init", lutop.get_hex() );

        inPortMap(muxlut,"i0","sel");
        inPortMap(muxlut,"i1","cfglut1_o5");
        inPortMap(muxlut,"i2","cfglut2_o5");
        inPortMap(muxlut,"i3","cfglut1_o6");
        inPortMap(muxlut,"i4","cfglut2_o6");
        inPortMapCst(muxlut,"i5","'1'");

        outPortMap(muxlut,"o5","o5", false);
        outPortMap(muxlut,"o6","o6", false);

        vhdl << muxlut->primitiveInstance("muxlut",this);
    }


}//namespace
