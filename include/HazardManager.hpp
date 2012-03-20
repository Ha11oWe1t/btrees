#ifndef HAZARD_MANAGER
#define HAZARD_MANAGER

template<typename Node, int Threads, int Size = 2, int Prefill = 25>
class HazardManager {
    public:
        HazardManager();
        ~HazardManager();

        /* Manage nodes */
        void releaseNode(Node* node);
        Node* getFreeNode();

        /* Manage references  */
        void publish(Node* node, int i);
        void release(int i);

    private:
        Node* Pointers[Threads][Size];
        Node* LocalQueues[Threads][2]; 
        
        bool isReferenced(Node* node);
};

template<typename Node, int Threads, int Size, int Prefill>
HazardManager<Node, Threads, Size, Prefill>::HazardManager(){
    for(int tid = 0; tid < Threads; ++tid){
        for(int j = 0; j < Size; ++j){
            Pointers[tid][j] = nullptr;
        }

        LocalQueues[tid][0] = nullptr;
        LocalQueues[tid][1] = nullptr;
    }
}

template<typename Node, int Threads, int Size, int Prefill>
HazardManager<Node, Threads, Size, Prefill>::~HazardManager(){
    for(int tid = 0; tid < Threads; ++tid){
        //No need to delete Hazard Pointers because each thread need to release its published references

        //Delete all the elements of the local queue
        if(LocalQueues[tid][0]){
            //There are only a single element in the local queue
            if(LocalQueues[tid][0] == LocalQueues[tid][1]){
                delete LocalQueues[tid][0];
            }
            //Delete all the elements of the queue            
            else {
                Node* node; 
                Node* pred = LocalQueues[tid][0];

                while((node = pred->nextNode)){
                    delete pred;
                    pred = node;
                }

                delete LocalQueues[tid][1];
            }
        }
    }
}

template<typename Node, int Threads, int Size, int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::releaseNode(Node* node){
    int tid = thread_num;

    node->nextNode = nullptr;

    if(!LocalQueues[tid][0]){
        LocalQueues[tid][0] = LocalQueues[tid][1] = node;
    } else {
        LocalQueues[tid][1]->nextNode = node;
        LocalQueues[tid][1] = node;
    }
}

template<typename Node, int Threads, int Size, int Prefill>
Node* HazardManager<Node, Threads, Size, Prefill>::getFreeNode(){
    int tid = thread_num;

    Node* node; 
    Node* pred = LocalQueues[tid][0];

    //If there are nodes on the local queue, try to get a non reference node
    if(pred){
        if(!isReferenced(pred)){
            LocalQueues[tid][0] = pred->nextNode;

            return pred;
        }

        while((node = pred->nextNode)){
            if(!isReferenced(node)){
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

template<typename Node, int Threads, int Size, int Prefill>
bool HazardManager<Node, Threads, Size, Prefill>::isReferenced(Node* node){
    for(int tid = 0; tid < Threads; ++tid){
        for(int i = 0; i < Size; ++i){
            if(Pointers[tid][i] == node){
                return true;
            }
        }
    }

    return false;
}
        

template<typename Node, int Threads, int Size, int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::publish(Node* node, int i){
    Pointers[thread_num][i] = node;
}

template<typename Node, int Threads, int Size, int Prefill>
void HazardManager<Node, Threads, Size, Prefill>::release(int i){
    Pointers[thread_num][i] = nullptr;
}

#endif
