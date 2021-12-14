#include "BaseMultiplierDSPSuperTilesXilinx.hpp"
#include "../PrimitiveComponents/Xilinx/Xilinx_LUT6.hpp"
#include "../PrimitiveComponents/Xilinx/Xilinx_CARRY4.hpp"
#include "../PrimitiveComponents/Xilinx/Xilinx_LUT_compute.h"

namespace flopoco {


Operator* BaseMultiplierDSPSuperTilesXilinx::generateOperator(
		Operator *parentOp,
		Target* target,
		Parametrization const & parameters) const
{
	return new BaseMultiplierDSPSuperTilesXilinxOp(
			parentOp,
			target,
            parameters.isSignedMultX(),
			parameters.isSignedMultY(),
            (TILE_SHAPE)parameters.getShapePara(),
			false
		);
}

const int BaseMultiplierDSPSuperTilesXilinx::shape_size[12][5] =    {{41, 34, 41+1, 58+1, 17},  // A (x,y,r,MSB,LSB)
                                                                     {41, 41, 41+1, 58+1, 17},  // B
                                                                     {41, 41, 41+1, 65+1, 24},  // C
                                                                     {34, 41, 41+1, 58+1, 17},  // D
                                                                     {24, 34, 58, 58,  0},  // E
                                                                     {48, 24, 58, 65,  7},  // F
                                                                     {24, 41, 58, 65,  7},  // G
                                                                     {41, 24, 58, 58,  0},  // H
                                                                     {41, 24, 58, 65,  7},  // I
                                                                     {24, 41, 58, 58,  0},  // J
                                                                     {34, 24, 58, 58,  0},  // K
                                                                     {24, 48, 58, 65,  7}}; // L

int BaseMultiplierDSPSuperTilesXilinx::getRelativeResultLSBWeight(Parametrization const& param) const
{
    if(!param.getShapePara() || param.getShapePara() > 12)
        throw string("Error in ") + string("srcFileName") + string(": shape unknown");
    return getRelativeResultLSBWeight((BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE)param.getShapePara());
}

int BaseMultiplierDSPSuperTilesXilinx::getRelativeResultMSBWeight(Parametrization const& param) const
{
    if(!param.getShapePara() || param.getShapePara() > 12)
        throw string("Error in ") + string("srcFileName") + string(": shape unknown");
    return getRelativeResultMSBWeight((BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE)param.getShapePara());
}

int BaseMultiplierDSPSuperTilesXilinx::isSuperTile(int rx1, int ry1, int lx1, int ly1, int rx2, int ry2, int lx2, int ly2){
    for(int i = 1; i <= 12; i++)
        if((mult_bounds[i].dsp1_rx == rx1 && mult_bounds[i].dsp1_ry == ry1 && mult_bounds[i].dsp1_lx == lx1 && mult_bounds[i].dsp1_ly == ly1
         && mult_bounds[i].dsp2_rx == rx2 && mult_bounds[i].dsp2_ry == ry2 && mult_bounds[i].dsp2_lx == lx2 && mult_bounds[i].dsp2_ly == ly2)
         ||(mult_bounds[i].dsp1_rx == rx2 && mult_bounds[i].dsp1_ry == ry2 && mult_bounds[i].dsp1_lx == lx2 && mult_bounds[i].dsp1_ly == ly2
         && mult_bounds[i].dsp2_rx == rx1 && mult_bounds[i].dsp2_ry == ry1 && mult_bounds[i].dsp2_lx == lx1 && mult_bounds[i].dsp2_ly == ly1))
            return i;
    return 0;
}

double BaseMultiplierDSPSuperTilesXilinx::getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY){
    int rx1 = mult_bounds[shape-1].dsp1_rx, rx2 = mult_bounds[shape-1].dsp2_rx, ry1 = mult_bounds[shape-1].dsp1_ry, ry2 = mult_bounds[shape-1].dsp2_ry;
    int x1_min = ((x_anchor + rx1 < 0)?0: x_anchor + rx1);
    int y1_min = ((y_anchor + ry1 < 0)?0: y_anchor + ry1);
    int x2_min = ((x_anchor + rx2 < 0)?0: x_anchor + rx2);
    int y2_min = ((y_anchor + ry2 < 0)?0: y_anchor + ry2);
    int lsb = ( x2_min + y2_min < x1_min + y1_min)?x2_min + y2_min:x1_min + y1_min;

    int lx1 = mult_bounds[shape-1].dsp1_lx, lx2 = mult_bounds[shape-1].dsp2_lx, ly1 = mult_bounds[shape-1].dsp1_ly, ly2 = mult_bounds[shape-1].dsp2_ly;
    int x1_max = ((wMultX < x_anchor + lx1+1)?wMultX: x_anchor + lx1+1);
    int y1_max = ((wMultY < y_anchor + ly1+1)?wMultY: y_anchor + ly1+1);
    int x2_max = ((wMultX < x_anchor + lx2+1)?wMultX: x_anchor + lx2+1);
    int y2_max = ((wMultY < y_anchor + ly2+1)?wMultY: y_anchor + ly2+1);
    int x_max, y_max;
    if(x1_max + y1_max < x2_max + y2_max){
        x_max = x2_max;
        y_max = y2_max;
    } else {
        x_max = x1_max;
        y_max = y1_max;
    }
    int msb = (x_max==1)?y_max:((y_max==1)?x_max:x_max+y_max);
    msb = (0 < shape && shape <= 4 && (msb+1 == getRelativeResultMSBWeight(shape) + x_anchor + y_anchor))?msb+1:msb;
    return (msb - lsb)*0.65;
}

