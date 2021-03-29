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
	
	bool initCons;
	StringRef initInd;
	bool finalCons;
	StringRef finalInd;
	
	StringRef indVar;

	int scaleV;
	int constV;
	int modInd;
	int divInd;
  };

  struct streamInfo {
  	char *name;
	std::vector<int> addrs;
  };

  struct Stat1Loop : public FunctionPass {
  		
	  static char ID;
	  Stat1Loop() : FunctionPass(ID) {}
	  
	  virtual void getAnalysisUsage(AnalysisUsage& AU) const override {
        	AU.addRequired<LoopInfoWrapperPass>();
		AU.addRequired<ScalarEvolutionWrapperPass>();
    	  }

	  std::map<StringRef, std::vector<int>> extractGEP(Instruction *inst) {
	  	  std::map<StringRef, std::vector<int>> retMap;
		  std::vector<int> dimsV;
		
		  GetElementPtrInst *gep = cast<GetElementPtrInst> (inst);
		  Type *tp = gep->getSourceElementType();
		  if(!tp->isArrayTy()) {
		  	errs() << "Not array type, exiting\n";
			exit(-1);
		  }
		  ArrayType *atp = cast<ArrayType> (tp);
		  while(atp->getElementType()->isArrayTy()) {
			  atp = cast<ArrayType> (atp->getElementType());
			  dimsV.push_back(atp->getNumElements());
		  }
		  if(dimsV.empty()) {
		  	dimsV.push_back(1);
		  }
		  //errs() << factor << " " << *tp << " ";

		  std::map<StringRef, bool> visited;
		  std::queue<Value*> valQ;


		  Value *v;
		  v = inst->getOperand(2);
		  valQ.push(v);
		  visited[v->getName()] = true;
		  std::vector<StringRef> indV;
		  while(!valQ.empty()) {
			  v = valQ.front();
			  valQ.pop();

			  Instruction *inst = cast<Instruction>(v);
			  if(llvm::isa<llvm::PHINode>(*inst)) {
			  	indV.push_back(v->getName());
				break;
			  }

			  for(Use &U : inst->operands()) {
				  v = U.get();
				  if(v->getName().startswith("for") == true || v->getName().empty()) {
					  continue;
				  }
				  if(visited.find(v->getName()) == visited.end()) {
					  valQ.push(v);
					  visited[v->getName()] = true;
				  }
			  }
		  }

		  for(StringRef ind : indV) {
		  	//errs() << "Ind var " << ind << "\n";
			retMap[ind] = dimsV;
		  }

		  return retMap;
	  }

	  bool reverseClosure(llvm::BasicBlock::iterator &inst, std::map<StringRef, Value*> defsMap, StringRef &alloc, char *type, std::vector<StringRef> *visits, std::map<StringRef, std::vector<int>> &hidFact) {
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
			  if(!inst2) {
			  	continue;
			  }

			  if(llvm::isa <llvm::GetElementPtrInst> (*inst2)) {
				  std::map<StringRef, std::vector<int>> factM = extractGEP(inst2);
				  hidFact.insert(factM.begin(), factM.end()); 
			  }
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
		  }*/
		  
		  
		  if((alloc.empty()) || (phiCounter == 0)) {
		  	return false;
		  }
		  else {
		  	return true;
		  }
	  }

	  struct LoopData parseLoop(Loop *li, ScalarEvolution &SE, std::vector<struct LoopData> &loopDataV) {
		  struct LoopData loopData;
		  PHINode *phinode;

		  phinode = li->getInductionVariable(SE);
		  //errs() << *phinode << "\n";

		  Value* vInd = cast<Value>(phinode);
		  loopData.indVar = vInd->getName();

		  ConstantInt *Ci;
		  llvm::Optional<Loop::LoopBounds> lbs = li->getBounds(SE);
		  Value& vInit = (*lbs).getInitialIVValue();
		  if(vInit.hasName()) {
			loopData.initCons = false;
		  	//errs() << vInit << "\n";
			Instruction *inst = cast<Instruction>(&vInit);
			if(inst->getOpcode() == Instruction::Add) {
				Ci = cast<ConstantInt>(inst->getOperand(1));
				loopData.initV = Ci->getSExtValue();
				loopData.initInd = inst->getOperand(0)->getName();
				//errs() << loopData.initInd << loopData.initV << "\n";
			}
		  } else {
			  Ci = cast<ConstantInt>(&vInit);
			  loopData.initV = Ci->getSExtValue(); 
			  loopData.initCons = true;
		  }
		  //errs() << "Initialization of loop is " << loopData.initV << "\n";

		  Value& vFinal = (*lbs).getFinalIVValue();
		  //errs() << "final value " << vFinal << "\n";
		  if(vFinal.hasName()) {
		  	loopData.finalInd = vFinal.getName();
			loopData.finalCons = false;
			for(struct LoopData &lp: loopDataV) {
				if(loopData.finalInd == lp.indVar) {
					loopData.finalV = lp.finalV;
					break;
				}
			}
		  }
		  else {
		  	Ci = cast<ConstantInt>(&vFinal);
		  	loopData.finalV = Ci->getSExtValue();
			loopData.finalCons = true;
		  }
		  //errs() << loopData.finalInd << loopData.finalV << loopData.finalCons << "\n";
		  //errs() << "Loop upper bound / goes upto " << loopData.finalV << "\n";

		  Value* step = (*lbs).getStepValue();
		  Ci = cast<ConstantInt>(step);
		  loopData.stepV = Ci->getSExtValue(); 
		  //errs() << "Step value of the loop is " << loopData.stepV << "\n";

		  loopData.lp = li;

		  return loopData;

	  }

	  void exprStride(std::vector<struct LoopData> &allLoopData, std::vector<struct LoopData *> &compLoopV, char *fname) {
		int factor = 1;
		for(int i = compLoopV.size() - 1; i >= 0; i--) {
			struct LoopData *ldata = compLoopV[i];
			ldata->divInd = factor;
			ldata->modInd = ldata->finalV;

			factor = factor * ldata->finalV;
		}

		int stride = 1, j = compLoopV.size() - 1;
		bool flag = false;
		for(int i = allLoopData.size() - 1; i >= 0; i--) {
			if(j >= 0 && compLoopV[j] == &allLoopData[i] && compLoopV[j]->scaleV == compLoopV[j]->divInd) {
				stride = stride * compLoopV[j]->finalV;
				j--;
				flag = true;
			}
			else if(flag == true) {
				break;
			}
		}

		errs() << "Based on expression analysis " << fname << " has continuous strides of size " << stride << "\n";
		
	  }
	  struct streamInfo computeStream(StringRef &func, StringRef &alloc, char *type, std::vector<struct LoopData> &allLoopData, std::vector<struct LoopData *> &compLoopV) {
		int factor = 1;
	  	for(int i = allLoopData.size() - 1; i >= 0; i--) {
			struct LoopData *ldata = &allLoopData[i];
			ldata->divInd = factor;
			ldata->modInd = ldata->finalV;

			factor = factor * ldata->finalV;
		}
		//errs() << factor << "\n";
		char *name = &func.str()[0];
		char *fname = (char *)malloc(256);
		strcpy(fname, name);
		strcat(fname, "_");
		name = &alloc.str()[0];
		strcat(fname, name);
		strcat(fname, "_");
		strcat(fname, type);
		strcat(fname, ".stream");
		//FILE *fp = fopen(fname , "w");

		struct streamInfo sInfo;
		sInfo.name = fname;
		for(int count = 0; count < factor; count++) {
			int pos = 0;
			std::vector<int> indList;
			std::map<StringRef, int> posIndMap;
			for(struct LoopData *ldata : compLoopV) {
				int indV = (count / ldata->divInd) % ldata->modInd;
				//errs() << count << " " << ldata.divInd << " " << ldata.modInd << " " << indV << "\n";
				indList.push_back(indV);
				pos = pos + indV * ldata->scaleV;
				pos = pos + ldata->constV;
			}
			
			//compute values of all surrounding loop ind vars
			for(struct LoopData &ldata : allLoopData) {
				int indV = (count / ldata.divInd) % ldata.modInd;
				posIndMap[ldata.indVar] = indV;
			}

			//check if finalV is compared with any indvar to filter stream addresses
			//check if initV is assigned any indVar to filter stream addresses
			int skip = false;
			for(struct LoopData &ldata : allLoopData) {
				if(ldata.finalCons == false) {
					if(posIndMap[ldata.indVar] >= posIndMap[ldata.finalInd]) {
						skip = true;
						break;
					}
				}

				if(ldata.initCons == false) {
					if(posIndMap[ldata.indVar] < (posIndMap[ldata.initInd] + ldata.initV)) {
						skip = true;
						break;
					}
				}
			}
			if(!skip) {
				sInfo.addrs.push_back(pos);
			}
			/*fprintf(fp, "%d", pos);
			for(int indV : indList) {
				fprintf(fp, " %d", indV);
			}
			fprintf(fp, "\n");*/
		}
		//fclose(fp);
		return sInfo;
	  }

	  void enumStride(struct streamInfo &sInfo) {
	  	int prevpos = sInfo.addrs[0];
		int curpos;
		int diff;
		int conCt = 0;
		std::map<int, int> oneStrideMap;
		std::map<int, std::vector<int>> jumpMap;
		int strideIn = 0;
		for(unsigned i = 1; i < sInfo.addrs.size(); i++) {
			curpos = sInfo.addrs[i];
			diff = curpos - prevpos;
			if(diff == 1) {
				conCt++;
			}
			else if(diff == 0) {
				continue;
			}
			else {
				conCt++;
				if(oneStrideMap.find(conCt) != oneStrideMap.end()) {
					oneStrideMap[conCt] = oneStrideMap[conCt] + 1;
				}
				else {
					oneStrideMap[conCt] = 1;
				}
				conCt = 0;


				if(jumpMap.find(diff) != jumpMap.end()) {
					std::vector<int> *posV = &jumpMap[diff];
					posV->push_back(strideIn);
				}
				else {
					std::vector<int> posV;
					posV.push_back(strideIn);
					jumpMap[diff] = posV;
				}

				strideIn++;

			}

			prevpos = curpos;
		}

		if(conCt != 0) {
			conCt++;
			if(oneStrideMap.find(conCt) != oneStrideMap.end()) {
				oneStrideMap[conCt] = oneStrideMap[conCt] + 1;
			}
			else {
				oneStrideMap[conCt] = 1;
			}
			conCt = 0;
		}

		errs() << "Stride analysis based on enumeration of " << sInfo.name << " which has total size "<< sInfo.addrs.size() << ":\n";
		for(auto &tup : oneStrideMap) {
			errs() << tup.first << " size continuous substream occurs " << tup.second << " times \n"; 
		}

		errs() << "Jump analysis: ";

		for(auto &tup : jumpMap) {
			errs() << tup.first << " ";
		}

		errs() << "\n";

	  }

	  int calcDist(std::vector<int> &addrs, int start, int end) {
	  	std::map<int, bool> unqMap;
		
		for(int i = start + 1; i < end; i++) {
			if(unqMap.find(addrs[i]) == unqMap.end()) {
				unqMap[addrs[i]] = true;
			}
		}
		
		return unqMap.size();
	  }
	  void enumReuse(struct streamInfo &sInfo) {
	  	std::map<int, std::vector<int>> cacheMap;
		std::map<int, int> distReuseMap;


		int cnt = 0;
		for(int &curpos : sInfo.addrs) {
			if(cacheMap.find(curpos) != cacheMap.end()) {
				std::vector<int>* addrV = &cacheMap[curpos];
				addrV->push_back(cnt);
			}
			else {
				std::vector<int> addrV;
				addrV.push_back(cnt);
				cacheMap[curpos] = addrV;
			}
			cnt++;
		}

		for(auto &cache : cacheMap) {
			std::vector<int> addrV = cache.second;
			int prevpos = addrV[0];
			for(unsigned i  = 1; i < addrV.size(); i++) {
				int curpos = addrV[i];
				int dist = calcDist(sInfo.addrs, prevpos, curpos);
				if(distReuseMap.find(dist) != distReuseMap.end()) {
					distReuseMap[dist] = distReuseMap[dist] + 1;
				}
				else {
					distReuseMap[dist] = 1;
				}
				prevpos = curpos;
			}
		}
		errs() << "Reuse analysis of " << sInfo.name << " : ";

		float total = 0.0;
		int weight = 0;
		for(auto &tup : distReuseMap) {
			//errs() << tup.first << " reuse distance occurs " << tup.second << " times \n"; 
			total += tup.second;
			weight += tup.first * tup.second;
		
		}

		if(total > 0.0) {
			errs() << (int)(weight/total) << " is the weighted reuse distance average\n";
		}
		else {
			errs() << " No reuse in the stream\n";
		}


	  }
	  void analyzeStat(std::vector<struct LoopData> &loopDataV, StringRef &alloc, StringRef &func, char* type, ScalarEvolution &SE, std::vector<StringRef> &visits, std::map<StringRef, Value*> &defsMap, std::map<StringRef, std::vector<int>> &hidFact) {
		  errs() << "Accessing " << alloc << " of type " << type << "\n";
		  std::vector<struct LoopData*> compLoopV;
		  for(struct LoopData &ldata : loopDataV) {
			  map<Value*, tuple<Value*, int, int, int>> IndVarMap = getDerived(loopDataV[0].lp, ldata.lp, SE);
			  /*for(StringRef visit : visits) {
				  if(IndVarMap.find(defsMap[visit]) != IndVarMap.end()) {
				  	  
					  //errs() << " with " << visit << " as the dervied induction variable considering innermost loop's base induction variable is " << ldata.indVar;
					  tuple<Value*, int, int, int> tup = IndVarMap[defsMap[visit]];
					  Value *base = get<0>(tup);
					  int scaleV = get<1>(tup);
					  errs() << "scaleV " << scaleV << "\n";
					  int constV = get<2> (tup);
					  struct LoopData *lp = &ldata;
					  errs() << base->getName();
					  int fact = 1;
					  for(int dim : hidFact[ldata.indVar]) {
					  	errs() << " * ";
						errs() << dim;
						fact = fact * dim;
					  }
					  errs() << " * " << scaleV << " + " ;

					  for(int dim : hidFact[ldata.indVar]) {
						  errs() << dim;
						  errs() << " * ";
					  }
					  errs() << constV << " + ";

					  ldata.scaleV = scaleV * fact;
					  ldata.constV = constV * fact;
					  compLoopV.push_back(lp);
					  //errs() << " dervied from base variable " << base->getName() << " with scale of " << ldata.scaleV << " and constant of " << ldata.constV << "\n";
					  break;
				  }
			  }*/
		 }

		 /*errs() << "\b\b \n";

		 struct streamInfo sInfo;
		 sInfo = computeStream(func, alloc, type, loopDataV, compLoopV);
		 exprStride(loopDataV, compLoopV, sInfo.name);
		 enumStride(sInfo);
		 */
		 //enumReuse(sInfo);
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

			loopData = parseLoop(lit, SE, loopDataV);		
			loopDataV.push_back(loopData);
			std::unique_ptr<LoopNest> lnest = LoopNest::getLoopNest(*lit, SE); 
			Loop *lin = lit;
			for(unsigned int i = 1; i < lnest->getNumLoops(); i++) {
				lin = lnest->getLoop(i);
				loopData = parseLoop(lin, SE, loopDataV);		
				loopDataV.push_back(loopData);
			}
			for(BasicBlock *BB : lit->getBlocks())
                	{
                    		//errs() << "basicb name: "<< BB->getName() <<"\n";
				if(true) { //BB->getName().startswith("for.body")) {
					for (BasicBlock::iterator itr = BB->begin(), e = BB->end(); itr != e; ++itr) {
						if(llvm::isa <llvm::StoreInst> (*itr) || llvm::isa<llvm::LoadInst> (*itr)) {
							StringRef alloc;
							StringRef func = F.getName();
							char type[16];
							std::vector<StringRef> visits;
							std::map<StringRef, std::vector<int>> hidFact;
							if(reverseClosure(itr, defsMap, alloc, type, &visits, hidFact) == true) {
								/*if(allocVMap.find(alloc) != allocVMap.end()) {
									continue;
								}*/
								allocVMap[alloc] = true;
								analyzeStat(loopDataV, alloc, func, type, SE, visits, defsMap, hidFact);
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
