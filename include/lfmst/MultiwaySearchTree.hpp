#ifndef MULTIWAY_SEARCH_TREE
#define MULTIWAY_SEARCH_TREE

#include <vector>

#include "hash.hpp"
#include "Utils.hpp"

//Lock-Free Multiway Search Tree
namespace lfmst {

struct Node;

//In this data structure keys can be null or POSITIVE_INFINITY
enum class KeyFlag : unsigned char {
    NORMAL,
    EMPTY,
    INF //POSITIVE_INFINITY
};

struct Key {
    KeyFlag flag;
    int key;

    Key() : flag(KeyFlag::EMPTY), key(0) {}
    Key(KeyFlag flag, int key) : flag(flag), key(key) {}

    bool operator==(const Key& rhs){
        return flag == rhs.flag && key == rhs.key;
    }
    
    bool operator!=(const Key& rhs){
        return !(*this == rhs);
    }
};

//Hold a set of key include the length of the set
struct Keys {
    Key* elements;
    const int length;

    Keys(int length) : length(length){
        elements = new Key[length];
    }

    Key& operator[](int index){
        return elements[index];
    }
};

//Hold a set of key including the lenght of the set
struct Children {
    Node** elements;
    const int length;

    Children(int length) : length(length){
        elements = (Node**) calloc(length, sizeof(Node*));
    }

    Node*& operator[](int index){
        return elements[index];
    }
};

struct Contents {
    Keys* items;
    Children* children;
    Node* link;

    Contents(Keys* items, Children* children, Node* link) : items(items), children(children), link(link)  {};
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

template<typename T, int Threads>
class MultiwaySearchTree {
    public:
        MultiwaySearchTree();
        
        bool contains(T value);
        bool add(T value);
        bool remove(T value);

    private:
        HeadNode* root;

        Search* traverseLeaf(Key key, bool cleanup);
        void traverseNonLeaf(Key key, int height, Search** results);
        Search* goodSamaritanCleanNeighbor(Key key, Search* results);
        bool removeFromNode(Key key, Search* results);

