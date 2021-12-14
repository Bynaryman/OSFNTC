#ifndef FLOPOCO_MULTIPLIERTILECOLLECTION_HPP
#define FLOPOCO_MULTIPLIERTILECOLLECTION_HPP
#include "BaseMultiplierCategory.hpp"
#include "BaseMultiplierCollection.hpp"

namespace flopoco {
    class MultiplierTileCollection {

    public:
        MultiplierTileCollection(Target *target, BaseMultiplierCollection* bmc, int mult_wX, int mult_wY, bool superTile, bool use2xk, bool useirregular, bool useLUT, bool useDSP, bool useKaratsuba);
        vector<BaseMultiplierCategory*> MultTileCollection;
        vector<BaseMultiplierCategory*> BaseTileCollection;
        vector<BaseMultiplierCategory*> VariableYTileCollection;
        vector<BaseMultiplierCategory*> VariableXTileCollection;
        vector<BaseMultiplierCategory*> SuperTileCollection;
        unsigned int variableTileOffset;

        static BaseMultiplierCategory *
        superTileSubtitution(vector<BaseMultiplierCategory*> mtc, int rx1, int ry1, int lx1, int ly1, int rx2, int ry2, int lx2, int ly2);

    private:
        void addBaseTile(BaseMultiplierCategory* mult);
        void addVariableXTile(BaseMultiplierCategory* mult);
        void addVariableYTile(BaseMultiplierCategory* mult);
        void addSuperTile(BaseMultiplierCategory* mult);
    };
}
#endif //FLOPOCO_MULTIPLIERTILECOLLECTION_HPP
