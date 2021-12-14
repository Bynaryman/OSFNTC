#include "BaseMultiplierCategory.hpp"

namespace flopoco {

	unsigned int BaseMultiplierCategory::Parametrization::getOutWordSize() const
	{
		if (wY_ == 1)
			return wX_;
		if (wX_ == 1)
			return wY_;
		return wX_ + wY_;
	}

	bool BaseMultiplierCategory::Parametrization::shapeValid(int x, int y) const
	{
		return bmCat_->shapeValid(*this, x, y);
	}

    int BaseMultiplierCategory::Parametrization::getRelativeResultLSBWeight() const
    {
        return bmCat_->getRelativeResultLSBWeight(*this);
    }

    int BaseMultiplierCategory::Parametrization::getRelativeResultMSBWeight() const
    {
        return bmCat_->getRelativeResultMSBWeight(*this);
    }

    BaseMultiplierCategory::Parametrization BaseMultiplierCategory::parametrize(
			int wX,
			int wY,
			bool isSignedX,
			bool isSignedY,
			int shape_para,
            string type,
            bool rectangular,
            const vector<int> &output_weights
		) const
	{
/*		int effectiveWY = (isSignedY) ? wY - deltaWidthUnsignedSigned_ : wY;
		bool isFlippedXY = (effectiveWY > maxWordSizeSmallInputUnsigned_);
		if (isFlippedXY) {
			std::swap(wX, wY);
			std::swap(isSignedX, isSignedY);
		}

		int maxSizeY = getMaxSecondWordSize(wX, isSignedX, isSignedY);
		if (maxSizeY < wY) {
            //cerr << "maxSizeY=" << maxWordSizeLargeInputUnsigned_ << " wY=" << wY << "maxSizeX=" << maxWordSizeSmallInputUnsigned_ << " wX=" << wX << endl;
			throw std::string("BaseMultiplierCategory::parametrize: error, required multiplier area is too big");
		}
*/
		return Parametrization(wX, wY, this, isSignedX, isSignedY, false, shape_para, output_weights);
	}

