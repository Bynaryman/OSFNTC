#include "Field.hpp"
#include <iostream>

namespace flopoco {
    Field::Field(unsigned int wX, unsigned int wY, bool signedIO, FieldState& baseState) : wX_(wX), wY_(wY), signedIO_(signedIO), currentStateID_(0U), baseState_{&baseState} {
        field_.resize(wY_);
        for(unsigned int i = 0; i < wY_; i++) {
            field_[i] = vector<ID>(wX_, currentStateID_);
        }
        currentStateID_++;

        initFieldState(baseState);
        baseID_ = baseState.getID();
    }

    Field::Field(const Field &copy) {
        wX_ = copy.wX_;
        wY_ = copy.wY_;

        field_.resize(wY_);
        for(unsigned int i = 0U; i < wY_; i++) {
            field_[i].resize(wX_);
            for(unsigned int j = 0; j < wX_; j++) {
                field_[i][j] = copy.field_[i][j];
            }
        }

        signedIO_ = copy.signedIO_;
    }

    Field::~Field() {
        field_.clear();
    }

    void Field::initFieldState(FieldState& fieldState) {
        fieldState.reset(this, currentStateID_++, wX_ * wY_);
        fieldState.setCursor(0U, 0U);
        fieldState.setField(this);
    }

    void Field::updateStateID(Field::FieldState &fieldState) {
        fieldState.setID(currentStateID_++);
    }

    void Field::reset() {
        initFieldState(*baseState_);
        baseID_ = baseState_->getID();
    }

    BaseMultiplierParametrization Field::checkDSPPlacement(const Cursor coord, BaseMultiplierCategory* tile, FieldState& fieldState, unsigned int maxX, unsigned int maxY) {
        unsigned int sizeX = std::min((unsigned int)tile->wX_DSPexpanded(coord.first, coord.second, wX_, wY_, signedIO_), maxX);
        unsigned int sizeY = std::min((unsigned int)tile->wY_DSPexpanded(coord.first, coord.second, wX_, wY_, signedIO_), maxY);
        ID fieldID = fieldState.getID();

        for (unsigned int i = coord.second; i < sizeY + coord.second; i++) {
            for (unsigned int j = coord.first; j < sizeX + coord.second; j++) {
                if (field_[i][j] == fieldID || field_[i][j] == baseID_) {
                    unsigned int area1 = i - coord.second + 1 * maxY;
                    unsigned int area2 = maxX * j - coord.second + 1;
                    if(area1 > area2) {
                        maxX = i - coord.second + 1;
                    }
                    else {
                        maxY = j - coord.second + 1;
                    }
                }
            }
        }

        for (unsigned int i = coord.second; i < sizeY; i++) {
            for (unsigned int j = coord.first; j < sizeX; j++) {
                if (field_[i][j] == fieldID || field_[i][j] == baseID_) {
                    return tile->parametrize(0, 0, false, false);
                }
            }
        }

        return tile->parametrize(sizeX, sizeY, false, false);
    }

    unsigned int Field::checkTilePlacement(const Cursor coord, BaseMultiplierCategory* tile, FieldState& fieldState) {
        unsigned int sizeX = tile->wX_DSPexpanded(coord.first, coord.second, wX_, wY_, signedIO_);
        unsigned int sizeY = tile->wY_DSPexpanded(coord.first, coord.second, wX_, wY_, signedIO_);
        unsigned int endX = coord.first + sizeX;
        unsigned int endY = coord.second + sizeY;
        unsigned int maxX = std::min(endX, wX_);
        unsigned int maxY = std::min(endY, wY_);

        unsigned int covered = 0;
        ID fieldID = fieldState.getID();

        if (tile->isIrregular() || tile->isKaratsuba()) {
            for (unsigned int i = coord.second; i < maxY; i++) {
                for (unsigned int j = coord.first; j < maxX; j++) {
                    //check if tile could cover this area
                    if (!tile->shape_contribution(j, i, coord.first, coord.second, wX_, wY_, signedIO_)) {
                        continue;
                    }

                    if (field_[i][j] == fieldID || field_[i][j] == baseID_) {
                        return 0;
                    }

                    covered++;
                }
            }
        }
        else {
            for (unsigned int i = coord.second; i < maxY; i++) {
                for (unsigned int j = coord.first; j < maxX; j++) {
                    if (field_[i][j] == fieldID || field_[i][j] == baseID_) {
                        return 0;
                    }

                    covered++;
                }
            }
        }

        return covered;
    }

