

#pragma once

#include <unordered_map>
#include <string>
#include "Urgc.h"

/*
用来存放可gc的指针
元素只能存放new出来的指针 
*/
template <typename T>
class GcMap
{
public:
    ~GcMap(){
        printf("~GcMap\n");
        for (auto entry: map){
            urgc.deref(this, entry.second);
        }
        map.clear();
    }
    void set(std::string key, T *ele)
    {
        auto deleter = [](void *p){
            delete (T*)p;
        };
        auto it = map.find(key);
        auto end = map.end();
        auto exists = it != end;
        auto old = exists ? it->second : nullptr;


        if (old != nullptr){
            urgc.deref(this, map[key]);
        }
        if (ele != nullptr){
            map[key] = ele;
            urgc.ref(this, map[key], deleter);
        } else if (exists) {
            map.erase(it);
        }
    }

    void remove(std::string key){
        set(key, nullptr);
    }
    int size()
    {
        return map.size();
    }
    T* get(string key){
        auto it = map.find(key);
        auto end = map.end();
        auto exists = it != end;
        if (!exists){
            return nullptr;
        }
        return it->second;
    }

protected:
    std::unordered_map<string, T*> map;
};