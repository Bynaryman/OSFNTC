#ifndef FLOPOCO_LINECURSOR_HPP
#define FLOPOCO_LINECURSOR_HPP

#include "Field.hpp"

namespace flopoco {
    class LineCursor : public BaseFieldState {
    public:
        void updateCursor() override {
            while(cursor_.second != field_->getHeight()) {
                //try to find next free position in the current line
                for(unsigned int i = 0U; i < field_->getWidth(); i++) {
                    if(!field_->checkPosition(i, cursor_.second, *this)) {
                        cursor_.first = i;
                        return;
                    }
                }

                //no free position in this line
                cursor_.second++;
            }

            cursor_.first = 0U;
            cursor_.second = 0U;
        }
    };
}


#endif //FLOPOCO_LINECURSOR_HPP
