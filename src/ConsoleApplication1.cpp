// ConsoleApplication1.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <functional>
#include "Urgc.h"


//template<typename F, typename Ret, typename ...Args>
//std::function<Ret(Args ...)> helper(Ret(F::*)(Args...));

//template<typename F, typename Ret, typename ...Args, typename T = Ret(Args...)>
//std::function<Ret(Args ...)> helper(Ret(F::*)(Args...) const);

//template<typename F, typename Ret, typename ...Args, typename T = Ret(Args...)>
//T helper(Ret(F::*)(Args...) );
//
//template<typename F, typename Ret, typename ...Args, typename T = Ret(Args...)>
//T helper(Ret(F::*)(Args...) const);
//
//
//template<typename F>
//struct GetLambda {
//    typedef decltype( helper(&F::operator()) ) type;
//};

class A {
public :
    int age;
    ~A() {
        printf("a free\n");
    }
};
void xx() {
    
}


class User {
public:
    int age = 0;
    ~User() {
        printf("release User. age:%d\n", age);
    }
};
class Node {
public:
    //此处的this，是为了让Local知道谁是引用的主体
    Local<Node> other{ nullptr, this };
    Local<Node> other2{ nullptr, this };
    std::string name = "";
    ~Node() {
        printf("release Node. name=:%s\n", name.c_str());
    }
};
void demo0() {
    Local<User> user = new User();
    ///出了作用域后，user就会自动释放掉
}

auto get_cb() {
    Local<User> user = new User();
    user->age = 923;
    //
    auto fn = CLOSURE([=]() {
        printf("hi urgc: user age:%d\n", user->age);
        });
    return fn;
}
void demo1() {
    auto fn = get_cb();
    fn->call();
}
auto create_a() {
    Local<Node> a = new Node();
    return a;
}
void demo2() {
    Local<Node> a = create_a();
    Local<Node> b = new Node();
    Local<Node> c = new Node();
    Local<Node> d = new Node();
    a->name = "a";
    b->name = "b";
    c->name = "c";
    d->name = "d";
    //循环引用
    a->other = b;
    b->other = a;

    a->other2 = c;
    c->other = d;
    d->other = a;
    d->other2 = b;
    //出了作用域后,a和b都会释放掉
}
int main()
{

    std::cout << "Hello World!\n";
    Local<A> a = new A();
    auto b = (xx(), []() {});
    printf("age :%d %d\n", a->age, b);
    int age = a->age;
    age = 12309;
    auto cb = closure_of([&]() {
        printf("var age:%d\n", age);
    });
    cb->call();


    std::function<void()> fn0 = []() {printf("fn0\n"); };
    std::function<void()> fn1 = [=]() {printf("fn1 age :%d\n", age); };
    std::function<void(int, int, int)> fn3 = [=](int b, int c, int g) {printf("catch:%d %d %d %d\n", age, b, c, g); };
    std::function<void(int, int, int)> fn4 = [&](int b, int c, int g) {printf("catch:%d\n", age); };
    printf("fn size:%d %d %d %d\n", sizeof(fn0), sizeof(fn1), sizeof(fn3), sizeof(fn4));

    auto c = create_closure<void()>();
    auto c2 = (Closure<void(int)>*)c;
    c2->call = [=](int age) {printf("hi:%d\n", age); };
    //memcpy(&c->call, &fn3, sizeof(fn0));
    /*auto c2 = (Closure<void(int, int, int)>*) c;
    c2->call(1, 2, 3);*/
    c2->call(10);
    delete  c;
    //auto c3 = create_closure<void()>();
    //std::function<void( int)> fn5 = [](int name) {printf("c4 name:%d\n", name); };

    ////memcpy(&c3->call, &fn5, sizeof(fn5));
    //////auto c4 = (Closure<void(int)>*)closure_of2(c3, fn5);
    ////auto c5 = (Closure<void(int)>*) c3;
    ////c5->call(2);
    ////c4->call(1);

    //auto sfn = closure_of2(c3, [](int name) {printf("lala name:%d\n", name); });
    //sfn->call(1233333);
    auto c5 = CLOSURE([=](int age, int name) {printf("hi %d, %d\n", age, name); });
    c5->call(994389, 3);
    printf("tmp\n");
    //delete c5;
    demo0();
    demo1();
    demo2();
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
