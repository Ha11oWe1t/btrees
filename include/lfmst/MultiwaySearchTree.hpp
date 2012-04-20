#ifndef MULTIWAY_SEARCH_TREE
#define MULTIWAY_SEARCH_TREE

#include <vector>
#include <algorithm>

#include "hash.hpp"
#include "Utils.hpp"
#include "HazardManager.hpp"

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
    int length;

    ~Keys(){
        if(elements){
//TODO            free(elements);
        }
    }

    Key& operator[](int index){
        return elements[index];
    }
};

//Hold a set of key including the lenght of the set
struct Children {
    Node** elements;
    int length;
    
    ~Children(){
        if(elements){
//TODO            free(elements);
        }
    }

    Node*& operator[](int index){
        return elements[index];
    }
};

struct Contents {
    Keys* items;
    Children* children;
    Node* link; //The next node
};

struct Node {
    Contents* contents;

    bool casContents(Contents* cts, Contents* newCts){
        return CASPTR(&contents, cts, newCts);
    }
};

struct Search {
    Node* node;
    Contents* contents;
    int index;
};

struct HeadNode {
    Node* node;
    int height;
};

template<typename T, int Threads>
class MultiwaySearchTree {
    public:
        MultiwaySearchTree();
        ~MultiwaySearchTree();
        
        bool contains(T value);
        bool add(T value);
        bool remove(T value);

    private:
        HeadNode* root;

        int randomSeed;
        
        HazardManager<HeadNode, Threads, 1, 1> roots;
        HazardManager<Node, Threads, 3> nodes;
        HazardManager<Contents, Threads, 4> nodeContents;
        HazardManager<Keys, Threads, 4> nodeKeys;
        HazardManager<Children, Threads, 4> nodeChildren;
        HazardManager<Search, Threads, 1> searches;

        HeadNode* newHeadNode(Node* node, int height);
        Search* newSearch(Node* node, Contents* contents, int index);
        Contents* newContents(Keys* items, Children* children, Node* link);
        Node* newNode(Contents* contents);
        Keys* newKeys(int length);
        Children* newChildren(int length);

        Keys* removeSingleItem(Keys* a, int index);
        Children* removeSingleItem(Children* a, int index);

        bool attemptSlideKey(Node* node, Contents* contents);
        bool shiftChild(Node* node, Contents* contents, int index, Node* adjustedChild);
        bool shiftChildren(Node* node, Contents* contents, Node* child1, Node* child2);
        bool dropChild(Node* node, Contents* contents, int index, Node* adjustedChild);
        bool slideToNeighbor(Node* sibling, Contents* sibContents, Key kkey, Key key, Node* child);
        Contents* deleteSlidedKey(Node* node, Contents* contents, Key key);
        Node* pushRight(Node* node, Key leftBarrier);

        Contents* cleanLink(Node* node, Contents* cts);
        void cleanNode(Key key, Node* node, Contents* cts, int index, Key leftBarrier);
        bool cleanNode1(Node* node, Contents* contents, Key leftBarrier);
        bool cleanNode2(Node* node, Contents* contents, Key leftBarrier);
        bool cleanNodeN(Node* node, Contents* contents, int index, Key leftBarrier);

        Search* traverseLeaf(Key key, bool cleanup);
        void traverseNonLeaf(Key key, int height, Search** results);
        Search* goodSamaritanCleanNeighbor(Key key, Search* results);
        bool removeFromNode(Key key, Search* results);

        Search* moveForward(Node* node, Key key, int hint);

        Keys* generateNewItems(Key key, Keys* keys, int index);
        Keys* generateLeftItems(Keys* children, int index);
        Keys* generateRightItems(Keys* children, int index);
        Children* generateNewChildren(Node* child, Children* children, int index);
        Children* generateLeftChildren(Children* children, int index);
        Children* generateRightChildren(Children* children, int index);
        Children* copyChildren(Children* rhs);

        //These methods can only be called from add
        bool insertLeafLevel(Key key, Search* results);
        bool beginInsertOneLevel(Key key, Search** results);
        Node* splitOneLevel(Key key, Search* result);
        void insertOneLevel(Key, Search** results, Node* right, int index);

        unsigned int randomLevel();
        HeadNode* increaseRootHeight(int height);
};

//Values for the random generation
static const int avgLength = 32;
static const int avgLengthMinusOne = 31;
static const int logAvgLength = 5; // log_2 of the average node length

template<typename T, int Threads>
HeadNode* MultiwaySearchTree<T, Threads>::newHeadNode(Node* node, int height){
    HeadNode* root = roots.getFreeNode();

    assert(root);

    root->node = node;
    root->height = height;

    return root;
}

