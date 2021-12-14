#ifndef ALTERA_LCELL_H
#define ALTERA_LCELL_H

#include "../Primitive.hpp"

namespace flopoco {
///
/// \brief The Altera_LCELL class. Implementation of the target specific Altera lcell_comb.
///
class Altera_LCELL : public Primitive {
        bool _hasSharedArith,_hasDontTouch,_hasLUT7;
      public:

        Altera_LCELL(Operator *parentOp, Target *target, const std::string & lut_mask, const bool &shared_arith = false, const bool &dont_touch = false );

        ~Altera_LCELL() {}

        const bool &hasSharedArith() const{ return _hasSharedArith; }
        const bool &hasLUT7() const{ return _hasLUT7;}
        const bool &hasDontTouch() const{ return _hasDontTouch;}
    };
}//namespace

#endif
