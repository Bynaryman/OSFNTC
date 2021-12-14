#include "TilingStrategyGreedy.hpp"
#include "BaseMultiplierIrregularLUTXilinx.hpp"
#include "BaseMultiplierXilinx2xk.hpp"
#include "LineCursor.hpp"
#include "NearestPointCursor.hpp"
#include "IntMultiplier.hpp"

#include <cstdlib>
#include <ctime>

namespace flopoco {
    TilingStrategyGreedy::TilingStrategyGreedy(
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
            MultiplierTileCollection& tiles):TilingStrategy(wX, wY, wOut, signedIO, bmc),
                                prefered_multiplier_{prefered_multiplier},
                                occupation_threshold_{occupation_threshold},
                                max_pref_mult_{maxPrefMult},
                                useIrregular_{useIrregular},
                                use2xk_{use2xk},
                                useSuperTiles_{useSuperTiles},
                                useKaratsuba_{useKaratsuba},
                                tileCollection_{tiles}
    {
        //copy vector
        tiles_ = tiles.BaseTileCollection;

        //sort base tiles
        std::sort(tiles_.begin(), tiles_.end(), [](BaseMultiplierCategory* a, BaseMultiplierCategory* b) -> bool { return a->efficiency() > b->efficiency(); });

        //insert 2k and k2 tiles after dsp tiles and before normal tiles
        if(use2xk) {
            for (unsigned int i = 0; i < tiles_.size(); i++) {
                if (tiles_[i]->getDSPCost() == 0) {
                    tiles_.insert(tiles_.begin() + i, new BaseMultiplierXilinx2xk(2, INT32_MAX));
                    tiles_.insert(tiles_.begin() + i, new BaseMultiplierXilinx2xk(INT32_MAX, 2));
                    break;
                }
            }
        }

        truncated_ = wOut != wX + wY;
        if(truncated_) {
            truncatedRange_ = (IntMultiplier::prodsize(wX, wY) - 1) - wOut;
        }
    }

    void TilingStrategyGreedy::solve() {
        NearestPointCursor fieldState;
        Field field(wX, wY, signedIO, fieldState);

        if(truncated_) {
            field.setTruncated(truncatedRange_, fieldState);
        }

        double cost = 0.0;
        unsigned int area = 0;
        unsigned int usedDSPBlocks = 0;
        //only one state, base state is also current state
        greedySolution(fieldState, &solution, nullptr, cost, area, usedDSPBlocks);
        cout << "Total cost: " << cost << " " << usedDSPBlocks << endl;
        cout << "Total area: " << area << endl;
    }

