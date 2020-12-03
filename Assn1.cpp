#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopNestAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Support/raw_ostream.h"
#include "LoopUtils.h"
#include "DervInd.h"
#include "llvm/IR/InstIterator.h"
#include <queue>
using namespace llvm;
using namespace std;

#define DEBUG_TYPE "assn1"
STATISTIC(FuncCounter, "Counts number of functions greeted");
STATISTIC(InstCounter, "Counts number of instructions greeted");
STATISTIC(LoadCounter, "Counts number of load instructions greeted");
STATISTIC(StoreCounter, "Counts number of store instructions greeted");
STATISTIC(LoopCounter, "Counts number of loops greeted");
STATISTIC(BBCounter, "Counts number of basic blocks greeted");

namespace {
  struct pathElem {
  	StringRef lab;
	Instruction *inst;
  };

  struct LoopData {
  	Loop *lp;
  	int initV;
	int stepV;
	int finalV;
	StringRef indVar;
  };

  typedef std::vector<std::vector<pathElem>> pathElems;
  struct Assn1Loop : public FunctionPass {
  		
	  static char ID;
	  Assn1Loop() : FunctionPass(ID) {}
	  
	  virtual void getAnalysisUsage(AnalysisUsage& AU) const override {
        	AU.addRequired<LoopInfoWrapperPass>();
		AU.addRequired<ScalarEvolutionWrapperPass>();
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
	  StringRef checkAlloc(std::vector<pathElem> &pathV, unsigned int nextpos) {
	  	StringRef alloc;
		while(nextpos < pathV.size()) {
			pathElem elem = pathV[nextpos];
			if(llvm::isa<llvm::GetElementPtrInst> (*elem.inst)) {
				break;
			}
			if(llvm::isa<llvm::AllocaInst> (*elem.inst)) {
				Value *v = cast<Value>(elem.inst);
				alloc = v->getName();
				break;
			}
			nextpos++;
		}


		return alloc;
	  }

	  StringRef checkInd(std::vector<pathElem> &pathV, unsigned int nextpos) {
	  	StringRef ind;
		while(nextpos < pathV.size()) {
			pathElem elem = pathV[nextpos];
			if(llvm::isa<llvm::PHINode>(*elem.inst)) {
				Value *v = cast<Value>(elem.inst);
				ind = v->getName();
				break;
			}
			nextpos++;
		}
		return ind;
	  }

	  void analyzePathLoop(pathElems &allPaths, std::vector<struct LoopData> &loopDataV, llvm::BasicBlock::iterator &inst) {
	  	StringRef alloc;
		std::map<StringRef, int> indVarMap;
		for(std::vector<pathElem> pathV : allPaths) {
			unsigned int nextpos = 0;
			unsigned int allocEptrCnt = 0;
			for(pathElem elem : pathV) {
				nextpos++;
				if(llvm::isa<llvm::GetElementPtrInst> (*elem.inst)) {
					if(elem.inst->getOperand(0)->getName() == elem.lab) {
						StringRef ret;
						ret = checkAlloc(pathV, nextpos);
						if(ret.empty()) {
							allocEptrCnt++;
						}
						else {
							alloc = ret;
						}
					}
					else if(elem.inst->getOperand(2)->getName() == elem.lab) {
						StringRef indVar = checkInd(pathV, nextpos);
						if(allocEptrCnt) {
							indVarMap[indVar] = 0;
						}
						else {
							indVarMap[indVar] = 1;
						}
						
					}
				}

			}
		}

		if(!alloc.empty() && !indVarMap.empty()) {
			if(indVarMap.size() == 1) { //singple loop logic
				if(indVarMap.find(loopDataV[0].indVar) != indVarMap.end()) {
					errs() << *inst << " defines a stream accessing the array " << alloc << " starting from location " << loopDataV[0].initV << " going upto " << loopDataV[0].finalV << " with the step value of " << loopDataV[0].stepV << "\n"; 
				}
			}
			else if(indVarMap.size() == 2) { //nested loop, total 2 loops logic
				for(struct LoopData loopData : loopDataV) {
					if(indVarMap.find(loopData.indVar) == indVarMap.end()) {
						return;
					}
				}
				//induction variables found in paths match the oncs found in loops
				errs() << *inst << " defines a stream accessing the 2D array " << alloc << " with ";
				for(std::pair<StringRef, int> indVar : indVarMap) {
					errs() << indVar.first << " as the " << (indVar.second == 0 ? "first index " : "second index ");
				}
				errs() << "\n";
				for(struct LoopData loopData : loopDataV) {
					errs() << loopData.indVar << " goes from " << loopData.initV << " to " << loopData.finalV << " with a step value of " << loopData.stepV << " " ;
				}
				errs() << loopDataV[1].indVar << " is the inner loop induction variable\n";
			}
		}

	  }
	  bool reverseClosure(llvm::BasicBlock::iterator &inst, std::map<StringRef, Value*> defsMap, StringRef &alloc, char *type, std::vector<StringRef> *visits) {
	  	  std::queue<StringRef> labQ;
		  int phiCounter = 0;
		  std::map <StringRef, bool> visited;
		  Value *v;
		  if(llvm::isa <llvm::StoreInst> (*inst)) {
		  	v = inst->getOperand(1);
			strcpy(type, "store");
		  }
		  else if(llvm::isa<llvm::LoadInst> (*inst)) {
		  	v = inst->getOperand(0);
			strcpy(type, "load");
		  }
		  labQ.push(v->getName());
		  visited[v->getName()] = true;

		  while(!labQ.empty()) {
		  	  StringRef v = labQ.front();
			  visits->push_back(v);
			  //errs() << v << " defined by " << *defsMap[v] << "\n";
			  labQ.pop();
			  Instruction* inst2 = cast<Instruction>(defsMap[v]);
			  if(llvm::isa <llvm::AllocaInst> (*inst2)) {
				  alloc = v;
			  }
			  else if(llvm::isa<llvm::PHINode>(*inst2)) {
				  phiCounter++;
			  }


			  for(Use &U : inst2->operands()) {
			  	 Value *v = U.get();
				 if(v->getName().startswith("for") == true || v->getName().empty()) {
					 continue;
				 }
				 if(visited.find(v->getName()) == visited.end()) {
				 	labQ.push(v->getName());
					visited[v->getName()] = true;
				 }
			  }

		  }
		  /*errs() << "All labels impacting the address in the instruction " << *inst << " are: ";
		  for(std::pair<StringRef, bool> elem : visited) {
		  	errs() << " " << elem.first;
		  }
		  errs() << "\nAll paths impacting the address in this instruction are\n";
		  */
		  
		  if((alloc.empty()) || (phiCounter == 0)) {
		  	return false;
		  }
		  else {
		  	return true;
		  }
	  }

	  struct LoopData parseLoop(Loop *li, ScalarEvolution &SE) {
		  struct LoopData loopData;
		  PHINode *phinode;

		  phinode = li->getInductionVariable(SE);
		  //errs() << *phinode << "\n";

		  Value* vInd = cast<Value>(phinode);
		  loopData.indVar = vInd->getName();

		  llvm::Optional<Loop::LoopBounds> lbs = li->getBounds(SE);
		  Value& vInit = (*lbs).getInitialIVValue();
		  ConstantInt *Ci;
		  Ci = cast<ConstantInt>(&vInit);
		  loopData.initV = Ci->getSExtValue(); 
		  //errs() << "Initialization of loop is " << loopData.initV << "\n";

		  Value& vFinal = (*lbs).getFinalIVValue();
		  Ci = cast<ConstantInt>(&vFinal);
		  loopData.finalV = Ci->getSExtValue(); 
		  //errs() << "Loop upper bound / goes upto " << loopData.finalV << "\n";

		  Value* step = (*lbs).getStepValue();
		  Ci = cast<ConstantInt>(step);
		  loopData.stepV = Ci->getSExtValue(); 
		  //errs() << "Step value of the loop is " << loopData.stepV << "\n";

		  loopData.lp = li;

		  return loopData;

	  }
	  void analyzeStat(std::vector<struct LoopData> &loopDataV, StringRef &alloc, char* type, ScalarEvolution &SE, std::vector<StringRef> &visits, std::map<StringRef, Value*> &defsMap) {
		  errs() << "Accessing " << alloc << " of type " << type << "\n";
		  for(struct LoopData ldata : loopDataV) {
			  map<Value*, tuple<Value*, int, int>> IndVarMap = getDerived(loopDataV[0].lp, ldata.lp, SE);
			  for(StringRef visit : visits) {
				  if(IndVarMap.find(defsMap[visit]) != IndVarMap.end()) {
					  errs() << " with " << visit << " as the dervied induction variable considering innermost loop's base induction variable is " << ldata.indVar;
					  tuple<Value*, int, int> tup = IndVarMap[defsMap[visit]];
					  Value *base = get<0>(tup);
					  int scaleV = get<1>(tup);
					  int constV = get<2> (tup);
					  errs() << " dervied from base variable " << base->getName() << " with scale of " << scaleV << " and constant of " << constV << "\n";
					  break;
				  }
			  }
		  }


	  }
	  bool runOnFunction(Function &F) override {
		errs() << "\nFunction name : " << F.getName() << "\n";
		InstCounter = 0;
		LoadCounter = 0;
		StoreCounter = 0;
		std::map<StringRef, Value*> defsMap;
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
			}

			if(llvm::isa <llvm::StoreInst> (*itr)) {
				StoreCounter++;
			}
			else if(llvm::isa <llvm::LoadInst> (*itr)) {
				LoadCounter++;
			}

			//errs() << "Instruction is " << *itr << "\n";

      		}

