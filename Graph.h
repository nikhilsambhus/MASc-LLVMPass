//
//  Graph.h
//  Graph
//
//  Created by Tarek Abdelrahman between 2021-03-11 and 2021-04-01.
//
// This set of classes represent Graphs. They are intended for small graphs, 10's
// of nodes and 100's of edges. They are rather time and space inefficient since
// almost all operations are O(|E|) or O(|V|).

#ifndef Graph_h
#define Graph_h

#include <iostream>
using namespace std;
#include <list>
#include <map>
#include <string>
#include <set>

#include "Node.h"
#include "Edge.h"

/******************************************************************************/
/* Definition of a base (abstract) class                                      */
/******************************************************************************/
class Graph {
protected:
    // This is the most basic representation of the graph: a list of nodes
    // and a list of edges. Both initially empty.
    list<Node> nodeList;
    list<Edge> edgeList;
    
    // A variable to indicate if a graph has been "optimized" or not
    bool optimized = false;
    
    // The following are used (and are valid) when the graph is optimized
    
    // The maximum ids of nodes and edges
    uint32_t max_node_id = 0;
    uint32_t max_edge_id = 0;
    
    // A dynamically allocated array of nodes for use when graph layout is optimized
    tuple<Node*, uint32_t, uint32_t>* nodesArray = nullptr;
    
    // A dynamically allocated array of pointer to outgoing edges
    Edge** outEdgesArray = nullptr;
    
    // A dynamically allocated array of pointer to incoming edges
    Edge** inEdgesArray = nullptr;
  
public:
    // The destrucor is made abstract to ensure class cannot be instantiated.
    virtual ~Graph() = 0;
    
    // Get the number of node/edges in the graph.
    uint32_t getNumNodes() const;
    uint32_t getNumEdges() const;
    
    // Get the maximum id value of a node/an edge. This is needed because some
    // operations create and index into arrays based on the node/edge id. However,
    // the ids *may not* be in the range of 0 to numNodes-1. If the graph has no
    // nodes/edges an unsinged -1 is returned.
    uint32_t getMaxNodeID() const;
    uint32_t getMaxEdgeID() const;
    
    // Get iterator over the nodes.
    list<Node>::iterator nodeBegin();
    list<Node>::iterator nodeEnd();
    
    // Get iterator over the edges.
    list<Edge>::iterator edgeBegin() {return edgeList.begin();}
    list<Edge>::iterator edgeEnd() {return edgeList.end();}
    
    // Find a node with the id given by the argument. Return either
    // a pointer to the found node or nullptr if no such node exists.
    virtual const Node* findNode(uint32_t id) const;
    
    // Add a node to the node list. Throw an exception if there is already
    // a node with the same id.
    virtual void addNode(uint32_t id, string lbl, double w = 1.0);
    
    // Delete a node with the id given by the argument. Throw an exception
    // if the node does not exist. All edges connected to the deleted node
    // are also deleted.
    virtual void deleteNode(uint32_t id);
    
    // Find an edge with the argument id. Return a pointer to the found edge
    // or nullptr if no such edge exists.
    virtual const Edge* findEdge(uint32_t id) const;
    
    // Add an edge to the edge list. Throw and exception if an edge with
    // the same edge id already exists.
    virtual void addEdge(uint32_t id, uint32_t src, uint32_t dest, string lbl, double w = 1.0);
    
    // Delete an edge pointed to by the argument. Throw an exception if
    // edge does not exist. 
    virtual void deleteEdge(uint32_t id);
    
    // Get the successors/predecessors of a node. The successors/predecessors depend
    // on the graph type and thus these abstract methods must be defined in derived
    // classes based on the type of the graph (directed/undirected/etc.).
    virtual void getSuccessors(uint32_t id, list<Node>& successors) const = 0;
    virtual void getPredecessors(uint32_t id, list<Node>& predecessors) const = 0;
    
    // Get the outgoing/incoming edges of a node. The outgoing/incoming edges depend
    // on the graph type and thus these abstract methods must be defined in derived
    // classes based on the type of the graph (directed/undirected/etc.).
    virtual void getInEdges(uint32_t id, list<Edge>& successors) const = 0;
    virtual void getOutEdges(uint32_t id, list<Edge>& predecessors) const = 0;
    
