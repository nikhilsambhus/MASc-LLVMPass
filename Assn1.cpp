#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "LoopUtils.h"
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
