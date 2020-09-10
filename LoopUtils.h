//****************************************************************
// The GPGPU Autotuning Compiler Project
// Copyright (c) 2014-2014 University of Toronto.
// All rights reserved.
// 
// See License.txt for licensing and redistribution.
// 
// File: 
//       LoopUtils.h
// 
// Description:
//       This code defines various utilities for manipulating and working with loops.
//
// Authors:
//       Robert Longo
//       Leslie Barron
//       Tarek Abdelrahman
//       Ayush Shrestha
//
// Revisions:
//       July 14, 2014: code for loop admissability added
//       June 10, 2014: code created
//*****************************************************************

#ifndef _LOOPUTIL_H_
#define _LOOPUTIL_H_

#include "llvm/ADT/Twine.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"

#include <vector>

namespace autotune {

    /* Returns the loop at the given depth from loop or null
     */
    llvm::Loop *getLoopForDepth(llvm::Loop *loop, unsigned depth);

    /* Checks if loop is perfectly nested to a given depth 
     * Depth should include outer loop, so a depth of 1 is always perfectly nested
     */
    bool isLoopPerfectlyNested(const llvm::Loop *loop, unsigned depth, bool printMessages = true);
    
    /* Returns the first Phi in the header that satisfies the following conditions:
     *   1) The Phi has 2 incoming values
     *   2) The first value is a constant int
     *   3) The second value is an Add or Sub instruction
     *   4) The second value's first argument is the Phi node
     *   5) The second value's second argument is a constant int
     */
    llvm::PHINode *getInductionVariable(const llvm::Loop *loop);

    /* Returns the number of Phis that satisfy the above conditions
     */
    unsigned getNumInductionVariables(const llvm::Loop *loop);

    /* Determines whether or not the loop is analyzable based on the fact that:
            1) No breaks statements (br to exit)
            2) The loop has only one induction variable
        loop:: the loop that will be analyzed
        RETURNS:: true if above criterion are satisfied*/
    bool isLoopAnalyzable(llvm::Loop *loop);
    
    /* Create a list of basic blocks that are the predecessors of the block
        _block:: the block to be analyzed
        _blocks:: a std::vector of blocks push blocks to. Must be allocated ahead of time. Calls push-back*/
    void getAllPredecessors(llvm::BasicBlock *_block, std::vector<llvm::BasicBlock *> &_blocks);
   
   /* Tries to find an instruction in the basic block with a specified opcode, and value
        _block:: the block to search through
        _opcode:: the opcode being sought
        _val:: the value being sought. Can be null if you only want the first matching instruction
        _isInstruction:: is the instruction that you are looking for the value?
        RETURNS:: a pointer to the instruction if it is found*/
   llvm::Instruction *getInstruction (llvm::BasicBlock *_block, unsigned _opcode, llvm::Value *_val=NULL, bool _isInstruction=false);
   
    /* Gets a pointer to the upper bound of the loop. Guarenteed to work if getCanonicalIndVar Passes
        loop:: loop to analyze
        _indvar:: the indvar associated with teh loop
        RETURNS:: ptr to the value */
    llvm::Value *getLoopUpperBound (const llvm::Loop *loop, const llvm::Value *_indvar);
    
    /* Gets the very first body of a loop (the block after the header)
        loop:: the loop to get the body of
        RETURNS:: a pointer to the loop body*/
    llvm::BasicBlock *getLoopBody(llvm::Loop *loop);

    // Gets all the induction variables in a loop into the second argument
    // Returns the number of induction variables found
    unsigned getAllInductionVariables(llvm::Loop* loop, 
                                  std::vector<llvm::PHINode*>* indVarListPtr);

    // Returns true if the loop is admissable
    // A loop is admissable if it has a single exit block, a single exiting block
    // and that exiting block is the loop's header. This effectively require "for"
    // loops without breaks. In addition, the loop is admissable only if branch 
    // instruction at the end of the header (loop terminator) uses one of the loop's
    // induction variables and has a limit that is a loop invariant. This effectively
    // requires a counted "for" loop where the count is not a function of the loop
    // body. The induction variable used to terminate the loop is called the loop
    // control variable.
    // TO DO: add a second parameter to the function to return the loop control variable
    bool isLoopAdmissable(llvm::Loop* loop, llvm::PHINode** singleLoopControlIndVar = NULL);
    

    // Returns true if the loop limit is loop invariant. This function extends the
    // isLoopInvaraint method of the Loop class when the limit is not recognized as
    // an invaraint by this method to recusively check if the operands are invaraints
    bool isLimitLoopInvariant(llvm::Value* loopLimit, llvm::Loop* loop);
    
    llvm::PHINode * getSingleLoopControlIndVar (llvm::Loop * loop);

}

#endif
