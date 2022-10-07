

#pragma once

#include <vector>
#include <functional>
#include "Urgc.h"

/*
用来存放可gc的指针
元素只能存放new出来的指针 
*/
template <typename T>
class GcList
{
public:
    ~GcList(){
        // printf("~GcList\n");
        for (auto p: vector){
            urgc.deref(this, p);
        }
        vector.clear();
    }
    void push_all(GcList<T>* src){
        // printf("GcList.push_all:%p\n", src);
        // printf("GcList.push_all:%p:%d\n", src, src->size());
        for (int i = 0, l = src->size(); i < l; i++){
            auto e = src->get(i);
            // printf("GcList.get:%p\n", e);
            if (e != nullptr){
                push(e);
            }
        }
    }
    void push(T *ele)
    {
        set_at(size(), ele);
    }
    int size()
    {
        return vector.size();
    }
    void insert_at(int idx, T*ele){
        if (idx >= vector.size()){
            vector.push_back(ele);
        } else {
            vector.insert(vector.begin() + idx, ele);
        }
        urgc.ref(this, ele, deleter);
    }
    std::function<void(void*)> deleter = [](void *p){
        delete (T*)p;
    };
    void set_at(int idx, T *ele)
    {
        if (idx >= vector.size())
        {
            vector.push_back(ele);
            urgc.ref(this, ele, deleter);
        }
        else
        {
            if (vector[idx] != nullptr){
                urgc.deref(this, vector[idx]);
            }
            vector[idx] = ele;
            urgc.ref(this, ele, deleter);
        }
    }

    void pop()
    {
        int size = vector.size();
        remove_at(size - 1);
    }
    T* get(int index){
        if (index >= size()){
            return nullptr;
        }
        return vector[index];
    }

    void clear(){
        while (size() > 0){
            pop();
        }
    }
    int index_of(T*ele){
        for (int i = 0, l = size(); i < l; i++){
            if (vector[i] == ele){
                return i;
            }
        }
        return -1;
    }
    bool remove(T* ele){
        int idx = index_of(ele);
        if (idx != -1){
            return remove_at(idx);
        }
        return false;
    }
    bool remove_at(int index)
    {
        int size = vector.size();
        if (size > 0 && index < size)
        {
            if (vector[index] != nullptr){
                urgc.deref(this, vector[index]);
            }
            vector.erase(vector.begin() + index);
            return true;
        }
        return false;
    }

protected:
    std::vector<T*> vector;
};