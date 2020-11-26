#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopNestAnalysis.h"
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
  struct LoopData {
  	int initV;
	int stepV;
	int finalV;
	StringRef indVar;
  };

  struct StatDyn1 : public FunctionPass {
  		
	  static char ID;
	  StatDyn1() : FunctionPass(ID) {}
	  
	  virtual void getAnalysisUsage(AnalysisUsage& AU) const override {
        	AU.addRequired<LoopInfoWrapperPass>();
		AU.addRequired<ScalarEvolutionWrapperPass>();
    	  }

	  bool reverseClosure(llvm::BasicBlock::iterator &inst, std::map<StringRef, Value*> defsMap, StringRef &alloc, Value **addr, char *type) {
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
		  *addr = v;

		  while(!labQ.empty()) {
		  	  StringRef v = labQ.front();
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

		  return loopData;

	  }
	  bool runOnFunction(Function &F) override {
		errs() << "\nFunction name : " << F.getName() << "\n";
		InstCounter = 0;
		LoadCounter = 0;
		StoreCounter = 0;
		std::map<StringRef, Value*> defsMap;
		bool modify = false;
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

			errs() << "Instruction is " << *itr << "\n";

      		}

		LoopCounter = 0;
		LoopInfo &Li = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
		struct LoopData loopData;
		ScalarEvolution &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE(); 
		Module *mod = F.getParent();
		LLVMContext &context = mod->getContext();
		for(Loop *lit : Li) {
			std::vector<struct LoopData> loopDataV;
			LoopCounter++;

			loopData = parseLoop(lit, SE);		
			loopDataV.push_back(loopData);
			std::unique_ptr<LoopNest> lnest = LoopNest::getLoopNest(*lit, SE); 
			if(lnest->getNumLoops() == 2) {
				Loop *lin = lnest->getInnermostLoop();
				loopData = parseLoop(lin, SE);		
				loopDataV.push_back(loopData);

			}
			for(BasicBlock *BB : lit->getBlocks())
                	{
                    		//errs() << "basicb name: "<< BB->getName() <<"\n";
				if(BB->getName().startswith("for.body")) {
					for (BasicBlock::iterator itr = BB->begin(), e = BB->end(); itr != e; ++itr) {
						if(llvm::isa <llvm::StoreInst> (*itr) || llvm::isa<llvm::LoadInst> (*itr)) {
							StringRef alloc;
							Value *addr;
							char type[16];
							if(reverseClosure(itr, defsMap, alloc, &addr, type) == true) {
								Instruction *inst = cast<Instruction>(itr);
								IRBuilder<> builder(inst);
								Value* allocV = builder.CreateGlobalStringPtr(alloc);
								Value* typeV = builder.CreateGlobalStringPtr(type);
								std::vector<Value*> args;
								args.push_back(allocV);
								args.push_back(typeV);
								args.push_back(addr);
								FunctionCallee func = mod->getOrInsertFunction("printAddr", Type::getVoidTy(context), Type::getInt8PtrTy(context), Type::getInt8PtrTy(context), Type::getFloatPtrTy(context)); 
								builder.CreateCall(func, args);
								modify = true;
								errs() << "Modifying by adding call to analyze stream of array " << alloc << " of type " << type << "\n";
							}

						}
					}
					
				}
                	}
		}


    		errs() << "Loop count in function " << F.getName() << " is : " << LoopCounter << "\n";
		
		return modify;
	  }
  };
}

char StatDyn1::ID = 0;
static RegisterPass<StatDyn1> Y("statdyn1", "Static dynamic analysis");
