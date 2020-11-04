#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
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
  	std::vector<StringRef> labels;
	User *instr;
  };
  struct graph {
	std::map<StringRef, std::vector<struct vertex>> AList;
	std::map<StringRef, bool> visited;
  };
  struct pathElem {
  	StringRef lab;
	Instruction *inst;
  };

  typedef std::vector<std::vector<pathElem>> pathElems;
  struct Assn1Loop : public FunctionPass {
  		
	  static char ID;
	  Assn1Loop() : FunctionPass(ID) {}
	  
	  virtual void getAnalysisUsage(AnalysisUsage& AU) const override {
        	AU.addRequired<LoopInfoWrapperPass>();
		AU.addRequired<ScalarEvolutionWrapperPass>();
    	  }


	  void printGraph(struct graph gpList) {
	  	for(std::pair<StringRef, std::vector<struct vertex>> vxP : gpList.AList) {
			errs() << vxP.first << " has following neighbours: ";
			std::vector<struct vertex> vList = vxP.second;
			for(struct vertex vtx : vList) {
				errs() << "Instruction " << *vtx.instr << " with operands ";
				for(StringRef lab : vtx.labels) {
					errs() << (lab.empty() ? "None" : lab) << "\t";
				}
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


		while(!vtx_queue.empty()) {
			struct vertex nxt_vtx = vtx_queue.front();
			vtx_queue.pop();
			errs() << " Instruction: " << *nxt_vtx.instr << " Labels: ";
				for(StringRef lab : nxt_vtx.labels) {
					errs() << (lab.empty() ? "None" : lab) << "\t";
					if(lab.empty() || lab.startswith("for")) {
						continue;
					}
					std::vector<struct vertex> vListNei = graphAL.AList.find(lab)->second;
					graphAL.visited[lab] = true;
					for(struct vertex vtx : vListNei) {
						for(StringRef vlab : vtx.labels) {
							if(vlab.startswith("for")) {
								continue;
							}
							if(graphAL.visited.find(vlab)->second == false)  {
								vtx_queue.push(vtx);
								break;
							}
						}
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
			std::vector<struct vertex> neighVs;
			for(User *us : ulist) {
				std::vector<StringRef> opLabs;
				for(auto op: us->operand_values()) {
					if(!op->hasName() || op->getName() == lab) {
						continue;
					}
					opLabs.push_back(op->getName());
				}
				if(us->hasName()) {
					opLabs.push_back(us->getName());
				}

				struct vertex vtr;
				vtr.instr = us;
				vtr.labels = opLabs;
				neighVs.push_back(vtr);
			}
			graphAL.AList.insert(std::pair<StringRef, std::vector<struct vertex>>(lab, neighVs));

		}

		return graphAL;
	  }

	  void addToPath(StringRef src, Instruction *inst, std::vector<StringRef> destV, pathElems &allPaths ) {
	  	if(src.empty()) {
			for(StringRef dest: destV) {
				std::vector<pathElem> pathV;
				pathElem elem;
				elem.lab = dest;
				elem.inst = inst;
				pathV.push_back(elem);
				allPaths.push_back(pathV);
			}
		}
		else {
			//search the path vector ending with source label
			//Replicate it for number of destination labels adding destination pathElem node
			for(std::vector<pathElem> &pathV : allPaths) {
				pathElem cmp = pathV.back();
				if(src == cmp.lab) {
					pathElem elem;
					if(destV.empty()) {
						elem.lab = "";
					}
					else {
						elem.lab = destV.back();
						destV.pop_back();
					}
					elem.inst = inst;
					pathV.push_back(elem);
					for(StringRef dest: destV) {
						std::vector<pathElem> cpPathV;
						copy(pathV.begin(), pathV.end() - 1, back_inserter(cpPathV));
						pathElem elem;
						elem.inst = inst;
						elem.lab = dest;
						cpPathV.push_back(elem);
						allPaths.push_back(cpPathV);
					}
				}
			}
		}
	  }

	  void printPaths(pathElems &allPaths) {
		int count = 1;
		for(std::vector<pathElem> pathV : allPaths) {
			errs() << "Path no. " << count << " ";
			for(pathElem elem: pathV) {
				errs() << *(elem.inst) << " dest label is: " << elem.lab << "\t";
			}
			count++;
			errs() << "\n";
		}
	  }
	  void reverseClosure(llvm::BasicBlock::iterator &inst, std::map<StringRef, Value*> defsMap) {
	  	  std::queue<StringRef> labQ;
		  pathElems allPaths;
		  std::map <StringRef, bool> visited;
		  std::vector<StringRef> destV;
		  Value *v;
		  if(llvm::isa <llvm::StoreInst> (*inst)) {
		  	v = inst->getOperand(1);
		  }
		  else if(llvm::isa<llvm::LoadInst> (*inst)) {
		  	v = inst->getOperand(0);
		  }
		  labQ.push(v->getName());
		  destV.push_back(v->getName());
		  visited[v->getName()] = true;

		  StringRef empty;
		  addToPath(empty, cast<Instruction>(inst), destV, allPaths);

		  while(!labQ.empty()) {
		  	  StringRef v = labQ.front();
			  //errs() << v << " defined by " << *defsMap[v] << "\n";
			  labQ.pop();
			  Instruction* inst2 = cast<Instruction>(defsMap[v]);
			  destV.clear();
			  for(Use &U : inst2->operands()) {
			  	 Value *v = U.get();
				 if(v->getName().startswith("for") == true || v->getName().empty()) {
					 continue;
				 }
				 if(visited.find(v->getName()) == visited.end()) {
				 	labQ.push(v->getName());
			  		destV.push_back(v->getName());
					visited[v->getName()] = true;
				 }
			  }

			  addToPath(v, inst2, destV, allPaths);
			  
		  }
		  errs() << "All labels impacting the address in the instruction " << *inst << " are: ";
		  for(std::pair<StringRef, bool> elem : visited) {
		  	errs() << " " << elem.first;
		  }
		  errs() << "\nAll Paths impacting the address in this instruction are\n";
		  printPaths(allPaths);
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
				
				if(v->getName().startswith("for") == true) {
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

		
		
		/*errs() << "Defs in this function are :\n";
		for(std::pair <StringRef, Value*>elem : defsMap) {
			errs() << "Name is " << elem.first << " Instruction defining is " << *elem.second << "\n"; 
		}*/
		
		/*
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

		//printClosure(phi->getName(), graphAL, defsMap.find(phi->getName())->second);
		//printClosure("mul", graphAL, defsMap.find("mul")->second);
		/*for(std::pair <StringRef, Value*>elem : defsMap) {
			printClosure(elem.first, graphAL, elem.second);
		}*/

		/*
		StringRef it_lab = "i";
		errs() << "Iterator is " << *defsMap[it_lab] << "\n";
		printClosure(it_lab, graphAL, defsMap[it_lab]);
		*/

		LoopCounter = 0;
		LoopInfo &Li = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
		for(Loop *lit : Li) {
			LoopCounter++;
			PHINode *phinode;
			/*phinode = autotune::getInductionVariable(lit);
			errs() << *phinode << "\n";
			*/
			ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE(); 

			phinode = lit->getInductionVariable(SE);
			errs() << *phinode << "\n";
			llvm::Optional<Loop::LoopBounds> lbs = lit->getBounds(SE);
			Value& vInit = (*lbs).getInitialIVValue();
			ConstantInt *Ci;
			Ci = cast<ConstantInt>(&vInit);
			errs() << "Initialization of loop is " << Ci->getSExtValue() << "\n";

			Value& vFinal = (*lbs).getFinalIVValue();
			Ci = cast<ConstantInt>(&vFinal);
			errs() << "Loop upper bound / goes upto " << Ci->getSExtValue() << "\n";

			Value* step = (*lbs).getStepValue();
			Ci = cast<ConstantInt>(step);
			errs() << "Step value of the loop is " << Ci->getSExtValue() << "\n";

			for(BasicBlock *BB : lit->getBlocks())
                	{
                    		errs() << "basicb name: "<< BB->getName() <<"\n";
				if(BB->getName().startswith("for.body")) {
					for (BasicBlock::iterator itr = BB->begin(), e = BB->end(); itr != e; ++itr) {
						if(llvm::isa <llvm::StoreInst> (*itr) || llvm::isa<llvm::LoadInst> (*itr)) {
							reverseClosure(itr, defsMap);										
						}
					}
					
				}
                	}
		}


    		errs() << "Loop count in function " << F.getName() << " is : " << LoopCounter << "\n";
		
		return false;
	  }
  };
}

char Assn1Loop::ID = 0;
static RegisterPass<Assn1Loop> Y("assn1loop", "Assignment 1 Pass");
