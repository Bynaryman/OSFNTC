#include <utility>

#include "TilingStrategyBeamSearch.hpp"
#include "LineCursor.hpp"
#include "NearestPointCursor.hpp"

namespace flopoco {
    TilingStrategyBeamSearch::TilingStrategyBeamSearch(
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
            MultiplierTileCollection& tiles,
            unsigned int beamRange
            ):TilingStrategyGreedy(wX, wY, wOut, signedIO, bmc, prefered_multiplier, occupation_threshold, maxPrefMult, useIrregular, use2xk, useSuperTiles, useKaratsuba, tiles),
            beamRange_{beamRange}
    {

    };

    void TilingStrategyBeamSearch::solve() {
        NearestPointCursor baseState;
        NearestPointCursor tempState;
        Field field(wX, wY, signedIO, baseState);

        unsigned int usedDSPBlocks = 0;
        unsigned int range = beamRange_;
        queue<unsigned int> path;

        double bestCost = 0.0;
        unsigned int bestArea = 0;

        if(truncated_) {
            field.setTruncated(truncatedRange_, baseState);
        }

        tempState.reset(baseState);

        greedySolution(tempState, nullptr, &path, bestCost, bestArea, usedDSPBlocks);
        unsigned int next = path.front();
        unsigned int lastPath = next;
        path.pop();

        double currentTotalCost = 0.0;
        unsigned int currentArea = 0;
        usedDSPBlocks = 0;
        vector<tuple<BaseMultiplierCategory*, BaseMultiplierParametrization, multiplier_coordinates_t>> dspBlocks;

        while(baseState.getMissing() > 0) {
            unsigned int minIndex = std::max(0, (int)next - (int)range);
            unsigned int maxIndex = std::min((unsigned int)tiles_.size() - 1, next + range);

            unsigned int neededX = field.getMissingLine(baseState);
            unsigned int neededY = field.getMissingHeight(baseState);

            lastPath = next;
            BaseMultiplierCategory* tile = tiles_[next];

            for (unsigned int i = minIndex; i <= maxIndex; i++) {
                //check if we got the already calculated greedy path
                if (i == lastPath) {
                    continue;
                }

                BaseMultiplierCategory* t = tiles_[i];

                tempState.reset(baseState);
                queue<unsigned int> tempPath;
                unsigned int tempUsedDSPBlocks = usedDSPBlocks;
                double tempCost = currentTotalCost;
                unsigned int tempArea = currentArea;

                vector<tuple<BaseMultiplierCategory*, BaseMultiplierParametrization, multiplier_coordinates_t>> localDSPBlocks = dspBlocks;

                if (placeSingleTile(tempState, tempUsedDSPBlocks, nullptr, neededX, neededY, t, tempCost, tempArea, bestCost, localDSPBlocks)) {
                    if(greedySolution(tempState, nullptr, &tempPath, tempCost, tempArea, tempUsedDSPBlocks, bestCost, &localDSPBlocks)) {
                        bestCost = tempCost;
                        bestArea = tempArea;
                        path = move(tempPath);
                        tile = t;
                    }
                }
            }

            //place single tile
            placeSingleTile(baseState, usedDSPBlocks, &solution, neededX, neededY, tile, currentTotalCost, currentArea, FLT_MAX, dspBlocks);

            if(path.size() > 0) {
                next = path.front();
                path.pop();
            }
        }

        if(useSuperTiles_) {
            performSuperTilePass(&dspBlocks, &solution, currentTotalCost);

            for(auto& tile: dspBlocks) {
                unsigned int x = std::get<2>(tile).first;
                unsigned int y = std::get<2>(tile).second;

                solution.push_back(make_pair(std::get<1>(tile).tryDSPExpand(x, y, wX, wY, signedIO), std::get<2>(tile)));

                currentTotalCost += std::get<0>(tile)->getLUTCost(x, y, wX, wY);
            }
        }

        cout << "Total cost: " << currentTotalCost << " " << usedDSPBlocks << endl;
        cout << "Total area: " << currentArea << endl;
    }

