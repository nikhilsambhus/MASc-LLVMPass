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


map<Value*, tuple<Value*, int, int>> getDerived(Loop *topmost, Loop *innermost, ScalarEvolution &SE) {
        map<Value*, tuple<Value*, int, int> > IndVarMap;

        // all induction variables should have phi nodes in the header
        // notice that this might add additional variables, they are treated as basic induction
        // variables for now
        // the header block
        BasicBlock* b_header = topmost->getHeader();
        // the body block
        BasicBlock* b_body;

	PHINode *phinode = innermost->getInductionVariable(SE);
	Value *inst = cast<Value>(phinode);
        IndVarMap[inst] = make_tuple(inst, 1, 0);
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
        while (true) {
          map<Value*, tuple<Value*, int, int> > NewMap = IndVarMap;
          // iterate through all blocks in the loop
          for (auto B: blks) {
            // iterate through all its instructions
            for (auto &I : *B) {
              // we only accept multiplication, addition, and subtraction
              // we only accept constant integer as one of theoperands
              if (auto *op = dyn_cast<BinaryOperator>(&I)) {
                Value *lhs = op->getOperand(0);
                Value *rhs = op->getOperand(1);
                // check if one of the operands belongs to indvars
                if (IndVarMap.count(lhs) || IndVarMap.count(rhs)) {
                  // case: Add
                  if (I.getOpcode() == Instruction::Add) {
                    ConstantInt* CIL = dyn_cast<ConstantInt>(lhs);
                    ConstantInt* CIR = dyn_cast<ConstantInt>(rhs);
                    if (IndVarMap.count(lhs) && CIR) {
                      tuple<Value*, int, int> t = IndVarMap[lhs];
                      int new_val = CIR->getSExtValue() + get<2>(t);
                      NewMap[&I] = make_tuple(get<0>(t), get<1>(t), new_val);
                    } else if (IndVarMap.count(rhs) && CIL) {
                      tuple<Value*, int, int> t = IndVarMap[rhs];
                      int new_val = CIL->getSExtValue() + get<2>(t);
                      NewMap[&I] = make_tuple(get<0>(t), get<1>(t), new_val);
                    }
                  // case: Sub
                  } else if (I.getOpcode() == Instruction::Sub) {
                    ConstantInt* CIL = dyn_cast<ConstantInt>(lhs);
                    ConstantInt* CIR = dyn_cast<ConstantInt>(rhs);
                    if (IndVarMap.count(lhs) && CIR) {
                      tuple<Value*, int, int> t = IndVarMap[lhs];
                      int new_val = get<2>(t) - CIR->getSExtValue();
                      NewMap[&I] = make_tuple(get<0>(t), get<1>(t), new_val);
                    } else if (IndVarMap.count(rhs) && CIL) {
                      tuple<Value*, int, int> t = IndVarMap[rhs];
                      int new_val = get<2>(t) - CIL->getSExtValue();
                      NewMap[&I] = make_tuple(get<0>(t), get<1>(t), new_val);
                    }
                  // case: Mul
                  } else if (I.getOpcode() == Instruction::Mul) {
                    ConstantInt* CIL = dyn_cast<ConstantInt>(lhs);
                    ConstantInt* CIR = dyn_cast<ConstantInt>(rhs);
                    if (IndVarMap.count(lhs) && CIR) {
                      tuple<Value*, int, int> t = IndVarMap[lhs];
                      int new_val = CIR->getSExtValue() * get<1>(t);
                      NewMap[&I] = make_tuple(get<0>(t), new_val, get<2>(t));
                    } else if (IndVarMap.count(rhs) && CIL) {
                      tuple<Value*, int, int> t = IndVarMap[rhs];
                      int new_val = CIL->getSExtValue() * get<1>(t);
                      NewMap[&I] = make_tuple(get<0>(t), new_val, get<2>(t));
                    }
                  }
                } // if operand in indvar
              } // if op is binop
            } // auto &I: B
          } // auto &B: blks
          if (NewMap.size() == IndVarMap.size()) break;
          else IndVarMap = NewMap;
        }

	/*for(pair<Value*, tuple<Value*, int, int>> elem : IndVarMap) {
		Value *derV = elem.first;
		tuple<Value *, int, int> tup = elem.second;
		Value *base = get<0>(tup);
		int scaleV = get<1>(tup);
		int constV = get<2> (tup);
		if(derV == base) {
			errs() << derV->getName() << " is the base induction variable\n";
		}
		else {
			errs() << derV->getName() << " is derived from " << base->getName() << " with scale " << scaleV << " and constant " << constV << "\n";
		}

	}*/

	return IndVarMap;
}