		LoopCounter = 0;
		LoopInfo &Li = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
		struct LoopData loopData;
		ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE(); 
		for(Loop *lit : Li) {
			std::vector<struct LoopData> loopDataV;
			LoopCounter++;

			loopData = parseLoop(lit, SE);		
			loopDataV.push_back(loopData);
			std::unique_ptr<LoopNest> lnest = LoopNest::getLoopNest(*lit, SE); 
			Loop *lin = lit;
			for(unsigned int i = 1; i < lnest->getNumLoops(); i++) {
				lin = lnest->getLoop(i);
				loopData = parseLoop(lin, SE);		
				loopDataV.push_back(loopData);
			}
			for(BasicBlock *BB : lit->getBlocks())
                	{
                    		//errs() << "basicb name: "<< BB->getName() <<"\n";
				if(true) { //BB->getName().startswith("for.body")) {
					for (BasicBlock::iterator itr = BB->begin(), e = BB->end(); itr != e; ++itr) {
						if(llvm::isa <llvm::StoreInst> (*itr) || llvm::isa<llvm::LoadInst> (*itr)) {
							StringRef alloc;
							char type[16];
							std::vector<StringRef> visits;
							if(reverseClosure(itr, defsMap, alloc, type, &visits) == true) {
								analyzeStat(loopDataV, alloc, type, SE, visits, defsMap);
							}
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