    bool TilingStrategyBeamSearch::placeSingleTile(BaseFieldState& fieldState, unsigned int& usedDSPBlocks, list<mult_tile_t>* solution, const unsigned int neededX, const unsigned int neededY, BaseMultiplierCategory* tile, double& cost, unsigned int& area, double cmpCost, vector<tuple<BaseMultiplierCategory*, BaseMultiplierParametrization, multiplier_coordinates_t>>& dspBlocks) {
        int dspBlockCnt = tile->getDSPCost();

        if(usedDSPBlocks + dspBlockCnt > max_pref_mult_) {
            return false;
        }

        if(dspBlockCnt == 0 && !tile->isVariable() && !tile->isIrregular() && (tile->wX() > neededX || tile->wY() > neededY)) {
            return false;
        }


        if(tile->isVariable()) {
            if(!(neededX >= 5 && neededY >= 2) && !(neededX >= 2 && neededY >= 5)) {
                return false;
            }

            if(neededX > neededY) {
                tile = tileCollection_.VariableXTileCollection[neededX - tileCollection_.variableTileOffset];
            }
            else {
                tile = tileCollection_.VariableYTileCollection[neededY - tileCollection_.variableTileOffset];
            }
        }

        Field* field = fieldState.getField();
        auto next (fieldState.getCursor());

        if(tile->isKaratsuba() && (((unsigned int)wX < tile->wX() + next.first) || ((unsigned int)wY < tile->wY() + next.second))) {
            return false;
        }

        BaseMultiplierParametrization param = tile->getParametrisation();
        if(dspBlockCnt == 1 && !tile->isKaratsuba()) {
            param = field->checkDSPPlacement(next, tile, fieldState, neededX, neededY);
            if(param.getMultXWordSize() == 0 || param.getMultYWordSize() == 0) {
                return false;
            }

            unsigned int tiles = param.getMultXWordSize() * param.getMultYWordSize();

            double usage = tiles / (double) tile->getArea();
            //check threshold
            if (usage < occupation_threshold_) {
                return false;
            }

            if(tiles == tile->getArea()) {
                param = tile->getParametrisation().tryDSPExpand(next.first, next.second, wX, wY, signedIO);
            }
        }
        else {
            unsigned int tiles = field->checkTilePlacement(next, tile, fieldState);
            if (tiles == 0) {
                return false;
            }

            if (tile->isIrregular()) {
                //try to settle irregular tiles
                bool didOp = false;
                do {
                    didOp = false;
                    // down
                    next.second -= 1;
                    unsigned int newTiles = field->checkTilePlacement(next, tile, fieldState);
                    if (newTiles < tiles) {
                        next.second += 1;
                    } else {
                        didOp = true;
                        tiles = newTiles;
                    }

                    // right
                    next.first -= 1;
                    newTiles = field->checkTilePlacement(next, tile, fieldState);
                    if (newTiles < tiles) {
                        next.first += 1;
                    } else {
                        didOp = true;
                        tiles = newTiles;
                    }
                } while (didOp);
            }
        }
        usedDSPBlocks += dspBlockCnt;

        auto coord (field->placeTileInField(next, tile, param, fieldState));

        if(useSuperTiles_ && tile->getDSPCost() == 1 && !tile->isKaratsuba()) {
            dspBlocks.push_back(make_tuple(tile, param, next));
            return true;
        }

        cost += tile->getLUTCost(next.first, next.second, wX, wY);

        if(dspBlockCnt == 0) {
            area += tile->getArea();
        }

        if(cost > cmpCost) {
            return false;
        }

        if(solution == nullptr) {
            return true;
        }

        // only try to expand dsp tiles
        solution->push_back(make_pair(param, next));

        return true;
    }
}