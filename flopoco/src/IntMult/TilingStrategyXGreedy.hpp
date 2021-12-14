#ifndef FLOPOCO_TILINGSTRATEGYXGREEDY_HPP
#define FLOPOCO_TILINGSTRATEGYXGREEDY_HPP

#include "TilingStrategyGreedy.hpp"

namespace flopoco {
    class TilingStrategyXGreedy : public TilingStrategyGreedy {
    public:
        TilingStrategyXGreedy(
                unsigned int wX,
                unsigned int wY,
                unsigned int wOut,
                bool signedIO,
                BaseMultiplierCollection* bmc,
                base_multiplier_id_t prefered_multiplier,
                float occupation_threshold,
                size_t maxPrefMult,
                bool useIrregular,
                bool use2xk,
                bool useSuperTiles,
                bool useKaratsuba,
                MultiplierTileCollection& tiles);
        void solve() override;

    private:
        vector<pair<unsigned int, unsigned int>> pairs_;
        void swapTiles(pair<unsigned int, unsigned int>);
    };
}


#endif
