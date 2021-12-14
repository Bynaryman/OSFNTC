#ifndef BaseMultiplierDSPSuperTilesXilinx_HPP
#define BaseMultiplierDSPSuperTilesXilinx_HPP

#include <string>
#include <iostream>
#include <string>
#include <gmp.h>
#include <gmpxx.h>
#include "mpfr.h"
#include "Target.hpp"
#include "Operator.hpp"
#include "Table.hpp"
#include "BaseMultiplierCategory.hpp"

namespace flopoco {

	/**
	* \brief Implementation of all super tiles of size 2 for Xilinx FPGAs according to "Resource Optimal Design of Large Multipliers for FPGAs", Martin Kumm & Johannes Kappauf
    **/

    class BaseMultiplierDSPSuperTilesXilinx : public BaseMultiplierCategory
    {
    public:
        /**
    	* \brief The shape enum. For their definition, see Fig. 5 of "Resource Optimal Design of Large Multipliers for FPGAs", Martin Kumm & Johannes Kappauf
        **/
        enum TILE_SHAPE{
            SHAPE_A =  1, //shape (a)
            SHAPE_B =  2, //shape (b)
            SHAPE_C =  3, //shape (c)
            SHAPE_D =  4, //shape (d)
            SHAPE_E =  5, //shape (e)
            SHAPE_F =  6, //shape (f)
            SHAPE_G =  7, //shape (g)
            SHAPE_H =  8, //shape (h)
            SHAPE_I =  9, //shape (i)
            SHAPE_J = 10, //shape (j)
            SHAPE_K = 11, //shape (k)
            SHAPE_L = 12  //shape (l)
        };

        BaseMultiplierDSPSuperTilesXilinx(
				TILE_SHAPE shape
			) : BaseMultiplierCategory{
				get_wX(shape),
				get_wY(shape),
				false,
				false,
				shape,
				"BaseMultiplierDSPSuperTilesXilinx_" + string(1,((char) shape) + 'A' - 1),
				false
		}{
		    this->shape = shape;
            this->wX = get_wX(shape);
            this->wY = get_wY(shape);
            this->wR = get_wR(shape);
		}

        bool isIrregular() const override { return true;}
        int getDSPCost() const override { return 2; }
        unsigned getArea() {return 2*24*17;}
        static int get_wX(BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE shape) {return shape_size[(int)shape-1][0];}
        static int get_wY(BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE shape) {return shape_size[(int)shape-1][1];}
        static int get_wR(BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE shape) {return shape_size[(int)shape-1][2];}
        static int getRelativeResultMSBWeight(BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE shape) {return shape_size[(int)shape-1][3];}
        static int getRelativeResultLSBWeight(BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE shape) {return shape_size[(int)shape-1][4];}
        double getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY);
        int getRelativeResultLSBWeight(Parametrization const& param) const;
        int getRelativeResultMSBWeight(Parametrization const& param) const;
		bool shapeValid(int x, int y);
        bool shapeValid(Parametrization const & param, unsigned x, unsigned y) const;
        float shape_utilisation(int shape_x, int shape_y, int wX, int wY, bool signedIO) override;
        int isSuperTile(int rx1, int ry1, int lx1, int ly1, int rx2, int ry2, int lx2, int ly2);

		Operator *generateOperator(Operator *parentOp, Target *target, Parametrization const & params) const final;

        /** Factory method */
        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
        /** Register the factory */
        static void registerFactory();


    private:
        TILE_SHAPE shape;
        int wX, wY, wR;
        bool xIsSigned_;
        bool yIsSigned_;
        static const int shape_size[12][5];

        struct tile_coords{
            int dsp1_rx;
            int dsp1_ry;
            int dsp1_lx;
            int dsp1_ly;
            int dsp2_rx;
            int dsp2_ry;
            int dsp2_lx;
            int dsp2_ly;
        };

        tile_coords mult_bounds[12] = {{0, 17, 23, 33, 17,  0, 40, 16},     //A
                                       {0, 17, 16, 40, 17,  0, 40, 16},     //B
                                       {0, 24, 23, 40, 24,  0, 40, 23},     //C
                                       {0, 17, 16, 40, 17,  0, 33, 23},     //D
                                       {0,  0, 23, 16,  0, 17, 23, 33},     //E
                                       {0,  7, 23, 23, 24,  0, 47, 16},     //F
                                       {7,  0, 23, 23,  0, 24, 23, 40},     //G
                                       {0,  0, 16, 23, 17,  0, 40, 16},     //H
                                       {0,  7, 23, 23, 24,  0, 40, 23},     //I
                                       {0,  0, 23, 16,  0, 17, 16, 40},     //J
                                       {0,  0, 16, 23, 17,  0, 33, 23},     //K
                                       {7,  0, 23, 23,  0, 24, 16, 47}};    //L

    };

    class BaseMultiplierDSPSuperTilesXilinxOp : public Operator
    {
    public:

        BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE shape;
        bool pipelineDSPs;
		BaseMultiplierDSPSuperTilesXilinxOp(Operator *parentOp, Target* target, bool isSignedX, bool isSignedY, BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE shape, bool pipelineDSPs);
		void emulate(TestCase * tc);

    private:
        //BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE shape;
    };
}
#endif
