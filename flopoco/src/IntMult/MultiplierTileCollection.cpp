#include "MultiplierTileCollection.hpp"
#include "BaseMultiplierDSP.hpp"
#include "BaseMultiplierLUT.hpp"
#include "BaseMultiplierXilinx2xk.hpp"
#include "BaseMultiplierIrregularLUTXilinx.hpp"
#include "BaseMultiplierDSPSuperTilesXilinx.hpp"
#include "BaseMultiplierDSPKaratsuba.hpp"

using namespace std;
namespace flopoco {

    MultiplierTileCollection::MultiplierTileCollection(Target *target, BaseMultiplierCollection *bmc, int mult_wX, int mult_wY, bool superTile, bool use2xk, bool useirregular, bool useLUT, bool useDSP, bool useKaratsuba) {
        //cout << bmc->size() << endl;
        if(useDSP) {
            addBaseTile(new BaseMultiplierDSP(24, 17, 1));
            addBaseTile(new BaseMultiplierDSP(17, 24, 1));
        }

        if(useLUT) {
            addBaseTile(new BaseMultiplierLUT(3, 3));
            addBaseTile(new BaseMultiplierLUT(2, 3));
            addBaseTile(new BaseMultiplierLUT(3, 2));
            addBaseTile(new BaseMultiplierLUT(1, 2));
            addBaseTile(new BaseMultiplierLUT(2, 1));
            addBaseTile(new BaseMultiplierLUT(1, 1));
        }

        if(superTile){
            for(int i = 1; i <= 12; i++) {
                addSuperTile(new BaseMultiplierDSPSuperTilesXilinx((BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE) i));
            }
        }

        if(use2xk){
            variableTileOffset = 4;
            for(int x = variableTileOffset; x <= mult_wX; x++) {
                addVariableXTile(new BaseMultiplierXilinx2xk(x, 2));
            }

            for(int y = variableTileOffset; y <= mult_wY; y++) {
                addVariableYTile(new BaseMultiplierXilinx2xk(2, y));
            }
        }

        if(useirregular){
            for(int i = 1; i <= 8; i++) {
                addBaseTile(new BaseMultiplierIrregularLUTXilinx((BaseMultiplierIrregularLUTXilinx::TILE_SHAPE) i));
            }
        }

        if(useKaratsuba) {
            unsigned int min = std::min((mult_wX - 16) / 48, (mult_wY - 24) / 48);
            for(unsigned int i = 0; i <= min; i++) {
                addBaseTile(new BaseMultiplierDSPKaratsuba(i, 16, 24));
            }
        }

/*        for(int i = 0; i < (int)bmc->size(); i++)
        {
            cout << bmc->getBaseMultiplier(i).getType() << endl;

            if( (bmc->getBaseMultiplier(i).getType().compare("BaseMultiplierDSPSuperTilesXilinx")) == 0){
                for(int i = 1; i <= 12; i++) {
                    MultTileCollection.push_back(
                            new BaseMultiplierDSPSuperTilesXilinx((BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE) i));
                }
            }

            if( (bmc->getBaseMultiplier(i).getType().compare("BaseMultiplierIrregularLUTXilinx")) == 0){
                for(int i = 1; i <= 8; i++) {
                    MultTileCollection.push_back(
                            new BaseMultiplierIrregularLUTXilinx((BaseMultiplierIrregularLUTXilinx::TILE_SHAPE) i));
                }
            }

        }
*/
//        cout << MultTileCollection.size() << endl;
    }

    void  MultiplierTileCollection::addBaseTile(BaseMultiplierCategory *mult) {
        MultTileCollection.push_back(mult);
        BaseTileCollection.push_back(mult);
    }

    void  MultiplierTileCollection::addSuperTile(BaseMultiplierCategory *mult) {
        MultTileCollection.push_back(mult);
        SuperTileCollection.push_back(mult);
    }

    void  MultiplierTileCollection::addVariableXTile(BaseMultiplierCategory *mult) {
        MultTileCollection.push_back(mult);
        VariableXTileCollection.push_back(mult);
    }

    void  MultiplierTileCollection::addVariableYTile(BaseMultiplierCategory *mult) {
        MultTileCollection.push_back(mult);
        VariableYTileCollection.push_back(mult);
    }

    BaseMultiplierCategory* MultiplierTileCollection::superTileSubtitution(vector<BaseMultiplierCategory*> mtc, int rx1, int ry1, int lx1, int ly1, int rx2, int ry2, int lx2, int ly2){
        for(int i = 0; i < (int)mtc.size(); i++)
        {
            if(mtc[i]->getDSPCost() == 2){
                int id = mtc[i]->isSuperTile(rx1, ry1, lx1, ly1, rx2, ry2, lx2, ly2);
                if(id){
                    return mtc[i+id];
                }
            }
        }
        return nullptr;
    }

}