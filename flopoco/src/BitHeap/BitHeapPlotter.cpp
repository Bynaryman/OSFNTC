/*
   A class used for plotting bitheaps

   This file is part of the FloPoCo project

Author : Florent de Dinechin, Kinga Illyes, Bogdan Popa, Matei Istoan

Initial software.
Copyright © INSA de Lyon, ENS-Lyon, INRIA, CNRS, UCBL
2016.
All rights reserved.

*/

#include "BitHeapPlotter.hpp"


using namespace std;

namespace flopoco
{

	BitheapPlotter::Snapshot::Snapshot(vector<vector<Bit*> > bits_, int maxHeight_, Bit *soonestBit_, Bit *soonestCompressibleBit_):
		maxHeight(maxHeight_)
	{
		soonestBit = soonestBit_;
		soonestCompressibleBit = soonestCompressibleBit_;

		for(unsigned int i=0; i<bits_.size(); i++)
		{
			vector<Bit*> newColumn;

			if(bits_[i].size() > 0)
			{
				for(vector<Bit*>::iterator it = bits_[i].begin(); it!=bits_[i].end(); ++it)
				{
					newColumn.push_back((*it)->clone());
				}
			}

			bits.push_back(newColumn);
		}
	}


	bool operator< (BitheapPlotter::Snapshot& b1, BitheapPlotter::Snapshot& b2)
	{
		return ((b1.soonestBit->signal->getCycle() < b2.soonestBit->signal->getCycle())
				|| (b1.soonestBit->signal->getCycle()==b2.soonestBit->signal->getCycle()
						&& b1.soonestBit->signal->getCriticalPath()<b2.soonestBit->signal->getCriticalPath()));
	}

	bool operator<= (BitheapPlotter::Snapshot& b1, BitheapPlotter::Snapshot& b2)
	{
		return ((b1.soonestBit->signal->getCycle()<b2.soonestBit->signal->getCycle())
				|| (b1.soonestBit->signal->getCycle()==b2.soonestBit->signal->getCycle()
						&& b1.soonestBit->signal->getCriticalPath()<=b2.soonestBit->signal->getCriticalPath()));
	}

	bool operator== (BitheapPlotter::Snapshot& b1, BitheapPlotter::Snapshot& b2)
	{
		return (b1.soonestBit->signal->getCycle()==b2.soonestBit->signal->getCycle()
				&& b1.soonestBit->signal->getCriticalPath()==b2.soonestBit->signal->getCriticalPath());
	}

	bool operator!= (BitheapPlotter::Snapshot& b1, BitheapPlotter::Snapshot& b2)
	{
		return ((b1.soonestBit->signal->getCycle()!=b2.soonestBit->signal->getCycle())
				|| (b1.soonestBit->signal->getCycle()==b2.soonestBit->signal->getCycle()
						&& b1.soonestBit->signal->getCriticalPath()!=b2.soonestBit->signal->getCriticalPath()));
	}

	bool operator> (BitheapPlotter::Snapshot& b1, BitheapPlotter::Snapshot& b2)
	{
		return ((b1.soonestBit->signal->getCycle()>b2.soonestBit->signal->getCycle())
				|| (b1.soonestBit->signal->getCycle()==b2.soonestBit->signal->getCycle()
						&& b1.soonestBit->signal->getCriticalPath()>b2.soonestBit->signal->getCriticalPath()));
	}

	bool operator>= (BitheapPlotter::Snapshot& b1, BitheapPlotter::Snapshot& b2)
	{
		return ((b1.soonestBit->signal->getCycle()>b2.soonestBit->signal->getCycle())
				|| (b1.soonestBit->signal->getCycle()==b2.soonestBit->signal->getCycle()
						&& b1.soonestBit->signal->getCriticalPath()>=b2.soonestBit->signal->getCriticalPath()));
	}


	BitheapPlotter::BitheapPlotter(BitHeap* bitheap_) :
			bitheap(bitheap_)
	{
		srcFileName = bitheap_->getName() + ":BitheapPlotter";
	}



	BitheapPlotter::~BitheapPlotter()
	{

	}


