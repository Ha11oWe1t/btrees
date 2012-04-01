#include <iostream>
#include <cassert>
#include <vector>
#include <algorithm>
#include <sstream>

//C++11
#include <limits>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>

//Thread local id
__thread unsigned int thread_num;

#include "zipf.hpp"

//Include all the trees implementations
#include "skiplist/SkipList.hpp"
#include "nbbst/NBBST.hpp"
#include "avltree/AVLTree.hpp"
#include "lfmst/MultiwaySearchTree.hpp"
#include "cbtree/CBTree.hpp"

//Number of nodes for the tests (for up to 32 threads)
#define N 10000//00         //A too big number can put nodes in swap

//For benchmark
#define OPERATIONS 1000000

//Chrono typedefs
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds milliseconds;
typedef std::chrono::microseconds microseconds;

void test();
void perfTest();
void memoryTest();

int main(int argc, const char* argv[]) {
    std::cout << "Concurrent Binary Trees test" << std::endl;

    //By default launch perf test
    if(argc == 1){
        perfTest();
    } else if(argc == 2) {
        std::string arg = argv[1];

        if(arg == "-perf"){
            perfTest();
        } else if(arg == "-test"){
            test();
        } else if(arg == "-memory"){
            memoryTest();
        } else {
            std::cout << "Unrecognized option " << arg << std::endl;
        }
    } else {
        std::cout << "Too many arguments" << std::endl;
    }

    return 0;
}

template<typename T>
void testST(const std::string& name){
    std::cout << "Test single-threaded (with " << N << " elements) " << name << std::endl;
    
    T tree;

    //TODO Make some remove when the tree is empty
    //TODO Make some contains when the tree is empty

    //Insert sequential numbers
    for(unsigned int i = 0; i < N; ++i){
        assert(!tree.contains(i));
        assert(tree.add(i));
        assert(tree.contains(i));
    }
    
    //Remove all the sequential numbers
    for(unsigned int i = 0; i < N; ++i){
        assert(tree.contains(i));
        assert(tree.remove(i));     
        assert(!tree.contains(i));
    }
    
    //Verify again that all the numbers have been removed
    for(unsigned int i = 0; i < N; ++i){
        assert(!tree.contains(i));
    }

    std::mt19937_64 engine(time(NULL));
    std::uniform_int_distribution<int> distribution(0, INT_MAX);
    auto generator = std::bind(distribution, engine);

    std::vector<int> rand;

    //Insert N random numbers in the tree
    for(unsigned int i = 0; i < N; ++i){
        int number = generator();

        if(tree.contains(number)){
            assert(!tree.add(number));     
            assert(tree.contains(number));
        } else {
            assert(tree.add(number));     
            assert(tree.contains(number));

            rand.push_back(number);
        }
    }
    
    //Try remove when the number is not in the tree
    for(unsigned int i = 0; i < N; ++i){
        int number = generator();

        if(!tree.contains(number)){
            assert(!tree.remove(number));
            assert(!tree.contains(number));
        }
    }

    //Avoid removing in the same order
    random_shuffle(rand.begin(), rand.end());

    //Verify that we can remove all the numbers from the tree
    for(int number : rand){
        assert(tree.contains(number));
        assert(tree.remove(number));
    }

    std::cout << "Test passed successfully" << std::endl;
}

//This test could be improved with some randomness
template<typename T, unsigned int Threads>
void testMT(){
    std::cout << "Test with " << Threads << " threads" << std::endl;

    T tree;

    std::vector<std::thread> pool;
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, i](){
            thread_num = i;

            int tid = thread_num;

            //Insert sequential numbers
            for(int j = tid * N; j < (tid + 1) * N; ++j){
                assert(!tree.contains(j));
                assert(tree.add(j));
                assert(tree.contains(j));
            }

            //Remove all the sequential numbers
            for(int j = tid * N; j < (tid + 1) * N; ++j){
                assert(tree.contains(j));
                assert(tree.remove(j));   
                assert(!tree.contains(j));
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    pool.clear();
    
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, i](){
            thread_num = i;

            //Verify that every numbers has been removed correctly
            for(unsigned int i = 0; i < Threads * N; ++i){
                assert(!tree.contains(i));
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});
    
    std::cout << "Test with " << Threads << " threads passed succesfully" << std::endl;
}