template<typename T, int Threads>
Search* MultiwaySearchTree<T, Threads>::newSearch(Node* node, Contents* contents, int index){
    Search* search = searches.getFreeNode();

    assert(search);

    search->node = node;
    search->contents = contents;
    search->index = index;

    return search;
}

template<typename T, int Threads>
Contents* MultiwaySearchTree<T, Threads>::newContents(Keys* items, Children* children, Node* link){
    Contents* contents = nodeContents.getFreeNode();

    assert(contents);

    contents->items = items;
    contents->children = children;
    contents->link = link;

    return contents;
}

template<typename T, int Threads>
Node* MultiwaySearchTree<T, Threads>::newNode(Contents* contents){
    Node* node = nodes.getFreeNode();

    assert(node);

    node->contents = contents;

    return node;
}
    
template<typename T, int Threads>
Keys* MultiwaySearchTree<T, Threads>::newKeys(int length){
    Keys* keys = nodeKeys.getFreeNode();

    assert(keys);

    keys->length = length;

    //TODO A little optimization would be to keep the array if the sizes are the same (or even if the old length is greater)

    //If the object has already been used
    if(keys->elements){
        free(keys->elements);
    }

    keys->elements = (Key*) calloc(length, sizeof(Key)); 

    return keys;
}

template<typename T, int Threads>
Children* MultiwaySearchTree<T, Threads>::newChildren(int length){
    Children* children = nodeChildren.getFreeNode();

    assert(children);

    children->length = length;

    //TODO A little optimization would be to keep the array if the sizes are the same (or even if the old length is greater)
    
    //If the object has already been used
    if(children->elements){
        free(children->elements);
    }

    children->elements = (Node**) calloc(length, sizeof(Node*));

    return children;
}

/* Some internal utilities */ 

static int search(Keys* items, Key key);
static int searchWithHint(Keys* items, Key key, int hint);
static int compare(Key k1, Key k2);

template<typename T>
Key special_hash(T value){
    int key = hash(value);

    return {KeyFlag::NORMAL, key};
}

template<typename T, int Threads>
MultiwaySearchTree<T, Threads>::MultiwaySearchTree(){
    Keys* keys = newKeys(1);
    (*keys)[0] = {KeyFlag::INF, 0};

    Contents* contents = newContents(keys, nullptr, nullptr);
    Node* node = newNode(contents);

    root = newHeadNode(node, 0);
    
    std::mt19937_64 engine(time(NULL));
    std::uniform_int_distribution<unsigned int> distribution(0, std::numeric_limits<int>::max());
    randomSeed = distribution(engine) | 0x0100;
}

template<typename T, int Threads>
MultiwaySearchTree<T, Threads>::~MultiwaySearchTree(){
    if(root->node){
        if(root->node->contents){
            if(root->node->contents->items){
                nodeKeys.releaseNode(root->node->contents->items);
            }
            
            if(root->node->contents->children){
                nodeChildren.releaseNode(root->node->contents->children);
            }

            nodeContents.releaseNode(root->node->contents);
        }

        nodes.releaseNode(root->node);
    }

    roots.releaseNode(root);
}

template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::contains(T value){
    Key key = special_hash(value);

    Node* node = this->root->node;
    
    Contents* contents = node->contents;
    nodeContents.publish(contents, 0);
    nodeKeys.publish(contents->items, 0);
    nodeChildren.publish(contents->children, 0);
    
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
        nodeContents.publish(contents, 0);
        nodeKeys.publish(contents->items, 0);
        nodeChildren.publish(contents->children, 0);
        
        index = search(contents->items, key);
    }

    while(true){
        if(-index - 1 == contents->items->length){
            node = contents->link;
        } else {
            nodeContents.release(0);
            nodeKeys.release(0);
            nodeChildren.release(0);

            return index >= 0;
        }
        
        contents = node->contents;
        nodeContents.publish(contents, 0);
        nodeKeys.publish(contents->items, 0);
        nodeChildren.publish(contents->children, 0);

        index = search(contents->items, key);
    }
}

template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::add(T value){
    Key key = special_hash(value);

    unsigned int height = randomLevel();
    if(height == 0){
        Search* results = traverseLeaf(key, false);
        return insertLeafLevel(key, results);
    } else {
        Search** results = (Search**) calloc(height + 1, sizeof(Search*));
        traverseNonLeaf(key, height, results);

        bool inserted = beginInsertOneLevel(key, results);
        if(!inserted){
            //Start at 1 because beginInsertOneLevel already cleaned the first index
            for(unsigned int i = 1; i < height + 1; ++i){
                if(results[i]){
                    searches.releaseNode(results[i]);
                }
            }

            free(results);

            return false;
        }

        for(unsigned int i = 0; i < height; ++i){
            Node* right = splitOneLevel(key, results[i]);
            insertOneLevel(key, results, right, i + 1);
        }
            
        for(unsigned int i = 0; i < height + 1; ++i){
            if(results[i]){
                searches.releaseNode(results[i]);
            }
        }

        free(results);

        return true;
    }
}
        
