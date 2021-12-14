#ifndef FLOPOCO_FIELD_HPP
#define FLOPOCO_FIELD_HPP

#include <utility>
#include <vector>
#include "BaseMultiplierCategory.hpp"

using namespace std;

namespace flopoco {
    typedef pair<unsigned int, unsigned int> Cursor;
    typedef unsigned long long int ID;

    class Field {
    public:
        class FieldState {
        public:
            FieldState() : id_{0U}, missing_{0U}, field_{nullptr} {}

            void setID(ID id) { id_= id; }
            ID getID() const { return id_; }

            void setCursor(unsigned int x, unsigned int y) { cursor_ = Cursor(x, y); }
            void setCursor(Cursor cursor) { cursor_ = cursor; }
            Cursor getCursor() const { return cursor_; }
            virtual void setField(Field* field) { field_ = field; }
            Field* getField() const { return field_; }

            virtual void reset(Field* field, ID id, unsigned int missing) {
                field_ = field;
                id_ = id;
                missing_ = missing;
            }

            virtual void reset(FieldState& baseState) {
                if(field_ != baseState.field_) {
                    field_ = baseState.field_;
                    field_->initFieldState(*this);
                }
                else {
                    field_->updateStateID(*this);
                }

                missing_ = baseState.missing_;
                cursor_ = Cursor(baseState.cursor_);
            }

            void decreaseMissing(unsigned int delta) { missing_ -= delta; }
            unsigned int getMissing() const { return missing_; }

            virtual void updateCursor() = 0;
        protected:
            ID id_;
            Cursor cursor_;
            unsigned int missing_;
            Field* field_;
        };

        Field(unsigned int wX, unsigned int wY, bool signedIO, FieldState& baseState);
        Field(const Field &copy);

        ~Field();

        void initFieldState(FieldState& fieldState);
        void updateStateID(FieldState& fieldState);

        unsigned int checkTilePlacement(const Cursor coord, BaseMultiplierCategory* tile, FieldState& fieldState);
        BaseMultiplierParametrization checkDSPPlacement(const Cursor coord, BaseMultiplierCategory* tile, FieldState& fieldState, unsigned int maxX, unsigned int maxY);
        Cursor placeTileInField(const Cursor coord, BaseMultiplierCategory* tile, BaseMultiplierParametrization& param, FieldState& fieldState);
        unsigned int getMissingLine(FieldState& fieldState);
        unsigned int getMissingHeight(FieldState& fieldState);
        unsigned int getWidth();
        unsigned int getHeight();
        void reset();
        void reset(Field& target);
        bool checkPosition(unsigned int x, unsigned int y, FieldState& fieldState);
        void printField();
        void setTruncated(unsigned int range, FieldState& fieldState);
        void printField(FieldState& fieldState);

    protected:
        vector<vector<ID>> field_;
        unsigned int wX_;
        unsigned int wY_;
        bool signedIO_;

        ID currentStateID_;
        FieldState* baseState_;
        ID baseID_;
    };

    typedef Field::FieldState BaseFieldState;
}


#endif
