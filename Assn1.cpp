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

	int scaleV;
	int constV;
	int modInd;
	int divInd;
  };

  struct Stat1Loop : public FunctionPass {
  		
	  static char ID;
	  Stat1Loop() : FunctionPass(ID) {}
	  
	  virtual void getAnalysisUsage(AnalysisUsage& AU) const override {
        	AU.addRequired<LoopInfoWrapperPass>();
		AU.addRequired<ScalarEvolutionWrapperPass>();
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
	  void computeStream(StringRef &alloc, char *type, std::vector<struct LoopData> &allLoopData, std::vector<struct LoopData *> &compLoopV) {
		int factor = 1;
	  	for(int i = allLoopData.size() - 1; i >= 0; i--) {
			struct LoopData *ldata = &allLoopData[i];
			ldata->divInd = factor;
			ldata->modInd = ldata->finalV;

			factor = factor * ldata->finalV;
			//errs() << ldata.indVar << " " << ldata.scaleV << " " << ldata.constV << " " << ldata.finalV << " " << ldata.divInd << " " << ldata.modInd << "\t";
		}
		//errs() << factor << "\n";
		char *name = &alloc.str()[0];
		char fname[128];
		strcpy(fname, name);
		strcat(fname, "_");
		strcat(fname, type);
		strcat(fname, ".stream");
		FILE *fp = fopen(fname , "w");
		for(int count = 0; count < factor; count++) {
			int pos = 0;
			for(struct LoopData *ldata : compLoopV) {
				int indV = (count / ldata->divInd) % ldata->modInd;
				//errs() << count << " " << ldata.divInd << " " << ldata.modInd << " " << indV << "\n";
				pos = pos + indV * ldata->scaleV;
				pos = pos + ldata->constV;
			}
			fprintf(fp, "%d\n", pos);
		}
		fclose(fp);
	  }
	  void analyzeStat(std::vector<struct LoopData> &loopDataV, StringRef &alloc, char* type, ScalarEvolution &SE, std::vector<StringRef> &visits, std::map<StringRef, Value*> &defsMap) {
		  errs() << "Accessing " << alloc << " of type " << type << "\n";
		  std::vector<struct LoopData*> compLoopV;
		  for(struct LoopData &ldata : loopDataV) {
			  map<Value*, tuple<Value*, int, int>> IndVarMap = getDerived(loopDataV[0].lp, ldata.lp, SE);
			  for(StringRef visit : visits) {
				  if(IndVarMap.find(defsMap[visit]) != IndVarMap.end()) {
					  errs() << " with " << visit << " as the dervied induction variable considering innermost loop's base induction variable is " << ldata.indVar;
					  tuple<Value*, int, int> tup = IndVarMap[defsMap[visit]];
					  Value *base = get<0>(tup);
					  int scaleV = get<1>(tup);
					  int constV = get<2> (tup);
					  struct LoopData *lp = &ldata;
					  ldata.scaleV = scaleV;
					  ldata.constV = constV;
					  compLoopV.push_back(lp);
					  errs() << " dervied from base variable " << base->getName() << " with scale of " << scaleV << " and constant of " << constV << "\n";
					  break;
				  }
			  }
		  }

		  computeStream(alloc, type, loopDataV, compLoopV);
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
		std::map<StringRef, bool> allocVMap;
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
								if(allocVMap.find(alloc) != allocVMap.end()) {
									continue;
								}
								allocVMap[alloc] = true;
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

char Stat1Loop::ID = 0;
static RegisterPass<Stat1Loop> Y("stat1loop", "Static analysis 1 in loop pass");