    bool TilingStrategyGreedy::greedySolution(BaseFieldState& fieldState, list<mult_tile_t>* solution, queue<unsigned int>* path, double& cost, unsigned int& area, unsigned int& usedDSPBlocks, double cmpCost, vector<tuple<BaseMultiplierCategory*, BaseMultiplierParametrization, multiplier_coordinates_t>>* dspBlocks) {
        Field* field = fieldState.getField();
        Cursor next (fieldState.getCursor());
        Cursor placementPos = next;
        Cursor possiblePlacementPos = next;
        double tempCost = cost;
        unsigned int tempArea = area;

        vector<tuple<BaseMultiplierCategory*, BaseMultiplierParametrization, multiplier_coordinates_t>> tmpBlocks;
        if(dspBlocks == nullptr) {
            dspBlocks = &tmpBlocks;
        }

        while(fieldState.getMissing() > 0) {
            unsigned int neededX = field->getMissingLine(fieldState);
            unsigned int neededY = field->getMissingHeight(fieldState);

            BaseMultiplierCategory* bm = tiles_[tiles_.size() - 1];
            BaseMultiplierParametrization tile = bm->getParametrisation().tryDSPExpand(next.first, next.second, wX, wY, signedIO);
            double efficiency = -1.0f;
            unsigned int tileIndex = tiles_.size() - 1;

            for(unsigned int i = 0; i < tiles_.size(); i++) {
                BaseMultiplierCategory *t = tiles_[i];

                if (t->getDSPCost() + usedDSPBlocks > max_pref_mult_) {
                    //all available dsp blocks got already used
                    continue;
                }

                if (use2xk_ && t->isVariable()) {
                    //no need to compare it to a dsp block / if there is not enough space anyway
                    if (efficiency < 0 && ((neededX >= 5 && neededY >= 2) || (neededX >= 2 && neededY >= 5))) {
                        t = nullptr;
                        unsigned int idx = i;
                        if(neededX > neededY) {
                            t = tileCollection_.VariableXTileCollection[neededX - tileCollection_.variableTileOffset];
                            idx = i + 1;
                        }
                        else {
                            t = tileCollection_.VariableYTileCollection[neededY - tileCollection_.variableTileOffset];
                            idx = i;
                        }

                        unsigned int tiles = field->checkTilePlacement(next, t, fieldState);
                        if (tiles == 0) {
                            continue;
                        }

                        bm = t;
                        tileIndex = idx;
                        tile = bm->getParametrisation();
                        placementPos = next;
                        break;
                    }
                }

                //check size for "normal", rectangular tiles (avoids unnecessary placement checks)
                if (t->getDSPCost() == 0 && !t->isIrregular() && (t->wX() > neededX || t->wY() > neededY)) {
                    continue;
                }

                if(t->isKaratsuba() && (((unsigned int)wX < t->wX() + next.first) || ((unsigned int)wY < t->wY() + next.second))) {
                    continue;
                }

                if(t->getDSPCost() == 1 && !t->isKaratsuba()) {
                    BaseMultiplierParametrization param = field->checkDSPPlacement(next, t, fieldState, neededX, neededY);
                    if(param.getMultXWordSize() == 0 || param.getMultYWordSize() == 0) {
                        continue;
                    }

                    unsigned int tiles = param.getMultXWordSize() * param.getMultYWordSize();

                    double usage = tiles / (double) t->getArea();
                    //check threshold
                    if (usage < occupation_threshold_) {
                        continue;
                    }

                    //TODO: find a better way for this, think about the effect of > vs >= here
                    if (tiles > efficiency) {
                        efficiency = tiles;
                    } else {
                        //no need to check anything else ... dsp block wasn't enough
                        break;
                    }

                    tile = param;
                    possiblePlacementPos = next;
                }
                else {
                    unsigned int tiles = field->checkTilePlacement(next, t, fieldState);
                    if (tiles == 0) {
                        continue;
                    }

                    possiblePlacementPos = next;

                    if(t->isIrregular()) {
                        //try to settle irregular tiles
                        bool didOp = false;
                        do {
                            didOp = false;
                            // down
                            possiblePlacementPos.second -= 1;
                            unsigned int newTiles = field->checkTilePlacement(possiblePlacementPos, t, fieldState);
                            if(newTiles < tiles) {
                                possiblePlacementPos.second += 1;
                            }
                            else {
                                didOp = true;
                                tiles = newTiles;
                            }

                            // right
                            possiblePlacementPos.first -= 1;
                            newTiles = field->checkTilePlacement(possiblePlacementPos, t, fieldState);
                            if(newTiles < tiles) {
                                possiblePlacementPos.first += 1;
                            }
                            else {
                                didOp = true;
                                tiles = newTiles;
                            }
                        } while(didOp);
                    }

                    if(t->isKaratsuba()) {
                        if (tiles > efficiency) {
                            efficiency = tiles;
                        } else {
                            break;
                        }
                    }
                    else {
                        double newEfficiency = t->efficiency() * (tiles / (double) t->getArea());
                        if (newEfficiency < efficiency) {
                            if (tiles == t->getArea()) {
                                //this tile wasn't able to compete with the current best tile even if it is used completely ... so checking the rest makes no sense
                                break;
                            }
                            continue;
                        }

                        efficiency = newEfficiency;

                        //no need to check other tiles
                        /*if(tiles == t->getArea()) {
                            break;
                        }*/
                    }

                    tile = t->getParametrisation();
                }

                bm = t;
                tileIndex = i;
                placementPos = possiblePlacementPos;
            }

            if(path != nullptr) {
                path->push(tileIndex);
            }

            int dsps = bm->getDSPCost();

            if(dsps == 1 && !bm->isKaratsuba()) {
                tile = tile.tryDSPExpand(next.first, next.second, wX, wY, signedIO);
            }

            auto coord (field->placeTileInField(placementPos, bm, tile, fieldState));
            if(dsps > 0) {
                usedDSPBlocks += dsps;
                //only add normal dspblocks for supertile pass
                if(dsps == 1 && useSuperTiles_ && !bm->isKaratsuba()) {
                    dspBlocks->push_back(make_tuple(bm, tile, placementPos));
                    next = coord;
                    continue;
                }
            }
            else {
                tempArea += bm->getArea();
            }

            tempCost += bm->getLUTCost(placementPos.first, placementPos.second, wX, wY);

            if(tempCost > cmpCost) {
                return false;
            }

            if(solution != nullptr) {
                //only try to expand dsp tiles
                solution->push_back(make_pair(tile, next));
            }

            next = coord;
        }

        //check each dsp block with another
        if(useSuperTiles_) {
            if(!performSuperTilePass(dspBlocks, solution, tempCost, cmpCost)) {
                return false;
            }

            for(auto& tile: *dspBlocks) {
                unsigned int x = std::get<2>(tile).first;
                unsigned int y = std::get<2>(tile).second;

                if(solution != nullptr) {
                    solution->push_back(make_pair(std::get<1>(tile).tryDSPExpand(x, y, wX, wY, signedIO), std::get<2>(tile)));
                }

                tempCost += std::get<0>(tile)->getLUTCost(x, y, wX, wY);

                if(tempCost > cmpCost) {
                    return false;
                }
            }
        }

        cost = tempCost;
        area = tempArea;
        return true;
    }

