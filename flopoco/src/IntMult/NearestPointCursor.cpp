#include "NearestPointCursor.hpp"
#include <cmath>

namespace flopoco {
    NearestPointCursor::NearestPointCursor() : BaseFieldState(), searchPos_{0} {
    }

    void NearestPointCursor::setField(Field *field) {
        BaseFieldState::setField(field);
        coordsLUT_.clear();

        unsigned int wX = field->getWidth();
        unsigned int wY = field->getHeight();

        for(unsigned int i = 0; i < wY; i++) {
            for(unsigned int j = 0; j < wX; j++) {
                float distance = std::sqrt((float)((j * j) + (i * i)));

                NextCoord nextCoord;
                nextCoord.distance = distance;
                nextCoord.coord = Cursor(j, i);
                coordsLUT_.push_back(nextCoord);
            }
        }

        std::sort(coordsLUT_.begin(), coordsLUT_.end(), [](const NextCoord& a, const NextCoord& b) -> bool { return a.distance < b.distance; });
    }

    void NearestPointCursor::reset(Field *field, ID id, unsigned int missing) {
        BaseFieldState::reset(field, id, missing);

        searchPos_ = 0U;
    }

    void NearestPointCursor::reset(BaseFieldState &baseState) {
        BaseFieldState::reset(baseState);
        searchPos_ = 0U;

        NearestPointCursor* cpy = dynamic_cast<NearestPointCursor*>(&baseState);
        if(cpy != nullptr) {
            searchPos_ = cpy->searchPos_;
        }
    }

    void NearestPointCursor::updateCursor() {
        if(missing_ == 0) {
            setCursor(0,0);
            return;
        }

        for(unsigned int i = searchPos_; i < coordsLUT_.size(); i++) {
            NextCoord& next = coordsLUT_[i];
            if(!field_->checkPosition(next.coord.first, next.coord.second, *this)) {
                setCursor(next.coord);
                searchPos_ = i;
                return;
            }
        }

        cout << "REACHED MAX SEARCH RADIUS" << endl;
        field_->printField();
        setCursor(0,0);
        return;
    }
}