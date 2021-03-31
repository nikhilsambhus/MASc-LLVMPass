#include "llvm/Analysis/LoopInfo.h" 
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopNestAnalysis.h"
using namespace llvm;

#include <tuple>
#include <map>
#include <iostream>
using namespace std;


map<Value*, tuple<Value*, int, int, int>> getDerived(Loop *topmost, Loop *innermost, ScalarEvolution &SE, vector<StringRef> &visits) {
        map<Value*, tuple<Value*, int, int, int> > IndVarMap;

        // all induction variables should have phi nodes in the header
        // notice that this might add additional variables, they are treated as basic induction
        // variables for now
        // the header block
        BasicBlock* b_header = topmost->getHeader();
        // the body block
        BasicBlock* b_body;

	PHINode *phinode = innermost->getInductionVariable(SE);
	Value *inst = cast<Value>(phinode);
        IndVarMap[inst] = make_tuple(inst, 0, 1, 0);
        /*for (auto &I : *b_header) {
          if (PHINode *PN = dyn_cast<PHINode>(&I)) {
            IndVarMap[&I] = make_tuple(&I, 1, 0);
          }
        }*/
        
        // get the total number of blocks as well as the block list
        //cout << L->getNumBlocks() << "\n";
        auto blks = topmost->getBlocks();

        // find all indvars
        // keep modifying the set until the size does not change
        // notice that over here, our set of induction variables is not precise
	map<Value*, bool> delVars;
        while (true) {
          map<Value*, tuple<Value*, int, int, int> > NewMap = IndVarMap;
          // iterate through all blocks in the loop
          for (auto B: blks) {
            // iterate through all its instructions
            for (auto &I : *B) {
              // we only accept multiplication, addition, and subtraction
              // we only accept constant integer as one of theoperands
	      //do analysis only if variable present in the visits/closure set passed
	      if(find(visits.begin(), visits.end(), I.getName()) == visits.end()) {
	      	continue;
	      }
              if (auto *op = dyn_cast<BinaryOperator>(&I)) {
                Value *lhs = op->getOperand(0);
                Value *rhs = op->getOperand(1);
                // check if one of the operands belongs to indvars
                if ((IndVarMap.count(lhs) || IndVarMap.count(rhs)) && !I.getName().startswith("inc")) {
                  // case: Add
                  if (I.getOpcode() == Instruction::Add) {
                    ConstantInt* CIL = dyn_cast<ConstantInt>(lhs);
                    ConstantInt* CIR = dyn_cast<ConstantInt>(rhs);
                    if (IndVarMap.count(lhs) && CIR) {
                      tuple<Value*, int, int, int> t = IndVarMap[lhs];
                      int new_val = CIR->getSExtValue() + get<3>(t);
                      NewMap[&I] = make_tuple(get<0>(t), get<1>(t), get<2>(t), new_val);
		      delVars[lhs] = true;
                    } else if (IndVarMap.count(rhs) && CIL) {
                      tuple<Value*, int, int, int> t = IndVarMap[rhs];
                      int new_val = CIL->getSExtValue() + get<3>(t);
                      NewMap[&I] = make_tuple(get<0>(t), get<1>(t), get<2>(t), new_val);
		      delVars[rhs] = true;
                    }
                  // case: Sub
                  } else if (I.getOpcode() == Instruction::Sub) {
                    ConstantInt* CIL = dyn_cast<ConstantInt>(lhs);
                    ConstantInt* CIR = dyn_cast<ConstantInt>(rhs);
                    if (IndVarMap.count(lhs) && CIR) {
                      tuple<Value*, int, int, int> t = IndVarMap[lhs];
                      int new_val = get<3>(t) - CIR->getSExtValue();
                      NewMap[&I] = make_tuple(get<0>(t), get<1>(t), get<2>(t), new_val);
		      delVars[lhs] = true;
                    } else if (IndVarMap.count(rhs) && CIL) {
                      tuple<Value*, int, int, int> t = IndVarMap[rhs];
                      int new_val = get<3>(t) - CIL->getSExtValue();
                      NewMap[&I] = make_tuple(get<0>(t), get<1>(t), get<2>(t), new_val);
		      delVars[rhs] = true;
                    }
                  // case: Mul
                  } else if (I.getOpcode() == Instruction::Mul) {
                    ConstantInt* CIL = dyn_cast<ConstantInt>(lhs);
                    ConstantInt* CIR = dyn_cast<ConstantInt>(rhs);
                    if (IndVarMap.count(lhs) && CIR) {
                      tuple<Value*, int, int, int> t = IndVarMap[lhs];
                      int new_val = CIR->getSExtValue() * get<2>(t);
                      NewMap[&I] = make_tuple(get<0>(t), get<1>(t), new_val, get<3>(t));
		      delVars[lhs] = true;
                    } else if (IndVarMap.count(rhs) && CIL) {
                      tuple<Value*, int, int, int> t = IndVarMap[rhs];
                      int new_val = CIL->getSExtValue() * get<2>(t);
                      NewMap[&I] = make_tuple(get<0>(t), get<1>(t), new_val, get<3>(t));
		      delVars[rhs] = true;
                    }
		  }
		  //case : Mod operation
		  else if(I.getOpcode() == Instruction::SRem) {
			  ConstantInt* CIL = dyn_cast<ConstantInt>(lhs);
			  ConstantInt* CIR = dyn_cast<ConstantInt>(rhs);
			  if (IndVarMap.count(lhs) && CIR) {
				  tuple<Value*, int, int, int> t = IndVarMap[lhs];
				  int new_val = CIR->getSExtValue() + get<1>(t);
				  NewMap[&I] = make_tuple(get<0>(t), new_val, get<2>(t), get<3>(t));
				  delVars[lhs] = true;
				  //errs() << "Srem found " << *lhs <<  *rhs << "\n";
			  }
		  }
                } // if operand in indvar
              } // if op is binop
            } // auto &I: B
          } // auto &B: blks
          if (NewMap.size() == IndVarMap.size()) break;
          else IndVarMap = NewMap;
        }

	for(auto &it : delVars) {
		//errs() << " Del " << it.first->getName() << "\n";
		IndVarMap.erase(it.first);
	}
	/*
	for(pair<Value*, tuple<Value*, int, int, int>> elem : IndVarMap) {
		Value *derV = elem.first;
		tuple<Value *, int, int, int> tup = elem.second;
		Value *base = get<0>(tup);
		int modV = get<1>(tup);
		int scaleV = get<2>(tup);
		int constV = get<3> (tup);
		if(derV == base) {
			errs() << derV->getName() << " is the base induction variable\n";
		}
		else {
			errs() << derV->getName() << " is derived from " << base->getName() << " with mod " << modV << ", with scale " << scaleV << " and constant " << constV << "\n";
		}

	}
	*/
	return IndVarMap;
}