	void BitheapPlotter::takeSnapshot(Bit *soonestBit, Bit *soonestCompressibleBit)
	{
		BitheapPlotter::Snapshot* s = new BitheapPlotter::Snapshot(bitheap->getBits(),
											bitheap->getMaxHeight(), soonestBit, soonestCompressibleBit);

		snapshots.push_back(s);
	}



	void BitheapPlotter::plotBitHeap()
	{
		drawInitialBitheap();
		drawBitheapCompression();
	}


	void BitheapPlotter::drawInitialBitheap()
	{
		int offsetY = 0, turnaroundX = 0;

		initializePlotter(true);

		offsetY += 20 + snapshots[0]->maxHeight * 10;
		turnaroundX = snapshots[0]->bits.size() * 10 + 80;

		drawInitialConfiguration(snapshots[0], offsetY, turnaroundX);

		closePlotter(offsetY, turnaroundX);
	}


	void BitheapPlotter::drawBitheapCompression()
	{
		initializePlotter(false);

		int offsetY = 0;
		int turnaroundX = snapshots[snapshots.size()-1]->bits.size() * 10 + 100;

		for(unsigned int i=0; i<snapshots.size(); i++)
		{
			offsetY += 25 + snapshots[i]->maxHeight * 10;

			drawConfiguration(i, snapshots[i], offsetY, turnaroundX);

			fig << "<line x1=\"" << turnaroundX + 350 << "\" y1=\"" << offsetY + 10
					<< "\" x2=\"" << 50 << "\" y2=\"" << offsetY + 10;
			if((i == 0) || (i>0 && i%2==1) || (i == snapshots.size()-1))
			{
				fig	<< "\" style=\"stroke:midnightblue;stroke-width:1\" />" << endl;
			}else
			{
				fig	<< "\" style=\"stroke:lightsteelblue;stroke-width:1\" />" << endl;
			}

			if((i == 0) || (i == snapshots.size()-1))
			{
				fig << "<line x1=\"" << turnaroundX + 350 << "\" y1=\"" << offsetY + 15
						<< "\" x2=\"" << 50 << "\" y2=\"" << offsetY + 15
						<< "\" style=\"stroke:midnightblue;stroke-width:1\" />" << endl;
			}

			if(i>0 && i%2==1)
			{
				fig << "<text x=\"" << 15 << "\" y=\"" << offsetY + 15
						<< "\" fill=\"midnightblue\">" << (i+1)/2 << "</text>" << endl;
			}
		}

		closePlotter(offsetY, turnaroundX);
	}


	void BitheapPlotter::initializePlotter(bool isInitial)
	{
		ostringstream figureFileName;
		ostringstream bitheapName;
		int bitheapWidth, bitheapHeight, bitheapLength;

		bitheapWidth  = (bitheap->msb - bitheap->lsb + 1) * 20 + 350;
		bitheapHeight = bitheap->height * snapshots.size() * 50;
		bitheapLength = bitheapHeight;

		if(bitheap->getName() == "")
		{
			bitheapName << bitheap->getOp()->getName() << "_bitheap_" << bitheap->getGUid();
			bitheap->name = bitheapName.str();
		}

		if(isInitial)
			figureFileName << "BitHeap_initial_" << bitheap->getName()  << ".svg";
		else
			figureFileName << "BitHeap_compression_" << bitheap->getName()  << ".svg";

		FILE* pfile;
		pfile  = fopen(figureFileName.str().c_str(), "w");
		fclose(pfile);

		fig.open (figureFileName.str().c_str(), ios::trunc);

		fig << "<?xml version=\"1.0\" standalone=\"no\"?>" << endl
				<< "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"" << endl
				<< "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">" << endl
				<< "<svg width=\"" << bitheapWidth << "\" height=\"" << bitheapHeight << "\" length=\"" << bitheapLength
				<< "\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" version=\"1.1\" onload=\"init(evt)\" >" << endl;

		addECMAFunction();
	}


