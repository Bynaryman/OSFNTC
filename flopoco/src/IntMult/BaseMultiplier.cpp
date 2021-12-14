#include "BaseMultiplier.hpp"

namespace flopoco {


BaseMultiplier::BaseMultiplier(bool isSignedX, bool isSignedY)
{
    srcFileName = "BaseMultiplier";
    uniqueName_ = "BaseMultiplier";

    this->isSignedX = isSignedX;
    this->isSignedY = isSignedY;
}

BaseMultiplier::~BaseMultiplier()
{
}

bool BaseMultiplier::shapeValid(int x, int y)
{
    if((x >= 0) && (x < wX) && (y >= 0) && (y < wY)) return true;
    return false;
}

	
}   //end namespace flopoco
