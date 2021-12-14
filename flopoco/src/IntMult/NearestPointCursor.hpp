#ifndef FLOPOCO_NEARESTPOINTCURSOR_HPP
#define FLOPOCO_NEARESTPOINTCURSOR_HPP


#include "Field.hpp"

namespace flopoco {
    class NearestPointCursor : public BaseFieldState {
    public:
        NearestPointCursor();

        void updateCursor() override;
        void setField(Field* field) override;
        void reset(BaseFieldState& baseState) override;
        void reset(Field* field, ID id, unsigned int missing) override;
    private:
        unsigned int searchPos_;

        struct NextCoord{
            Cursor coord;
            float distance;
        };

        vector<NextCoord> coordsLUT_;
    };
}


#endif //FLOPOCO_NEARESTPOINTCURSOR_HPP
