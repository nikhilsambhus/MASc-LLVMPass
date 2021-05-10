#include "llvm/ADT/Statistic.h"
#include "llvm/Transforms/Scalar.h"
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
#include "llvm/IR/InstIterator.h"
#include <queue>
#include <pthread.h>
using namespace llvm;
using namespace std;
struct graph {
	map<Value*, vector<Value*>> adjListMap;
	map<Value*, StringRef> ldstMap;
};

class genGraph {
	struct graph DFGbody;
	void dispVal(Value*);
	void dispChar(const char *);
	public:
	void addToGraph(Value*, StringRef, char*);
	void printGraph();
	void compStats();
};
