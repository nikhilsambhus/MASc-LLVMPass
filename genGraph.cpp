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

void genGraph::printGraph() {
	for(auto elem : DFGbody.adjListMap) {
		errs() << "Node ";
		dispVal(elem.first);
		errs() <<  " has following neighbours ";
		for(auto vl : elem.second) {
			dispVal(vl);
		}
		errs() << "\n";
	}
}