bool BaseMultiplierDSPSuperTilesXilinx::shapeValid(Parametrization const& param, unsigned x, unsigned y) const
{
    //check if (x,y) coordinate lies inside of the super tile shape:
    switch(param.getShapePara())
    {
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_A:
            if((x <= 16) && (y <= 16)) return false;
            if((x >= 24) && (y >= 17)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_B:
            if((x <= 16) && (y <= 16)) return false;
            if((x >= 17) && (y >= 17)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_C:
            if((x <= 23) && (y <= 23)) return false;
            if((x >= 24) && (y >= 24)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_D:
            if((x <= 16) && (y <= 16)) return false;
            if((x >= 17) && (y >= 24)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_E:
            //rectangular shape, nothing to check
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_F:
            if((x <= 23) && (y <= 6)) return false;
            if((x >= 24) && (y >= 17)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_G:
            if((x <= 6) && (y <= 23)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_H:
            if((x >= 17) && (y >= 17)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_I:
            if((x <= 23) && (y <= 6)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_J:
            if((x >= 17) && (y >= 17)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_K:
            //rectangular shape, nothing to check
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_L:
            if((x <= 6) && (y <= 23)) return false;
            if((x >= 17) && (y >= 24)) return false;
            break;
        default:
            throw string("Error in ") + string("srcFileName") + string(": shape unknown");
    }

    //check outer bounds:
    if(!((x >= 0) && (x < param.getTileXWordSize()) && (y >= 0) && (y < param.getTileYWordSize()))) return false;

    return true;
}

bool BaseMultiplierDSPSuperTilesXilinx::shapeValid(int x, int y)
{
    //check if (x,y) coordinate lies inside of the super tile shape:
    switch(shape)
    {
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_A:
            if((x <= 16) && (y <= 16)) return false;
            if((x >= 24) && (y >= 17)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_B:
            if((x <= 16) && (y <= 16)) return false;
            if((x >= 17) && (y >= 17)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_C:
            if((x <= 23) && (y <= 23)) return false;
            if((x >= 24) && (y >= 24)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_D:
            if((x <= 16) && (y <= 16)) return false;
            if((x >= 17) && (y >= 24)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_E:
              //rectangular shape, nothing to check
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_F:
            if((x <= 23) && (y <= 6)) return false;
            if((x >= 24) && (y >= 17)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_G:
            if((x <= 6) && (y <= 23)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_H:
            if((x >= 17) && (y >= 17)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_I:
            if((x <= 23) && (y <= 6)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_J:
            if((x >= 17) && (y >= 17)) return false;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_K:
            //rectangular shape, nothing to check
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_L:
            if((x <= 6) && (y <= 23)) return false;
            if((x >= 17) && (y >= 24)) return false;
            break;
        default:
            throw string("Error in ") + string("srcFileName") + string(": shape unknown");
    }

    //check outer bounds:
    if(!((x >= 0) && (x < wX) && (y >= 0) && (y < wY))) return false;

    return true;
}

/**
* \brief determine the occupation ratio of a given shape in range [0..1]
*   the calculation is done geometrically, by determining the area of the two rectangles of the DSP-Blocks,
*   that are enclosed by the box around the area to be tiled for the generation of the main multiplier
**/
    float BaseMultiplierDSPSuperTilesXilinx::shape_utilisation(int shape_x, int shape_y, int mult_wX, int mult_wY, bool signedIO){
        if(0 <= shape_x && (shape_x + wX) < mult_wX && 0 <= shape_y && (shape_y + wY) < mult_wY ){
            return 1.0;
        } else if(((shape_x-wX) < 0 || mult_wX <= shape_x) && ((shape_y+wY)  < 0 || mult_wY <= shape_y )){
            return 0.0;
        } else {
            int wXrel = mult_wX - shape_x;
            int wYrel = mult_wY - shape_y;
            int oXrel = 0 - shape_x;
            int oYrel = 0 - shape_y;
            int ya1 = mult_bounds[(int)shape-1].dsp1_ry;
            int ya2 = mult_bounds[(int)shape-1].dsp1_ly+1;
            int yau = ( (ya1 <= oYrel)?((oYrel < ya2)?oYrel:ya2):ya1 );     //upper boundary of the area in rectangle a
            int yad = ( (ya1 <= wYrel)?((wYrel < ya2)?wYrel:ya2):ya1 );     //downward boundary of the area in rectangle a
            int xa1 = mult_bounds[(int)shape-1].dsp1_rx;
            int xa2 = mult_bounds[(int)shape-1].dsp1_lx+1;
            int xar = ( (xa1 <= oXrel)?((oXrel < xa2)?oXrel:xa2):xa1 );     //right boundary in rectangle a
            int xal = ( (xa1 <= wXrel)?((wXrel < xa2)?wXrel:xa2):xa1 );     //left boundary in rectangle a
            int yb1 = mult_bounds[(int)shape-1].dsp2_ry;
            int yb2 = mult_bounds[(int)shape-1].dsp2_ly+1;
            int ybu = ( (yb1 <= oYrel)?((oYrel < yb2)?oYrel:yb2):yb1 );
            int ybd = ( (yb1 <= wYrel)?((wYrel < yb2)?wYrel:yb2):yb1 );
            int xb1 = mult_bounds[(int)shape-1].dsp2_rx;
            int xb2 = mult_bounds[(int)shape-1].dsp2_lx+1;
            int xbr = ( (xb1 <= oXrel)?((oXrel < xb2)?oXrel:xb2):xb1 );
            int xbl = ( (xb1 <= wXrel)?((wXrel < xb2)?wXrel:xb2):xb1 );
            int a = (xal - xar)*(yad - yau);
            int b = (xbl - xbr)*(ybd - ybu);
            int used = ((0 <= a)?a:-a) + ((0 <= b)?b:-b);
            int total = (xa2-xa1)*(ya2-ya1) + (xb2-xb1)*(yb2-yb1);
            //cout << a << " " << b << " " << (float)used/total << endl;
            return (float)used/total;
        }
    }

BaseMultiplierDSPSuperTilesXilinxOp::BaseMultiplierDSPSuperTilesXilinxOp(Operator *parentOp, Target* target, bool isSignedX, bool isSignedY, BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE shape, bool pipelineDSPs) : Operator(parentOp,target)
{
    useNumericStd();

    char shapeAsChar = ((char) shape) + 'a' - 1; //convert enum to char

    srcFileName = "BaseMultiplierDSPSuperTilesXilinx";
    uniqueName_ = string("BaseMultiplierDSPSuperTilesXilinxShape_") + string(1,shapeAsChar);
    //cout << uniqueName_ << endl;
    //char shapeAsChar = ((char) shape) + 'a' - 1; //convert enum to char
    setNameWithFreqAndUID(string("BaseMultiplierDSPSuperTilesXilinxShape_") + string(1,shapeAsChar));

    this->shape = shape;
    this->pipelineDSPs = pipelineDSPs;

    if((isSignedX == true) || (isSignedY == true)) throw string("unsigned inputs currently not supported by BaseMultiplierDSPSuperTilesXilinxOp, sorry");

    declare(getTarget()->DSPMultiplierDelay(),"D1", 41); //temporary output of the first DSP
    declare(getTarget()->DSPMultiplierDelay(),"D2", 41); //temporary output of the second DSP

    switch(shape)
    {
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_A:
            //total operation is: (X(23 downto 0) * Y(33 downto 17) << 17) + (X(40 downto 17) * Y(16 downto 0) << 17)
            //output shift is excluded to avoid later increase of bitheap
            //realized as:        ((X(23 downto 0) * Y(33 downto 17) + X(40 downto 17) * Y(16 downto 0))
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(23 downto 0)) * unsigned(Y(33 downto 17)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(40 downto 17)) * unsigned(Y(16 downto 0)));" << endl;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_B:
            //total operation is: X(16 downto 0) * Y(40 downto 17) + X(40 downto 17) * Y(16 downto 0)
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(16 downto 0)) * unsigned(Y(40 downto 17)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(40 downto 17)) * unsigned(Y(16 downto 0)));" << endl;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_C:
            //total operation is: (X(23 downto 0) * Y(40 downto 24) << 24) + (X(40 downto 24) * Y(23 downto 0) << 24)
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(23 downto 0)) * unsigned(Y(40 downto 24)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(40 downto 24)) * unsigned(Y(23 downto 0)));" << endl;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_D:
            //total operation is: (X(16 downto 0) * Y(40 downto 17) << 17) + (X(33 downto 17) * Y(23 downto 0) << 17)
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(16 downto 0)) * unsigned(Y(40 downto 17)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(33 downto 17)) * unsigned(Y(23 downto 0)));" << endl;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_E:
            //total operation is: (X(23 downto 0) * Y(16 downto 0)) + (X(23 downto 0) * Y(33 downto 17) << 17)
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(23 downto 0)) * unsigned(Y(16 downto 0)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(23 downto 0)) * unsigned(Y(33 downto 17)));" << endl;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_F:
            //total operation is: ((X(23 downto 0) * Y(23 downto 7)) << 7) + (X(47 downto 24) * Y(16 downto 0) << 24)
            //realized as:        (X(23 downto 0) * Y(23 downto 7) + ((X(47 downto 24) * Y(16 downto 0)) << 17)
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(23 downto 0)) * unsigned(Y(23 downto 7)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(47 downto 24)) * unsigned(Y(16 downto 0)));" << endl;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_G:
            //total operation is: ((X(23 downto 7) * Y(23 downto 0) << 7) + (X(23 downto 0) * Y(40 downto 24) << 24)
            //realized as:        (X(23 downto 7) * Y(23 downto 0)) + (X(23 downto 0) * Y(40 downto 24) << 17)
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(23 downto 7)) * unsigned(Y(23 downto 0)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(23 downto 0)) * unsigned(Y(40 downto 24)));" << endl;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_H:
            //total operation is: (X(16 downto 0) * Y(23 downto 0) + (X(40 downto 17) * Y(16 downto 0) << 17)
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(16 downto 0)) * unsigned(Y(23 downto 0)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(40 downto 17)) * unsigned(Y(16 downto 0)));" << endl;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_I:
            //total operation is: ((X(23 downto 0) * Y(23 downto 7)) << 7) + (X(40 downto 24) * Y(16 downto 0) << 24)
            //realized as:        (X(23 downto 0) * Y(23 downto 7)) + (X(40 downto 24) * Y(16 downto 0) << 17)
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(23 downto 0)) * unsigned(Y(23 downto 7)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(40 downto 24)) * unsigned(Y(23 downto 0)));" << endl;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_J:
            //total operation is: (X(23 downto 0) * Y(16 downto 0)) + ((X(16 downto 0) * Y(40 downto 17)) << 17)
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(23 downto 0)) * unsigned(Y(16 downto 0)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(16 downto 0)) * unsigned(Y(40 downto 17)));" << endl;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_K:
            //total operation is: (X(16 downto 0) * Y(23 downto 0) + (X(33 downto 17) * Y(23 downto 0) << 17)
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(16 downto 0)) * unsigned(Y(23 downto 0)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(33 downto 17)) * unsigned(Y(23 downto 0)));" << endl;
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_L:
            //total operation is: ((X(23 downto 7) * Y(23 downto 0) << 7) + (X(16 downto 0) * Y(47 downto 24) << 24)
            //realized as:        (X(23 downto 7) * Y(23 downto 0)) + (X(16 downto 0) * Y(47 downto 24) << 17)
            vhdl << tab << "D1 <= std_logic_vector(unsigned(X(23 downto 7)) * unsigned(Y(23 downto 0)));" << endl;
            vhdl << tab << "D2 <= std_logic_vector(unsigned(X(16 downto 0)) * unsigned(Y(47 downto 24)));" << endl;
            break;
        default:
            throw string("Error in ") + srcFileName + string(": shape unknown");
    }

    if(pipelineDSPs) //ToDo: decide on target frequency whether to pipeline or not (and how depth)
    {
//        nextCycle(); //!!
    }

    declare(getTarget()->DSPAdderDelay(), "T",BaseMultiplierDSPSuperTilesXilinx::get_wR(shape));
    if((shape >= BaseMultiplierDSPSuperTilesXilinx::SHAPE_A) && (shape <= BaseMultiplierDSPSuperTilesXilinx::SHAPE_D))
    {
        //tilings (a) to (d) doesn't have a shift
        vhdl << tab << "T <= std_logic_vector(unsigned('0' & D1) + unsigned('0' & D2));" << endl;
    }
    else
    {
        //tilings (e) to (l) have a 17 bit shift
        vhdl << tab << "T(57 downto 17) <= std_logic_vector(unsigned(D2) + unsigned(\"00000000000000000\" & D1(40 downto 17)));" << endl;
        vhdl << tab << "T(16 downto 0) <= D1(16 downto 0);" << endl;
    }


    //T(57 downto 17) <= std_logic_vector(unsigned(D2) + unsigned("00000000000000000" & D1(40 downto 17)));
    //T(16 downto 0) <= D1(16 downto 0);
    //O <= std_logic_vector(unsigned((64 downto 41 => '0') & D1) + unsigned(D2 & (16 downto 0 => '0'))); --T;
