#include "IntMult/TilingStrategy.hpp"
#include "IntMultiplier.hpp"

namespace flopoco {


	TilingStrategy::TilingStrategy(int wX, int wY, int wOut, bool signedIO, BaseMultiplierCollection *baseMultiplierCollection) :
			wX(wX), wY(wY), wOut(wOut), signedIO(signedIO), baseMultiplierCollection(baseMultiplierCollection), target(baseMultiplierCollection->getTarget())
	{

	}

	void TilingStrategy::printSolution()
	{
		for (auto& tile : solution)
		{
			BaseMultiplierCategory::Parametrization& parametrization = tile.first;
			multiplier_coordinates_t& coordinates = tile.second;

			cerr << "multiplier of type " << parametrization.getMultType() << " placed at (" << coordinates.first << "," << coordinates.second << ") of size (" << parametrization.getTileXWordSize() << ", " << parametrization.getTileYWordSize() << ")" << endl;
		}

	}

	void TilingStrategy::printSolutionTeX(ofstream &outstream, int wTrunc, bool triangularStyle)
	{
		cerr << "Dumping multiplier schema in multiplier.tex\n";
		outstream << "\\documentclass{standalone}\n\\usepackage{tikz}\n\n\\begin{document}\n\\begin{tikzpicture}[yscale=-1,xscale=-1]\n";
		if(triangularStyle)
		{
			outstream << "\\draw[thick] (0, 0) -- ("<< wX << ", 0) -- ("<<(wX+wY) <<", " << wY<<") -- ("<< wY << ", " << wY <<") -- cycle;\n";
			for (size_t i = 1 ; i < static_cast<size_t>(wY) ; ++i) {
				outstream << "\\draw[dotted, thin, gray] ("<<i<<", " << i <<") -- (" << wX + i << ", " << i << ");\n";
			}
			for (size_t i = 1 ; i < static_cast<size_t>(wX) ; ++i) {
				outstream << "\\draw[dotted, thin, gray] ("<< i <<", 0) -- (" << (wY + i) << ", " << wY << ");\n";
			}
			for (auto& tile : solution) {
				auto& parametrization = tile.first;
				auto& coordinates = tile.second;
				int xstart = coordinates.first + coordinates.second;
				int ystart = coordinates.second;
				int xend = xstart + static_cast<int>(parametrization.getTileXWordSize());
				int yend = ystart + static_cast<int>(parametrization.getTileYWordSize());
				int deltaY = static_cast<int>(parametrization.getTileYWordSize());
				string color = (parametrization.isSignedMultX() || parametrization.isSignedMultY()) ? "red" : "blue";
				outstream << "\\draw[fill="<< color <<", fill opacity=0.3] (" << xstart << ", " << ystart << ") -- (" <<
					xend << ", " << ystart << ") -- ("<< xend + deltaY <<", "<< yend<<") -- ("<< xstart + deltaY <<", "<< yend <<")--cycle;\n";
				cerr << "Got one tile at (" << xstart << ", " << ystart << ") of size (" << parametrization.getTileXWordSize() << ", " << parametrization.getTileYWordSize() << ").\n";
			}

			int offset = IntMultiplier::prodsize(wX, wY) - wOut;

			if (offset > 0) {
				float startY = (wX <= offset) ? (offset - wX) + 0.5  : 0;
				float endY =  (offset >= wY) ? wY : offset + 0.5;
				outstream << "\\draw[ultra thick, green] (" << offset << ".5, " << startY << ") -- (" << offset << ".5, " << endY << ");" << endl;
			}

			if (wTrunc > offset) {
				int truncOffset = IntMultiplier::prodsize(wX, wY) - wTrunc;
				float startY = (wX <= truncOffset) ? (truncOffset - wX) + 0.5  : 0;
				float endY =  (truncOffset >= wY) ? wY : truncOffset + 0.5;
				outstream << "\\draw[ultra thick, brown] (" << truncOffset << ".5, " << startY << ") -- (" << truncOffset << ".5, " << endY << ");" << endl;
			}

			for (size_t i = 0 ; i < static_cast<size_t>(wX) ; ++i) {
				for (size_t j = 0 ; j < static_cast<size_t>(wY) ; ++j) {
					string color = (j+i >= (size_t)offset) ? "black" : "purple";
					outstream << "\\fill["<< color <<"] ("<<(j+i + 1)<<", " << j <<".5) circle (0.125);\n";
				}
			}

			outstream << "\\draw[red, thick] (0, 0) -- ("<< wX << ", 0) -- ("<<(wX+wY) <<", " << wY<<") -- ("<< wY << ", " << wY <<") -- cycle;\n";
		} else {
			outstream << "\\draw[thick] (0, 0) rectangle (" << wX << ", " << wY << ");\n";
			for (size_t i = 0 ; i < static_cast<size_t>(wY) ; ++i) {
				outstream << "\\draw[dotted, very thin, gray] (0, " << i <<") -- (" << wX << ", " << i << ");\n";
			}
			for (size_t i = 0 ; i < static_cast<size_t>(wX) ; ++i) {
				outstream << "\\draw[dotted, very thin, gray] ("<< i <<", 0) -- (" << i << ", " << wY << ");\n";
			}
			for (auto& tile : solution) {
				auto& parametrization = tile.first;
				auto& coordinates = tile.second;
				int xstart = coordinates.first;
				int ystart = coordinates.second;
				int xend = xstart + static_cast<int>(parametrization.getTileXWordSize());
				int yend = ystart + static_cast<int>(parametrization.getTileYWordSize());
				for (int y = ystart; y < yend; y++){        // for not rectangular tiles, every field in its maximum outline has to be checked if it is covered
                    for (int x = xstart; x < xend; x++){
                        if(parametrization.shapeValid(x-xstart,y-ystart)){
                            outstream << "\\fill[fill=gray, fill opacity=0.3] (" << x << ", " << y << ") rectangle (" <<
                                      x+1 << ", " << y+1 << ");\n";
                            if((parametrization.shapeValid(x-xstart,y-ystart) && x-xstart == 0) || (x-xstart > 0 && !parametrization.shapeValid(x-xstart-1,y-ystart)))      //draw outline of individual tile, if neighbouring tile is not covered by it
                                outstream << "\\draw (" << x << "," << y << ") -- (" << x << "," << y+1 <<");\n";
                            if((parametrization.shapeValid(x-xstart,y-ystart) && y-ystart == 0) || (y-ystart > 0 && !parametrization.shapeValid(x-xstart,y-ystart-1)))
                                outstream << "\\draw (" << x << "," << y << ") -- (" << x+1 << "," << y <<");\n";
                            if(!parametrization.shapeValid(x-xstart+1,y-ystart))
                                outstream << "\\draw (" << x+1 << "," << y << ") -- (" << x+1 << "," << y+1 <<");\n";
                            if(!parametrization.shapeValid(x-xstart,y-ystart+1))
                                outstream << "\\draw (" << x << "," << y+1 << ") -- (" << x+1 << "," << y+1 <<");\n";
                        }
                    }
                }
/*				outstream << "\\draw[fill=gray, fill opacity=0.3] (" << xstart << ", " << ystart << ") rectangle (" <<
					xend << ", " << yend << ");\n";
*/				cerr << "Got one tile at (" << xstart << ", " << ystart << ") of size (" << parametrization.getTileXWordSize() << ", " << parametrization.getTileYWordSize() << ").\n";
			}
		}
		outstream << "\\end{tikzpicture}\n\\end{document}\n";
	}