    Cursor Field::placeTileInField(const Cursor coord, BaseMultiplierCategory* tile, BaseMultiplierParametrization& param, FieldState& fieldState) {
        unsigned int sizeX = param.getMultXWordSize();
        unsigned int sizeY = param.getMultYWordSize();
        unsigned int endX = coord.first + sizeX;
        unsigned int endY = coord.second + sizeY;
        unsigned int maxX = std::min(endX, wX_);
        unsigned int maxY = std::min(endY, wY_);

        ID fieldID = fieldState.getID();
        unsigned int updateMissing = 0U;

        if(tile->isIrregular() || tile->isKaratsuba()) {
            for (unsigned int i = coord.second; i < maxY; i++) {
                for (unsigned int j = coord.first; j < maxX; j++) {
                    //check if tile could cover this area and if area is free
                    if (tile->shape_contribution(j, i, coord.first, coord.second, wX_, wY_, signedIO_) &&
                        field_[i][j] != fieldID && field_[i][j] != baseID_) {
                        field_[i][j] = fieldID;
                        updateMissing++;
                    }
                }
            }
        }
        else {
            for (unsigned int i = coord.second; i < maxY; i++) {
                for (unsigned int j = coord.first; j < maxX; j++) {
                    if (field_[i][j] != fieldID && field_[i][j] != baseID_) {
                        field_[i][j] = fieldID;
                        updateMissing++;
                    }
                }
            }
        }

        fieldState.decreaseMissing(updateMissing);
        fieldState.updateCursor();

        return fieldState.getCursor();
    }

    unsigned int Field::getWidth() {
        return wX_;
    }

    unsigned int Field::getHeight() {
        return wY_;
    }

    unsigned int Field::getMissingLine(FieldState& fieldState) {
        unsigned int missing = 0U;
        Cursor c = fieldState.getCursor();
        ID fieldID = fieldState.getID();

        for(unsigned int i = c.first; i < wX_; i++) {
            if(field_[c.second][i] == fieldID || field_[c.second][i] == baseID_) {
                break;
            }
            missing++;
        }

        return missing;
    }

    unsigned int Field::getMissingHeight(FieldState& fieldState) {
        unsigned int missing = 0U;
        Cursor c = fieldState.getCursor();
        ID fieldID = fieldState.getID();

        for(unsigned int i = c.second; i < wY_; i++) {
            if(field_[i][c.first] == fieldID || field_[i][c.first] == baseID_) {
                break;
            }
            missing++;
        }

        return missing;
    }

    void Field::printField() {
        //TODO: mirror output
        for(auto v: field_) {
            for(auto c: v) {
                cout << c;
            }
            cout << endl;
        }
    }

    bool Field::checkPosition(unsigned int x, unsigned int y, Field::FieldState &fieldState) {
        return (field_[y][x] == fieldState.getID() || field_[y][x] == baseID_);
    }

    void Field::setTruncated(unsigned int range, FieldState& fieldState) {
        ID fieldID = fieldState.getID();
        unsigned int updateMissing = 0;
        for(unsigned int y = 0; y < wY_; y++) {
            if(y > range) {
                break;
            }

            for(unsigned int x = 0; x < wX_; x++) {
                if(y + x > range) {
                   break;
                }

                field_[y][x] = fieldID;
                updateMissing++;
            }
        }

        fieldState.decreaseMissing(updateMissing);
        fieldState.updateCursor();
    }

    void Field::printField(FieldState& fieldState) {
        ID fieldID = fieldState.getID();

        for(auto v: field_) {
            for(auto c: v) {
                cout << (c == fieldID || c == baseID_);
            }
            cout << endl;
        }
    }
}
