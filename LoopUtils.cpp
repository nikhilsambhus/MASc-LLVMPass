//****************************************************************
// The GPGPU Autotuning Compiler Project
// Copyright (c) 2014-2014 University of Toronto.
// All rights reserved.
// 
// See License.txt for licensing and redistribution.
// 
// File: 
//       LoopUtils.cpp
// 
// Description:
//       This code defines implements utilities for manipulating and working with loops.
//
// Authors:
//       Robert Longo
//       Leslie Barron
//       Tarek Abdelrahman
//       Ayush Shrestha
//
// Revisions:
//       May  29, 2015: isLimitLoopInvariant function added
//       May  27, 2015: code for loop admissability updated
//       July 14, 2014: code for loop admissability added
//       June 10, 2014: code created
//*****************************************************************

#include "LoopUtils.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Debug.h"
#include "llvm/Analysis/CFG.h" // For predecessors of a basic block
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/InstrTypes.h"

using namespace llvm;
using namespace std;

namespace autotune {

    /* Returns the loop at the given depth from loop or null
     */
    Loop *getLoopForDepth(Loop *loop, unsigned depth) {
        assert(loop && "Loop cant be null");

        for(unsigned d = 0; d < depth; d++) {
            if(loop->getSubLoopsVector().size() == 0) return NULL;
            loop = loop->getSubLoopsVector()[0];
            
        }

        return loop;
    }

    /* Checks if loop is perfectly nested to a given depth 
     * Depth should include outer loop, so a depth of 1 is always perfectly nested
     */
    bool isLoopPerfectlyNested(const llvm::Loop *loop, unsigned depth, bool printMessages) {
        assert(loop && "Loop cant be null");

        for(unsigned i = 0; i < depth - 1; i++) {
            const std::vector<Loop *> &subLoops = loop->getSubLoops();

            if(subLoops.size() > 1) {
                if (printMessages) errs() << "Error loop cannot have more than 1 child loop\n";
                //DEBUG( errs() << subLoops.size() << " Loops were detected beneath loop " << *(loop) << '\n');
                return false;
            } else if (subLoops.size() == 1) {
                // Total blocks in the outer loop should be number of blocks in inner loop + 4
                // which are outer header, outer latch, inner preheader, and inner exit block
                if(subLoops[0]->getNumBlocks() + 4 != loop->getNumBlocks()) {
                    if (printMessages) errs() << "Error loop is not perfectly nested\n";
                    return false;
                } else {
                    BasicBlock *innerExit = subLoops[0]->getUniqueExitBlock();
                    BasicBlock *innerPreheader = subLoops[0]->getLoopPreheader();

                    // Check the preheader and exit block only contain a BranchInst
                    if(!isa<BranchInst>(*innerExit->begin()) || !isa<BranchInst>(*innerPreheader->begin())) {
                        if (printMessages) errs() << "Error loop is not perfectly nested\n";
                        return false;
                    }
                }
            } else if(subLoops.size() == 0) {
                if (printMessages) errs() << "Error no loop at depth " << (i + 2) << "\n";
                return false;
            }

            loop = subLoops[0];
        }

        return true;
    }

    PHINode *getInductionVariable(const Loop *loop) {
        BasicBlock *H = loop->getHeader();

        BasicBlock *Incoming = 0, *Backedge = 0;
        pred_iterator PI = pred_begin(H);
        assert(PI != pred_end(H) &&
             "Loop must have at least one backedge!");
        Backedge = *PI++;
        if (PI == pred_end(H)) return 0;  // dead loop
        Incoming = *PI++;
        if (PI != pred_end(H)) return 0;  // multiple backedges?

        if (loop->contains(Incoming)) {
            if (loop->contains(Backedge))
                return 0;
            std::swap(Incoming, Backedge);
        } else if (!loop->contains(Backedge))
            return 0;

        // Loop over all of the PHI nodes, looking for a indvar.
        for (BasicBlock::iterator I = H->begin(); isa<PHINode>(I); ++I) {
            PHINode *PN = cast<PHINode>(I);
            if (isa<ConstantInt>(PN->getIncomingValueForBlock(Incoming)))
                if (Instruction *Inc =
                    dyn_cast<Instruction>(PN->getIncomingValueForBlock(Backedge)))
                    if ((Inc->getOpcode() == Instruction::Add || Inc->getOpcode() == Instruction::Sub) && Inc->getOperand(0) == PN)
                        if (isa<ConstantInt>(Inc->getOperand(1)))
                            if (Inc->getParent() == loop->getLoopLatch())
                                return PN;
        }
        return 0;
    }