    void TilingStrategy::printSolutionSVG(ofstream &outstream, int wTrunc, bool triangularStyle)
    {
        string colour[] = { "Red", "Green", "Blue", "Cyan", "Magenta", "Yellow", "Violet", "Lime", "Orange", "Pink", "Beige" };
        int xmin=0, ymin=0,xmax=0, ymax=0, col=0;
        for (auto& tile : solution) {
            auto& parametrization = tile.first;
            auto &coordinates = tile.second;
            if(coordinates.first < xmin)
                xmin = coordinates.first;
            if(coordinates.second < ymin)
                ymin = coordinates.second;
            if(coordinates.first + static_cast<int>(parametrization.getTileXWordSize()) > xmax)
                xmax = coordinates.first + static_cast<int>(parametrization.getTileXWordSize());
            if(coordinates.second + static_cast<int>(parametrization.getTileYWordSize()) > ymax)
                ymax = coordinates.second + static_cast<int>(parametrization.getTileYWordSize());
        }
        xmin = (xmin < 0)?-xmin:0;
        ymin = (ymin < 0)?-ymin:0;
        int width = xmax + xmin;

        cerr << "Dumping multiplier schema in multiplier.svg\n";
        outstream << "<?xml version=\"1.0\" standalone=\"no\"?>\n<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
        outstream << "<svg width=\"" << 10*(xmax+xmin) << "\" height=\"" << 10*(ymax+ymin) << "\"  xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" version=\"1.1\">\n";

        outstream << "\t<rect x=\"" << 10*(width-xmin-wX) << "\" y=\"" << 10*ymin << "\" width=\"" << 10*(wX) << "\" height=\"" << 10*(wY) << "\" style=\"fill:none;stroke-width:1;stroke:darkgray\" />\n";
        outstream << "\t<g fill=\"none\" stroke=\"darkgray\" stroke-width=\"1\">\n";
        for (size_t i = 0 ; i < static_cast<size_t>(wY) ; ++i) {
            outstream << "\t\t<path stroke-dasharray=\"1, 1\" d=\"M" << 10*(width-xmin-wX) << " " << 10*(i+ymin) << " l" << 10*wX << " 0\" />\n";
        }
        for (size_t i = 0 ; i < static_cast<size_t>(wX) ; ++i) {
            outstream << "\t\t<path stroke-dasharray=\"1, 1\" d=\"M" << 10*(width-i-xmin) << " " << 10*ymin << " l0 " << 10*wY << "\" />\n";
        }
        for (auto& tile : solution) {
            auto& parametrization = tile.first;
            auto& coordinates = tile.second;
            int xstart = coordinates.first;
            int ystart = coordinates.second;
            int xend = xstart + static_cast<int>(parametrization.getTileXWordSize());
            int yend = ystart + static_cast<int>(parametrization.getTileYWordSize());
            for (int y = ystart; y < yend; y++){        // for not rectangular tiles, every field in its maximum outline has to be checked if it is covered
                for (int x = xstart; x < xend; x++){
                    if(parametrization.shapeValid(x-xstart,y-ystart)){
                        outstream << "\t\t<rect x=\"" << 10*(width-x-xmin-1) << "\" y=\"" << 10*(y+ymin) << "\" width=\"" << 10 << "\" height=\"" << 10 << "\" style=\"fill:gray;fill-opacity:0.1;stroke:none\" />\n";
                        if((parametrization.shapeValid(x-xstart,y-ystart) && x-xstart == 0) || (x-xstart > 0 && !parametrization.shapeValid(x-xstart-1,y-ystart)))      //draw outline of individual tile, if neighbouring tile is not covered by it
                            outstream << "\t\t<line x1=\"" << 10*(width-x-xmin) << "\" y1=\"" << 10*(y+ymin)  << "\" x2=\"" << 10*(width-x-xmin) << "\" y2=\"" << 10*(y+ymin+1) << "\" style=\"stroke:black;stroke-width:1\" /> \n";
                        if((parametrization.shapeValid(x-xstart,y-ystart) && y-ystart == 0) || (y-ystart > 0 && !parametrization.shapeValid(x-xstart,y-ystart-1)))
                            outstream << "\t\t<line x1=\"" << 10*(width-x-xmin) << "\" y1=\"" << 10*(y+ymin)  << "\" x2=\"" << 10*(width-x-xmin-1) << "\" y2=\"" << 10*(y+ymin)  << "\" style=\"stroke:black;stroke-width:1\" /> \n";
                        if(!parametrization.shapeValid(x-xstart+1,y-ystart))
                            outstream << "\t\t<line x1=\"" << 10*(width-x-xmin-1) << "\" y1=\"" << 10*(y+ymin)  << "\" x2=\"" << 10*(width-x-xmin-1) << "\" y2=\"" << 10*(y+ymin+1) << "\" style=\"stroke:black;stroke-width:1\" /> \n";
                        if(!parametrization.shapeValid(x-xstart,y-ystart+1))
                            outstream << "\t\t<line x1=\"" << 10*(width-x-xmin) << "\" y1=\"" << 10*(y+ymin+1)  << "\" x2=\"" << 10*(width-x-xmin-1) << "\" y2=\"" << 10*(y+ymin+1) << "\" style=\"stroke:black;stroke-width:1\" /> \n";
                    }
                }
            }

            if(26 < parametrization.getMultType().size() &&  parametrization.getMultType().substr(0,26).compare("BaseMultiplierDSPKaratsuba") == 0){
                //int order = stoi(parametrization.getMultType().substr(parametrization.getMultType().find("size")+4, parametrization.getMultType().size()) );
                cout << parametrization.getMultXWordSize() << " " << parametrization.getMultYWordSize()  << " " << parametrization.getShapePara() << endl;
                if(!parametrization.getShapePara()) continue;
/*                int wX=24, wY=24, k = 0;
                while(!(wX <= 24 && wY <= 16)  || !(wX <= 16 && wY <= 24)  || (16+24 < wX+wY) || ((int)parametrization.getMultXWordSize() != k + wX) || ((int)parametrization.getMultYWordSize() != k + wY)){
                    wX--;
                    if(wX <= 2){
                        wX = 24;
                        wY--;
                        if(wY <= 2){
                            wY = 24;
                            k++;
                        }
                    }
                }
                cout << "wX " << wX  << " wY " << wY << " k " << k << endl;
*/

                int gcd=parametrization.getMultXWordSize(), b=parametrization.getMultYWordSize(), t;
                //gcd = (0 < parametrization.getShapePara())?gcd/parametrization.getShapePara():gcd;
                //b = (0 < parametrization.getShapePara())?b/parametrization.getShapePara():b;
                while (b != 0){
                    t = b;
                    b = gcd % b;
                    gcd = t;
                }
                if(24 < gcd && gcd % 2 == 0) gcd /= 2;
                cout << "gcd " << gcd << endl;


                int tX=0, tY=0;
                for(int k = 0; k <= (int)parametrization.getMultXWordSize() && k <= (int)parametrization.getMultYWordSize(); k += gcd){
                    for(int y = gcd; y <= 25 && !tY; y += gcd){
                        for(int x = gcd; x <= 25 && !tX; x += gcd){
                            if((int)parametrization.getMultXWordSize()  == k + x && (int)parametrization.getMultYWordSize()  == k + y && x+y < 41){
                                   tX = x;
                                   tY = y;
                            }
                        }
                    }
                }
                /*while(kxy != 0 && (kxy/2)%tX == 0 && (kxy/2)%tY == 0){
                    kxy /= 2;
                }*/

                int kxy = gcd;
                for(; kxy % tX || kxy % tY; kxy += gcd);
                cout << "first possible position " << kxy << endl;

                cout << "wX " << tX  << " wY " << tY << " k " << kxy << endl;

                for(int xy = 0; xy <= parametrization.getShapePara(); xy++){     //diagonal
                    outstream << "\t\t<rect x=\"" << 10*(width-xmin-xstart -(kxy*xy + tX/2 + 2)) << "\" y=\"" << 10*(ystart+kxy*xy + tY/2 - 2) << "\" width=\"" << 10*4 << "\" height=\"" << 10*4 << "\" style=\"fill:" << colour[col%11] << ";fill-opacity:1.0;stroke:none\" />\n";
                    //createMult(kxy*xy/gcd, kxy*xy/gcd);
                    for(int nr = 0; nr < xy; nr++) {     //karatsuba substitution
                        outstream << "\t\t<rect x=\"" << 10*(width-xmin-xstart -(kxy*nr + tX/2 + 2)) << "\" y=\"" << 10*(ystart+kxy*xy + tY/2 - 2) << "\" width=\"" << 10*4 << "\" height=\"" << 10*4 << "\" style=\"fill:white;fill-opacity:1.0;stroke:" << colour[col%11] << ";stroke-width:1\" />\n";
                        outstream << "\t\t<rect x=\"" << 10*(width-xmin-xstart -(kxy*xy + tX/2 + 2)) << "\" y=\"" << 10*(ystart+kxy*nr + tY/2 - 2) << "\" width=\"" << 10*4 << "\" height=\"" << 10*4 << "\" style=\"fill:white;fill-opacity:1.0;stroke:" << colour[col%11] << ";stroke-width:1\" />\n";
                        outstream << "\t\t<path d=\"M" << 10*(width-xmin-xstart -(kxy*xy + tX/2)) << "," << 10*(ystart+kxy*nr + tY/2) << " A18000,18000 0 0,1 " << 10*(width-xmin-xstart -(kxy*nr + tX/2 )) << "," << 10*(ystart+kxy*xy + tY/2) << "\" style=\"fill:none;stroke:" << colour[col%11] << ";stroke-width:3\" />\n";
                        //createRectKaratsuba(kxy*nr/gcd,kxy*xy/gcd,kxy*xy/gcd,kxy*nr/gcd);
                    }
                }
                col++;
            }

        }
        outstream << "\t</g>\n";
        outstream << "</svg>\n";
    }

}   //end namespace flopoco