	void BitheapPlotter::closePlotter(int offsetY, int turnaroundX)
	{
		fig << "<line x1=\"" << turnaroundX + 30 << "\" y1=\"" << 20 << "\" x2=\"" << turnaroundX + 30
				<< "\" y2=\"" << offsetY +30 << "\" style=\"stroke:midnightblue;stroke-width:1\" />" << endl;

		fig << "<rect class=\"tooltip_bg\" id=\"tooltip_bg\" x=\"0\" y=\"0\" rx=\"4\" ry=\"4\" width=\"55\" height=\"17\" visibility=\"hidden\"/>" << endl;
		fig << "<text class=\"tooltip\" id=\"tooltip\" x=\"0\" y=\"0\" visibility=\"hidden\">Tooltip</text>" << endl;

		fig << "</svg>" << endl;

		fig.close();
	}


	void BitheapPlotter::drawInitialConfiguration(Snapshot* snapshot, int offsetY, int turnaroundX)
	{
		fig << "<line x1=\"" << turnaroundX + 150 << "\" y1=\""
				<< offsetY +10 << "\" x2=\"" << turnaroundX - snapshot->bits.size()*10 - 50
				<< "\" y2=\"" << offsetY +10 << "\" style=\"stroke:lightsteelblue;stroke-width:1\" />" << endl;

		for(unsigned int i=0; i<snapshot->bits.size(); i++)
		{
			if(snapshot->bits[i].size() > 0)
			{
				for(unsigned int j=0; j<snapshot->bits[i].size(); j++)
				{
					drawBit(j, turnaroundX, offsetY, snapshot->bits[i][j]);
				}
			}
		}
	}


	void BitheapPlotter::drawConfiguration(int index, Snapshot *snapshot, int offsetY, int turnaroundX)
	{
		int ci,c1,c2,c3;		//print the critical path as a number as a rational number, in nanoseconds
		int ci2,c12,c22,c32;	//print the critical path as a number as a rational number, in nanoseconds
		int cpint = snapshot->soonestBit->signal->getCriticalPath() * 1000000000000;
		int cp2int = snapshot->soonestCompressibleBit->signal->getCriticalPath() * 1000000000000;

		c3 = cpint % 10;
		cpint = cpint / 10;
		c2 = cpint % 10;
		cpint = cpint / 10;
		c1 = cpint % 10;
		cpint = cpint / 10;
		ci = cpint % 10;

		c32 = cp2int % 10;
		cp2int = cp2int / 10;
		c22 = cp2int % 10;
		cp2int = cp2int / 10;
		c12 = cp2int % 10;
		cp2int = cp2int / 10;
		ci2 = cp2int % 10;

		if(index == 0)
		{
			fig << "<text x=\"" << turnaroundX + 50 << "\" y=\"" << offsetY + 0
					<< "\" fill=\"midnightblue\">" << "initial bitheap contents" << "</text>" << endl;
		}else if(index == (int)snapshots.size()-1)
		{
			fig << "<text x=\"" << turnaroundX + 50 << "\" y=\"" << offsetY + 0
					<< "\" fill=\"midnightblue\">" << "before final addition" << "</text>" << endl;
		}else
		{
			fig << "<text x=\"" << turnaroundX + 50 << "\" y=\"" << offsetY - 20
					<< "\" fill=\"midnightblue\">" << snapshot->soonestBit->signal->getCycle() << "</text>" << endl
				<< "<text x=\"" << turnaroundX + 80 << "\" y=\"" << offsetY - 20
					<< "\" fill=\"midnightblue\">" << ci << "." << c1 << c2 << c3 << " ns"  << "</text>" << endl
				<< "<text x=\"" << turnaroundX + 180 << "\" y=\"" << offsetY - 20
									<< "\" fill=\"midnightblue\"> soonest bit </text>" << endl;
			fig << "<text x=\"" << turnaroundX + 50 << "\" y=\"" << offsetY + 0
					<< "\" fill=\"midnightblue\">" << snapshot->soonestCompressibleBit->signal->getCycle() << "</text>" << endl
				<< "<text x=\"" << turnaroundX + 80 << "\" y=\"" << offsetY + 0
					<< "\" fill=\"midnightblue\">" << ci2 << "." << c12 << c22 << c32 << " ns"  << "</text>" << endl
				<< "<text x=\"" << turnaroundX + 180 << "\" y=\"" << offsetY + 0
									<< "\" fill=\"midnightblue\"> soonest compressible bit </text>" << endl;
		}

		for(unsigned int i=0; i<snapshot->bits.size(); i++)
		{
			if(snapshot->bits[i].size() > 0)
			{
				for(unsigned int j=0; j<snapshot->bits[i].size(); j++)
				{
					drawBit(j, turnaroundX, offsetY, snapshot->bits[i][j]);
				}
			}
		}
	}


