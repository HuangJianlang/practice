//
// Created by Jianlang Huang on 11/5/20.
//

#include <mach/host_info.h>

typedef struct tagNode{
    double item;
    struct tagNode* next;
}Node;

class Iterator{
private:
    Node* node;

public:
    Iterator() : node(nullptr){

    }

    Iterator(Node* data) : node(data){

    }

    double operator*(){
        return node->item;
    }

    Iterator& operator++(){
        node = node->next;
        return *this;
    }

    Iterator operator++(int){
        Iterator ret = *this;
        node = node->next;
        return ret;
    }

    bool operator==(Iterator iter){
        if (iter == nullptr){
            return this->node == nullptr;
        } else {
            if (this->node->item == iter.node->item){
                return true;
            }
        }

        return false;
    }

    bool operator!=(Iterator iterator){
        return ! (*this == iterator);
    }
};

Iterator find_all (Iterator iter, const double val){
    Iterator start = iter;
    for(; start != nullptr; start++){

    }
}