#ifndef HASH
#define HASH

template<typename T>
int hash(T value);

template<>
int hash<int>(int value){
    return value;
}

#endif