    // This method optimizes the layout of the graph. Should any
    // nodes/edges be added or deleted, the optimized flag is turned off and it
    // is expected the method will be called again. The implementation may differ
    // depending on the type of graph, so the method is made abstract.
    virtual void optimize() = 0;
};

/******************************************************************************/
/* Definition of a class represents directed graphs (digraphs).               */
/******************************************************************************/
class DiGraph : public Graph {
public:
    // Constructors
    // Build an empty DiGraph
    DiGraph();
    
    // Build a DiGraph from a dot file (type of graph in file must be digraph)
    DiGraph(string dotfile);
    
    // Gets all successors of a node. Throws an exception if a node with the id
    // in the first argument does not exist. Adds the successors to the list
    // specificed by the second argument
    void getSuccessors(uint32_t id, list<Node>& successors) const;
    
    // Gets all predecessors of a node. Throws an exception if a node with the id
    // in the first argument does not exist. Adds the predecessors to the list
    // specificed by the second argument
    void getPredecessors(uint32_t id, list<Node>& predecessors) const;
    
    // Gets all outgoing edges of a node. Throws an exception if a node with the
    // id in the argument does not exist. Adds the outgoing edges to the list
    // specificed by the second argument
    void getOutEdges(uint32_t id, list<Edge>& outs) const;
    
    // Gets all incoming edges of a node. Throws an exception if a node with the
    // id in the argument does not exist. Adds the incoming edges to the list
    // specificed by the second argument
    void getInEdges(uint32_t id, list<Edge>& ins) const;

    // This method optimizes the layout of the graph. It sorts the node list be
    // the node id. It then sorts the edge list by the srcNodeID of each edge.
    // It further constructs a map between each node id and the start and end
    // outgoing edges. It finally sets the optimized flag to true. This is done
    // to make graph operations faster. The method should be run after constructing
    // the graph when the graph is no longer expected to change. Should any
    // nodes/edges be added or deleted, the optimized flag is turned off and it
    // is expected the method will be called again. The implementation may differ
    // depending on the type of graph, so the method is made abstract
    virtual void optimize();
    
    // Prints the graph to the output stream (for debugging mostly)
    friend ostream& operator<<(ostream& os, DiGraph& g);
};

/******************************************************************************/
/* Definition of a class that represents undirected graphs.                   */
/******************************************************************************/
class UndGraph : public Graph {
public:
    // Constructors
    // Build an empty UndGraph
    UndGraph();
    
    // Build an UndGraph from a dot file (type of graph in file must be graph)
    UndGraph(string dotfile);
    
    // Gets all neighbors of a node. Throws an exception if a node with the id
    // in the first argument does not exist. Adds the neighbors to the list
    // specificed by the second argument.
    // The methods getSuccessors/getPredecessors simply get the neighbors
    void getNeighbors(uint32_t id, list<Node>& neighbors) const;
    void getSuccessors(uint32_t id, list<Node>& successors) const;
    void getPredecessors(uint32_t id, list<Node>& predecessors) const;
    
    // Gets all edges of a node. Throws an exception if a node with the
    // id in the argument does not exist. Adds the edges to the list
    // specificed by the second argument
    // The methods getInEdges/getOutEdges simply get the edges of a node
    void getEdges(uint32_t id, list<Edge>& edges) const;
    void getInEdges(uint32_t id, list<Edge>& successors) const;
    void getOutEdges(uint32_t id, list<Edge>& predecessors) const;
    
    virtual void optimize();
    
    // Prints the graph to the output stream (for debugging mostly)
    friend ostream& operator<<(ostream& os, DiGraph& g);
};

/******************************************************************************/
/* Definition of a class represents directed a acyclic graph (dag).It         */
/* inherits from DiGraph and adds checks to make sure graph has no cycles.    */
/******************************************************************************/
class DAG : public DiGraph {
private:
    string name = "";
    
public:
    // Constructors
    DAG();
    DAG(string dotfile);
    
    // Accessor/Mutator for the graph name
    string getName() const;
    void setName(string n);
    
    // Prints the graph to the output stream (for debugging mostly)
    friend ostream& operator<<(ostream& os, DAG& g);
};

#endif /* Graph_h */
