# If we don't need RTTI or EH, there's no reason to export anything
# from the hello plugin.
if( NOT LLVM_REQUIRES_RTTI )
  if( NOT LLVM_REQUIRES_EH )
	  set(LLVM_EXPORTED_SYMBOL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/StaticPass.exports)
  endif()
endif()

if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()


add_llvm_library( LLVMAssn1 MODULE BUILDTREE_ONLY
  StaticPass.cpp
  StatDyn1.cpp
  DervInd.cpp
  genGraph.cpp
  Skeleton.cpp
  LoopUtils.cpp
  #Edge.cpp
  #Graph.cpp
  #GraphUtils.cpp
  #Node.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )

target_link_libraries(LLVMAssn1 "/home/sambhusn/llvm-project/llvm/lib/Transforms/LLVMAssngs/cgramap/Graph/lib/libgraph.a")