/*    //output zero padding
    vhdl << tab << "T <= std_logic_vector(unsigned(";
    if(BaseMultiplierDSPSuperTilesXilinx::get_wR(shape) > 41 + max(mult_bounds[(int)shape-1].dsp2_rx, mult_bounds[(int)shape-1].dsp2_ry))
        vhdl << "(" << BaseMultiplierDSPSuperTilesXilinx::get_wR(shape)-1 << " downto " << 41 + max(mult_bounds[(int)shape-1].dsp2_rx, mult_bounds[(int)shape-1].dsp2_ry) << " => '0') & ";
    vhdl << "D2";
    if(max(mult_bounds[(int)shape-1].dsp2_rx, mult_bounds[(int)shape-1].dsp2_ry) > 0)
        vhdl << " & (" << max(mult_bounds[(int)shape-1].dsp2_rx, mult_bounds[(int)shape-1].dsp2_ry)-1 << " downto 0 => '0')";
    vhdl << " ) + unsigned(D1";
    if(max(mult_bounds[(int)shape-1].dsp1_rx, mult_bounds[(int)shape-1].dsp1_ry) > 0)
        vhdl << " & (" << max(mult_bounds[(int)shape-1].dsp1_rx, mult_bounds[(int)shape-1].dsp1_ry)-1 << " downto 0 => '0')";
    vhdl << "));" << endl;
*/

