#pragma once

#include "TilingStrategy.hpp"

#ifdef HAVE_SCALP
#include <ScaLP/Solver.h>
#include <ScaLP/Exception.h>    // ScaLP::Exception
#include <ScaLP/SolverDynamic.h> // ScaLP::newSolverDynamic
#endif //HAVE_SCALP
#include <iomanip>
#include "BaseMultiplier.hpp"
#include "BaseMultiplierDSPSuperTilesXilinx.hpp"
#include "BaseMultiplierIrregularLUTXilinx.hpp"
#include "MultiplierTileCollection.hpp"

namespace flopoco {

/*!
 * The TilingStrategyOptimalILP class
 */
class TilingStrategyOptimalILP : public TilingStrategy
{

public:
    using TilingStrategy::TilingStrategy;
    TilingStrategyOptimalILP(
        unsigned int wX,
        unsigned int wY,
        unsigned int wOut,
        bool signedIO,
        BaseMultiplierCollection* bmc,
		base_multiplier_id_t prefered_multiplier,
		float occupation_threshold,
		size_t maxPrefMult,
        MultiplierTileCollection tiles_);

    void solve() override;

private:
    base_multiplier_id_t small_tile_mult_;
    size_t numUsedMults_;
    size_t max_pref_mult_;
    float occupation_threshold_;
    int dpX, dpY, dpS, wS;
    vector<BaseMultiplierCategory*> tiles;
#ifdef HAVE_SCALP
    void constructProblem();

    ScaLP::Solver *solver;
#endif
};

}