        int randomLevel();
        Search* moveForward(Node* node, Key key);
        HeadNode* increaseRootHeight(int height);
        Node* cleanLink(Node* node, Contents* cts);
        void cleanNode(Key key, Node* node, Contents* cts, int index, Key leftBarrier);
};

/* Some internal utilities */ 
int search(Keys* items, Key key);
bool cleanNode1(Node* node, Contents* contents, Key leftBarrier);
bool cleanNode2(Node* node, Contents* contents, Key leftBarrier);
bool cleanNodeN(Node* node, Contents* contents, int index, Key leftBarrier);
Node* pushRight(Node* node, Key leftBarrier);
bool attemptSlideKey(Node* node, Contents* contents);
bool shiftChild(Node* node, Contents* contents, int index, Node* adjustedChild);
bool shiftChildren(Node* node, Contents* contents, Node* child1, Node* child2);
bool dropChild(Node* node, Contents* contents, int index, Node* adjustedChild);
bool slideToNeighbor(Node* sibling, Contents* sibContents, Key key, Node* child);
Contents* deleteSlidedKey(Node* node, Contents* contents, Key key);
bool insertLeafLevel(Key key, Search* results);
bool beginInsertOneLevel(Key key, Search** results);
Node* splitOneLevel(Key key, Search* result);
void insertOneLevel(Key, Search** results, Node* right, int index);
Keys* generateNewItems(Key key, Keys* keys, int index);
Children* generateNewChildren(Node* child, Children* children, int index);

template<typename T>
Key special_hash(T value){
    int key = hash(value);

    return {KeyFlag::NORMAL, key};
}

template<typename T, int Threads>
MultiwaySearchTree<T, Threads>::MultiwaySearchTree(){
    //init
}

template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::contains(T value){
    Key key = special_hash(value);

    Node* node = this->root->node;
    Contents* contents = node->contents;
    
    int index = search(contents->items, key);
    while(contents->children){
        if(-index -1 == contents->items->length){
            node = contents->link;
        } else if(index < 0){
            node = (*contents->children)[-index -1];
        } else {
            node = (*contents->children)[index];
        }

        contents = node->contents;
        index = search(contents->items, key);
    }

    while(true){
        if(-index - 1 == contents->items->length){
            node = contents->link;
        } else {
            return index >= 0;
        }
        
        contents = node->contents;
        index = search(contents->items, key);
    }
}

template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::add(T value){
    Key key = special_hash(value);

    int height = randomLevel();
    if(height == 0){
        Search* results = traverseLeaf(key, false);
        return insertLeafLevel(key, results);
    } else {
        Search** results = (Search**) calloc(height + 1, sizeof(Search*));
        traverseNonLeaf(key, height, results);

        bool old = beginInsertOneLevel(key, results);
        if(old){
            return false;
        }

        for(int i = 0; i < height; ++i){
            Node* right = splitOneLevel(key, results[i]);
            insertOneLevel(key, results, right, i + 1);
        }

        return true;
    }
}
        
template<typename T, int Threads>
Search* MultiwaySearchTree<T, Threads>::traverseLeaf(Key key, bool cleanup){
    Node* node = root->node;
    Contents* contents = node->contents;
    int index = search(contents->items, key);
    Key leftBarrier = {KeyFlag::EMPTY, 0};

    while(contents->children){
        if(-index - 1 == contents->items->length){
            if(contents->items->length > 0){
                leftBarrier = (*contents->items)[contents->items->length - 1];
            }
            
            node = cleanLink(node, contents);
        } else {
            if(index < 0){
                index = -index - 1;
            }

            if(cleanup){
                cleanNode(key, node, contents, index, leftBarrier);
            }
            
            node = (*contents->children)[index];
            leftBarrier = {KeyFlag::EMPTY, 0};
        }
        
        contents = node->contents;
        index = search(contents->items, key);
    }

    while(true){
        if(index >  -contents->items->length -1){
            return new Search(node, contents, index);
        } else {
            node = cleanLink(node, contents);
        }

        contents = node->contents;
        index = search(contents->items, key);
    }
}

template<typename T, int Threads>
void MultiwaySearchTree<T, Threads>::traverseNonLeaf(Key key, int target, Search** storeResults){
    HeadNode* root = this->root;

    if(root->height < target){
        root = increaseRootHeight(target);
    }

    int height = root->height;
    Node* node = root->node;

    while(true){
        Contents* contents = node->contents;
        int index = search(contents->items, key);

        if(-index - 1 == contents->items->length){
            node = contents->link;
        } else if(height == 0){
            storeResults[0] = new Search(node, contents, index);
            return;
        } else {
            Search* results = new Search(node, contents, index);
            results = goodSamaritanCleanNeighbor(key, results);

            if(height <= target){
                storeResults[height] = results;
            }
            if(index < 0){
                index = -index - 1;
            }

            node = (*contents->children)[index];
            height = height - 1;
        }
    }
}

template<typename T>
T* removeSingleItem(T* a, int index){
    T* newArray = new T(a->length - 1);

    for(int i = 0; i < index; ++i){
        (*newArray)[i] = (*a)[i];
    }

    for(int i = index + 1; i < a->length; ++i){
        (*newArray)[i - 1] = (*a)[i];
    }

    return newArray;
}

template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::remove(T value){
    Key key = special_hash(value);
    Search* results = traverseLeaf(key, true);

    return removeFromNode(key, results);
}

template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::removeFromNode(Key key, Search* results){
    while(true){
        Node* node = results->node;
        Contents* contents = results->contents;
        int index = results->index;

        if(index < 0){
            return false;
        } else {
            Keys* newKeys = removeSingleItem(contents->items, index);

            Contents* update = new Contents(newKeys, nullptr, contents->link);

            if(node->casContents(contents, update)){
                return true;
            } else {
                results = moveForward(node, key);
            }
        }
    }
}

template<typename T, int Threads>
Node* MultiwaySearchTree<T, Threads>::cleanLink(Node* node, Contents* contents){
    while(true){
        Node* newLink = pushRight(contents->link, {KeyFlag::EMPTY, 0});

        if(newLink == contents->link){
            return contents->link;
        }

        Contents* update = new Contents(contents->items, contents->children, newLink);

        if(node->casContents(contents, update)){
            return update->link;
        }

        contents = node->contents;
    }
}

int compare(Key k1, Key k2){
    if(k1.flag == KeyFlag::INF){
        return 1;
    }
    
    if(k2.flag == KeyFlag::INF){
        return -1;
    }

    return k1.key - k2.key; //TODO Check if 1 - 2 or 2 - 1
}

template<typename T, int Threads>
void MultiwaySearchTree<T, Threads>::cleanNode(Key key, Node* node, Contents* contents, int index, Key leftBarrier){
    while(true){
        int length = contents->items->length;

        if(length == 0){
            return;
        } else if(length == 1){
            if(cleanNode1(node, contents, leftBarrier)){
                return;
            }
        } else if(length == 2){
            if(cleanNode2(node, contents, leftBarrier)){
                return;
            }
        } else {
            if(cleanNodeN(node, contents, index, leftBarrier)){
                return;
            }
        }

        contents = node->contents;
        index = search(contents->items, key);

        if(-index - 1 == contents->items->length){
            return;
        } else if(index < 0){
            index = -index -1;
        }
    }
}

bool cleanNode1(Node* node, Contents* contents, Key leftBarrier){
    bool success = attemptSlideKey(node, contents);

    if(success){
        return true;
    }

    Key key = (*contents->items)[0];

    if(leftBarrier.flag != KeyFlag::EMPTY && compare(key, leftBarrier) <= 0){
        leftBarrier = {KeyFlag::EMPTY, 0};
    }

    Node* childNode = (*contents->children)[0];
    Node* adjustedChild = pushRight(childNode, leftBarrier);

    if(adjustedChild == childNode){
        return true;
    }

    return shiftChild(node, contents, 0, adjustedChild);
}

bool cleanNode2(Node* node, Contents* contents, Key leftBarrier){
    bool success = attemptSlideKey(node, contents);

    if(success){
        return true;
    }

    Key key = (*contents->items)[0];

    if(leftBarrier.flag != KeyFlag::EMPTY && compare(key, leftBarrier) <= 0){
        leftBarrier = {KeyFlag::EMPTY, 0};
    }

    Node* childNode1 = (*contents->children)[0];
    Node* adjustedChild1 = pushRight(childNode1, leftBarrier);
    leftBarrier = (*contents->items)[0];
    Node* childNode2 = (*contents->children)[1];
    Node* adjustedChild2 = pushRight(childNode2, leftBarrier);

    if((adjustedChild1 == childNode1) && (adjustedChild2 == childNode2)){
        return true;
    }

    return shiftChildren(node, contents, adjustedChild1, adjustedChild2);
}

bool cleanNodeN(Node* node, Contents* contents, int index, Key leftBarrier){
    Key key0 = (*contents->items)[0];

    if(index > 0){
        leftBarrier = (*contents->items)[index - 1];
    } else if(leftBarrier.flag != KeyFlag::EMPTY && compare(key0, leftBarrier) <= 0){
        leftBarrier = {KeyFlag::EMPTY, 0};
    }

    Node* childNode = (*contents->children)[index];
    Node* adjustedChild = pushRight(childNode, leftBarrier);

    if(index == 0 || index == contents->children->length - 1){
        if(adjustedChild == childNode){
            return true;
        }

        return shiftChild(node, contents, index, adjustedChild);
    }

    Node* adjustedNeighbor = pushRight((*contents->children)[index + 1], (*contents->items)[index]);

    if(adjustedNeighbor == adjustedChild){
        return dropChild(node, contents, index, adjustedChild);
    } else if(adjustedChild != childNode){
        return shiftChild(node, contents, index, adjustedChild);
    } else {
        return true;
    }
}

Node* pushRight(Node* node, Key leftBarrier){
    while(true){
        Contents* contents = node->contents;

        int length = contents->items->length;

        if(length == 0){
            node = contents->link;
        } else if(leftBarrier.flag == KeyFlag::EMPTY || compare((*contents->items)[length - 1], leftBarrier) > 0){
            return node;
        } else {
            node = contents->link;
        }
    }
}

#define MAX_HEIGHT 10

//TODO Put the generator at the class level
template<typename T, int Threads>
int MultiwaySearchTree<T, Threads>::randomLevel(){
    static std::mt19937 gen;
    std::geometric_distribution<int> dist(1.0 - 1.0 / 32.0);
    
    //TODO If rand > MAX_HEIGHT return MAX_LEVEL
    int x;
    do{
        x = dist(gen);
    } while (x > MAX_HEIGHT);

    return x;
}

int search(Keys* items, Key key){
    int low = 0;
    int high = items->length - 1;

    if(low > high){
        return -1;
    }

    if((*items)[high].flag == KeyFlag::INF){
        high--;
    }

    while(low <= high){
        int mid = (low + high) >> 1; //TODO check
        Key midVal = (*items)[mid];
        int cmp = compare(key, midVal);

        if(cmp > 0){
            low = mid + 1;
        } else if(cmp < 0){
            high = mid - 1;
        } else {
            return mid;
        }
    }
    
    return -(low + 1); //not found
}

template<typename T, int Threads>
HeadNode* MultiwaySearchTree<T, Threads>::increaseRootHeight(int target){
    HeadNode* root = this->root;
    int height = root->height;

    while(height < target){
        Keys* keys = new Keys(1);
        (*keys)[0].flag = KeyFlag::INF;
        Children* children = new Children(1); 
        (*children)[0] = root->node;
        Contents* contents = new Contents(keys, children, nullptr);
        Node* newNode = new Node(contents);
        HeadNode* update = new HeadNode(newNode, height + 1);
        CASPTR(&this->root, root, update);
        root = this->root;
        height = root->height;
    }

    return root;
}

template<typename T, int Threads>
Search* MultiwaySearchTree<T, Threads>::moveForward(Node* node, Key key){
    while(true){
        Contents* contents = node->contents;

        int index = search(contents->items, key);

        if(index > -contents->items->length - 1){
            return new Search(node, contents, index);
        } else {
            node = contents->link;
        }
    }
}

Children* copyChildren(Children* rhs){
    Children* copy = new Children(rhs->length);

    for(int i = 0; i < rhs->length; ++i){
        (*copy)[i] = (*rhs)[i];
    }

    return copy;
}

bool shiftChild(Node* node, Contents* contents, int index, Node* adjustedChild){
    Children* newChildren = copyChildren(contents->children);
    (*newChildren)[index] = adjustedChild;

    Contents* update = new Contents(contents->items, newChildren, contents->link);
    return node->casContents(contents, update);
}

bool shiftChildren(Node* node, Contents* contents, Node* child1, Node* child2){
    Children* newChildren = new Children(2);
    (*newChildren)[0] = child1;
    (*newChildren)[0] = child2;

    Contents* update = new Contents(contents->items, newChildren, contents->link);
    return node->casContents(contents, update);
}

bool dropChild(Node* node, Contents* contents, int index, Node* adjustedChild){
    int length = contents->items->length;

    Keys* newKeys = new Keys(length - 1);
    Children* newChildren = new Children(length - 1);

    for(int i = 0; i < index; ++i){
        (*newKeys)[i] = (*contents->items)[i];
        (*newChildren)[i] = (*contents->children)[i];
    }
    
    (*newChildren)[index] = adjustedChild;
    
    for(int i = index + 1; i < length; ++i){
        (*newKeys)[i - 1] = (*contents->items)[i];
    }

    for(int i = index + 2; i < length; ++i){
        (*newChildren)[i - 1] = (*contents->children)[i];
    }

    Contents* update = new Contents(newKeys, newChildren, contents->link);
    return node->casContents(contents, update);
}

bool attemptSlideKey(Node* node, Contents* contents){
    if(!contents->link){
        return false;
    }

    int length = contents->items->length;
    Key kkey = (*contents->items)[length - 1];
    Node* child = (*contents->children)[length - 1];
    Node* sibling = pushRight(contents->link, {KeyFlag::EMPTY, 0});
    Contents* siblingContents = sibling->contents;
    Node* nephew = nullptr;

    if(siblingContents->children->length == 0){
        return false;
    } else {
        nephew = (*siblingContents->children)[0];
    }

    if(compare((*siblingContents->items)[0], kkey) > 0){
        nephew = pushRight(nephew, kkey);
    } else {
        nephew = pushRight(nephew, {KeyFlag::EMPTY, 0});
    }

    if(nephew != child){
        return false;
    }

    bool success = slideToNeighbor(sibling, siblingContents, kkey, child);
    if(success){
        deleteSlidedKey(node, contents, kkey);
    }

    return false;
}

bool slideToNeighbor(Node* sibling, Contents* sibContents, Key key, Node* child){
    int index = search(sibContents->items, key);

    if(index >= 0){
        return true;
    } else if(index < -1){
        return false;
    }

    Keys* keys = generateNewItems(key, sibContents->items, 0);
    Children* children = generateNewChildren(child, sibContents->children, 0);

    Contents* update = new Contents(keys, children, sibContents->link);
    if(sibling->casContents(sibContents, update)){
        return true;
    } else {
        return false;
    }
}

Contents* deleteSlidedKey(Node* node, Contents* contents, Key key){
    int index = search(contents->items, key);

    if(index < 0){
        return contents;
    }

    Keys* keys = removeSingleItem(contents->items, index);
    Children* children = removeSingleItem(contents->children, index);

    Contents* update = new Contents(keys, children, contents->link);
    if(node->casContents(contents, update)){
        return update;
    } else {
        return contents;
    }
}

} //end of lfmst

#endif