/*
    if(pipelineDSPs) //ToDo: decide on target frequency whether to pipeline or not (and how depth)
    {
        nextCycle();
    }
*/

    vhdl << tab << "R <= T;" << endl;

    addOutput("R", BaseMultiplierDSPSuperTilesXilinx::get_wR(shape));

    addInput("X", BaseMultiplierDSPSuperTilesXilinx::get_wX(shape), true);
    addInput("Y", BaseMultiplierDSPSuperTilesXilinx::get_wY(shape), true);
}

OperatorPtr BaseMultiplierDSPSuperTilesXilinx::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args)
{
    int shape;
	bool xIsSigned,yIsSigned,isPipelined;
    UserInterface::parseStrictlyPositiveInt(args, "shape", &shape);
	UserInterface::parseBoolean(args,"xIsSigned",&xIsSigned);
	UserInterface::parseBoolean(args,"yIsSigned",&yIsSigned);
	UserInterface::parseBoolean(args,"isPipelined",&isPipelined);

	return new BaseMultiplierDSPSuperTilesXilinxOp(parentOp,target,xIsSigned,yIsSigned,(TILE_SHAPE)shape,isPipelined);
}

void BaseMultiplierDSPSuperTilesXilinx::registerFactory()
{
    UserInterface::add( "BaseMultiplierDSPSuperTilesXilinx", // name
                        "Implements a DSP block commonly found in FPGAs incl. pre-adders and post-adders computing R = (X1+X2) * Y + Z",
                        "BasicInteger", // categories
                        "",
                        "shape(int): Shape ID (1-12) of the DSP-Superblock;\
                        isPipelined(bool)=0: use pipelining;\
						xIsSigned(bool)=0: input X is signed;\
						yIsSigned(bool)=0: input Y is signed;",
                       "",
                       BaseMultiplierDSPSuperTilesXilinx::parseArguments
                       ) ;
}