template<typename T, int Threads>
Search* MultiwaySearchTree<T, Threads>::traverseLeaf(Key key, bool cleanup){
    Node* node = this->root->node;

    Contents* contents = node->contents;
    nodeContents.publish(contents, 0);
    nodeKeys.publish(contents->items, 0);
    nodeChildren.publish(contents->children, 0);

    int index = search(contents->items, key);
    Key leftBarrier = {KeyFlag::EMPTY, 0};

    while(contents->children){
        if(-index - 1 == contents->items->length){
            if(contents->items->length > 0){
                leftBarrier = (*contents->items)[contents->items->length - 1];
            }
            
            node = cleanLink(node, contents)->link;
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
        nodeContents.publish(contents, 0);
        nodeKeys.publish(contents->items, 0);
        nodeChildren.publish(contents->children, 0);

        index = search(contents->items, key);
    }

    while(true){
        if(index > -contents->items->length -1){
            nodeContents.release(0);
            nodeKeys.release(0);
            nodeChildren.release(0);

            return newSearch(node, contents, index);
        } else {
            node = cleanLink(node, contents)->link;
        }

        contents = node->contents;
        nodeContents.publish(contents, 0);
        nodeKeys.publish(contents->items, 0);
        nodeChildren.publish(contents->children, 0);

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
        nodeContents.publish(contents, 0);
        nodeKeys.publish(contents->items, 0);
        nodeChildren.publish(contents->children, 0);

        int index = search(contents->items, key);

        if(-index - 1 == contents->items->length){
            node = contents->link;
        } else if(height == 0){
            storeResults[0] = newSearch(node, contents, index);

            nodeContents.release(0);
            nodeKeys.release(0);
            nodeChildren.release(0);

            return;
        } else {
            Search* first_results = newSearch(node, contents, index);
            Search* results = goodSamaritanCleanNeighbor(key, first_results);

            if(results != first_results){
                searches.releaseNode(first_results);
            }

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
            searches.releaseNode(results);
            
            return false;
        } else {
            nodeContents.publish(contents, 0);
            nodeKeys.publish(contents->items, 0);
            nodeChildren.publish(contents->children, 0);

            Keys* newKeys = removeSingleItem(contents->items, index);

            Contents* update = newContents(newKeys, nullptr, contents->link);

            if(node->casContents(contents, update)){
                nodeContents.releaseNode(contents);
                nodeChildren.releaseNode(contents->children);
                nodeKeys.releaseNode(contents->items);
                
                nodeContents.release(0);
                nodeKeys.release(0);
                nodeChildren.release(0);

                searches.releaseNode(results);

                return true;
            } else {
                nodeKeys.releaseNode(newKeys);
                nodeContents.releaseNode(update);
                
                nodeContents.release(0);
                nodeKeys.release(0);
                nodeChildren.release(0);

                searches.releaseNode(results);

                results = moveForward(node, key, index);
            }
        }
    }
}

template<typename T, int Threads>
Contents* MultiwaySearchTree<T, Threads>::cleanLink(Node* node, Contents* contents){
    while(true){
        nodeContents.publish(contents, 1);
        
        Node* newLink = pushRight(contents->link, {KeyFlag::EMPTY, 0});

        if(newLink == contents->link){
            nodeContents.release(1);
            nodeKeys.release(1);
            nodeChildren.release(1);

            return contents;
        }
        
        nodeKeys.publish(contents->items, 1);
        nodeChildren.publish(contents->children, 1);

        Contents* update = newContents(contents->items, contents->children, newLink);
        if(node->casContents(contents, update)){
            nodeContents.releaseNode(contents);
            
            nodeContents.release(1);
            nodeKeys.release(1);
            nodeChildren.release(1);

            return update;
        } else {
            nodeContents.releaseNode(update);
        }

        nodeContents.release(1);
        nodeKeys.release(1);
        nodeChildren.release(1);

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
        nodeContents.publish(contents, 1);
        nodeKeys.publish(contents->items, 1);
        nodeChildren.publish(contents->children, 1);

        int length = contents->items->length;

        if(length == 0){
            return;
        } else if(length == 1){
            if(cleanNode1(node, contents, leftBarrier)){
                nodeContents.release(1);
                nodeKeys.release(1);
                nodeChildren.release(1);

                //Perhaps node->items[0] should be released there

                return;
            }
        } else if(length == 2){
            if(cleanNode2(node, contents, leftBarrier)){
                nodeContents.release(1);
                nodeKeys.release(1);
                nodeChildren.release(1);
                return;
            }
        } else {
            if(cleanNodeN(node, contents, index, leftBarrier)){
                nodeContents.release(1);
                nodeKeys.release(1);
                nodeChildren.release(1);
                return;
            }
        }

        contents = node->contents;
        nodeContents.publish(contents, 1);
        nodeKeys.publish(contents->items, 1);
        nodeChildren.publish(contents->children, 1);
        
        index = search(contents->items, key);

        if(-index - 1 == contents->items->length){
            nodeContents.release(1);
            nodeKeys.release(1);
            nodeChildren.release(1);
            return;
        } else if(index < 0){
            index = -index -1;
        }
    }
}

//contents must be published
//contents->items must be published
//contents->children must be published
template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::cleanNode1(Node* node, Contents* contents, Key leftBarrier){
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

//contents must be published by parent
//contents->items must be published
//contents->children must be published
template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::cleanNode2(Node* node, Contents* contents, Key leftBarrier){
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

//contents must be published by parent
//contents->items must be published
//contents->children must be published
template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::cleanNodeN(Node* node, Contents* contents, int index, Key leftBarrier){
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

template<typename T, int Threads>
Node* MultiwaySearchTree<T, Threads>::pushRight(Node* node, Key leftBarrier){
    while(true){
        Contents* contents = node->contents;
        nodeContents.publish(contents, 2);
        nodeKeys.publish(contents->items, 2);

        int length = contents->items->length;

        if(length == 0){
            node = contents->link;
        } else if(leftBarrier.flag == KeyFlag::EMPTY || compare((*contents->items)[length - 1], leftBarrier) > 0){
            nodeContents.release(2);
            nodeKeys.release(2);

            return node;
        } else {
            node = contents->link;
        }
    }
}

template<typename T, int Threads>
unsigned int MultiwaySearchTree<T, Threads>::randomLevel(){
    unsigned int x = randomSeed;
    x ^= x << 13;
    x ^= x >> 17;
    randomSeed = x ^= x << 5;
    unsigned int level = 1;
    while ((x & avgLengthMinusOne) == 0) {
        if ((level % 6) == 0) {
            x = randomSeed;
            x ^= x << 13;
            x ^= x >> 17;
            randomSeed = x ^= x << 5;
        } else {
            x >>= logAvgLength;
        }

        level++;
    }

    return (level - 1);
}

//The Contents containing items should have been published
//items should have been published
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
        int mid = (low + high) >> 1;
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

//The Contents containing items should have been published
//items should have been published
int searchWithHint(Keys* items, Key key, int hint){
    int low = 0;
    int mid = hint;
    int high = items->length - 1;

    if(low > high){
        return -1;
    }

    if((*items)[high].flag == KeyFlag::INF){
        high--;
    }

    if(mid > high){
        mid = (low + high) >> 1;
    }

    while(low <= high){
        Key midVal = (*items)[mid];

        int cmp = compare(key, midVal);
        if(cmp > 0){
            low = mid + 1;
        } else if(cmp < 0){
            high = mid - 1;
        } else {
            return mid;
        }

        mid = (low + high) >> 1;
    }
    
    return -(low + 1); //not found
}

template<typename T, int Threads>
HeadNode* MultiwaySearchTree<T, Threads>::increaseRootHeight(int target){
    HeadNode* root = this->root;
    roots.publish(root, 0);

    int height = root->height;

    while(height < target){
        Keys* keys = newKeys(1);
        (*keys)[0].flag = KeyFlag::INF;

        Children* children = newChildren(1); 
        (*children)[0] = root->node;
        
        Contents* contents = newContents(keys, children, nullptr);
        Node* newHeadNodeNode = newNode(contents);
        HeadNode* update = newHeadNode(newHeadNodeNode, height + 1);
        
        if(CASPTR(&this->root, root, update)){
            roots.releaseNode(root);
        } else {
            nodeChildren.releaseNode(children);
            nodeKeys.releaseNode(keys);
            nodeContents.releaseNode(contents);
            nodes.releaseNode(newHeadNodeNode);
            roots.releaseNode(update);
        }

        root = this->root;
        roots.publish(root, 0);

        height = root->height;
    }

    roots.release(0);

    return root;
}

template<typename T, int Threads>
Search* MultiwaySearchTree<T, Threads>::moveForward(Node* node, Key key, int hint){
    while(true){
        Contents* contents = node->contents;
        nodeContents.publish(contents, 1);
        nodeKeys.publish(contents->items, 1);

        int index = searchWithHint(contents->items, key, hint);
        if(index > -contents->items->length - 1){
            nodeContents.release(1);
            nodeKeys.release(1);

            return newSearch(node, contents, index);
        } else {
            node = contents->link;
        }
    }
}

//contents must be published by parent
//contents->items must be published
//contents->children must be published
template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::shiftChild(Node* node, Contents* contents, int index, Node* adjustedChild){
    Children* children = copyChildren(contents->children);
    (*children)[index] = adjustedChild;

    Contents* update = newContents(contents->items, children, contents->link);
    if(node->casContents(contents, update)){
        nodeContents.releaseNode(contents);
        nodeChildren.releaseNode(contents->children);

        return true;
    } else {
        nodeChildren.releaseNode(children);
        nodeContents.releaseNode(update);

        return false;
    }
}

//contents must be published by parent
//contents->items must be published
//contents->children must be published
template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::shiftChildren(Node* node, Contents* contents, Node* child1, Node* child2){
    Children* children = newChildren(2);
    (*children)[0] = child1;
    (*children)[1] = child2;

    Contents* update = newContents(contents->items, children, contents->link);
    if(node->casContents(contents, update)){
        nodeContents.releaseNode(contents);
        nodeChildren.releaseNode(contents->children);

        return true;
    } else {
        nodeChildren.releaseNode(children);
        nodeContents.releaseNode(update);

        return false;
    }
}

//contents must be published by parent
//contents->children must be published
//contents->item must be published
template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::dropChild(Node* node, Contents* contents, int index, Node* adjustedChild){
    int length = contents->items->length;

    Keys* keys = newKeys(length - 1);
    Children* children = newChildren(length - 1);
    
    std::copy(contents->items->elements, contents->items->elements + index, keys->elements);
    std::copy(contents->children->elements, contents->children->elements + index, children->elements);
    
    (*children)[index] = adjustedChild;
    
    std::copy(contents->items->elements + index + 1, contents->items->elements + length, keys->elements + index);
    std::copy(contents->children->elements + index + 2, contents->children->elements + length, children->elements + index + 1);

    Contents* update = newContents(keys, children, contents->link);
    if(node->casContents(contents, update)){
        nodeContents.releaseNode(contents);
        nodeChildren.releaseNode(contents->children);
        nodeKeys.releaseNode(contents->items);

        return true;
    } else {
        nodeChildren.releaseNode(children);
        nodeKeys.releaseNode(keys);
        nodeContents.releaseNode(update);

        return false;
    }
}

//contents is published by parent
//contents->items is published
//contents->children is published
template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::attemptSlideKey(Node* node, Contents* contents){
    if(!contents->link){
        return false;
    }

    int length = contents->items->length;
    Key kkey = (*contents->items)[length - 1];
    Node* child = (*contents->children)[length - 1];
    Node* sibling = pushRight(contents->link, {KeyFlag::EMPTY, 0});
    
    Contents* siblingContents = sibling->contents;
    nodeContents.publish(siblingContents, 2);
    nodeKeys.publish(siblingContents->items, 2);
    nodeChildren.publish(siblingContents->children, 2);

    Node* nephew = nullptr;

    if(siblingContents->children->length == 0){
        nodeContents.release(2);
        nodeKeys.release(2);
        nodeChildren.release(2);

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
        nodeContents.release(2);
        nodeKeys.release(2);
        nodeChildren.release(2);

        return false;
    }

    bool success = slideToNeighbor(sibling, siblingContents, kkey, kkey, child);
    if(success){
        deleteSlidedKey(node, contents, kkey);
    }
    
    nodeContents.release(2);
    nodeKeys.release(2);
    nodeChildren.release(2);

    return true;
}

//sibContents is published by parent
//sibContents->items is published
//sibContents->children is published
template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::slideToNeighbor(Node* sibling, Contents* sibContents, Key kkey, Key key, Node* child){
    int index = search(sibContents->items, key);
    if(index >= 0){
        return true;
    } else if(index < -1){
        return false;
    }

    Keys* keys = generateNewItems(kkey, sibContents->items, 0);
    Children* children = generateNewChildren(child, sibContents->children, 0);

    Contents* update = newContents(keys, children, sibContents->link);
    if(sibling->casContents(sibContents, update)){
        nodeContents.releaseNode(sibContents);
        nodeKeys.releaseNode(sibContents->items);
        nodeChildren.releaseNode(sibContents->children);

        return true;
    } else {
        nodeKeys.releaseNode(keys);
        nodeChildren.releaseNode(children);
        nodeContents.releaseNode(update);

        return false;
    }
}

//contents is published
//contents->items is published
//contents->children is published
template<typename T, int Threads>
Contents* MultiwaySearchTree<T, Threads>::deleteSlidedKey(Node* node, Contents* contents, Key key){
    int index = search(contents->items, key);
    if(index < 0){
        return contents;
    }

    Keys* keys = removeSingleItem(contents->items, index);
    Children* children = removeSingleItem(contents->children, index);

    Contents* update = newContents(keys, children, contents->link);
    if(node->casContents(contents, update)){
        nodeContents.releaseNode(contents);
        nodeChildren.releaseNode(contents->children);
        nodeKeys.releaseNode(contents->items);

        return update;
    } else {
        nodeKeys.releaseNode(keys);
        nodeChildren.releaseNode(children);
        nodeContents.releaseNode(update);

        return contents;
    }
}

template<typename T, int Threads>
Search* MultiwaySearchTree<T, Threads>::goodSamaritanCleanNeighbor(Key key, Search* results){
    Node* node = results->node;
    
    Contents* contents = results->contents;
    nodeContents.publish(contents, 2);

    if(!contents->link){
        nodeContents.release(2);

        return results;
    }
    
    nodeKeys.publish(contents->items, 2);
    nodeChildren.publish(contents->children, 2);

    int length = contents->items->length;
    Key leftBarrier = (*contents->items)[length - 1];
    Node* child = (*contents->children)[length - 1];
    Node* sibling = pushRight(contents->link, {KeyFlag::EMPTY, 0});

    Contents* siblingContents = sibling->contents;
    nodeContents.publish(siblingContents, 3);
    nodeKeys.publish(siblingContents->items, 3);
    nodeChildren.publish(siblingContents->children, 3);

    Node* nephew = nullptr;
    Node* adjustedNephew = nullptr;

    if(siblingContents->children->length == 0){
        contents = cleanLink(node, node->contents);
        int index = search(contents->items, key);
        
        nodeContents.release(2);
        nodeKeys.release(2);
        nodeChildren.release(2);
        nodeContents.release(3);
        nodeKeys.release(3);
        nodeChildren.release(3);

        return newSearch(node, contents, index); //Perhaps a problem as there are no reference to contents
    } else {
        nephew = (*siblingContents->children)[0];
    }

    if(compare((*siblingContents->items)[0], leftBarrier) > 0){
        adjustedNephew = pushRight(nephew, leftBarrier);
    } else {
        adjustedNephew = pushRight(nephew, {KeyFlag::EMPTY, 0});
    }

    if(nephew != child){
        if(adjustedNephew != nephew){
            shiftChild(sibling, siblingContents, 0, adjustedNephew);
        }
    } else {
        bool success = slideToNeighbor(sibling, siblingContents, leftBarrier, leftBarrier, child);//TODO check leftBarrier
        if(success){
            contents = deleteSlidedKey(node, contents, leftBarrier);//TODO Check leftBarrier
            nodeContents.publish(contents, 2);
            nodeKeys.publish(contents->items, 2);
            
            int index = search(contents->items, key);
            
            nodeContents.release(2);
            nodeKeys.release(2);
            nodeChildren.release(2);
            nodeContents.release(3);
            nodeKeys.release(3);
            nodeChildren.release(3);
            
            return newSearch(node, contents, index); //Perhaps a problem as there are no reference to contents
        }
    }
    
    nodeContents.release(2);
    nodeKeys.release(2);
    nodeChildren.release(2);
    nodeContents.release(3);
    nodeKeys.release(3);
    nodeChildren.release(3);

    return results;
}

template<typename T, int Threads>
Node* MultiwaySearchTree<T, Threads>::splitOneLevel(Key key, Search* results){
    Search* entry_results = results;

    while(true){
        Node* node = results->node;
        
        Contents* contents = results->contents;
        nodeContents.publish(contents, 0);
        nodeKeys.publish(contents->items, 0);
        nodeChildren.publish(contents->children, 0);
        
        int index = results->index;
        int length = contents->items->length;

        if(index < 0){
            nodeContents.release(0);
            nodeKeys.release(0);
            nodeChildren.release(0);

            if(results != entry_results){
                searches.releaseNode(results);
            }
            
            return nullptr;
        } else if(length < 2 || index == (length - 1)){
            nodeContents.release(0);
            nodeKeys.release(0);
            nodeChildren.release(0);

            if(results != entry_results){
                searches.releaseNode(results);
            }
            
            return nullptr;
        }

        Keys* leftKeys = generateLeftItems(contents->items, index);
        Keys* rightKeys = generateRightItems(contents->items, index);
        Children* leftChildren = generateLeftChildren(contents->children, index);
        Children* rightChildren = generateRightChildren(contents->children, index);
        
        Contents* rightContents = newContents(rightKeys, rightChildren, contents->link);
        Node* right = newNode(rightContents);
        Contents* left = newContents(leftKeys, leftChildren, right);

        if(node->casContents(contents, left)){
            nodeContents.releaseNode(contents);
            nodeChildren.releaseNode(contents->children);
            nodeKeys.releaseNode(contents->items);
            
            nodeContents.release(0);
            nodeKeys.release(0);
            nodeChildren.release(0);

            if(results != entry_results){
                searches.releaseNode(results);
            }

            return right;
        } else {
            nodeKeys.releaseNode(leftKeys);
            nodeKeys.releaseNode(rightKeys);
            nodeChildren.releaseNode(leftChildren);
            nodeChildren.releaseNode(rightChildren);

            nodeContents.releaseNode(rightContents);
            nodes.releaseNode(right);
            nodeContents.releaseNode(left);

            if(results != entry_results){
                searches.releaseNode(results);
            }

            results = moveForward(node, key, index);
        }
        
        nodeContents.release(0);
        nodeKeys.release(0);
        nodeChildren.release(0);
    }
}

template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::insertLeafLevel(Key key, Search* results){
    while(true){
        Node* node = results->node;
        
        Contents* contents = results->contents;
        nodeContents.publish(contents, 0);
        nodeKeys.publish(contents->items, 0);
        nodeChildren.publish(contents->children, 0);
        
        Keys* keys = contents->items;
        int index = results->index;

        if(index >= 0){
            nodeContents.release(0);
            nodeKeys.release(0);
            nodeChildren.release(0);
            
            searches.releaseNode(results);

            return false; //TODO Check that            
        } else {
            index = -index - 1;
            Keys* newKeys = generateNewItems(key, keys, index);
            
            Contents* update = newContents(newKeys, nullptr, contents->link);
            if(node->casContents(contents, update)){
                nodeContents.releaseNode(contents);
                nodeChildren.releaseNode(contents->children);
                nodeKeys.releaseNode(contents->items);
                
                nodeContents.release(0);
                nodeKeys.release(0);
                nodeChildren.release(0);
                
                searches.releaseNode(results);
                
                return true;
            } else {
                nodeKeys.releaseNode(newKeys);
                nodeContents.releaseNode(update);

                searches.releaseNode(results);

                results = moveForward(node, key, index);
            }

            nodeContents.release(0);
            nodeKeys.release(0);
            nodeChildren.release(0);
        }
    }
}

template<typename T, int Threads>
bool MultiwaySearchTree<T, Threads>::beginInsertOneLevel(Key key, Search** resultsStore){
    Search* results = resultsStore[0];

    while(true){
        Node* node = results->node;
        
        Contents* contents = results->contents;
        nodeContents.publish(contents, 0);
        nodeKeys.publish(contents->items, 0);
        nodeChildren.publish(contents->children, 0);

        int index = results->index;
        Keys* keys = contents->items;

        if(index >= 0){
            nodeContents.release(0);
            nodeKeys.release(0);
            nodeChildren.release(0);
                
            //If we return false, the value in resultsStore will never be used
            searches.releaseNode(results);
            
            return false;
        } else {
            index = -index - 1;

            Keys* newKeys = generateNewItems(key, keys, index);
            
            Contents* update = newContents(newKeys, nullptr, contents->link);
            if(node->casContents(contents, update)){
                nodeContents.releaseNode(contents);
                nodeChildren.releaseNode(contents->children);
                nodeKeys.releaseNode(contents->items);
                
                nodeContents.release(0);
                nodeKeys.release(0);
                nodeChildren.release(0);
                
                searches.releaseNode(results);
                if(resultsStore[0] != results){
                    searches.releaseNode(resultsStore[0]);
                }

                resultsStore[0] = newSearch(node, update, index);

                return true;
            } else {
                nodeKeys.releaseNode(newKeys);
                nodeContents.releaseNode(update);
                
                searches.releaseNode(results);
                
                results = moveForward(node, key, index);
            }

            nodeContents.release(0);
            nodeKeys.release(0);
            nodeChildren.release(0);
        }
    }
}

template<typename T, int Threads>
void MultiwaySearchTree<T, Threads>::insertOneLevel(Key key, Search** resultsStore, Node* child, int target){
    if(!child){
        return;
    }

    Search* results = resultsStore[target];
    Search* entry_results = results;

    while(true){
        Node* node = results->node;
        
        Contents* contents = results->contents;
        nodeContents.publish(contents, 0);
        nodeKeys.publish(contents->items, 0);
        nodeChildren.publish(contents->children, 0);

        int index = results->index;

        if(index >= 0){
            if(results != entry_results){
                searches.releaseNode(results);
            }

            nodeContents.release(0);
            nodeKeys.release(0);
            nodeChildren.release(0);

            return;
        } else if(index > -contents->items->length - 1){
            index = -index -1;

            Keys* newKeys = generateNewItems(key, contents->items, index);
            Children* newChildren = generateNewChildren(child, contents->children, index + 1);
            
            Contents* update = newContents(newKeys, newChildren, contents->link);
            if(node->casContents(contents, update)){
                if(results != entry_results){
                    searches.releaseNode(results);
                }

                nodeContents.releaseNode(contents);
                nodeChildren.releaseNode(contents->children);
                nodeKeys.releaseNode(contents->items);
                
                nodeContents.release(0);
                nodeKeys.release(0);
                nodeChildren.release(0);

                searches.releaseNode(resultsStore[target]);

                resultsStore[target] = newSearch(node, update, index);
                
                return;
            } else {
                nodeKeys.releaseNode(newKeys);
                nodeChildren.releaseNode(newChildren);
                nodeContents.releaseNode(update);
            
                if(results != entry_results){
                    searches.releaseNode(results);
                }
                
                results = moveForward(node, key, index);
            }
        } else {
            if(results != entry_results){
                searches.releaseNode(results);
            }

            results = moveForward(node, key, -index - 1);
        }
        
        nodeContents.release(0);
        nodeKeys.release(0);
        nodeChildren.release(0);
    }
}

/* Utility methods to manipulate arrays */

template<typename T, int Threads>
Children* MultiwaySearchTree<T, Threads>::copyChildren(Children* rhs){
    Children* copy = newChildren(rhs->length);
    
    std::copy(rhs->elements, rhs->elements + rhs->length, copy->elements);

    return copy;
}

template<typename T, int Threads>
Keys* MultiwaySearchTree<T, Threads>::removeSingleItem(Keys* a, int index){
    int length = a->length;
    Keys* newArray = newKeys(length - 1);

    std::copy(a->elements, a->elements + index, newArray->elements);
    std::copy(a->elements + index + 1, a->elements + length, newArray->elements + index);

    return newArray;
}

template<typename T, int Threads>
Children* MultiwaySearchTree<T, Threads>::removeSingleItem(Children* a, int index){
    int length = a->length;
    Children* newArray = newChildren(length - 1);

    std::copy(a->elements, a->elements + index, newArray->elements);
    std::copy(a->elements + index + 1, a->elements + length, newArray->elements + index);

    return newArray;
}

template<typename T, int Threads>
Keys* MultiwaySearchTree<T, Threads>::generateNewItems(Key key, Keys* items, int index){
    if(!items){
        return nullptr;
    }

    int length = items->length;
    Keys* newItems = newKeys(length + 1);
    
    std::copy(items->elements, items->elements + index, newItems->elements);
    (*newItems)[index] = key;
    std::copy(items->elements + index, items->elements + length, newItems->elements + index + 1);
    
    return newItems;
}

template<typename T, int Threads>
Children* MultiwaySearchTree<T, Threads>::generateNewChildren(Node* child, Children* children, int index){
    if(!children){
        return nullptr;
    }

    int length = children->length;
    Children* newItems = newChildren(length + 1);
    
    std::copy(children->elements, children->elements + index, newItems->elements);
    (*newItems)[index] = child;
    std::copy(children->elements + index, children->elements + length, newItems->elements + index + 1);
    
    return newItems;
}

template<typename T, int Threads>
Keys* MultiwaySearchTree<T, Threads>::generateLeftItems(Keys* items, int index){
    if(!items){
        return nullptr;
    }

    Keys* newItems = newKeys(index + 1);
    std::copy(items->elements, items->elements + index + 1, newItems->elements);
    return newItems;
}

template<typename T, int Threads>
Children* MultiwaySearchTree<T, Threads>::generateLeftChildren(Children* children, int index){
    if(!children){
        return nullptr;
    }

    Children* newItems = newChildren(index + 1);
    std::copy(children->elements, children->elements + index + 1, newItems->elements);
    return newItems;
}

template<typename T, int Threads>
Keys* MultiwaySearchTree<T, Threads>::generateRightItems(Keys* items, int index){
    if(!items){
        return nullptr;
    }

    Keys* newItems = newKeys(items->length - index - 1);
    std::copy(items->elements + index + 1, items->elements + items->length, newItems->elements);
    return newItems;
}

template<typename T, int Threads>
Children* MultiwaySearchTree<T, Threads>::generateRightChildren(Children* children, int index){
    if(!children){
        return nullptr;
    }

    Children* newItems = newChildren(children->length - index - 1);
    std::copy(children->elements + index + 1, children->elements + children->length, newItems->elements);
    return newItems;
}

} //end of lfmst

#endif
