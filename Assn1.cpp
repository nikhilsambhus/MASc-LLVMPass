#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "LoopUtils.h"
#include "llvm/IR/InstIterator.h"
#include <queue>
using namespace llvm;

#define DEBUG_TYPE "assn1"
STATISTIC(FuncCounter, "Counts number of functions greeted");
STATISTIC(InstCounter, "Counts number of instructions greeted");
STATISTIC(LoadCounter, "Counts number of load instructions greeted");
STATISTIC(StoreCounter, "Counts number of store instructions greeted");
STATISTIC(LoopCounter, "Counts number of loops greeted");
STATISTIC(BBCounter, "Counts number of basic blocks greeted");

namespace {
  struct vertex {
  	StringRef label;
	User *instr;
  };
  struct graph {
	std::map<StringRef, std::vector<struct vertex>> AList;
	std::map<StringRef, bool> visited;
  };
  struct Assn1Loop : public FunctionPass {
  		
	  static char ID;
	  Assn1Loop() : FunctionPass(ID) {}
	  
	  virtual void getAnalysisUsage(AnalysisUsage& AU) const override {
        	AU.addRequired<LoopInfoWrapperPass>();
    	  }


	  void printGraph(struct graph gpList) {
	  	for(std::pair<StringRef, std::vector<struct vertex>> vxP : gpList.AList) {
			errs() << vxP.first << " has following neighbours: ";
			std::vector<struct vertex> vList = vxP.second;
			for(struct vertex vtx : vList) {
				errs() << "Instruction " << *vtx.instr << " with operand " << (vtx.label.empty() ? "None" : vtx.label) << "\t";
			}
			errs() << "\n";
		}
	  }

	  void initVisit(struct graph *graphAL) {
	  	for(std::pair<StringRef, std::vector<struct vertex>> elem : graphAL->AList) {
			graphAL->visited[elem.first] = false;
		}
          }
	  void printClosure(StringRef lab, struct graph graphAL, Value* def) {
	  	errs() << "Definition of " << lab << " is " << *def << " Its closure is ";
		initVisit(&graphAL);

		std::queue<struct vertex> vtx_queue;
		std::vector<struct vertex> vList = graphAL.AList.find(lab)->second;
		graphAL.visited[lab] = true;
		for(struct vertex vtx : vList) {
			vtx_queue.push(vtx);
		}


		while(!vtx_queue.empty() ) {
			struct vertex nxt_vtx = vtx_queue.front();
			vtx_queue.pop();
			errs() << " Instruction: " << *nxt_vtx.instr << " Label: " << (nxt_vtx.label.empty() ? "None" : nxt_vtx.label) << "\t";
			if(nxt_vtx.label.empty()) {
				continue;
			}
			std::vector<struct vertex> vListNei = graphAL.AList.find(nxt_vtx.label)->second;
			graphAL.visited[nxt_vtx.label] = true;
			for(struct vertex vtx : vListNei) {
				if(graphAL.visited.find(vtx.label)->second == false) {
					vtx_queue.push(vtx);
				}
			}
			
		}

		errs() << "\n";

	  }
	  struct graph convertToGraph(std::map<StringRef, Value*> defs, std::map<StringRef, std::vector<User*>> uses) {
	 	struct graph graphAL;

		for(std::pair <StringRef, std::vector<User*>>elem : uses) {
			StringRef lab = elem.first;
			std::vector<User *> ulist = elem.second;
			//errs() << "\n" << lab << " Is the base label\n";
			std::vector<struct vertex> neighVs;
			for(User *us : ulist) {
				//errs() << "Instruction "<< *us << " has operand ";
				StringRef opLab;
				for(auto op: us->operand_values()) {
					if(!op->hasName() || op->getName() == lab) {
						continue;
					}
					opLab = op->getName();
				}
				if(opLab.empty() && us->hasName()) {
					opLab = us->getName();
				}

				/*if(opLab.empty()) {
					errs() << "No operand other than base ";
				}
				else {
					errs() << opLab << " ";
				}*/
				struct vertex vtr;
				vtr.instr = us;
				vtr.label = opLab;
				neighVs.push_back(vtr);
			}
			graphAL.AList.insert(std::pair<StringRef, std::vector<struct vertex>>(lab, neighVs));

		}

		return graphAL;
	  }

	  bool runOnFunction(Function &F) override {
		errs() << "\nFunction name : " << F.getName() << "\n";
		InstCounter = 0;
		LoadCounter = 0;
		StoreCounter = 0;
		std::map<StringRef, Value*> defsMap;
		std::map<StringRef, std::vector<User *>> usesMap;
		for(inst_iterator itr = inst_begin(F), etr = inst_end(F); itr != etr; itr++) {
			InstCounter++;

			for (Use &U : itr->operands()) {
				Value *v = U.get();
				if(v->hasName() == false) {
					continue;
				}
				
				if(defsMap.find(v->getName()) != defsMap.end()) {
					continue;
				}
				defsMap.insert(std::pair<StringRef, Value*>(v->getName(), v));
				//errs() << *v << " with name "<< v->getName() << " with type "<< *(v->getType()) << " Uses are: ";
				std::vector<User*> users;
				for(use_iter : v->uses() ) {
					User *ut = use_iter.getUser();
					users.push_back(ut);
					//errs() << *ut  << " " ;
				}
				usesMap.insert(std::pair<StringRef, std::vector<User *>>(v->getName(), users));
				//errs() << "Uses ends ";
			}

			if(llvm::isa <llvm::StoreInst> (*itr)) {
				StoreCounter++;
			}
			else if(llvm::isa <llvm::LoadInst> (*itr)) {
				LoadCounter++;
			}

			errs() << "Instruction is " << *itr << "\n";

      		}

		/*
		errs() << "Defs in this function are :\n";
		for(std::pair <StringRef, Value*>elem : defsMap) {
			errs() << "Name is " << elem.first << " Instruction defining is " << *elem.second << "\n"; 
		}

		errs() << "Uses in this function are :\n";
		for(std::pair <StringRef, std::vector<User*>>elem : usesMap) {
			errs() << "Name is " << elem.first << " Instruction using are ";
			std::vector<User*> users = elem.second; 
			for(auto& it : users) {
				errs() << *it << " ";
			}
			errs() << "\n";
		}*/

		struct graph graphAL = convertToGraph(defsMap, usesMap);
		
		//printGraph(graphAL);

		for(std::pair <StringRef, Value*>elem : defsMap) {
			printClosure(elem.first, graphAL, elem.second);
		}

		LoopCounter = 0;
		LoopInfo &Li = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
		for(Loop *lit : Li) {
			LoopCounter++;
			PHINode *phinode = autotune::getInductionVariable(lit);
			//errs() << *phinode << "\n";
			for(BasicBlock *BB : lit->getBlocks())
                	{
                    		errs() << "basicb name: "<< BB->getName() <<"\n";
                	}
		}


    		errs() << "Loop count in function " << F.getName() << " is : " << LoopCounter << "\n";
		return false;
	  }
  };
}

char Assn1Loop::ID = 0;
static RegisterPass<Assn1Loop> Y("assn1loop", "Assignment 1 Pass");
