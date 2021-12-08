//
//  Node.h
//  Graph
//
//  Created by Tarek Abdelrahman on 2021-03-11.
//
// This is the definition of the class Node. It represents a node in a
// graph. Each node has an ID, a label and a weight. The weight can be
// ignored in the nodes are unweighted.

#ifndef Node_h
#define Node_h

#include <iostream>
using namespace std;

class Node {
private:
    // The id of the node.
    uint32_t nodeID = 0;
    
    // The weight of the node, if nodes are weighted.
    double weight = 1.0;
    
    // The label of the node.
    string label ="";

public:
    // Constructor. Must provide at least an ID and a label.
    Node(uint32_t i, string lbl, double w = 1.0);
    
    // Accessors.
    uint32_t getID() const;
    double getWeight() const;
    string getLabel() const;
    
    // Mutators. Note that the ID cannot be modified once a node is created.
    void setWeight(double w);
    void setLabel(string op);
    
    // Print the node (for debugging mostly).
    friend ostream& operator<<(ostream&, const Node& n);
};

#endif /* Node_h */