	void BitheapPlotter::drawBit(int index, int turnaroundX, int offsetY, Bit *bit)
	{
		const std::string colors[] = { "darkblue", "blue", "royalblue", "dodgerblue", "teal", "deepskyblue", "cyan",
										"lightseagreen", "mediumgreen", "forestgreen", "olivedrab", "darkolivegreen", "darkgreen", "darkslategrey"};
		const unsigned int colorsNumber = 14;
		int ci,c1,c2,c3;	//print cp as a number, as a rational number, in nanoseconds
		int criticalPath = (int)(bit->signal->getCriticalPath() * 1000000000000);

		c3 = criticalPath % 10;
		criticalPath /= 10;
		c2 = criticalPath % 10;
		criticalPath /= 10;
		c1 = criticalPath % 10;
		criticalPath /= 10;
		ci = criticalPath % 10;

		if(bit->type == BitType::compressed)
		{
#if COMPRESSED_BITS_PATTERN //enable/disable pattern altogether for compressed bits
			fig << "<pattern id=\"diagonalHatchRightLeft\" patternUnits=\"userSpaceOnUse\" width=\"3\" height=\"3\">"
					<< "<path d=\"M-1,1 l3,-3"
					<< "M0,3 l3,-3"
					<< "M2,4 l3,-3\" "
					<< "style=\"stroke:" << colors[bit->colorCount % colorsNumber] << "; stroke-width:1\" />"
				<< "</pattern>" << endl;
#endif
			fig << "<circle cx=\"" << turnaroundX - bit->weight*10 - 5 << "\""
				<< " cy=\"" << offsetY - index*10 - 5 << "\""
				<< " r=\"3\""
#if COMPRESSED_BITS_PATTERN //enable/disable pattern altogether for compressed bits
				<< " fill=\"url(#diagonalHatchRightLeft)\" stroke=\"black\" stroke-width=\"0.5\""
#else
				<< " fill=\"white\" stroke=\"black\" stroke-width=\"0.5\""
#endif
				<< " onmousemove=\"ShowTooltip(evt, \'" << bit->getName() << ", " << bit->signal->getCycle() << " : " << ci << "." << c1 << c2 << c3 << " ns\')\""
				<< " onmouseout=\"HideTooltip(evt)\" />" << endl;
		}
		else if(bit->type == BitType::justAdded)
		{
			fig << "<pattern id=\"diagonalHatchLeftRight\" patternUnits=\"userSpaceOnUse\" width=\"3\" height=\"3\">"
					<< "<path d=\"M2,-1 l3,3"
					<< "M0,0 l3,3"
					<< "M-1,2 l3,3\" "
					<< "style=\"stroke:" << colors[bit->colorCount % colorsNumber] << "; stroke-width:1\" />"
				<< "</pattern>" << endl;
			fig << "<circle cx=\"" << turnaroundX - bit->weight*10 - 5 << "\""
				<< " cy=\"" << offsetY - index*10 - 5 << "\""
				<< " r=\"3\""
				<< " fill=\"url(#diagonalHatchLeftRight)\" stroke=\"red\" stroke-width=\"0.5\""
				<< " onmousemove=\"ShowTooltip(evt, \'" << bit->getName() << ", " << bit->signal->getCycle() << " : " << ci << "." << c1 << c2 << c3 << " ns\')\""
				<< " onmouseout=\"HideTooltip(evt)\" />" << endl;
		}
		else
		{
			fig << "<circle cx=\"" << turnaroundX - bit->weight*10 - 5 << "\""
				<< " cy=\"" << offsetY - index*10 - 5 << "\""
				<< " r=\"3\""
				<< " fill=\"" << colors[bit->colorCount % colorsNumber] << "\" stroke=\"black\" stroke-width=\"0.5\""
				<< " onmousemove=\"ShowTooltip(evt, \'" << bit->getName() << ", " << bit->signal->getCycle() << " : " << ci << "." << c1 << c2 << c3 << " ns\')\""
				<< " onmouseout=\"HideTooltip(evt)\" />" << endl;
		}
	}


