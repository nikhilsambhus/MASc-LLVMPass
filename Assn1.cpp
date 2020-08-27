#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/InstIterator.h"
using namespace llvm;

#define DEBUG_TYPE "assn1"

STATISTIC(FuncCounter, "Counts number of functions greeted");
STATISTIC(InstCounter, "Counts number of instructions greeted");
STATISTIC(LoadCounter, "Counts number of load instructions greeted");
STATISTIC(StoreCounter, "Counts number of store instructions greeted");
STATISTIC(LoopCounter, "Counts number of loops greeted");
STATISTIC(BBCounter, "Counts number of basic blocks greeted");

namespace {
  struct Assn1Loop : public FunctionPass {
	  static char ID;
	  Assn1Loop() : FunctionPass(ID) {}
	  
	  virtual void getAnalysisUsage(AnalysisUsage& AU) const override {
        	AU.addRequired<LoopInfoWrapperPass>();
    	  }

	  bool runOnFunction(Function &F) override {
		LoopCounter = 0;
		LoopInfo &Li = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
		//for(LoopInfo::iterator lit = Li.begin(), lend = Li.end(); lit != lend; ++lit) {
		for(Loop *lit : Li) {
			LoopCounter++;
			PHINode *phinode = getInductionVariable(lit);
			errs() << *phinode << "\n";
			for(BasicBlock *BB : lit->getBlocks())
                	{
                    		errs() << "basicb name: "<< BB->getName() <<"\n";
                	}
		}

    		errs() << "Loop count in function " << F.getName() << " is : " << LoopCounter << "\n";
		return false;
	  }
  };

  struct Assn1 : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    Assn1() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
	for(auto &F : M) {

		if(!F.size()) { continue;}

		errs() << "\nFunction name : " << F.getName() << "\n";
		InstCounter = 0;
		LoadCounter = 0;
		StoreCounter = 0;
		BBCounter = 0;

      		for(inst_iterator itr = inst_begin(F), etr = inst_end(F); itr != etr; itr++) {
	      		InstCounter++;
			if(llvm::isa <llvm::StoreInst> (*itr)) {
				StoreCounter++;
			}
			else if(llvm::isa <llvm::LoadInst> (*itr)) {
				LoadCounter++;
				Value *addr = itr->getOperand(0);
				errs() << *addr << " is first operand of instruction ";

			}
			errs() << *itr << "\n";

      		}

		for(BasicBlock& BB : F) {
			BBCounter++;
		}

    		errs() << "Instruction count in this function is : " << InstCounter << "\n";
    		errs() << "Basic Blocks count in this function is : " << BBCounter << "\n";
    		errs() << "Load Instruction count in this function is : " << LoadCounter << "\n";
    		errs() << "Store Instruction count in this function is : " << StoreCounter << "\n";

		++FuncCounter;
	}
	
	errs() << "\nTotal number of functions are : " << FuncCounter << "\n\n";
	return false;
    }
  };
}

char Assn1::ID = 0;
char Assn1Loop::ID = 0;
static RegisterPass<Assn1> X("assn1", "Assignment 1 Pass");
static RegisterPass<Assn1Loop> Y("assn1loop", "Assignment 1 Pass");
