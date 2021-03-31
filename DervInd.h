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


map<Value*, tuple<Value*, int, int, int>> getDerived(Loop *topmost, Loop *innermost, ScalarEvolution &SE, vector<StringRef> &visits); 