#define TEST(type, name) \
    testST<type<int, 1>>(name);\
    std::cout << "Test multi-threaded (with " << N << " elements) " << name << std::endl;\
    testMT<type<int, 2>, 2>();\
    testMT<type<int, 3>, 3>();\
    testMT<type<int, 4>, 4>();\
    testMT<type<int, 6>, 6>();\
    testMT<type<int, 8>, 8>();\
    testMT<type<int, 12>, 12>();\
    testMT<type<int, 16>, 16>();\
    testMT<type<int, 32>, 32>();

void test(){
    std::cout << "Tests the different versions" << std::endl;

    TEST(skiplist::SkipList, "SkipList")
    TEST(nbbst::NBBST, "Non-Blocking Binary Search Tree")
    //TEST(avltree::AVLTree, "Optimistic AVL Tree")
    //TEST(lfmst::MultiwaySearchTree, "Lock Free Multiway Search Tree");
    //TEST(cbtree::CBTree, "Counter Based Tree");
}

template<typename Tree, unsigned int Threads>
void random_bench(const std::string& name, unsigned int range, unsigned int add, unsigned int remove){
    Tree tree;

    Clock::time_point t0 = Clock::now();
    
    std::vector<std::thread> pool;
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, range, add, remove, i](){
            thread_num = i;

            unsigned int tid = thread_num;

            std::mt19937_64 engine(time(0) + tid);

            std::uniform_int_distribution<int> valueDistribution(0, range);
            auto valueGenerator = std::bind(valueDistribution, engine);

            std::uniform_int_distribution<int> operationDistribution(0, 99);
            auto operationGenerator = std::bind(operationDistribution, engine);

            for(int i = 0; i < OPERATIONS; ++i){
                unsigned int value = valueGenerator();
                unsigned int op = operationGenerator();

                if(op < add){
                    tree.add(value);
                } else if(op < (add + remove)){
                    tree.remove(value);
                } else {
                    tree.contains(i);
                }
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();

    milliseconds ms = std::chrono::duration_cast<milliseconds>(t1 - t0);
    unsigned long throughput = (Threads * OPERATIONS) / ms.count();

    std::cout << name << " througput with " << Threads << " threads = " << throughput << " operations / ms" << std::endl;
}

#define BENCH(type, name, range, add, remove)\
    random_bench<type<int, 1>, 1>(name, range, add, remove);\
    random_bench<type<int, 2>, 2>(name, range, add, remove);\
    random_bench<type<int, 3>, 3>(name, range, add, remove);\
    random_bench<type<int, 4>, 4>(name, range, add, remove);\
    random_bench<type<int, 8>, 8>(name, range, add, remove);

void random_bench(unsigned int range, unsigned int add, unsigned int remove){
    std::cout << "Bench with " << OPERATIONS << " operations/thread, range = " << range << ", " << add << "% add, " << remove << "% remove, " << (100 - add - remove) << "% contains" << std::endl;

    //TODO Check why the test are slower than the bench itself

    BENCH(skiplist::SkipList, "SkipList", range, add, remove);
    BENCH(nbbst::NBBST, "Non-Blocking Binary Search Tree", range, add, remove);
    //BENCH(avltree::AVLTree, "Optimistic AVL Tree", range, add, remove)
    //BENCH(lfmst::MultiwaySearchTree, "Lock-Free Multiway Search Tree", range, add, remove);
    //BENCH(cbtree::CBTree, "Counter Based Tree", range, add, remove);
}

void random_bench(unsigned int range){
    random_bench(range, 50, 50);   //50% put, 50% remove, 0% contains
    random_bench(range, 20, 10);   //20% put, 10% remove, 70% contains
    random_bench(range, 9, 1);     //9% put, 1% remove, 90% contains
}

void random_bench(){
    //random_bench(2000);        //Key in {0, 2000}
    random_bench(200000);        //Key in {0, 200000}
}

void duration(Clock::time_point t0, Clock::time_point t1){
    microseconds us = std::chrono::duration_cast<microseconds>(t1 - t0);

    if(us.count() < 100000){
        std::cout << us.count() << "us";
    } else {
        milliseconds ms = std::chrono::duration_cast<milliseconds>(t1 - t0);

        std::cout << ms.count() << "ms";
    }
}

template<typename Tree, unsigned int Threads>
void seq_construction_bench(const std::string& name, unsigned int size){
    Tree tree;

    Clock::time_point t0 = Clock::now();
    
    unsigned int part = size / Threads;

    std::vector<std::thread> pool;
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, part, size, i](){
            thread_num = i;

            unsigned int tid = thread_num;

            for(unsigned int i = tid * part; i < (tid + 1) * part; ++i){
                tree.add(i);
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();

    std::cout << "Construction of " << name << " with " << size << " elements took ";
    duration(t0, t1); 
    std::cout << " with " << Threads << " threads" << std::endl;

    //Empty the tree
    for(unsigned int i = 0; i < size; ++i){
        tree.remove(i);
    }
}

#define SEQ_CONSTRUCTION(type, name, size)\
    seq_construction_bench<type<int, 1>, 1>(name, size);\
    seq_construction_bench<type<int, 2>, 2>(name, size);\
    seq_construction_bench<type<int, 3>, 3>(name, size);\
    seq_construction_bench<type<int, 4>, 4>(name, size);\
    seq_construction_bench<type<int, 8>, 8>(name, size);

void seq_construction_bench(){
    std::cout << "Bench the seq construction time of each data structure" << std::endl;

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000, 20000000};

    for(auto size : sizes){
        SEQ_CONSTRUCTION(skiplist::SkipList, "SkipList", size);
        SEQ_CONSTRUCTION(skiplist::SkipList, "NBBST", size);
    }

    //TODO Continue that
}

template<typename Tree, unsigned int Threads>
void random_construction_bench(const std::string& name, unsigned int size){
    Tree tree;

    Clock::time_point t0 = Clock::now();

    std::vector<int> elements;
    for(unsigned int i = 0; i < size; ++i){
        elements.push_back(i);
    }

    random_shuffle(elements.begin(), elements.end());
    
    unsigned int part = size / Threads;

    std::vector<std::thread> pool;
    for(unsigned int i = 0; i < Threads; ++i){
        pool.push_back(std::thread([&tree, &elements, part, size, i](){
            thread_num = i;

            unsigned int tid = thread_num;

            for(unsigned int i = tid * part; i < (tid + 1) * part; ++i){
                tree.add(elements[i]);
            }
        }));
    }

    for_each(pool.begin(), pool.end(), [](std::thread& t){t.join();});

    Clock::time_point t1 = Clock::now();

    std::cout << "Construction of " << name << " with " << size << " elements took ";
    duration(t0, t1);
    std::cout << " with " << Threads << " threads" << std::endl;

    //Empty the tree
    for(unsigned int i = 0; i < size; ++i){
        tree.remove(i);
    }
}

#define RANDOM_CONSTRUCTION(type, name, size)\
    random_construction_bench<type<int, 1>, 1>(name, size);\
    random_construction_bench<type<int, 2>, 2>(name, size);\
    random_construction_bench<type<int, 3>, 3>(name, size);\
    random_construction_bench<type<int, 4>, 4>(name, size);\
    random_construction_bench<type<int, 8>, 8>(name, size);

void random_construction_bench(){
    std::cout << "Bench the random construction time of each data structure" << std::endl;

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000, 20000000};

    for(auto size : sizes){
        RANDOM_CONSTRUCTION(skiplist::SkipList, "SkipList", size);
        RANDOM_CONSTRUCTION(skiplist::SkipList, "NBBST", size);
    }

    //TODO Continue that
}

