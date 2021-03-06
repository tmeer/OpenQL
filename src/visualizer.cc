/**
 * @file   visualizer.cc
 * @date   08/2020
 * @author Tim van der Meer
 * @brief  definition of the visualizer
 */
 
#include "visualizer.h"
#include "visualizer_internal.h"

#include <iostream>

using namespace cimg_library;

namespace ql
{
// --- QUESTIONS ---
// wait/barrier gates do not appear in a program's gate list! how to know they are there?
//		>> should be fixed

// the measure instruction in hw config does not contain a classical line argument?
//		>> assume measure goes to the classical bit line corresponding to the number of the qubit being measured (if no args given)

// how to determine the duration of a cycle? hw dependent? is it configured somewhere?
//		>> configured in hardware config file

// --- DONE ---
// visualization of custom gates
// option to enable or disable classical bit lines
// different types of cycle/duration(ns) labels
// gate duration outlines in gate color

// -- IN PROGRESS ---
// read cycle duration from hardware config file, instead of having hardcoded value
// display wait/barrier gate
// measure custom gates go to their respective classical bit line
// 'cutting' circuits where nothing/not much is happening both in terms of idle cycles and idle qubits
// read paper of andreas

// --- FUTURE WORK ---
// TODO: implement a generic grid structure object to contain the visual structure of the circuit, to ease positioning of components in all the drawing functions
// TODO: implement actual measurement symbol
// TODO: generate default gate visuals from the configuration file
// TODO: change IOUT to DOUT (IOUT is used to avoid debug information from other source files while developing the visualizer!)
// TODO: representing the gates as waveforms
// TODO: allow the user to set the layout object from Python
// TODO: add option to save the image and/or open the window

unsigned int cycleDuration = 40;

void visualize(const ql::quantum_program* program, const Layout layout)
{
    IOUT("starting visualization...");
	
    IOUT("validating layout...");
	validateLayout(layout);

    // Get the gate list from the program.
    IOUT("getting gate list...");
    std::vector<ql::gate*> gates;
    std::vector<ql::quantum_kernel> kernels = program->kernels;
    for (ql::quantum_kernel kernel : kernels)
    {
        circuit c = kernel.get_circuit();
        gates.insert( gates.end(), c.begin(), c.end() );
    }
    
	// Calculate amount of cycles.
    IOUT("calculating amount of cycles...");
    unsigned int amountOfCycles = calculateAmountOfCycles(gates);

	// Compress the circuit in terms of cycles and gate duration if the option has been set.
	if (layout.cycles.compressCycles)
	{
        IOUT("compressing circuit...");
		std::vector<bool> filledCycles(amountOfCycles);
		for (unsigned int i = 0; i < gates.size(); i++)
		{
			filledCycles.at(gates.at(i)->cycle) = true;
		}

        //replace with DOUT
		//std::cout << "amount of cycles before compression: " << amountOfCycles << std::endl;
		unsigned int amountOfCompressions = 0;
		for (unsigned int i = 0; i < filledCycles.size(); i++)
		{
			//std::cout << i;
			if (filledCycles.at(i) == false)
			{
				//std::cout << " not filled" << std::endl;
				//std::cout << "\tcompressing... min cycle to compress: " << i - amountOfCompressions << std::endl;
				for (unsigned int j = 0; j < gates.size(); j++)
				{
					const unsigned int gateCycle = (unsigned int)gates.at(j)->cycle;
					//std::cout << "\tgate cycle: " << gateCycle;
					if (gateCycle >= i - amountOfCompressions)
					{
						gates.at(j)->cycle = gates.at(j)->cycle - 1;
						//std::cout << " -> compressing cycle" << std::endl;
					}
					else
					{
						//std::cout << " -> no compression" << std::endl;
					}
				}
				amountOfCycles--;
				amountOfCompressions++;
			}
			//else
				//std::cout << " filled" << std::endl;
		}
		//std::cout << "amount of cycles after compression: " << amountOfCycles << std::endl;
	}

	// Calculate amount of qubits and classical bits.
    IOUT("calculating amount of qubits and classical bits...");
	const unsigned int amountOfQubits = calculateAmountOfBits(gates, &gate::operands);
	const unsigned int amountOfCbits = calculateAmountOfBits(gates, &gate::creg_operands);
	CircuitData circuitData = { amountOfQubits, amountOfCbits, amountOfCycles };

    
	// Calculate image width and height based on the amount of cycles and amount of operands. The height depends on whether classical bit lines are grouped or not.
    IOUT("calculating image width and height...");
	const unsigned int width = (layout.bitLine.drawLabels ? layout.bitLine.labelColumnWidth : 0) + amountOfCycles * layout.grid.cellSize + 2 * layout.grid.borderSize;
	const unsigned int amountOfRows = amountOfQubits + (layout.bitLine.groupClassicalLines ? (amountOfCbits > 0 ? 1 : 0) : amountOfCbits);
	const unsigned int height = (layout.cycles.showCycleNumbers ? layout.cycles.rowHeight : 0) + amountOfRows * layout.grid.cellSize + 2 * layout.grid.borderSize;

    
	// Initialize image.
    IOUT("initializing image...");
	const unsigned int numberOfChannels = 3;
	CImg<unsigned char> image(width, height, 1, numberOfChannels);
	image.fill(255);

	// Draw the cycle numbers if the option has been set.
	if (layout.cycles.showCycleNumbers)
	{
        IOUT("drawing cycle numbers...");
		drawCycleNumbers(image, layout, circuitData);
	}

	// Draw the quantum and classical bit lines.
    IOUT("drawing qubit lines...");
	for (unsigned int i = 0; i < amountOfQubits; i++)
	{
		drawBitLine(image, layout, QUANTUM, i, circuitData);
	}
	
	// Draw the classical lines if enabled.
	if (layout.bitLine.showClassicalLines)
	{
		// Draw the grouped classical bit lines if the option is set.
		if (amountOfCbits > 0 && layout.bitLine.groupClassicalLines)
		{
			IOUT("drawing grouped classical bit lines...");
			drawGroupedClassicalBitLine(image, layout, circuitData);
		}
		// Otherwise draw each classical bit line seperate.
		else
		{
			IOUT("drawing ungrouped classical bit lines...");
			for (unsigned int i = amountOfQubits; i < amountOfQubits + amountOfCbits; i++)
			{
				drawBitLine(image, layout, CLASSICAL, i, circuitData);
			}
		}
	}


	// Draw the gates.
    IOUT("drawing gates...");
	for (gate* gate : gates)
	{
        //const GateVisual gateVisual = layout.gateVisuals.at(gate->type());
        IOUT("drawing gate: [name: " + gate->name + "]");
		drawGate(image, layout, circuitData, gate);
	}

	// Display the image.
    IOUT("displaying image...");
	image.display("Quantum Circuit");

    IOUT("visualization complete...");
}

void validateLayout(const Layout layout)
{

}

unsigned int calculateAmountOfBits(const std::vector<ql::gate*> gates, const std::vector<size_t> ql::gate::* operandType)
{
	//TODO: handle circuits not starting at the c- or q-bit with index 0

	unsigned int minAmount = -1; // unsigned, so -1 is equal to the maximum value of the type
	unsigned int maxAmount = 0;

	for (const gate* gate : gates)
	{
		std::vector<size_t>::const_iterator begin = (gate->*operandType).begin();
		const std::vector<size_t>::const_iterator end = (gate->*operandType).end();
		
		for (; begin != end; ++begin)
		{
			const size_t number = *begin;
			if (number < minAmount)
				minAmount = (unsigned int) number;
			if (number > maxAmount)
				maxAmount = (unsigned int) number;
		}
	}

	// If both minAmount and maxAmount are at their original values, the list of operands for all the gates was empty.
	// This means there are no operands of the given type for these gates and we return 0.
	if (minAmount == (unsigned int) -1 && maxAmount == 0)
		return 0;
	else
		return 1 + maxAmount - minAmount; // +1 because: max - min = #qubits - 1
}

unsigned int calculateAmountOfCycles(const std::vector<ql::gate*> gates)
{
    unsigned int amountOfCycles = 0;
	for (const gate* gate : gates)
	{
		const unsigned int gateCycle = (unsigned int)gate->cycle;
		if (gateCycle > amountOfCycles)
			amountOfCycles = gateCycle;
	}
	amountOfCycles++; // because the cycles start at zero, we add one to get the true amount of cycles
	const gate* lastGate = gates.at(gates.size() - 1);
	const unsigned int lastGateDuration = (unsigned int)lastGate->duration;
	const unsigned int lastGateDurationInCycles = lastGateDuration / cycleDuration;
	if (lastGateDurationInCycles > 1)
	{
		amountOfCycles += lastGateDurationInCycles - 1;
	}

    return amountOfCycles;
}

void drawCycleNumbers(cimg_library::CImg<unsigned char>& image, const Layout layout, const CircuitData circuitData)
{
	for (unsigned int i = 0; i < circuitData.amountOfCycles; i++)
	{
		std::string cycleLabel;
		if (layout.cycles.showCyclesInNanoSeconds)
		{
			cycleLabel = std::to_string(i * cycleDuration);
		}
		else
		{
			cycleLabel = std::to_string(i);
		}
		
		const char* text = cycleLabel.c_str();
		CImg<unsigned char> imageTextDimensions;
		const unsigned char color = 1;
		imageTextDimensions.draw_text(0, 0, text, &color, 0, 1, layout.bitLine.fontHeight);
		const unsigned int textWidth = imageTextDimensions.width();
		const unsigned int textHeight = imageTextDimensions.height();

		const unsigned int labelColumnWidth = layout.bitLine.drawLabels ? layout.bitLine.labelColumnWidth : 0;
		const unsigned int xGap = (layout.grid.cellSize - textWidth) / 2;
		const unsigned int yGap = (layout.grid.cellSize - textHeight) / 2;
		const unsigned int xCycle = layout.grid.borderSize + labelColumnWidth + i * layout.grid.cellSize + xGap;
		const unsigned int yCycle = layout.grid.borderSize + yGap;

		image.draw_text(xCycle, yCycle, text, layout.cycles.fontColor.data(), 0, 1, layout.cycles.fontHeight);
	}
}

void drawBitLine(cimg_library::CImg<unsigned char> &image, const Layout layout, const BitType bitType, const unsigned int row, const CircuitData circuitData)
{
	const unsigned int cycleNumbersRowHeight = layout.cycles.showCycleNumbers ? layout.cycles.rowHeight : 0;
	const unsigned int labelColumnWidth = layout.bitLine.drawLabels ? layout.bitLine.labelColumnWidth : 0;
	const unsigned int x0 = labelColumnWidth + layout.grid.borderSize;
	const unsigned int x1 = labelColumnWidth + layout.grid.borderSize + circuitData.amountOfCycles * layout.grid.cellSize;
	const unsigned int y = cycleNumbersRowHeight + layout.grid.borderSize + row * layout.grid.cellSize + layout.grid.cellSize / 2;

	std::array<unsigned char, 3> bitLineColor;
	std::array<unsigned char, 3> bitLabelColor;
	switch (bitType)
	{
		case CLASSICAL:
			bitLineColor = layout.bitLine.cBitLineColor;
			bitLabelColor = layout.bitLine.cBitLabelColor;
			break;
		case QUANTUM:
			bitLineColor = layout.bitLine.qBitLineColor;
			bitLabelColor = layout.bitLine.qBitLabelColor;
			break;
	}

	image.draw_line(x0, y, x1, y, bitLineColor.data());

	// Draw the bit line label if enabled.
	if (layout.bitLine.drawLabels)
	{
		const unsigned int bitIndex = (bitType == CLASSICAL) ? (row - circuitData.amountOfQubits) : row;
		const std::string bitTypeText = (bitType == CLASSICAL) ? "c" : "q";
		std::string label = bitTypeText + std::to_string(bitIndex);
		const char* text = label.c_str();
		CImg<unsigned char> imageTextDimensions;
		const unsigned char color = 1;
		imageTextDimensions.draw_text(0, 0, text, &color, 0, 1, layout.bitLine.fontHeight);
		const unsigned int textWidth = imageTextDimensions.width();
		const unsigned int textHeight = imageTextDimensions.height();
		const unsigned int xGap = (layout.grid.cellSize - textWidth) / 2;
		const unsigned int yGap = (layout.grid.cellSize - textHeight) / 2;
		const unsigned int xLabel = layout.grid.borderSize + xGap;
		const unsigned int yLabel = layout.grid.borderSize + cycleNumbersRowHeight + row * layout.grid.cellSize + yGap;
		image.draw_text(xLabel, yLabel, text, bitLabelColor.data(), 0, 1, layout.bitLine.fontHeight);
	}
}

void drawGroupedClassicalBitLine(cimg_library::CImg<unsigned char>& image, const Layout layout, const CircuitData circuitData)
{
	const unsigned int cycleNumbersRowHeight = layout.cycles.showCycleNumbers ? layout.cycles.rowHeight : 0;
	const unsigned int labelColumnWidth = layout.bitLine.drawLabels ? layout.bitLine.labelColumnWidth : 0;
	const unsigned int x0 = labelColumnWidth + layout.grid.borderSize;
	const unsigned int x1 = labelColumnWidth + layout.grid.borderSize + circuitData.amountOfCycles * layout.grid.cellSize;
	const unsigned int y = cycleNumbersRowHeight + layout.grid.borderSize + circuitData.amountOfQubits * layout.grid.cellSize + layout.grid.cellSize / 2;

	image.draw_line(x0, y - layout.bitLine.groupedClassicalLineGap, x1, y - layout.bitLine.groupedClassicalLineGap, layout.bitLine.cBitLineColor.data());
	image.draw_line(x0, y + layout.bitLine.groupedClassicalLineGap, x1, y + layout.bitLine.groupedClassicalLineGap, layout.bitLine.cBitLineColor.data());
	//TODO: store the dashed line parameters in the layout object
	image.draw_line(x0 + 8, y + layout.bitLine.groupedClassicalLineGap + 2, x0 + 12, y - layout.bitLine.groupedClassicalLineGap - 3, layout.bitLine.cBitLineColor.data());
	//TODO: draw a number indicating the amount of classical lines that are grouped
	const std::string label = std::to_string(circuitData.amountOfClassicalBits);
	const char* text = label.c_str();
	CImg<unsigned char> imageTextDimensions;
	const unsigned char color = 1;
	imageTextDimensions.draw_text(0, 0, text, &color, 0, 1, layout.bitLine.fontHeight);
	//const unsigned int textWidth = imageTextDimensions.width();
	//const unsigned int textHeight = imageTextDimensions.height();
	const unsigned int xLabel = x0 + 8;
	const unsigned int yLabel = y - layout.bitLine.groupedClassicalLineGap - 3 - 13;
	image.draw_text(xLabel, yLabel, text, layout.bitLine.cBitLabelColor.data(), 0, 1, layout.bitLine.fontHeight);

	// Draw the bit line label if enabled.
	if (layout.bitLine.drawLabels)
	{
		const std::string label = "C";
		const char* text = label.c_str();
		CImg<unsigned char> imageTextDimensions;
		const unsigned char color = 1;
		imageTextDimensions.draw_text(0, 0, text, &color, 0, 1, layout.bitLine.fontHeight);
		const unsigned int textWidth = imageTextDimensions.width();
		const unsigned int textHeight = imageTextDimensions.height();
		const unsigned int xGap = (layout.grid.cellSize - textWidth) / 2;
		const unsigned int yGap = (layout.grid.cellSize - textHeight) / 2;
		const unsigned int xLabel = layout.grid.borderSize + xGap;
		const unsigned int yLabel = layout.grid.borderSize + cycleNumbersRowHeight + circuitData.amountOfQubits * layout.grid.cellSize + yGap;
		image.draw_text(xLabel, yLabel, text, layout.bitLine.cBitLabelColor.data(), 0, 1, layout.bitLine.fontHeight);
	}
}

void drawGate(cimg_library::CImg<unsigned char> &image, const Layout layout, const CircuitData circuitData, gate* const gate)
{
	const unsigned int amountOfOperands = (unsigned int)gate->operands.size() + (unsigned int)gate->creg_operands.size();
	const unsigned int cycleNumbersRowHeight = layout.cycles.showCycleNumbers ? layout.cycles.rowHeight : 0;
	const unsigned int labelColumnWidth = layout.bitLine.drawLabels ? layout.bitLine.labelColumnWidth : 0;
	
	IOUT("drawing gate with name: '" << gate->name << "'");
	
	GateVisual gateVisual;
	if (gate->type() == __custom_gate__)
	{
		IOUT("Custom gate found. Using user specified visualization.");
		gateVisual = gate->gateVisual;
	}
	else
	{
		IOUT("Default gate found. Using default visualization!");
		gateVisual = layout.defaultGateVisuals.at(gate->type());
	}

	if (amountOfOperands > 1)
	{
        IOUT("setting up multi-operand gate...");
		// Draw the lines between each node. If this is done before drawing the nodes, there is no need to calculate line segments, we can just draw one
		// big line between the nodes and the nodes will be drawn on top of those.
		// Note: does not work with transparent nodes! If those are ever implemented, the connection line drawing will need to be changed!

		unsigned int minRow = -1; // maximum value of an unsigned int is equal to -1
		unsigned int maxRow = 0;
		for (unsigned int i = 0; i < gate->operands.size(); i++)
		{
			const unsigned int operand = gate->operands.at(i);
			if (operand < minRow)
				minRow = operand;
			if (operand > maxRow)
				maxRow = operand;
		}
		if (layout.bitLine.groupClassicalLines)
		{
			if (gate->creg_operands.size() > 0)
			{
				maxRow = circuitData.amountOfQubits;
			}
		}
		else
		{
			for (unsigned int i = 0; i < gate->creg_operands.size(); i++)
			{
				const unsigned int operand = gate->creg_operands.at(i) + circuitData.amountOfQubits;
				if (operand < minRow)
					minRow = operand;
				if (operand > maxRow)
					maxRow = operand;
			}
		}
		const unsigned int column = (unsigned int)gate->cycle;

		Position4 connectionPosition =
		{
			layout.grid.borderSize + labelColumnWidth + column * layout.grid.cellSize + layout.grid.cellSize / 2,
			layout.grid.borderSize + cycleNumbersRowHeight + minRow * layout.grid.cellSize + layout.grid.cellSize / 2,
			layout.grid.borderSize + labelColumnWidth + column * layout.grid.cellSize + layout.grid.cellSize / 2,
			layout.grid.borderSize + cycleNumbersRowHeight + maxRow * layout.grid.cellSize + layout.grid.cellSize / 2
		};

		//TODO: probably have connection line type as part of a gate's visual definition
		if (gate->type() == __measure_gate__)
		{
			if (layout.measurements.drawConnection)
			{
				const unsigned groupedClassicalLineOffset = layout.bitLine.groupClassicalLines ? layout.bitLine.groupedClassicalLineGap : 0;

				image.draw_line(connectionPosition.x0 - layout.measurements.lineSpacing, connectionPosition.y0,
					connectionPosition.x1 - layout.measurements.lineSpacing, connectionPosition.y1 - layout.measurements.arrowSize - groupedClassicalLineOffset,
					gateVisual.connectionColor.data());

				image.draw_line(connectionPosition.x0 + layout.measurements.lineSpacing, connectionPosition.y0,
					connectionPosition.x1 + layout.measurements.lineSpacing, connectionPosition.y1 - layout.measurements.arrowSize - groupedClassicalLineOffset,
					gateVisual.connectionColor.data());

				const unsigned int x0 = connectionPosition.x1 - layout.measurements.arrowSize / 2;
				const unsigned int y0 = connectionPosition.y1 - layout.measurements.arrowSize - groupedClassicalLineOffset;
				const unsigned int x1 = connectionPosition.x1 + layout.measurements.arrowSize / 2;
				const unsigned int y1 = connectionPosition.y1 - layout.measurements.arrowSize - groupedClassicalLineOffset;
				const unsigned int x2 = connectionPosition.x1;
				const unsigned int y2 = connectionPosition.y1 - groupedClassicalLineOffset;
				image.draw_triangle(x0, y0, x1, y1, x2, y2, gateVisual.connectionColor.data(), 1);
			}
		}
		else
		{
			image.draw_line(connectionPosition.x0, connectionPosition.y0, connectionPosition.x1, connectionPosition.y1, gateVisual.connectionColor.data());
		}
        IOUT("finished setting up multi-operand gate");
	}

	// Draw the gate duration outline if the option has been set.
	if (!layout.cycles.compressCycles && layout.cycles.showGateDurationOutline)
	{
        IOUT("drawing gate duration outline...");
		const unsigned int gateDurationInCycles = ((unsigned int)gate->duration) / cycleDuration;
		// Only draw the gate outline if the gate takes more than one cycle.
		if (gateDurationInCycles > 1)
		{
			for (unsigned int i = 0; i < amountOfOperands; i++)
			{
				const unsigned int columnStart = (unsigned int)gate->cycle;
				const unsigned int columnEnd = columnStart + gateDurationInCycles - 1;
				const unsigned int row = gate->operands[i];

				const unsigned int x0 = layout.grid.borderSize + labelColumnWidth + columnStart * layout.grid.cellSize + layout.cycles.gateDurationGap;
				const unsigned int y0 = layout.grid.borderSize + cycleNumbersRowHeight + row * layout.grid.cellSize + layout.cycles.gateDurationGap;
				const unsigned int x1 = layout.grid.borderSize + labelColumnWidth + (columnEnd + 1) * layout.grid.cellSize - +layout.cycles.gateDurationGap;
				const unsigned int y1 = layout.grid.borderSize + cycleNumbersRowHeight + (row + 1) * layout.grid.cellSize - +layout.cycles.gateDurationGap;
				
				// Draw the outline in the colors of the node.
				const Node node = gateVisual.nodes.at(i);
				image.draw_rectangle(x0, y0, x1, y1, node.backgroundColor.data(), layout.cycles.gateDurationAlpha);
				image.draw_rectangle(x0, y0, x1, y1, node.outlineColor.data(), layout.cycles.gateDurationOutLineAlpha, 0xF0F0F0F0);
				
				//image.draw_rectangle(x0, y0, x1, y1, layout.cycles.gateDurationOutlineColor.data(), layout.cycles.gateDurationAlpha);
				//image.draw_rectangle(x0, y0, x1, y1, layout.cycles.gateDurationOutlineColor.data(), layout.cycles.gateDurationOutLineAlpha, 0xF0F0F0F0);
			}
		}
	}

	// Draw the nodes.
    IOUT("drawing gate nodes...");
	for (unsigned int i = 0; i < amountOfOperands; i++)
	{
        IOUT("drawing gate node with index: " + std::to_string(i) + "...");
        //TODO: change the try-catch later on! the gate config will be read from somewhere else than the default layout
        try
        {
		    const Node node = gateVisual.nodes.at(i);
            const BitType operandType = (i >= gate->operands.size()) ? CLASSICAL : QUANTUM;
            const unsigned int index = (operandType == QUANTUM) ? i : (i - (unsigned int)gate->operands.size());
            const NodePositionData positionData =
            {
	            (layout.grid.cellSize - node.radius * 2) / 2,
	            labelColumnWidth,
	            cycleNumbersRowHeight,
	            (unsigned int)gate->cycle,
	            operandType == CLASSICAL ? (unsigned int)gate->creg_operands.at(index) + circuitData.amountOfQubits : (unsigned int)gate->operands.at(index)
            };

            switch (node.type)
            {
	            case NONE:		DOUT("node.type = NONE"); break; // Do nothing.
	            case GATE:		DOUT("node.type = GATE"); drawGateNode(image, layout, circuitData, node, positionData); break;
	            case CONTROL:	DOUT("node.type = CONTROL"); drawControlNode(image, layout, circuitData, node, positionData); break;
	            case NOT:		DOUT("node.type = NOT"); drawNotNode(image, layout, circuitData, node, positionData); break;
	            case CROSS:		DOUT("node.type = CROSS"); drawCrossNode(image, layout, circuitData, node, positionData); break;
                default:        EOUT("Unknown gate display node type!"); break;
            }
        }
        catch (const std::out_of_range& e)
        {
            return;
        }
		
        IOUT("finished drawing gate node with index: " + std::to_string(i) + "...");
	}

	// Draw the measurement symbol.
		//const unsigned int xGap = 2;
		//const unsigned int yGap = 13;

		//const unsigned int x0 = position.x0 + xGap;
		//const unsigned int y0 = position.y0 + yGap;
		//const unsigned int x1 = position.x1 + xGap;
		//const unsigned int y1 = y0;

		//const unsigned int xa = x0 + (x1 - x0) / 3;
		//const unsigned int ya = y0 + yGap / 2;
		//const unsigned int xb = x1 - (x1 - x0) / 3;
		//const unsigned int yb = ya;

		//const unsigned int u0 = xa - x0;
		//const unsigned int v0 = ya - y0;
		//const unsigned int u1 = x1 - xb;
		//const unsigned int v1 = y1 - yb;

		//image.draw_spline(x0, y0, u0, v0, x1, y1, u1, v1, layout.operation.gateNameColor.data());
}

void drawGateNode(cimg_library::CImg<unsigned char>& image, const Layout layout, const CircuitData circuitData, const Node node, const NodePositionData positionData)
{
	const Position4 position =
	{
		layout.grid.borderSize + positionData.labelColumnWidth + positionData.column * layout.grid.cellSize + positionData.gap,
		layout.grid.borderSize + positionData.cycleNumbersRowHeight + positionData.row * layout.grid.cellSize + positionData.gap,
		layout.grid.borderSize + positionData.labelColumnWidth + (positionData.column + 1) * layout.grid.cellSize - positionData.gap,
		layout.grid.borderSize + positionData.cycleNumbersRowHeight + (positionData.row + 1) * layout.grid.cellSize - positionData.gap
	};

	// Draw the gate background.
	image.draw_rectangle(position.x0, position.y0, position.x1, position.y1, node.backgroundColor.data());
	image.draw_rectangle(position.x0, position.y0, position.x1, position.y1, node.outlineColor.data(), 1, 0xFFFFFFFF);

	// Draw the gate symbol. The width and height of the symbol are calculated first to correctly position the symbol within the gate.
	const char* text = node.displayName.c_str();
	CImg<unsigned char> imageTextDimensions;
	const unsigned char color = 1;
	imageTextDimensions.draw_text(0, 0, text, &color, 0, 1, node.fontHeight);
	const unsigned int textWidth = imageTextDimensions.width();
	const unsigned int textHeight = imageTextDimensions.height();
	image.draw_text(position.x0 + (node.radius * 2 - textWidth) / 2, position.y0 + (node.radius * 2 - textHeight) / 2, text, node.fontColor.data(), 0, 1, node.fontHeight);
}

void drawControlNode(cimg_library::CImg<unsigned char>& image, const Layout layout, const CircuitData circuitData, const Node node, const NodePositionData positionData)
{
	const Position2 connectionPosition =
	{
		layout.grid.borderSize + positionData.labelColumnWidth + positionData.column * layout.grid.cellSize + layout.grid.cellSize / 2,
		layout.grid.borderSize + positionData.cycleNumbersRowHeight + positionData.row * layout.grid.cellSize + layout.grid.cellSize / 2
	};

	image.draw_circle(connectionPosition.x, connectionPosition.y, node.radius, node.backgroundColor.data());
}

void drawNotNode(cimg_library::CImg<unsigned char>& image, const Layout layout, const CircuitData circuitData, const Node node, const NodePositionData positionData)
{
	// TODO: allow for filled not node instead of only an outline not node

	const Position2 notPosition =
	{
		layout.grid.borderSize + positionData.labelColumnWidth + positionData.column * layout.grid.cellSize + layout.grid.cellSize / 2,
		layout.grid.borderSize + positionData.cycleNumbersRowHeight + positionData.row * layout.grid.cellSize + layout.grid.cellSize / 2
	};

	// Draw the outlined circle.
	image.draw_circle(notPosition.x, notPosition.y, node.radius, node.backgroundColor.data(), 1, 0xFFFFFFFF);

	// Draw two lines to represent the plus sign.
	const unsigned int xHor0 = notPosition.x - node.radius;
	const unsigned int xHor1 = notPosition.x + node.radius;
	const unsigned int yHor = notPosition.y;

	const unsigned int xVer = notPosition.x;
	const unsigned int yVer0 = notPosition.y - node.radius;
	const unsigned int yVer1 = notPosition.y + node.radius;

	image.draw_line(xHor0, yHor, xHor1, yHor, node.backgroundColor.data());
	image.draw_line(xVer, yVer0, xVer, yVer1, node.backgroundColor.data());
}

void drawCrossNode(cimg_library::CImg<unsigned char>& image, const Layout layout, const CircuitData circuitData, const Node node, const NodePositionData positionData)
{
	const Position2 crossPosition =
	{
		layout.grid.borderSize + positionData.labelColumnWidth + positionData.column * layout.grid.cellSize + layout.grid.cellSize / 2,
		layout.grid.borderSize + positionData.cycleNumbersRowHeight + positionData.row * layout.grid.cellSize + layout.grid.cellSize / 2
	};

	// Draw two diagonal lines to represent the cross.
	const unsigned int x0 = crossPosition.x - node.radius;
	const unsigned int y0 = crossPosition.y - node.radius;
	const unsigned int x1 = crossPosition.x + node.radius;
	const unsigned int y1 = crossPosition.y + node.radius;

	image.draw_line(x0, y0, x1, y1, node.backgroundColor.data());
	image.draw_line(x0, y1, x1, y0, node.backgroundColor.data());
}

} // ql
