//
//  Edge.h
//  Graph
//
//  Created by Tarek Abdelrahman on 2021-03-11.
//
// This class defines an edge in a graph. Each edge has an ID, a source and
// a destination node IDs, a label and a weight. The weight defaults to 1.0
// if the edge is unweighted.

#ifndef Edge_h
#define Edge_h

#include <iostream>
using namespace std;

class Edge {
private:
    // The id of the edge; must be unique.
    uint32_t edgeID = 0;
    
    // A pointer to the source node of the edge.
    uint32_t srcNodeID = 0;
    
    // A pointer to the destination node of the edge.
    uint32_t destNodeID = 0;
    
    // The label of this edge.
    string label = "";
    
    // The weight of this edge.
    double weight = 1.0;
    
public:
    // Constructor.
    Edge(uint32_t id, uint32_t src, uint32_t dest, string lbl, double w = 1.0);
    
    // Accessors
    uint32_t getID() const;
    uint32_t getSrcNodeID() const;
    uint32_t getDestNodeID() const;
    string getLabel() const;
    double getWeight() const;
    
    // Mutators. Note that the IDs cannot be changed once an edge is created.
    void setLabel(string s);
    void setWeight(double w);
    
    // Print the edge to the output stream, used mostly for debugging.
    friend ostream& operator<<(ostream& os, const Edge& e);
};

#endif /* Edge_h */
