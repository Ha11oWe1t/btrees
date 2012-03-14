#ifndef HAZARD_MANAGER
#define HAZARD_MANAGER

#include <omp.h>

#define NUM_THREADS 4 //TODO Use a template parameter for the max number of threads

template<typename Node, int Size = 2>
class HazardManager {
    public:
        HazardManager();

        void releaseNode(Node* node);
        Node* getFreeNode();

    private:
        Node* Pointers[NUM_THREADS][Size];
        Node* LocalQueues[NUM_THREADS][2]; 
        
        bool isReferenced(Node* node);
};

template<typename Node, int Size>
HazardManager<Node, Size>::HazardManager(){
    for(int tid = 0; tid < NUM_THREADS; ++tid){
        for(int j = 0; j < Size; ++j){
            Pointers[tid][j] = LocalQueues[tid][j] = nullptr;
        }
    }
}

template<typename Node, int Size>
void HazardManager<Node, Size>::releaseNode(Node* node){
    int tid = omp_get_thread_num();

    node->nextNode = nullptr;

    if(!LocalQueues[tid][0]){
        LocalQueues[tid][0] = LocalQueues[tid][1] = node;
    } else {
        LocalQueues[tid][1]->nextNode = node;
        LocalQueues[tid][1] = node;
    }
}

template<typename Node, int Size>
Node* HazardManager<Node, Size>::getFreeNode(){
    int tid = omp_get_thread_num();

    Node* node; 
    Node* pred = LocalQueues[tid][0];

    if(pred){
        if(!isReferenced(pred)){
            LocalQueues[tid][0] = pred->nextNode;
            return pred;
        }

        while((node = pred->nextNode)){
            if(!IsReferenced(node)){
                if(!(pred->nextNode = node->nextNode)){
                    LocalQueues[tid][1] = pred;
                }

                return node;
            } else {
                pred = node;
            }
        }
    }

    //The queue is empty or every node is still referenced by other threads
    return new Node();
}

template<typename Node, int Size>
bool HazardManager<Node, Size>::isReferenced(Node* node){
    for(int tid = 0; tid < NUM_THREADS; ++tid){
        for(int i = 0; i < Size; ++i){
            if(Pointers[tid][i] == node){
                return true;
            }
        }
    }

    return false;
}

#endif
