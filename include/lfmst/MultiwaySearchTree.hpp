#ifndef MULTIWAY_SEARCH_TREE
#define MULTIWAY_SEARCH_TREE

#include <vector>

#include "hash.hpp"
#include "Utils.hpp"

//Lock-Free Multiway Search Tree
namespace lfmst {

struct Node;

struct array {
    int* elements;
    const int length;

    array(int length) : length(length){
        elements = (int*) calloc(length, sizeof(int*));
    }

    int& operator[](int index){
        return elements[index];
    }
};

struct Contents {
    array* items;
    Node** children;
    Node* link;

    Contents(array* items, Node** children, Node* link) : items(items), children(children), link(link)  {};
};

struct Node {
    Contents* contents;

    Node(Contents* contents) : contents(contents) {}

    bool casContents(Contents* cts, Contents* newCts){
        return CASPTR(&contents, cts, newCts);
    }
};

struct Search {
    Node* node;
    Contents* contents;
    int index;

    Search();
    Search(Node* node, Contents* contents, int index) : node(node), contents(contents), index(index) {}
};

struct HeadNode {
    Node* node;
    const int height;

    HeadNode(Node* node, int height) : node(node), height(height) {}
};

template<typename T>
class MultiwaySearchTree {
    public:
        MultiwaySearchTree();
        
        bool contains(T value);
        bool add(T value);
        bool remove(T value);

    private:
        HeadNode* root;

        int search(array* items, int key); //Performs a binary search
        int randomLevel();
        void traverseAndTrack(T value, int h, Search** srchs);
        Search* traverseAndCleanup(T value);
        bool insertList(T value, Search** srchs, Node* child, int h);
        Node* splitList(T value, Search** srch);
        Search* moveForward(Node* node, T value);
        HeadNode* increaseRootHeight(int height);
        Node* cleanLink(Node* node, Contents* cts);
        void cleanNode(Node* node, Contents* cts, int i, int max);
};

template<typename T>
MultiwaySearchTree<T>::MultiwaySearchTree(){
    //init
}

template<typename T>
bool MultiwaySearchTree<T>::contains(T value){
    int key = hash(value);

    Node* node = root->node;
    Contents* cts = node->contents;

    int i = search(cts->items, key);
    while(cts->children){
        if(-i -1 == cts->items->length){
            node = cts->link;
        } else if(i < 0){
            node = cts->children[-i -1];
        } else {
            node = cts->children[i];
        }

        cts = node->contents;
        i = search(cts->items, key);
    }

    while(true){
        if(-i -1 == cts->items->length){
            node = cts->link;
        } else if(i < 0){
            return false;
        } else {
            return true;
        }

        cts = node->contents;
        i = search(cts->items, key);
    }
}

template<typename T>
bool MultiwaySearchTree<T>::add(T value){
    int height = randomLevel();
    Search** srchs = (Search**) calloc(height + 1, sizeof(Search*));
    traverseAndTrack(value, height, srchs);
    bool success = insertList(value, srchs, nullptr, 0);
    if(!success){
        return false;
    }

    for(int i = 0; i < height; ++i){
        Node* right = splitList(value, &srchs[i]);
        insertList(value, srchs, right, i + 1);
    }

    return true;
}
        
template<typename T>        
void MultiwaySearchTree<T>::traverseAndTrack(T value, int h, Search** srchs){
    int key = hash(value);

    HeadNode* root = this->root;

    if(root->height < h){
        root = increaseRootHeight(h);
    }

    int height = root->height;
    Node* node = root->node;
    Search* res = nullptr;
    while(true){
        Contents* cts = node->contents;
        int i = search(cts->items, key);

        if(-i -1 == cts->items->length){
            node = cts->link;
        } else {
            res = new Search(node, cts, i);

            if(height <= h){
                srchs[height] = res;
            }

            if(height == 0){
                return;
            }

            if(i < 0){
                i = -i -1;
            }

            node = cts->children[i];
            --height;
        }
    }
}

template<typename T>
bool MultiwaySearchTree<T>::remove(T value){
    Search* srch = traverseAndCleanup(value);

    while(true){
        Node* node = srch->node;
        Contents* cts = srch->contents;
        if(srch->index < 0){
            return false;
        }

        array* items = nullptr;//cts.items \ v; //difference
        Contents* update = new Contents(items, nullptr, cts->link);

        if(node->casContents(cts, update)){
            return true;
        }

        srch = moveForward(node, value);
    }
}

template<typename T>
Search* MultiwaySearchTree<T>::traverseAndCleanup(T value){
    int key = hash(value);

    Node* node = root->node;

    Contents* cts = node->contents;
    array* items = cts->items;
    int i = search(items, key);
    int max = -1; //TODO Change that

    while(cts->children){
        if(-i -1 == items->length){
            if(items->length > 0){
                max = (*items)[items->length - 1];
            }

            node = cleanLink(node, cts);
        } else {
            if (i < 0){
                i = -i -1;
            }

            cleanNode(node, cts, i, max);
            node = cts->children[i];
            max = -1; //TODO change that
        }

        cts = node->contents;
        items = cts->items;
        i = search(items, key);
    }

    while(true){
        if(i > -cts->items->length - 1){
            return new Search(node, cts, i);
        }

        node = cleanLink(node, cts);
        cts = node->contents;
        i = search(cts->items, key);
    }
}

int binary_search(int a[], int low, int high, int target) {
    while (low <= high) {
        int middle = low + (high - low)/2;

        if (target < a[middle]){
            high = middle - 1;
        } else if (target > a[middle]){
            low = middle + 1;
        } else {
            return middle;
        }
    }

    return -1;
}

#define MAX_HEIGHT 10

template<typename T>
int MultiwaySearchTree<T>::randomLevel(){
    static std::mt19937 gen;
    std::geometric_distribution<int> dist(1.0 - 1.0 / 32.0);
    
    int x;
    do{
        x = dist(gen);
    } while (x > MAX_HEIGHT);

    return x;
}

template<typename T>
int MultiwaySearchTree<T>::search(array* items, int key){
    return binary_search(items->elements, 0, items->length, key);
}

template<typename T>
HeadNode* MultiwaySearchTree<T>::increaseRootHeight(int target){
    HeadNode* root = this->root;
    int height = root->height;

    while(height < target){
        array* keys = new array(1);
        Node** children = (Node**) calloc(1, sizeof(Node*));
        (*keys)[0] = INT_MAX;//TODO Check that            
        children[0] = root->node;
        Contents* contents = new Contents(keys, children, nullptr);
        Node* newNode = new Node(contents);
        HeadNode* update = new HeadNode(newNode, height + 1);
        CASPTR(&this->root, root, update);
        root = this->root;
        height = root->height;
    }

    return root;
}


} //end of lfmst

#endif