	void BitheapPlotter::addECMAFunction()
	{
		fig << endl;
		fig << "<style>" << endl <<
				tab << ".caption{" << endl <<
				tab << tab << "font-size: 15px;" << endl <<
				tab << tab << "font-family: Georgia, serif;" << endl <<
				tab << "}" << endl <<
				tab << ".tooltip{" << endl <<
				tab << tab << "font-size: 12px;" << endl <<
				tab << "}" << endl <<
				tab << ".tooltip_bg{" << endl <<
				tab << tab << "fill: white;" << endl <<
				tab << tab << "stroke: black;" << endl <<
				tab << tab << "stroke-width: 1;" << endl <<
				tab << tab << "opacity: 0.85;" << endl <<
				tab << "}" << endl <<
				"</style>" << endl;
		fig << endl;
		fig << "<script type=\"text/ecmascript\"> <![CDATA[  " << endl << endl <<
				tab << "var svg = document.querySelector('svg');" << endl <<
				endl <<
				tab << "var pt = svg.createSVGPoint();" << endl <<
				endl <<
				tab << "function cursorLocation(evt){" << endl <<
				tab << tab << "pt.x = evt.clientX; pt.y = evt.clientY;" << endl <<
				tab << tab << "return pt.matrixTransform(svg.getScreenCTM().inverse());" << endl <<
				tab << "}" << endl <<
				endl <<
				tab << "function init(evt) {" << endl <<
				tab << tab << "if ( window.svgDocument == null ) {" << endl <<
				tab << tab << tab << "svgDocument = evt.target.ownerDocument;" << endl <<
				tab << tab << "}" << endl <<
				tab << tab << "tooltip = svgDocument.getElementById('tooltip');" << endl <<
				tab << tab << "tooltip_bg = svgDocument.getElementById('tooltip_bg');" << endl <<
				tab << "}" << endl <<
				endl <<
				tab << "function ShowTooltip(evt, mouseovertext) {" << endl <<
				tab << tab << "var loc = cursorLocation(evt);" << endl <<
				endl <<
				tab << tab << "tooltip.setAttributeNS(null,\"x\",loc.x+10);" << endl <<
				tab << tab << "tooltip.setAttributeNS(null,\"y\",loc.y+30);" << endl <<
				tab << tab << "tooltip.firstChild.data = mouseovertext;" << endl <<
				tab << tab << "tooltip.setAttributeNS(null,\"visibility\",\"visible\");" << endl <<
				endl <<
				tab << tab << "length = tooltip.getComputedTextLength();" << endl <<
				tab << tab << "tooltip_bg.setAttributeNS(null,\"width\",length+8);" << endl <<
				tab << tab << "tooltip_bg.setAttributeNS(null,\"x\",loc.x+7);" << endl <<
				tab << tab << "tooltip_bg.setAttributeNS(null,\"y\",loc.y+18);" << endl <<
				tab << tab << "tooltip_bg.setAttributeNS(null,\"visibility\",\"visible\");" << endl <<
				tab << "}" << endl <<
				endl <<
				tab << "function HideTooltip(evt) {" << endl <<
				tab << tab << "tooltip.setAttributeNS(null,\"visibility\",\"hidden\");" << endl <<
				tab << tab << "tooltip_bg.setAttributeNS(null,\"visibility\",\"hidden\");" << endl <<
				tab << "}]]>" << endl <<
				"</script>" << endl;
		fig << endl;
	}

}