    unsigned getNumInductionVariables(const Loop *loop) {
        BasicBlock *H = loop->getHeader();

        BasicBlock *Incoming = 0, *Backedge = 0;
        pred_iterator PI = pred_begin(H);
        assert(PI != pred_end(H) &&
             "Loop must have at least one backedge!");
        Backedge = *PI++;
        if (PI == pred_end(H)) return 0;  // dead loop
        Incoming = *PI++;
        if (PI != pred_end(H)) return 0;  // multiple backedges?

        if (loop->contains(Incoming)) {
            if (loop->contains(Backedge))
                return 0;
            std::swap(Incoming, Backedge);
        } else if (!loop->contains(Backedge))
            return 0;

        unsigned count = 0;

        // Loop over all of the PHI nodes, looking for a indvar.
        for (BasicBlock::iterator I = H->begin(); isa<PHINode>(I); ++I) {
            PHINode *PN = cast<PHINode>(I);
            if (isa<ConstantInt>(PN->getIncomingValueForBlock(Incoming)))
                if (Instruction *Inc =
                    dyn_cast<Instruction>(PN->getIncomingValueForBlock(Backedge)))
                    if ((Inc->getOpcode() == Instruction::Add || Inc->getOpcode() == Instruction::Sub) && Inc->getOperand(0) == PN)
                        if (isa<ConstantInt>(Inc->getOperand(1)))
                            if (Inc->getParent() == loop->getLoopLatch())
                                count++;
        }
        return count;
    }
    
    bool isLoopAnalyzable(llvm::Loop *loop)
    {
        // Check that isSimplifyForm is ok
        if (!loop -> isLoopSimplifyForm())
        {
            //DEBUG(errs() << "Loop failed the simplify form test\n");
            return false;
        }

        // Check for no breaks
        BasicBlock *bb = loop -> getExitBlock() -> getSinglePredecessor(); // Get the predecessor to the exit block
     
        if ( bb == NULL // No predecessor, or too many
                || bb != loop -> getHeader()) // OR the block before exit block isn't the header
                {
                    //DEBUG(errs() << "No predecessor or too many for loop\n");
                    return false;
                } 

	// Check for only 1 unique exit block
        const unsigned numExitBlocks = 30; // For the small vector     
        SmallVector<BasicBlock *, numExitBlocks> exitBlocks;
        loop -> getUniqueExitBlocks(exitBlocks); 
        SmallVector<BasicBlock *, numExitBlocks>::iterator start = exitBlocks.begin();
        start++; // Move it up one (should be at the end  
        SmallVector<BasicBlock *, numExitBlocks>::iterator finish = exitBlocks.end();
	if (start != finish)
        {
            //DEBUG(errs() << "Too many exit blocks. There should only be 1\n");
            return false;
        }


        // Check that there is exactly one phi node - i.e. one induction variable
        unsigned numIndVar = getNumInductionVariables (loop);
        if (numIndVar != 1)        
        {
            //DEBUG(errs() << "Only 1 induction variable allowed in loop, " << numIndVar << " were detected. \n");
            return false;
        }  

        return true;
    }
    
    void getAllPredecessors(llvm::BasicBlock *_block, std::vector<llvm::BasicBlock *> &_blocks)
    {
         // Get the iterators for every predecessor of inner header
        pred_iterator PI = pred_begin(_block);
        pred_iterator E = pred_end(_block);

        // Add a block to the list
        while (PI != E)
        {
            _blocks.push_back (*PI);
        
            PI++; // Advance the iterator    
        }
    }
    
