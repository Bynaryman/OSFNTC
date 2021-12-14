#include "TilingStrategyXGreedy.hpp"
#include "LineCursor.hpp"
#include "NearestPointCursor.hpp"
#include <cmath>

namespace flopoco {
    TilingStrategyXGreedy::TilingStrategyXGreedy(
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
            MultiplierTileCollection& tiles):TilingStrategyGreedy(wX, wY, wOut, signedIO, bmc, prefered_multiplier, occupation_threshold, maxPrefMult, useIrregular, use2xk, useSuperTiles, useKaratsuba, tiles)
    {
        //find all paired tiles
        for(unsigned int i = 0; i < tiles_.size(); i++) {
            BaseMultiplierCategory* tile = tiles_[i];
            if(tile->getDSPCost() != 1) {
                break;
            }

            for(unsigned int j = 0; j < tiles_.size(); j++) {
                BaseMultiplierCategory* cmp = tiles_[j];
                if(i == j) {
                    continue;
                }

                if(cmp->getDSPCost() != 1) {
                    break;
                }

                if(cmp->wX() == tile->wY() && cmp->wY() == tile->wX()) {
                    pair<unsigned int, unsigned int> p(std::min(i, j), std::max(i, j));
                    bool unique = true;
                    for(auto& u: pairs_) {
                        if(u.first == p.first && u.second == p.second) {
                            unique = false;
                            break;
                        }
                    }

                    if(unique) {
                        pairs_.push_back(p);
                    }
                }
            }
        }
    };

    void TilingStrategyXGreedy::solve() {
        NearestPointCursor baseState;
        NearestPointCursor tempState;
        Field field(wX, wY, signedIO, baseState);

        if(truncated_) {
            field.setTruncated(truncatedRange_, baseState);
        }

        tempState.reset(baseState);

        list<mult_tile_t> bestSolution;
        double bestCost = 0.0;
        unsigned int bestArea = 0;
        unsigned int bestUsedDSPBlocks = 0;
        greedySolution(tempState, &bestSolution, nullptr, bestCost, bestArea, bestUsedDSPBlocks);
        int runs = std::pow(2, pairs_.size());
        for(int i = 1; i < runs; i++) {
            tempState.reset(baseState);
            int last = i - 1;
            for(unsigned int j = 0; j < pairs_.size(); j++) {
                int cmp = last ^ i;
                int mask =  1 << j;
                if((mask & cmp) != 0) {
                    swapTiles(pairs_[j]);
                }
            }

            list<mult_tile_t> solution;
            double cost = 0;
            unsigned int area = 0;
            unsigned int usedDSPBlocks = 0;
            if(greedySolution(tempState, &solution, nullptr, cost, area, usedDSPBlocks, bestCost)) {
                bestCost = cost;
                bestArea = area;
                bestUsedDSPBlocks = usedDSPBlocks;
                bestSolution = std::move(solution);
            }
        }

        cout << "Total cost: " << bestCost << " " << bestUsedDSPBlocks << endl;
        cout << "Total area: " << bestArea << endl;
        solution = std::move(bestSolution);
    }

    void TilingStrategyXGreedy::swapTiles(pair<unsigned int, unsigned int> p) {
        BaseMultiplierCategory* temp = tiles_[p.first];
        tiles_[p.first] = tiles_[p.second];
        tiles_[p.second] = temp;
    }
}