void BaseMultiplierDSPSuperTilesXilinxOp::emulate(TestCase * tc) {
	mpz_class sx = tc->getInputValue("X");
	mpz_class sy = tc->getInputValue("Y");

	mpz_class d1 = 0;
	mpz_class d2 = 0;
	mpz_class t;
    switch(shape)
    {
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_A:
            //total operation is: X(23 downto 0) * Y(33 downto 17) + X(40 downto 17) * Y(16 downto 0)
            d1 = (sx & ((1<<24)-1)) * ((sy>>17) & ((1<<17)-1));
            d2 = ((sx>>17) & ((1<<24)-1)) * (sy & ((1<<17)-1));
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_B:
            //total operation is: X(16 downto 0) * Y(40 downto 17) + X(40 downto 17) * Y(16 downto 0)
            d1 = (sx & ((1<<17)-1)) * ((sy>>17) & ((1<<24)-1));
            d2 = ((sx>>17) & ((1<<24)-1)) * (sy & ((1<<17)-1));
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_C:
            //total operation is: X(23 downto 0) * Y(40 downto 24) + X(40 downto 24) * Y(23 downto 0)
            d1 = (sx & ((1<<24)-1)) * ((sy>>24) & ((1<<17)-1));
            d2 = ((sx>>24) & ((1<<17)-1)) * (sy & ((1<<24)-1));
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_D:
            //total operation is: X(16 downto 0) * Y(40 downto 17) + X(33 downto 17) * Y(23 downto 0)
            d1 = (sx & ((1<<17)-1)) * ((sy>>17) & ((1<<24)-1));
            d2 = ((sx>>17) & ((1<<17)-1)) * (sy & ((1<<24)-1));
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_E:
            //total operation is: (X(23 downto 0) * Y(16 downto 0)) + (X(23 downto 0) * Y(33 downto 17) << 17)
            d1 = (sx & ((1<<24)-1)) * (sy & ((1<<17)-1));
            d2 = (sx & ((1<<24)-1)) * ((sy>>17) & ((1<<17)-1));
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_F:
            //total operation is: ((X(23 downto 0) * Y(23 downto 7)) << 7) + (X(47 downto 24) * Y(16 downto 0) << 24)
            //realized as:        (X(23 downto 0) * Y(23 downto 7) + ((X(47 downto 24) * Y(16 downto 0)) << 17)
            d1 = (sx & ((1<<24)-1)) * ((sy>>7) & ((1<<17)-1));
            d2 = ((sx>>24) & ((1<<24)-1)) * (sy & ((1<<17)-1));
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_G:
            //total operation is: ((X(23 downto 7) * Y(23 downto 0) << 7) + (X(23 downto 0) * Y(40 downto 24) << 24)
            //realized as:        (X(23 downto 7) * Y(23 downto 0)) + (X(23 downto 0) * Y(40 downto 24) << 17)
            d1 = ((sx>>7) & ((1<<17)-1)) * (sy & ((1<<24)-1));
            d2 = (sx & ((1<<24)-1)) * ((sy>>24) & ((1<<17)-1));
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_H:
            //total operation is: (X(16 downto 0) * Y(23 downto 0) + (X(40 downto 17) * Y(16 downto 0) << 17)
            d1 = (sx & ((1<<17)-1)) * (sy & ((1<<24)-1));
            d2 = ((sx>>17) & ((1<<24)-1)) * (sy & ((1<<17)-1));
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_I:
            //total operation is: ((X(23 downto 0) * Y(23 downto 7)) << 7) + (X(40 downto 24) * Y(16 downto 0) << 24)
            //realized as:        (X(23 downto 0) * Y(23 downto 7)) + (X(40 downto 24) * Y(16 downto 0) << 17)
            d1 = (sx & ((1<<24)-1)) * ((sy>>7) & ((1<<17)-1));
            d2 = ((sx>>24) & ((1<<17)-1)) * (sy & ((1<<24)-1));
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_J:
            //total operation is: (X(23 downto 0) * Y(16 downto 0)) + ((X(16 downto 0) * Y(40 downto 17)) << 17)
            d1 = (sx & ((1<<24)-1)) * (sy & ((1<<17)-1));
            d2 = (sx & ((1<<17)-1)) * ((sy>>17) & ((1<<24)-1));
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_K:
            //total operation is: (X(16 downto 0) * Y(23 downto 0) + (X(33 downto 17) * Y(23 downto 0) << 17)
            d1 = (sx & ((1<<17)-1)) * (sy & ((1<<24)-1));
            d2 = ((sx>>17) & ((1<<17)-1)) * (sy & ((1<<24)-1));
            break;
        case BaseMultiplierDSPSuperTilesXilinx::SHAPE_L:
            //total operation is: ((X(23 downto 7) * Y(23 downto 0) << 7) + (X(16 downto 0) * Y(47 downto 24) << 24)
            //realized as:        (X(23 downto 7) * Y(23 downto 0)) + (X(16 downto 0) * Y(47 downto 24) << 17)
            d1 = ((sx>>7) & ((1<<17)-1)) * (sy & ((1<<24)-1));
            d2 = (sx & ((1<<17)-1)) * ((sy>>24) & ((1<<24)-1));
            break;
        default:
            throw string("Error: shape unknown");
            break;
    }

    if((shape >= BaseMultiplierDSPSuperTilesXilinx::SHAPE_A) && (shape <= BaseMultiplierDSPSuperTilesXilinx::SHAPE_D))
    {
        //tilings (a) to (d) doesn't have a shift
        t = d1 + d2;
    } else {
        //tilings (e) to (l) have a 17 bit shift
        t = (d2 + ((d1 >> 17)&0xFFFFFF) << 17) + (d1&0x1FFFF);
    }

	tc->addExpectedOutput("R",t);
}


}   //end namespace flopoco

