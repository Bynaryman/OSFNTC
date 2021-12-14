#include "Instance.hpp"

using namespace std;

namespace flopoco {

	Instance::Instance(std::string name_, Operator* op_) :
		name(name_), op(op_)
	{
		clearInputPorts();
		clearOutputPorts();

		hasBeenImplemented = false;
	}

	Instance::Instance(std::string name_, Operator* op_, map<std::string, Signal*> inPortMap_, map<std::string, Signal*> outPortMap_) :
		name(name_), op(op_)
	{
		clearInputPorts();
		clearOutputPorts();

		hasBeenImplemented = false;

		addInputPorts(inPortMap_);
		addOutputPorts(outPortMap_);
	}

	Instance::~Instance(){};


	std::string Instance::getName()
	{
		return name;
	}

	void Instance::setName(std::string name_)
	{
		name = name_;
	}

	Operator* Instance::getOperator()
	{
		return op;
	}

	void Instance::setOperator(Operator* op_)
	{
		op = op_;
	}

	bool Instance::getHasBeenImplemented()
	{
		return hasBeenImplemented;
	}

	void Instance::setHasBeenImplemented(bool hasBeenImplemented_)
	{
		hasBeenImplemented_ = hasBeenImplemented;
	}

	void Instance::addInputPort(std::string portName, Signal* connectedSignal)
	{
		//check if the port mapping already exists
		if(inPortMap.find(portName) != inPortMap.end())
			throw("Error in Instance::addInputPort: trying to add a port mapping that already exists: port name = " + portName);

		inPortMap[portName] = connectedSignal;
	}

	void Instance::addInputPorts(map<std::string, Signal*> signals)
	{
		//check if the list of signals is non-null
		if(signals.size() == 0)
			throw("Error in Instance::addInputPorts: trying to add an empty list of port mappings");

		for(map<std::string, Signal*>::iterator it = signals.begin(); it != signals.end(); it++)
			if(it->first == "")
				throw("Error in Instance::addInputPorts: trying to add a port mappings with an empty port name");
			else if(it->second == NULL)
				throw("Error in Instance::addInputPorts: trying to add a port mappings with an empty signal");
			else
				addInputPort(it->first, it->second);
	}

	void Instance::removeInputPort(std::string portName)
	{
		//check if the port mapping exists
		if(inPortMap.find(portName) == inPortMap.end())
			throw("Error in Instance::removeInputPort: trying to remove a port mapping that does not exists: port name = " + portName);

		inPortMap.erase(portName);
	}

	void Instance::clearInputPorts()
	{
		inPortMap.clear();
	}

	void Instance::addOutputPort(std::string portName, Signal* connectedSignal)
	{
		//check if the port mapping already exists
		if(outPortMap.find(portName) != outPortMap.end())
			throw("Error in Instance::addOutputPort: trying to add a port mapping that already exists: port name = " + portName);

		outPortMap[portName] = connectedSignal;
	}

	void Instance::addOutputPorts(map<std::string, Signal*> signals)
	{
		//check if the list of signals is non-null
		if(signals.size() == 0)
			throw("Error in Instance::addOutputPorts: trying to add an empty list of port mappings");

		for(map<std::string, Signal*>::iterator it = signals.begin(); it != signals.end(); it++)
			if(it->first == "")
				throw("Error in Instance::addOutputPort: trying to add a port mappings with an empty port name");
			else if(it->second == NULL)
				throw("Error in Instance::addOutputPort: trying to add a port mappings with an empty signal");
			else
				addOutputPort(it->first, it->second);
	}

	void Instance::removeOutputPort(std::string portName)
	{
		//check if the port mapping exists
		if(outPortMap.find(portName) == outPortMap.end())
			throw("Error in Instance::removeInputPort: trying to remove a port mapping that does not exists: port name = " + portName);

		outPortMap.erase(portName);
	}

	void Instance::clearOutputPorts()
	{
		outPortMap.clear();
	}

} // namespace flopoco
