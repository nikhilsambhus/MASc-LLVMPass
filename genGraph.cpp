#include "genGraph.h"
void genGraph::addToGraph(Value *ins, StringRef alloc, char *type) {
	DFGbody.ldstMap[ins] = alloc;
	if(strcmp(type, "store") == 0) {
		return;
	}

	queue<Value*> valQ;
	valQ.push(ins);
	while(!valQ.empty()) {
		Value *vl = valQ.front();
		valQ.pop();
		vector<Value*> adjV;
		for(auto U : vl->users()) {
			Value *us = cast<Value> (U);
			adjV.push_back(us);
			if(DFGbody.adjListMap.find(us) == DFGbody.adjListMap.end()) {
				valQ.push(us);
			}
			//errs() << "Value used in " << *us << "\n";
		}
		DFGbody.adjListMap[vl] = adjV;
	}
}
void genGraph::computePaths(Value *vl, pathElems &allPaths) {
	queue<Value*> valQ;
	valQ.push(vl);
	bool flag = true;
	while(!valQ.empty()) {
		Value *vl = valQ.front();
		valQ.pop();
		vector<Value*> adjV = DFGbody.adjListMap[vl];
		for(Value* v : adjV) {
			valQ.push(v);
		}

		if(adjV.size() == 0) {
			flag = false;
			continue;
		}
		if(flag == true) {
			StringRef empty;
			addToPath(empty, vl, adjV, allPaths);
			flag = false;
		}
		else {
			addToPath(vl->getName(), vl, adjV, allPaths);
		}
	}
}
void genGraph::dispVal(Value *vl) {
	if(DFGbody.ldstMap.find(vl) != DFGbody.ldstMap.end()) {
		if(llvm::isa <llvm::StoreInst> (*vl)) {
			errs() << "store ";
		}
		else if(llvm::isa <llvm::LoadInst> (*vl)) {
			errs() << "load ";
		}
		errs() << DFGbody.ldstMap[vl] << " " << vl->getName() << " ";
	}
	else {
		errs() << *vl << " ";
	}
}

void genGraph::printGraph(string fname, map<Value*,int> strideMap) {
	map<Value*, uint32_t> nodeIds;
	uint32_t id = 0;
	for(auto elem : DFGbody.adjListMap) {
		errs() << "Node ";
		dispVal(elem.first);
		nodeIds[elem.first] = id;

		//add nodes to lib graph
		Value *v = elem.first;
		Instruction *inst = cast<Instruction>(v);
		const char *opName = inst->getOpcodeName();
		string name = opName;
		if(opName == std::string("call")) {
			string c_name = cast<CallInst>(inst)->getCalledFunction()->getName().str();
			//errs() << c_name;
			name = name + " " + c_name;
		}
		if(strideMap.find(v) != strideMap.end()) {
			name = name + ";" + to_string(strideMap[v]);
		}
		libGrph.addNode(id, name);
		id++;
		errs() <<  " has following neighbours ";
		for(auto vl : elem.second) {
			dispVal(vl);
		}
		errs() << "\n";
	}

	id = 0; //iterate through adj element and add edges to libgraph using stored nodeIds
	for(auto elem : DFGbody.adjListMap) {
		uint32_t src = nodeIds[elem.first];
		for(auto vl : elem.second) {
			uint32_t dest = nodeIds[vl];
			libGrph.addEdge(id, src, dest, "");	
			id++;
		}
	}

	fname = fname + ".dot";
	//errs() << fname << "\n";
	toDOT(fname, libGrph);

}

void genGraph::printPaths(pathElems &allPaths) {
	int count = 1;
	for(std::vector<Value*> pathV : allPaths) {
		errs() << "Path no. " << count << " ";
		/*for(Value *elem: pathV) {
			errs() << *(elem) << "\t";
		}*/
		count++;
		errs() << "has size " << pathV.size();
		errs() << "\t";
	}
}
void genGraph::loadPaths() {
	for(auto elem : DFGbody.adjListMap) {
		Instruction *inst = cast<Instruction>(elem.first);
		if(llvm::isa<llvm::LoadInst> (*inst)) {
			errs() << "\nLoad " << *inst << " has following path size : ";
			pathElems allPaths;
			computePaths(elem.first, allPaths);
			//errs() << allPaths.size() << "\n";
			printPaths(allPaths);
		}
	}

}
void genGraph::dispChar(const char *str) {
	for(unsigned i = 0; i < strlen(str) ; i++){
		errs() << str[i];
	}
}
void genGraph::compStats() {
	map<unsigned, unsigned> statMap;
	map<unsigned, const char *> opMap;
	for(auto elem : DFGbody.adjListMap) {
		Value *v = elem.first;
		Instruction *inst = cast<Instruction>(v);
		const char *opName = inst->getOpcodeName();
		unsigned op = inst->getOpcode();
		if(opMap.find(op) == opMap.end()) {
			opMap[op] = opName;
			statMap[op] = 1;
		}
		else {
			statMap[op] += 1;
		}
	}

	for(auto elem : statMap) {
		dispChar(opMap[elem.first]);
		errs() << " occurs " << elem.second << " times\n";
	}
	errs() << "Graph has " << libGrph.getNumNodes() << " number of nodes\n";
}
void genGraph::addToPath(StringRef src, Value *val, std::vector<Value*> destV, pathElems &allPaths ) {
	if(src.empty()) {
		for(Value* dest: destV) {
			std::vector<Value*> pathV;
			pathV.push_back(val);
			pathV.push_back(dest);
			allPaths.push_back(pathV);
		}
	}
	else {
		//search the path vector ending with source label
		//Replicate it for number of destination labels adding destination pathElem node
		for(std::vector<Value*> &pathV : allPaths) {
			Value* cmp = pathV.back();
			if(cmp->hasName() && (src == cmp->getName())) {
				Value* elem;
				elem = destV.back();
				destV.pop_back();
				pathV.push_back(elem);

				for(Value* dest: destV) {
					std::vector<Value*> cpPathV;
					copy(pathV.begin(), pathV.end(), back_inserter(cpPathV));
					Value* elem = dest;
					cpPathV.push_back(elem);
					allPaths.push_back(cpPathV);
				}
				break;
			}
		}
	}
}


