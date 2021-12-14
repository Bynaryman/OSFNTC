#include "Operator.hpp"
#include "BaseMultiplierCategory.hpp"

namespace flopoco
{

    class BaseMultiplierIrregularLUTXilinx : public BaseMultiplierCategory
    {
    public:
        /**
    	* \brief The shape enum.
         *
         *  _ _     _        _ _       _     _ _ _    _ _      _ _     _
         * |_|_|_  |_|_     |_|_|_    |_|_  |_|_|_|  |_|_|_   |_|_|   |_|_
         * |_|_|_| |_|_|_     |_|_|   |_|_|   |_|_|  |_|_|_|  |_|_|   |_|_|
         * |_|_|_|   |_|_|              |_|                     |_|   |_|_|
         * shape A  shape B shape C shape D shape E  shape F shape G  shape H
        **/
        enum TILE_SHAPE{
            SHAPE_A =  1, //shape (a)
            SHAPE_B =  2, //shape (b)
            SHAPE_C =  3, //shape (c)
            SHAPE_D =  4, //shape (d)
            SHAPE_E =  5, //shape (e)
            SHAPE_F =  6, //shape (f)
            SHAPE_G =  7, //shape (e)
            SHAPE_H =  8, //shape (f)
        };

        BaseMultiplierIrregularLUTXilinx(
                TILE_SHAPE shape
        ) : BaseMultiplierCategory{
                get_wX(shape),
                get_wY(shape),
                false,
                false,
                shape,
                "BaseMultiplierIrregularLUTXilinx_" + string(1,((char) shape) + '1' - 1),
                false
        }{
            this->shape = shape;
            this->wX = get_wX(shape);
            this->wY = get_wY(shape);
        }

        bool isIrregular() const override { return true;}
        int getDSPCost() const final {return 0;}
        double getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY);
        int ownLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY);
        static int get_wX(BaseMultiplierIrregularLUTXilinx::TILE_SHAPE shape) {return shape_size[(int)shape-1][0];}
        static int get_wY(BaseMultiplierIrregularLUTXilinx::TILE_SHAPE shape) {return shape_size[(int)shape-1][1];}
        static int get_wR(BaseMultiplierIrregularLUTXilinx::TILE_SHAPE shape) {return shape_size[(int)shape-1][2];}
        static int getRelativeResultMSBWeight(BaseMultiplierIrregularLUTXilinx::TILE_SHAPE shape) {return shape_size[(int)shape-1][3];}
        static int getRelativeResultLSBWeight(BaseMultiplierIrregularLUTXilinx::TILE_SHAPE shape) {return shape_size[(int)shape-1][4];}
        //int getArea(BaseMultiplierIrregularLUTXilinx::TILE_SHAPE shape) const {return shape_size[(int)shape-1][5];}
        unsigned getArea(void) override {return shape_size[(int)shape-1][5];}
        int getRelativeResultLSBWeight(Parametrization const& param) const;
        int getRelativeResultMSBWeight(Parametrization const& param) const;
        bool shapeValid(int x, int y);
        bool shapeValid(Parametrization const & param, unsigned x, unsigned y) const;

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
        static const int shape_size[8][6];
    };

    class BaseMultiplierIrregularLUTXilinxOp : public Operator {
    public:
        BaseMultiplierIrregularLUTXilinxOp(Operator *parentOp, Target* target, bool isSignedX, bool isSignedY,
                BaseMultiplierIrregularLUTXilinx::TILE_SHAPE shape);

        void emulate(TestCase* tc);
        static TestList unitTest(int index);
        static int get_pattern(BaseMultiplierIrregularLUTXilinx::TILE_SHAPE shape) { return bit_pattern[(int)shape-1];};

    private:
        BaseMultiplierIrregularLUTXilinx::TILE_SHAPE shape;
        int wX, wY;
        bool isSignedX, isSignedY;
        static const unsigned short bit_pattern[8];
        mpz_class function(int xy);
    };

}
