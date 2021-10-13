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
#include "genGraph.h"
#include "llvm/IR/InstIterator.h"
#include <queue>
#include <pthread.h>
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
  #define NUM_TDS 16
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
	vector<int> scaleVV;
	vector<int> constVV;
	vector<int> modVV;
	vector<vector<int>> opsVV;
	vector<vector<int>> factsVV;
	int hidFact;

	int modInd;
	int divInd;
  };

  struct streamInfo {
  	char *name;
	std::vector<int> addrs;
  };

  struct helperArgs {
  	vector<struct LoopData*> compLoopV;
	int factor;
	int pno;
	vector<int> addrsV;
  };

  struct streamProp {
  	struct streamInfo sInfo;
	bool isIndirect;
	bool isConstant;
	int s_size;
  };

  struct Stat1Loop : public FunctionPass {
  		
	  static char ID;
	  map<Value*, struct streamProp> propMap;
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

			  if(v->hasName() == false) {
			  	continue;
			  }

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

	  bool reverseClosure(llvm::BasicBlock::iterator &inst, std::map<StringRef, Value*> defsMap, StringRef &alloc, char *type, std::vector<StringRef> *visits, std::map<StringRef, std::vector<int>> &hidFact, bool &isIndirect, bool &isConstant) {
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
			  if(propMap.find(defsMap[v]) != propMap.end()) {
			  	//errs() << "Indirect access\n";
				isIndirect = true;
				isConstant = false;
				return true;
			  }
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
				  //errs() << v << "\n";
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
		  
		  
		  //errs() << *inst << alloc << phiCounter << "\n";
		  if((alloc.empty())) {
		  	return false;
		  }
		  else if(phiCounter == 0) {
		  	isConstant = true;
		  	isIndirect = false;
		  	return true;
		  }
		  else {
		  	isConstant = false;
		  	isIndirect = false;
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

	  void computeScaleV(struct LoopData *ldata) {
		  int sum = 0;
		  for(unsigned i = 0; i < ldata->opsVV.size(); i++) {
			  int val = 1;
			  for(unsigned j = 0; j < ldata->opsVV[i].size(); j++) {
				  if(ldata->opsVV[i][j] == Oprs::Mul) {
					  val = val * ldata->factsVV[i][j];
				  }
			  }
			  sum = sum + val;
		  }
		  ldata->scaleV = sum * ldata->hidFact;
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
			computeScaleV(compLoopV[j]);
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

	  static  void * computeHelper(void *arg) {
		  struct helperArgs *args = (struct helperArgs*)arg;
		  
		  int psize = args->factor/NUM_TDS;
		  int start = args->pno * psize;
		  int end;
		  if(args->pno == (NUM_TDS - 1)) {
			  end = args->factor;
		  }
		  else {
			  end = start + psize;
		  }

		  for(int count = start; count < end; count++) {
			int pos = 0;
			//std::vector<int> indList;
			//std::map<StringRef, int> posIndMap;
			for(struct LoopData *ldata : args->compLoopV) {
				int indV = (count / ldata->divInd) % ldata->modInd;
				//errs() << count << " " << ldata.divInd << " " << ldata.modInd << " " << indV << "\n";
				//indList.push_back(indV);
				int sum = 0;
				for(unsigned i = 0; i < ldata->opsVV.size(); i++) {
					int val = indV;
					for(unsigned j = 0; j < ldata->opsVV[i].size(); j++) {
						switch(ldata->opsVV[i][j]) {
							case Oprs::Mul : val = val * ldata->factsVV[i][j]; break;
							case Oprs::Add : val = val + ldata->factsVV[i][j]; break;
							case Oprs::And : val = val & ldata->factsVV[i][j]; break;
							case Oprs::Mod : val = val % ldata->factsVV[i][j]; break;
							case Oprs::Rshift : val = val >> ldata->factsVV[i][j]; break;
							default : errs() << "Error unknown operator found\n"; exit(1);
						}
					}
					sum = sum + val * ldata->hidFact;
				}
				pos = pos + sum;
			}
		
		        /*
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
			*/
			args->addrsV.push_back(pos);

			/*fprintf(fp, "%d", pos);
			for(int indV : indList) {
				fprintf(fp, " %d", indV);
			}
			fprintf(fp, "\n");*/
		}

		return NULL;
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
		
		struct helperArgs args[NUM_TDS];
		for(int i = 0; i < NUM_TDS; i++) {
			args[i].compLoopV = compLoopV;
			args[i].factor = factor;
			args[i].pno = i;
		}


		pthread_t tids[NUM_TDS];
		for(int i = 0; i < NUM_TDS; i++) {
			pthread_create(&tids[i], NULL, computeHelper, &args[i]);
		}
		for(int i = 0; i < NUM_TDS; i++) {
			pthread_join(tids[i], NULL);
		}


		for(int i = 0; i < NUM_TDS; i++) {
			for(int add : args[i].addrsV) {
				sInfo.addrs.push_back(add);
			}
		}
		//fclose(fp);
		return sInfo;
	  }

	  int enumStride(struct streamInfo &sInfo) {
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
		int minRet = INT_MAX;
		for(auto &tup : oneStrideMap) {
			errs() << tup.first << " size continuous substream occurs " << tup.second << " times \n"; 
			if(minRet > tup.first) {
				minRet = tup.first;
			}
		}

		errs() << "Jump analysis: ";

		for(auto &tup : jumpMap) {
			errs() << tup.first << " : " << tup.second.size() << "\t";
		}

		errs() << "\n";

		return minRet;

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

	  struct streamProp analyzeStat(std::vector<struct LoopData> &loopDataV, StringRef &alloc, StringRef &func, char* type, ScalarEvolution &SE, std::vector<StringRef> &visits, std::map<StringRef, Value*> &defsMap, std::map<StringRef, std::vector<int>> &hidFact) {
		  errs() << "Accessing " << alloc << " of type " << type << "\n";
		  std::vector<struct LoopData*> compLoopV;
		  for(struct LoopData &ldata : loopDataV) {
			  map<Value*, tuple<Value*, vector<int>, vector<int> >> IndVarMap = getDerived(loopDataV[0].lp, ldata.lp, SE, visits);
			  bool lpAdded = false;
			  ldata.factsVV.clear();
			  ldata.opsVV.clear();
			  for(StringRef visit : visits) {
				  if(IndVarMap.find(defsMap[visit]) != IndVarMap.end()) {
				  	  
					  //errs() << " with " << visit << " as the dervied induction variable considering innermost loop's base induction variable is " << ldata.indVar;
					  tuple<Value*, vector<int>, vector<int>> tup = IndVarMap[defsMap[visit]];
					  Value *base = get<0>(tup);
					  vector<int> factsV = get<1>(tup);
					  vector<int> opsV = get<2>(tup);
					  struct LoopData *lp = &ldata;
					  errs() << base->getName();
					  int fact = 1;
					  for(int dim : hidFact[ldata.indVar]) {
					  	//errs() << " * ";
						//errs() << dim;
						fact = fact * dim;
					  }
					  ldata.factsVV.push_back(factsV);
					  ldata.opsVV.push_back(opsV);
					  for(unsigned i = 0; i < factsV.size(); i++) {
					  	errs() << " ";
						switch(opsV[i]) {
							case Oprs::Add : errs() << "+"; break;
							case Oprs::Mul : errs() << "*"; break;
							case Oprs::And : errs() << "&"; break;
							case Oprs::Rshift : errs() << ">>"; break;
							case Oprs::Mod : errs() << "%"; break;
						}
						
						errs() << " " << factsV[i];
					  }

					  errs() << " + ";

					  ldata.hidFact = fact;
					  if(!lpAdded) {
					  	compLoopV.push_back(lp);
					  }
					  lpAdded = true;
					  //errs() << " dervied from base variable " << base->getName() << " with scale of " << ldata.scaleV << " and constant of " << ldata.constV << "\n";
					  //break;
				  }
			  }
		 }

		 errs() << "\b\b \n";

		 struct streamProp sProp;
		 struct streamInfo sInfo;
		 sInfo = computeStream(func, alloc, type, loopDataV, compLoopV);
		 errs() << "Computed stream of addresses\n";
		 //exprStride(loopDataV, compLoopV, sInfo.name);
		 int size = enumStride(sInfo);
		  
 		 sProp.sInfo = sInfo;
		 sProp.s_size = size;
		 //enumReuse(sInfo);
		 return sProp;
	  }

	  void analyzeDeps() {
	  	map<Value*, bool> visited;
		for(auto elem : propMap) {
			if(elem.second.isIndirect == true || elem.second.isConstant == true) {
				visited[elem.first] = true;
			}
			else {
				visited[elem.first] = false;
			}
		}
		
		vector<vector<struct streamProp>> matchVV;
		for(auto elem : propMap) {
			if(visited[elem.first] == true) {
				continue;
			}
			visited[elem.first] = true;
			char *first = elem.second.sInfo.name;
			vector<struct streamProp> matchV;
			matchV.push_back(elem.second);
			for(auto elem2 : propMap) {
				if(visited[elem2.first] == true) {
					continue;
				}
				if(strcmp(first, elem2.second.sInfo.name) == 0) {
					visited[elem2.first] = true;
					matchV.push_back(elem2.second);
				}
			}
			matchVV.push_back(matchV);
		}

		for(auto matchV : matchVV) {
			if(matchV.size() > 1) {
				errs() << matchV[0].sInfo.name << " occurs " << matchV.size()  << " times ";
				int start = INT_MAX, end = INT_MIN;
				int count = 1;
				for(auto sProp : matchV) {
					if(start > sProp.sInfo.addrs[0]) {
						start = sProp.sInfo.addrs[0];
					}
					
					if(end < sProp.sInfo.addrs.back()) {
						end = sProp.sInfo.addrs.back();
					}

					errs() << " Stream " << count << ":[" << sProp.sInfo.addrs[0] << "-" << sProp.sInfo.addrs.back() << "] ";
					count++;
				}

				errs() << "\nCumulative: start=" << start << " end=" << end << " with window size of " << matchV[0].sInfo.addrs.size() << "\n";

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
		std::map<StringRef, bool> allocVMap;
		for(Loop *lit : Li) {
			propMap.clear();
			std::vector<struct LoopData> loopDataV;
			LoopCounter++;
			genGraph graphVal;
			map<Value*, int> strideMap;
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
							bool isIndirect;
							bool isConstant;
							if(reverseClosure(itr, defsMap, alloc, type, &visits, hidFact, isIndirect, isConstant) == true) {
								/*if(allocVMap.find(alloc) != allocVMap.end()) {
									continue;
								}
								allocVMap[alloc] = true;
								*/
								Value *vl = cast<Value>(itr);
								struct streamProp sProp;
								if(isIndirect == false && isConstant == false) {
									sProp = analyzeStat(loopDataV, alloc, func, type, SE, visits, defsMap, hidFact);
									//errs() << "Load/Store inst is " << *vl << " accessing "<< alloc << " of type " << type << "\n";
									strideMap[vl] = sProp.s_size;
								}
								else if(isIndirect == true) {
		  							errs() << "Indirect access " << alloc << " of type " << type << "; cannot enumrate stream addresses\n";
									strideMap[vl] = 1;
								}
								else if(isConstant == true) {
		  							errs() << "Constant address access " << alloc << " of type " << type << "\n";
									strideMap[vl] = 1;
								}
								sProp.isIndirect = isIndirect;
								sProp.isConstant = isConstant;
								propMap[vl] = sProp;
								graphVal.addToGraph(vl, alloc, type);
							}
						}
					}
					
				}
                	}

			
			string name = F.getName().str();
			name = name + std::to_string(LoopCounter);
			graphVal.printGraph(name, strideMap);
			graphVal.compStats();
			//graphVal.loadPaths();
			analyzeDeps();
			errs() << "Loop " << LoopCounter << " analyzed\n";
		}


    		errs() << "Loop count in function " << F.getName() << " is : " << LoopCounter << "\n";
		
		return false;
	  }
  };
}

char Stat1Loop::ID = 0;
static RegisterPass<Stat1Loop> Y("stat1loop", "Static analysis 1 in loop pass");