    bool TilingStrategyGreedy::performSuperTilePass(vector<tuple<BaseMultiplierCategory*, BaseMultiplierParametrization, multiplier_coordinates_t>>* dspBlocks, list<mult_tile_t>* solution, double& cost, double cmpCost) {
        if(dspBlocks->size() == 1) {
            return true;
        }

        vector<tuple<BaseMultiplierCategory*, BaseMultiplierParametrization, multiplier_coordinates_t>> tempBlocks;
        tempBlocks = std::move(*dspBlocks);
        dspBlocks->clear();

        //TODO: reuse vector (init with max dsp)
        vector<bool> used(tempBlocks.size(), false);

        for(unsigned int i = 0; i < tempBlocks.size(); i++) {
            if(used[i]) {
                continue;
            }

            bool found = false;
            auto &dspBlock1 = tempBlocks[i];
            unsigned int coordX1 = std::get<2>(dspBlock1).first;
            unsigned int coordY1 = std::get<2>(dspBlock1).second;
            unsigned int sizeX1 = wX - std::get<1>(dspBlock1).getMultXWordSize() == coordX1 ? std::get<0>(dspBlock1)->wX_DSPexpanded(coordX1, coordY1, wX, wY, signedIO) : std::get<1>(dspBlock1).getMultXWordSize();
            unsigned int sizeY1 = wY - std::get<1>(dspBlock1).getMultYWordSize() == coordY1 ? std::get<0>(dspBlock1)->wY_DSPexpanded(coordX1, coordY1, wX, wY, signedIO) : std::get<1>(dspBlock1).getMultYWordSize();

            for (unsigned int j = i + 1; j < tempBlocks.size(); j++) {
                if(used[j]) {
                    continue;
                }

                auto &dspBlock2 = tempBlocks[j];

                unsigned int coordX2 = std::get<2>(dspBlock2).first;
                unsigned int coordY2 = std::get<2>(dspBlock2).second;
                unsigned int sizeX2 = wX - std::get<1>(dspBlock2).getMultXWordSize() == coordX2 ? std::get<0>(dspBlock2)->wX_DSPexpanded(coordX2, coordY2, wX, wY, signedIO) : std::get<1>(dspBlock2).getMultXWordSize();
                unsigned int sizeY2 = wY - std::get<1>(dspBlock2).getMultYWordSize() == coordY2 ? std::get<0>(dspBlock2)->wY_DSPexpanded(coordX2, coordY2, wX, wY, signedIO) : std::get<1>(dspBlock2).getMultYWordSize();

                Cursor baseCoord;
                baseCoord.first = std::min(coordX1, coordX2);
                baseCoord.second = std::min(coordY1, coordY2);

                int rx1 = coordX1 - baseCoord.first;
                int ry1 = coordY1 - baseCoord.second;
                int lx1 = rx1 + sizeX1 - 1;
                int ly1 = ry1 + sizeY1 - 1;

                int rx2 = coordX2 - baseCoord.first;
                int ry2 = coordY2 - baseCoord.second;
                int lx2 = rx2 + sizeX2 - 1;
                int ly2 = ry2 + sizeY2 - 1;

                BaseMultiplierCategory *tile = MultiplierTileCollection::superTileSubtitution(tileCollection_.SuperTileCollection, rx1, ry1, lx1, ly1, rx2, ry2, lx2, ly2);
                if (tile == nullptr) {
                    continue;
                }

                if (solution != nullptr) {
                    solution->push_back(make_pair(tile->getParametrisation(), baseCoord));
                }

                cost += tile->getLUTCost(baseCoord.first, baseCoord.second, wX, wY);
                if(cost > cmpCost) {
                    return false;
                }

                used[i] = true;
                used[j] = true;
                found = true;
                break;
            }

            if(!found) {
                dspBlocks->push_back(dspBlock1);
            }
        }

        return true;
    }
}