	int BaseMultiplierCategory::getMaxSecondWordSize(
			int firstW,
			bool isW1Signed,
			bool isW2signed
		) const
	{
		int effectiveW1Size = firstW;
		if (isW1Signed)
		   effectiveW1Size += deltaWidthUnsignedSigned_;
		if (effectiveW1Size > maxWordSizeLargeInputUnsigned_)
			return 0;
		int maxLimit = (effectiveW1Size <= maxWordSizeSmallInputUnsigned_) ? maxWordSizeLargeInputUnsigned_: maxWordSizeSmallInputUnsigned_;
		if (isW2signed)
			maxLimit += deltaWidthUnsignedSigned_;

		int inputWidthSumBounds = (maxWordSizeLargeInputUnsigned_+ maxWordSizeSmallInputUnsigned_) - effectiveW1Size;
		if (isW2signed)
			inputWidthSumBounds += deltaWidthUnsignedSigned_;

		int finalLimit = (inputWidthSumBounds <= maxLimit) ? inputWidthSumBounds : maxLimit;
		finalLimit = (finalLimit < 0) ? 0 : finalLimit;
		return finalLimit;
	}


bool BaseMultiplierCategory::shapeValid(Parametrization const& param, unsigned x, unsigned y) const
{
    auto xw = param.getTileXWordSize();
    auto yw = param.getTileYWordSize();

	return (x >= 0 && x < xw && y >= 0 && y < yw);
}

bool BaseMultiplierCategory::shapeValid(int x, int y) {
    int xw = tile_param.wX_;
    int yw = tile_param.wY_;
    return (x >= 0 && x < xw && y >= 0 && y < yw);
}

int BaseMultiplierCategory::getRelativeResultLSBWeight(Parametrization const & param) const
{
    return 0;
}

int BaseMultiplierCategory::getRelativeResultMSBWeight(Parametrization const & param) const
{
    if(param.getTileXWordSize() == 0 || param.getTileYWordSize() == 0)
        return 0;
    if(param.getTileXWordSize() == 1)
        return param.getTileYWordSize();
    if(param.getTileYWordSize() == 1)
        return param.getTileXWordSize();
    return param.getTileXWordSize() + param.getTileYWordSize();
}

// determines if a position (x,y) is coverd by a tile (s), relative to the tiles origin position(shape_x,shape_y)
bool BaseMultiplierCategory::shape_contribution(int x, int y, int shape_x, int shape_y, int wX, int wY, bool signedIO){
    if(getDSPCost() == 1){
        int sign_x = (signedIO && wX-(int)tile_param.wX_-1 == shape_x)?1:0;      //The Xilinx DSP-Blocks can process one bit more if signed
        int sign_y = (signedIO && wY-(int)tile_param.wY_-1 == shape_y)?1:0;
        return ( 0 <= x-shape_x && x-shape_x < (int)tile_param.wX_+sign_x && 0 <= y-shape_y && y-shape_y < (int)tile_param.wY_+sign_y );
    } else {
        return shapeValid(x-shape_x, y-shape_y);
    }
}

//determine the occupation ratio of a given multiplier tile range [0..1],
float BaseMultiplierCategory::shape_utilisation(int shape_x, int shape_y, int wX, int wY, bool signedIO){
    if(0 <= shape_x && (shape_x + (int)tile_param.wX_) < wX && 0 <= shape_y && (shape_y + (int)tile_param.wY_) < wY ){
        return 1.0;
    } else if((shape_x + (int)tile_param.wX_ < 0 || wX <= shape_x) && (shape_y + (int)tile_param.wY_ < 0 || wY <= shape_y )){
        return 0.0;
    } else {
        if(rectangular){
            //return (float)((tile_param.wX_-((0 < shape_x)?shape_x:-shape_x))*(tile_param.wY_-((0 < shape_y)?shape_y:-shape_y)))/(float)(tile_param.wX_*tile_param.wY_);
            int contx = ((int)tile_param.wX_-((shape_x < 0)?-shape_x:0) - ((wX < shape_x+(int)tile_param.wX_)?shape_x+tile_param.wX_-wX:0));
            int conty = ((int)tile_param.wY_-((shape_y < 0)?-shape_y:0) - ((wY < shape_y+(int)tile_param.wY_)?shape_y+tile_param.wY_-wY:0));
            return (float)(contx*conty)/(float)(tile_param.wX_*tile_param.wY_);
        } else {
            unsigned covered_positions = 0, utilized_positions = 0;
            for(int y = shape_y; y < shape_y + (int)tile_param.wY_; y++){
                for(int x = shape_x; x < shape_x + (int)tile_param.wX_; x++){
                    if(shape_contribution(x, y, shape_x, shape_y, wX, wY, signedIO)){
                        covered_positions++;
                        if(0 <= x && 0 <= y && x < wX && y < wY){
                            utilized_positions++;
                        }
                    }
                }
            }
            return (float)utilized_positions/(float)covered_positions;
        }
    }
}

BaseMultiplierCategory::Parametrization BaseMultiplierCategory::Parametrization::tryDSPExpand(int m_x_pos, int m_y_pos, int wX, int wY, bool signedIO) {
    bool isSignedX = false, isSignedY = false;
    int tile_width = wX_, tile_height = wY_;
    if(bmCat_->getDSPCost()){
        if(signedIO && (wX-m_x_pos-(int)wX_)== 1){           //enlarge the Xilinx DSP Multiplier by one bit, if the inputs are signed and placed to process the MSBs
            tile_width++;
        }
        if(signedIO && (wY-m_y_pos-(int)wY_)== 1){
            tile_height++;
        }
    }
    if(signedIO && (wX-m_x_pos-tile_width)== 0){           //if the inputs are signed, the MSBs of individual tiles at MSB edge the tiled area |_ have to be signed
        isSignedX = true;
    }
    if(signedIO && (wY-m_y_pos-tile_height)== 0){
        isSignedY = true;
    }
    return Parametrization(tile_width, tile_height, bmCat_, isSignedX, isSignedY, false, shape_para_,  output_weights);
}

    int BaseMultiplierCategory::wX_DSPexpanded(int m_x_pos, int m_y_pos, int wX, int wY, bool signedIO) {
        int tile_width = tile_param.wX_;
        if(tile_param.bmCat_->getDSPCost()){
            if(signedIO && (wX-m_x_pos-(int)tile_param.wX_)== 1){           //enlarge the Xilinx DSP Multiplier by one bit, if the inputs are signed and placed to process the MSBs
                tile_width++;
            }
        }
        return tile_width;
	}

    int BaseMultiplierCategory::wY_DSPexpanded(int m_x_pos, int m_y_pos, int wX, int wY, bool signedIO) {
        int tile_height = tile_param.wY_;
        if(tile_param.bmCat_->getDSPCost()){
            if(signedIO && (wY-m_y_pos-(int)tile_param.wY_)== 1){           //enlarge the Xilinx DSP Multiplier by one bit, if the inputs are signed and placed to process the MSBs
                tile_height++;
            }
        }
        return tile_height;
	}

    double BaseMultiplierCategory::getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY){
        int x_min = ((x_anchor < 0)?0: x_anchor);
        int y_min = ((y_anchor < 0)?0: y_anchor);
        int lsb = x_min + y_min;

        int x_max = ((wMultX < x_anchor + (int)wX())?wMultX: x_anchor + (int)wX());
        int y_max = ((wMultY < y_anchor + (int)wY())?wMultY: y_anchor + (int)wY());
        int msb = x_max+y_max;

        int ws = (x_max-x_min==1)?y_max-y_min:((y_max-y_min==1)?x_max-x_min:msb - lsb);
        return ws*0.65;
    }

    int BaseMultiplierCategory::ownLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY) {
        return 0;
    }

}