void perfTest(){
    std::cout << "Tests the performance of the different versions" << std::endl;

    //Launch the random benchmark
    random_bench();

    //Launch the construction benchmark
    seq_construction_bench();
    random_construction_bench();
}

std::string memory(double size){
    std::stringstream stream;
    
    if(size > (1024.0 * 1024.0)){
        stream << (size / (1024.0 * 1024.0)) << "GB";
    } else if(size > 1024){
        stream << (size / 1024.0) << "MB";
    } else {
        stream << size << "KB";
    }

    return stream.str();
}

template<typename Tree>
void memory(const std::string& name, int size){
    double vm1, rss1;
    double vm2, rss2;
    process_mem_usage(vm1, rss1);

    Tree tree;

    for(int i = 0; i < size; ++i){
        tree.add(i);
    }
    
    process_mem_usage(vm2, rss2);
    std::cout << name << "-" << size << " is using " << memory((rss2 - rss1) * 2) << std::endl;
    
    for(int i = 0; i < size; ++i){
        tree.remove(i);
    }
}

void memoryTest(){
    std::cout << "Test the memory consumption of each version" << std::endl;

    std::vector<int> sizes = {50000, 100000, 500000, 1000000, 5000000, 10000000, 20000000};

    for(auto size : sizes){
        memory<skiplist::SkipList<int, 32>>("SkipList", size);
        memory<nbbst::NBBST<int, 32>>("NBBST", size);
    }
}