    llvm::Instruction *getInstruction ( llvm::BasicBlock *_block, unsigned _opcode, llvm::Value *_val, bool _isInstruction)
    {
        BasicBlock::iterator iptr = _block -> begin();
        int numinst = distance(iptr, _block -> end());
    
        // Iterate over isntructions
        for (int i = 0; i < numinst; i++)
        {
            // Try to find a match in opcode
            if ( (*iptr).getOpcode() == _opcode)
            {
                // Don't care about having a value
                if (_val == NULL) 
                    return &(*iptr);
                
                // The value is the instruction
                if (_isInstruction)
                {
                    // Try to cast the instruction, and see if it matches
                    Value *inst = dyn_cast<Value>(&(*iptr));
            
                    if ( _val -> getValueName() == inst -> getValueName() )
                        return &(*iptr);
                }
                else // The value is one of the operands
                {    
                    // Try to find a match in value
                    int numOps = (*iptr).getNumOperands();
                    for (int j = 0; j < numOps; j++)
                    {
                        if ((*iptr).getOperand(j) -> getValueName() == _val -> getValueName()) // Same value
                            return &(*iptr);
                    }
                }                 
            }
        
            iptr++; // Advance the pointer
        }
        
        return NULL;
    }
    
    llvm::Value *getLoopUpperBound(const llvm::Loop *loop, const llvm::Value *_indvar)
    {
        Value *upperbound = NULL; // The Induction Var
    
        // Get Basic blocks associated with the loop
        BasicBlock *header = loop->getHeader();
        
        // Find the branch instruction at the end of the block
        Instruction *branchInst = getInstruction (header, Instruction::Br);
        if (branchInst == NULL) return NULL;
        
        // Strip out the first operand of the branch, which is a compare instruction
        Instruction *cmp = dyn_cast<Instruction>(branchInst -> getOperand(0));
        if (cmp == NULL) return NULL; // Not what we needed
        
        // Try to strip out second operand, the value of the compare
        int numOps = cmp -> getNumOperands();
        for (int k = 0; k < numOps; k++)
        {
            upperbound = cmp -> getOperand(k); // Strip out the operand
        
            if (upperbound != _indvar)
                return upperbound;
        }
        
        // If we got here, nothing was found
        return NULL;
    }
    
    
   /* llvm::BasicBlock *getLoopBody(llvm::Loop *loop)
    {
        // Get the header's terminator instruction
        BasicBlock *outerHeader = loop -> getHeader();
        TerminatorInst *outerHeaderBranch = outerHeader->getTerminator();
        assert(outerHeaderBranch -> getNumSuccessors() == 2 && "Branch must be conditional");

        // Return the target of the header
        return outerHeaderBranch->getSuccessor(0);

        return NULL;
    }*/
    
    
    unsigned getAllInductionVariables (llvm::Loop* loop, std::vector<PHINode*>* indVarListPtr) {

        BasicBlock* H = loop->getHeader();
        BasicBlock* Incoming = 0;
        BasicBlock* Backedge = 0;
        pred_iterator PI = pred_begin(H);
        assert(PI != pred_end(H) && "Loop must have at least one backedge!");
        Backedge = *PI++;
        if (PI == pred_end(H)) return 0;  // dead loop
        Incoming = *PI++;
        if (PI != pred_end(H)) return 0;  // multiple backedges?

        if (loop->contains(Incoming)) {
           if (loop->contains(Backedge)) return 0;
           std::swap(Incoming, Backedge);
        } else if (!loop->contains(Backedge)) return 0;

        unsigned count = 0;

        // Loop over all of the PHI nodes, looking for a indvar.
        for (BasicBlock::iterator I = H->begin(); isa<PHINode>(I); ++I)
        {
            PHINode *PN = cast<PHINode>(I);
            Value * incomingVal = PN->getIncomingValueForBlock(Incoming);
            Value * backedgeVal = PN->getIncomingValueForBlock(Backedge);
            
            if ( isa<ConstantInt>(incomingVal) ||
                    (isa<Instruction>(incomingVal) && incomingVal->getType()->getTypeID() == Type::IntegerTyID && loop->isLoopInvariant(incomingVal)) )
                if ( Instruction *Inc = dyn_cast<Instruction>(backedgeVal) )
                {
                    if ( Inc->getOpcode() == Instruction::Add || Inc->getOpcode() == Instruction::Sub )
                    {
                        if ( Inc->getOperand(0) == PN )
                        {
                            Value * incrementOperand = Inc->getOperand(1);
                            if ( isa<ConstantInt>(incrementOperand) ||
                                    (isa<Instruction>(incrementOperand) && incrementOperand->getType()->getTypeID() == Type::IntegerTyID && loop->isLoopInvariant(incrementOperand)) )
                            {
                                indVarListPtr->push_back(PN);
                                count++;
                            }
                        }
                        else if ( Inc->getOperand(1) == PN )
                        {
                            Value * incrementOperand = Inc->getOperand(0);
                            if ( isa<ConstantInt>(incrementOperand) ||
                                    (isa<Instruction>(incrementOperand) && incrementOperand->getType()->getTypeID() == Type::IntegerTyID && loop->isLoopInvariant(incrementOperand)) )
                            {
                                indVarListPtr->push_back(PN);
                                count++;
                            }
                        }
                    }
                }
        }
        
        return count;
    }


    
    bool isLoopAdmissable(llvm::Loop* loop, PHINode** singleLoopControlIndVar)
    {
        if (singleLoopControlIndVar != NULL)
            *singleLoopControlIndVar = NULL;

        // First we check if the loop has been simplified, if not then it is not admissable
        if (!loop->isLoopSimplifyForm())
        {
            errs() << "Loop failed the simplify form test\n";
            errs() << "You may want to run the optimizer with \"-loop-simplify\" option \n";
            return false;
        }

        // The loop must have a single unique exit block
        // The function return NULL if there is more than one exit block
        if (!loop->getUniqueExitBlock())
        {
            errs() << "Loop does not a single exit block\n";
            return false;
        }

        // The loop must have a single existing basic block
        // The function return NULL if there is more than one exiting block
        BasicBlock* exiting = loop->getExitingBlock();
        if (!exiting)
        {
            errs() << "Loop has multiple exiting block\n";
            return false;
        }

        // The exiting block must be the same as the header of the loop
        BasicBlock* header = loop->getHeader();
        if (header != exiting)
        {
            errs() << "Loop exiting block is not the header\n";
            return false;
        }
        
        PHINode * loopControlVar = getSingleLoopControlIndVar (loop);
        
        if (loopControlVar == NULL)
            return false;
        
        // Get the conditional branch from the exiting BB
        Instruction * branchInst = getInstruction (header, Instruction::Br);

        // The first operand of the branch is the compare instruction
        Instruction * cmpInst = dyn_cast<Instruction> (branchInst -> getOperand(0));
        
        Value* loopLimit = NULL;
        Value * firstOp = cmpInst -> getOperand(0);
        Value * secondOp = cmpInst -> getOperand(1);
        
        if (firstOp == loopControlVar)
            loopLimit = secondOp;
        else
            loopLimit = firstOp;
        
        if (!loopLimit)
        {
            errs() << "Cannot find loop limit\n";
            return false;
        }

        // make sure that the upper bound is a loop invariant
//        if ( !loop->isLoopInvariant(loopLimit) )
        if ( !isLimitLoopInvariant(loopLimit, loop) ) 
        {
            errs() << "Loop limit is not loop invariant\n";
            //DEBUG( errs() << "    " << *loopLimit << '\n' );
            return false;
        }
        
        // get the instruction that is incrementing/decrementing the induction variable
        Instruction * incrInst = dyn_cast<Instruction>(loopControlVar->getIncomingValue(1));
        assert (incrInst != NULL && "The second incoming value of a induction variable PHINode should be an instruction.");
        
        // make sure the increment/decrement instruction is in the latch
        if ( incrInst->getParent() != loop->getLoopLatch() )
        {
            errs() << "Loop controlling induction variable must be incremented or decremented in the latch.\n";
            return false;
        }
        
        // make sure that the loop control induction variable was not modified in the loop's body
        if (incrInst->getOperand(0) != loopControlVar && incrInst->getOperand(1) != loopControlVar)
        {
            errs() << "Loop controlling induction must not be modified in the loop's body.\n";
            return false;
        }
        
        // ensure that incrInst is only used once which would be one of the incoming values in the loop control variable PHINode
        // this ensures that the loop control variable will only be used in the next iteration after being incremented/decremented
        if ( !(incrInst->hasOneUse()) )
        {
            errs() << "Loop control induction variable should not be used after the increment/decrement instruction in the same loop iteration.\n";
            return false;
        }
       
        if (singleLoopControlIndVar != NULL)
            *singleLoopControlIndVar = loopControlVar;
        
        
       // In the process of getting all induction variables, we made sure that the 
       // loop is incremented or decremented by a constant amount, so no need to 
       // check again
        
        // Now we have passed all these tests, the loop is admissable, return true
        return true;
    }
    

