# urgc

unique reference garbage collect


## 简介

一个c++库。用法类似于智能指针，同时解决循环引用的问题,也不需要开发者特意标注强弱引用.

## 初心

c++性能极高，能充分利用机器的性能。随着c++的升级, 加入了很多优秀特性，如lambda, 协程， module,
极大地提高了开发效率。 但是内存管理还是很棘手。 
虽然有智能指针，但是在游戏开发，GUI领域，对象引用关系复杂，循环引用 不可避免,所有权也不清晰,使用智能指针还是得
非常小心,才能避免泄漏。

作者希望此库为开发者带来更好的开发体验，为用户带来又快又轻巧又炫的软件。

## 与可达性分析型gc的区别

可达性分析型gc，遍历活着的对象，然后把没有遍历到的对象释放掉。
urgc可以认为遍历可能死亡的对象,释放真的死亡的对象，重新整理未真实死亡的对象的引用

因为是遍历可能死亡的对象，所以urgc具有局部性，不像可达性分析gc需要遍历完整对象图


## 与引用计数的区别

引用计数为了避免循环引用，需要特意标注弱引用.
urgc可以处理循环引用，也有相应的环遍历的代价

## 环遍历

遍历循环引用，是不可避免的。
举个极端的例子，有1百万个对象引用成环，
除了遍历引用链，第0个对象，是没有办法知道， 离它50万个引用外的对象b跟它是不是构成了循环引用。
有可能对象b引用了对象c,就导致了一个有1百万个对象的环成立，
对象b解引用对象c,就导致了一个有1百万个对象的环解体。

## 用法

### 最简单的例子
```c++
#include "Urgc.h"

class User {
public:
    int age;
};

int main(){
    Local<User> user = new User();
    ///出了作用域后，user就会自动释放掉
    return 0;
}
```

### lambda

内存管理，管理的是堆上的内存，所以lambda也需要封装到堆上。
```c++
#include "Urgc.h"

class User {
public:
    int age;
};
auto get_cb(){
    Local<User> user = new User();
    user->age = 923;
    //
    auto fn = CLOSURE([=](){
        printf("hi urgc: user age:%d\n", user->age);
    });
    return fn;
}
int main(){
    auto fn = get_cb();
    fn->call(); //此处打印出: hi urgc: user age: 923
    ///出了作用域， fn和user都会被释放掉
    return 0;
}

```

### 循环引用
```c++
#include "Urgc.h"

class Node {
public:
    //此处的this，是为了让Local知道谁是引用的主体
    Local<Node> other{nullptr, this};
    Local<Node> other2{nullptr, this};
};
void say(){
    Local<Node> a = new Node();
    Local<Node> b = new Node();
    Local<Node> c = new Node();
    Local<Node> d = new Node();
    //循环引用
    a->other = b;
    b->other = a;
    
    a->other2 = c;
    c->other = d;
    d->other = a;
    d->other2 = b;
    //出了作用域后,a和b都会释放掉
}
int main(){
    say();
    return 0;
}

```


## 原理

引用计数，如果要解决循环引用，可以在`release`引用时，遍历引用，判断是否成环。
但是实践中，是做不到的，因为遍历引用的代价太大了。

urgc是在引用计数上加以改进。 

urgc引入一条规则: **一个对象可以被多个对象强引用，但是同一时间，只有一个强引用被标记为主引用**。
这条规则的作用是：在对象引用图上，构建了主引用树。树的定义，是可以保证不会有环出现。

主引用树的作用是：**当且仅当**主引用被释放时，遍历主引用树，为树下的对象寻找新的主引用，如果有找到，就说明对象
有别的主引用树接纳，可以存活，没有就说明是死亡对象，可以释放。

主引用树，就是**自动管理的唯一所有权**。


## 性能

在GUI环境下测试，几百个节点，性能消耗约为 `delete` 函数的4倍


## 独立的gc线程

目前实现，是借助`RAII`,在当前线程下加减引用。
优点是可以做到立即释放，缺点是有性能代价，而且没办法充分利用多线程。

希望能将加减引用转为事件，交给独立的gc线程处理。 这样对工作线程的压力就极小。

## 适用场景

想像一下，一个GUI应用，c++写的，本身性能就极高，而且也不会因为触发gc,导致掉帧，那体验该有多丝滑。

## 路线

[x] 省略lambda类型声明
[x] 线程安全
[] 独立的gc线程
[] 补充更多的测试





