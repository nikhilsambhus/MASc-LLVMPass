//
//  GraphUtils.h
//  Graph
//
//  Created by Tarek Abdelrahman on 2021-03-11.
//

#ifndef GraphUtils_h
#define GraphUtils_h

#include <iostream>
using namespace std;

// Determines of the graph is a DAG. Returns true if it is, otherwise, returns false
bool isDAG(DAG& g);

// Detemine the shorest path between a source node and a destination node (specified by
// their ids) in graph g. The shortest path is placed in the array path (which much
// be pre allocated) and the actual shortest distance in mindist. Throws an exception
// if parents is nullptr or if the nodes with ids src/dest do not exist.
void shortestPath(Graph& g, std::set<uint32_t> src, uint32_t dest, uint32_t* parents, double& mindist);

// Function for writing a dot file decribing the graph
void toDOT(string filename, Graph& g);

// Function for reading a dot file decribing the graph
// The form of dot files that can be read is very limited
// Refer to documentation for more details
void fromDOT(string filename, Graph& g);

// Performs a depth first traversal of the graph starting at u with a parent p.
// This method assumes arrays start, finish, parent and visited have been
// allocated and properly initialized.
void DFS(Graph& g, uint32_t u, uint32_t p);


#endif /* GraphUtils_h */