    bool isLimitLoopInvariant(llvm::Value* loopLimit, llvm::Loop* targetLoop) {

       //DEBUG(errs() << "Loop Limit is: " << *loopLimit << "\n");

       // Global variable are not represented by PHI nodes, so we must
       // explicitly determine if a global variable is modified in the
       // loop body. 

       // Is the loop limit is a global value?
       if (isa<GlobalVariable>(loopLimit) ) {
          //DEBUG(errs() << "....Found global variable\n");
          // Must have no stores to this global variable in the loop
          // Iterate over the basic blocks of the loop
          for (Loop::block_iterator bb_iter = targetLoop->block_begin(), E = targetLoop->block_end(); bb_iter != E; ++bb_iter) {
               BasicBlock* bb = *bb_iter;
               // Iterate over the instructions in the basic block
               for (BasicBlock::iterator instr_iter = bb->begin(),e = bb->end(); instr_iter != e; ++instr_iter) {
                   if (StoreInst* store_inst = dyn_cast<StoreInst>(instr_iter)) {
                      //DEBUG(errs() << "......Found a store, operand is: " << *(store_inst->getPointerOperand()) << "\n");
                      if (loopLimit == store_inst->getPointerOperand()) return(false);
                   }
               }
          }
       }
 
       // At this point, the loop limit is not a global variable

       // Is the limit identified as invariant by LLVM? 
       if (targetLoop->isLoopInvariant(loopLimit)) return (true);

       // Check to see of the loop limit is the result of an instruction, if not, return false
       Instruction* limitInstr = dyn_cast<Instruction>(loopLimit);
       if (!limitInstr) return (false);

       // Check to see if the instruction is a PHI instruction
       if (isa<PHINode>(limitInstr)) { 
          //DEBUG(errs() << "....Found PHI instr\n");
          return (false);
       }

       // At this point we have an instruction, so let's check if its operands are loop invariant
       bool isInvariant = true;
       for (User::op_iterator OI=limitInstr->op_begin(),OE=limitInstr->op_end(); OI != OE; ++OI) {
           Value* operand = static_cast<Value *>(*OI);
           //DEBUG(errs() << "....Found operand: " << *operand << "\n");
           isInvariant = isInvariant && isLimitLoopInvariant (operand, targetLoop);
           //DEBUG(errs() << "........isInvariant = " << isInvariant << "\n");
        }
        return (isInvariant);
    }
    
        
    PHINode * getSingleLoopControlIndVar (Loop * loop)
    {
        // Get the conditional branch and the cmp instruction from the exiting BB
        Instruction * branchInst = getInstruction (loop->getHeader(), Instruction::Br);
        if (branchInst == NULL)
        {
            errs() << "Unable to find branch instruction in the header\n";
            return NULL;
        }

        // The first operand of the branch is the compare instruction
        Instruction * cmpInst = dyn_cast<Instruction> (branchInst -> getOperand(0));
        if (cmpInst == NULL)
        {
            errs() << "Unable to file compare instruction in the header\n";
            return NULL;
        }

        // Identify the loop control variable from the cmp instruction
        // The compare instruction must have exactly two operands
        int numOpds = cmpInst->getNumOperands();
        if (numOpds != 2)
        {
            errs() << "Compare instruction in the header does not have 2 operands\n";
            return NULL;
        }
        
        std::vector<PHINode*> indVarList;
        unsigned numIndVars = getAllInductionVariables (loop, &indVarList);
        
        if (numIndVars == 0)
        {
            errs() << "Could not find any admissible integer type loop control induction variable.\n";
            return NULL;
        }
        
        Value * firstOp = cmpInst -> getOperand(0);
        Value * secondOp = cmpInst -> getOperand(1);
        
        unsigned numLoopControlIndVars = 0;
        PHINode * singleLoopControlIndVar = NULL;
        
        for (std::vector<PHINode*>::iterator iter = indVarList.begin(); iter != indVarList.end(); ++iter)
        {
            if (*iter == firstOp || *iter == secondOp)
            {
                if (numLoopControlIndVars == 0)
                {
                    ++numLoopControlIndVars;
                    singleLoopControlIndVar = *iter;
                }
            
                else
                {
                    errs() << "Loop has more than one induction variable that is used as loop control variables.\n";
                    return NULL;
                }
            }
        }
        
        if (singleLoopControlIndVar == NULL)
        {
            errs() << "Could not find any admissible integer type loop control induction variable.\n";
            return NULL;
        }
        
        return singleLoopControlIndVar;
    }

} // namespace
