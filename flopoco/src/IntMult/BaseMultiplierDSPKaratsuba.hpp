#ifndef FLOPOCO_BaseMultiplierDSPKaratsuba_HPP
#define FLOPOCO_BaseMultiplierDSPKaratsuba_HPP

#include <string>
#include <iostream>
#include <string>
#include <gmp.h>
#include <gmpxx.h>
#include "mpfr.h"
#include "Target.hpp"
#include "Operator.hpp"
#include "BitHeap/BitHeap.hpp"
#include "Table.hpp"
#include "BaseMultiplierCategory.hpp"

namespace flopoco {


    class BaseMultiplierDSPKaratsuba : public BaseMultiplierCategory
    {
    public:

        BaseMultiplierDSPKaratsuba(
                int size,
                int wX,
                int wY
        ) : BaseMultiplierCategory{
        get_wX(size, wX, wY),
        get_wY(size, wX, wY),
        false,
        false,
        size,
        "BaseMultiplierDSPKaratsuba_size" + to_string(size),
        true,
        get_output_weights(size, wX, wY)
        }, wX(wX), wY(wY), wR(wX+wY), n(size){}

        static int get_output_count(int n, int wX, int wY);
        static vector<int> get_output_weights(int n, int wX, int wY);
        static int get_wX(int n, int wX, int wY) {return fpr(wX, wY, gcd(wX, wY))*n+wX;}
        static int get_wY(int n, int wX, int wY) {return fpr(wX, wY, gcd(wX, wY))*n+wY;}
        static int get_wR(int n, int wX, int wY) {return get_wX(n, wX, wY) + get_wY(n, wX, wY);}
        static int getRelativeResultMSBWeight(int n, int wX, int wY) {return get_wR(n, wX, wY);}
        static int getRelativeResultLSBWeight(int n, int wX, int wY) {return 0;}
        float shape_utilisation(int shape_x, int shape_y, int mult_wX, int mult_wY, bool signedIO) override;
        int getDSPCost() const final;
        double getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY);
        int ownLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY);
        bool shapeValid(const Parametrization &param, unsigned int x, unsigned int y) const;
        bool shapeValid(int x, int y);
        bool isKaratsuba() const override { return true;}
        unsigned getArea(void) override {return (n+1)*wX*(n+1)*wY;}

        Operator *generateOperator(Operator *parentOp, Target *target, Parametrization const & params) const final;

        /** Factory method */
        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
        /** Register the factory */
        static void registerFactory();

        static int gcd(int a, int b) { return b == 0 ? a : gcd(b, a % b);}
    private:
        int wX, wY, wR, n;
        static long fpr(int wX, int wY, int gcd) {long kxy = gcd; for(; kxy % wX || kxy % wY; kxy += gcd); return kxy;}

    };

    class BaseMultiplierDSPKaratsubaOp : public Operator
    {
    public:

        BaseMultiplierDSPKaratsubaOp(Operator *parentOp, Target* target, int wX, int wY, int k);
        void emulate(TestCase * tc);
    protected:
        BitHeap *bitHeap;
    private:
        int wX, wY, wR, n, dsp_cnt = 0;
        bool child_op;
        static int gcd(int a, int b) { return b == 0 ? a : gcd(b, a % b);}

        int TileBaseMultiple;
        void createRectKaratsuba(int i, int j);
        void createMult(int i, int j);
    };

}
#endif //FLOPOCO_BaseMultiplierDSPKaratsuba_HPP
