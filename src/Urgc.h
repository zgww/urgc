#pragma once

#include <iostream>
#include <algorithm>
#include <vector>
#include <functional>
#include <unordered_map>

#include <cassert>
#include <type_traits>
#include <string.h>

using namespace std;

//根引用。 
#define ROOT_REF (unsigned long)-1l

class GcObj {
public:
	virtual ~GcObj() {};

};

/*
 入引用管理
 */
class RefMgr {
public:
	//null:表示未确定唯一引用
	//ROOT_REF:来自根的引用
	//other: 其他
	void* unit_source = nullptr;    //唯一引用源
	void* ghost_source = nullptr;    //幽灵引用（备忘unit_source)
	vector<void*> all_sources; //所有引用源
	void* target;
	std::function<void(void*)> deleter = nullptr;//自定义删除函数

	RefMgr();
	RefMgr(void* _target);
	//设置强引用
	void set_unit_source(void* obj);

	void ref(void* source);
	//从all_sources删除第一个source
	void remove_first_matched_source_from_all_sources(void* source);

	//释放指针
	void delete_target();
};


//设置全局的source.
//为了支持lambda.lambda引用 local时，会丢失source. 所以设置好global_source,有global_source时，Local默认使用global_source
//设置完Lambda后，就置空global_source
void Local_set_global_source(GcObj* source);

GcObj* Local_get_global_source();

template <typename T>
class Local;

/*唯一强引用gc*/
class Urgc {
public:
	//入引用管理:记录被引用(target)->所有入引用管理器
	unordered_map<void*, RefMgr> target_in_ref_mgr;
	//出引用管理列表:source=> RefMgr list of targets
	unordered_map<void*, vector<RefMgr*>> out_ref_mgrs_map;

	//通过被引用者查找入引用管理
	RefMgr* get_ref_mgr_by_target(void* target);
	//不存在就创建 入引用
	RefMgr* goc_ref_mgr_by_target(void* target, std::function<void(void*)> deleter=nullptr);
	//source-target构成Key. 用来唯一引用一个RefMgr
	string fmt_key(void* source, void* target);

	template<typename T>
	void ref(Local<T> source, void* target, std::function<void(void*)> deleter=nullptr) {
		ref(source.get(), target, deleter);
	}
	//记录引用
	void ref(void* source, void* target, std::function<void(void*)> deleter=nullptr);

	//记录出 引用管理器
	void add_out_ref_mgr(void* p, RefMgr* mgr);
	//移除出 引用管理器
	void remove_out_ref_mgr(void* p, RefMgr* mgr);
	/*
	 解引用:从target的被引用列表(all_sources)中删除，删除source->target的出引用记录
	 */
	void deref(void* source, void* target);

	/*
	 子树处理:
	 记录当前节点到“待删除节点列表“中，
	 子树处理:
	 强引用失效后
	 遍历子节点，置空unit_source,转入ghost_source, （备用，有可能回复）
	 查找新的强引用。要求新的强引用，不是循环引用，也不会引用到当前处理中的节点。
	 如果有找到，就使用新的强引用，置空ghost_source. 该子节点处理完成。
	 如果没有找到，记录该子节点到“待删除节点列表”中，然后递归处理该子节点的子节点。
	 所以节点处理完成后，遍历”待删除节点列表“， 重新查找强引用。 如果有找到，就归位，同时遍历子树中没有强引用的节点，更新强引用。
	 最后再次遍历”待删除节点列表“，删除指针,删除节点的mgr,删除节点的出引用
	 */
	 //执行最终的删除
	void delete_will_deletes(vector<RefMgr*>& will_deletes);

	//子树出路寻找完毕后，有些待删除节点，可以回归. 此时的节点，unit_source=null, ghost_source=原强引用。
	void recover_will_deletes(vector<RefMgr*>& will_deletes);
	//子树恢复
	void recover_node(RefMgr* mgr, void* source);
	//子树寻找新的出路
	void children_find_new_chance(void* parent,
		vector<RefMgr*>& will_deletes);
	void child_find_new_chance(void* parent, RefMgr* mgr,
		vector<RefMgr*>& will_deletes);

	template<typename T>
	void set_pointer(Local<T> source, void** ptr, void* target) {
		*ptr = target;
		ref(source.target, target);
	}

	//设置指针。
	void set_pointer(void* source, void** ptr, void* target);
	//判断是不是循环引用
	//进行环识别：唯一强引用构成树，所以，构成唯一强引用环的唯一可能，是新的强引用来自子树
	//所以，通过上溯父，到根，如果不会找到自己，就说明该引用来自子树外。所以可用。
	bool is_cycle_ref(void* source, void* target);
	//向上判断引用是否可用：非环，也不会到unit_source==nullptr(无效的引用)的情况
	bool check_ref(void* source, void* target);
	//解引用。返回是否需要释放
	bool _deref_then_check_need_delete(RefMgr* mgr, void* source);
	//报告
	void report(std::string title = "");
};


extern Urgc urgc;


//局部变量引用指针


// template<class T>
// class Local ;
// class LocalCopy {
// public:
// 	Local *from;
// 	Local *to;
// }
template<class T>
class Local 
{
	//static_assert(!std::is_same<T, GcObj>::value, "泛型必须 继承 GcObj\n");
public:
	// static bool in_global_source_transaction = false;
	// std::vector<LocalCopy> delay_locals;

	///为了构建 closure,不需要指明类型
	// static start_global_source(){
	// 	in_global_source_transaction = true;
	// }
	// static commit_global_source(){
	// 	in_global_source_transaction = false;
	// 	//应用ref/deref

	// 	for (auto &copy: delay_locals){
	// 		copy.to->deleter = copy.from->deleter;
	// 		copy.to->set_target_and_source(copy.from->target, copy.from->source);
	// 	}
	// 	delay_locals.clear();
	// }

	Local(T* p, std::function<void(void*)> deleter = nullptr) {
		//static_assert(!std::is_same<T, GcObj>::value, "泛型必须 继承 GcObj\n");
		this->deleter = deleter;

		// printf("初值构造:%p %s\n", p, typeid(p).name());
		set_target(p);
	}
	Local(const Local& from) {
		void *ori_source = source;

		// if (in_global_source_transaction){ //收集复制构造的信息
		// 	LocalCopy copy;
		// 	copy.from = from;
		// 	copy.to = this;
		// 	delay_locals.push_back(copy);
		// 	return;
		// }

		//复制构造。当lambda捕获时
		if (source == (GcObj*)ROOT_REF && Local_get_global_source() != nullptr)
			source = Local_get_global_source();

		this->deleter = from.deleter;
		// printf("\tLocal复制构造:self:%p(g:%p)=>%p from: %p=>%p\n", ori_source, source, target, from.source, from.target);
		set_target(from.target);
	}
	Local(T* p, void* source, std::function<void(void*)> deleter = nullptr) {
		//        ASSERT(source, "source不能设为null");
		//        ASSERT(this->source == (GcObj*)ROOT_REF, "Local.source不能多次设置");

		// printf("初值构造(指定source):%p=>%p %s\n", source, p, typeid(p).name());
		this->deleter = deleter;
		this->source = source;
		set_target(p);
	}

	~Local() {
		// printf("Local释放:%p=>%p\n", source, target);
		deref();
		// printf("\tLocal释放完成:\n");
	}
	void set_target(T* target) {
		deref();
		this->target = target;
		ref();
	}
	void set_target_and_source(T* target, void *source) {
		// printf("设置target和source:旧:%p=>%p 新:%p=>%p\n", this->source, this->target, source, target);
		deref();
		this->source = source;
		this->target = target;
		ref();
	}

	void deref() {
		if (target) {
			urgc.deref(source, (GcObj*)target);
			// printf("Local.deref:%p=>%p\n", source, target);
			target = nullptr;
		}
	}
	void ref() {
		if (target) {
			// printf("ref %p %s\n", target, typeid((T*)target).name());
			if (deleter == nullptr) {
				deleter = [=](void* target) {
					// printf("Local.deleter默认删除器,执行删除:%p %s\n", target, typeid((T*)target).name());
					delete (T*)target;
				};
			}
			urgc.ref(source, (GcObj*)target, deleter);
		}
	}
	T* get() {
		return target;
	}
	T* operator->() const {
		return target;
	}
	T operator *() const {
		return *target;
	}
	T **operator&()
	{
		return &target;
	}
	operator T*(){
		return target;
	}
	bool operator==(const std::nullptr_t &v)
	{
		return target == v;
	}
	Local<T> &operator=(const Local<T> &from)
	{
		// printf("\tLocal 赋值:%p=>%p\n", source, from.target);
		deref();
		void *p = nullptr;

		// printf("\tLocal 赋值先解引用:%p=>%p\n", source, from.target);
		target = from.target;
		ref();
		// printf("\tLocal 赋值引用:%p=>%p\n", source, from.target);
		return *this;
	}

public:
	T *target = nullptr; // 被引用的内存
	void *source = (void *)ROOT_REF; // 上级
	std::function<void(void *)> deleter = nullptr;
};

template <typename _Res, typename... _ArgTypes>
class Closure;

template<typename F, typename Ret, typename ...Args>
Closure<Ret(Args ...)> helper(Ret(F::*)(Args...));

template<typename F, typename Ret, typename ...Args>
Closure<Ret(Args ...)> helper(Ret(F::*)(Args...) const);

template<typename F, typename Ret, typename ...Args>
std::function<Ret(Args ...)> helper2(Ret(F::*)(Args...));

template<typename F, typename Ret, typename ...Args>
std::function<Ret(Args ...)> helper2(Ret(F::*)(Args...) const);

/*
封装lambda.
lambda捕获Local型局部变量时，没法指定local.source. 会导致引用关系丢失。
所以先new Closure, 提前设置好global_source, 然后
*/
template <typename _Res, typename... _ArgTypes>
class Closure;

template <typename _Res, typename... _ArgTypes>
class Closure<_Res(_ArgTypes...)> : public GcObj
{
public:
	Closure()
	{
		Local_set_global_source(this);
	}
	virtual ~Closure()
	{
		printf("free closure\n");
	}
	Local<Closure<_Res(_ArgTypes...)>> wrap(std::function<_Res(_ArgTypes...)> fn)
	{
		assert(this->call == nullptr && "closure不能wrap多次\n");
		assert(Local_get_global_source() != nullptr && "Closure.wrap要与 create_closure配对使用\n");

		this->call = fn;
		Local_set_global_source(nullptr);
		//		return this;
		return Local<Closure<_Res(_ArgTypes...)>>(this);
	}
	template<typename F>
	auto wrap2(F f)
	{
		assert(this->call == nullptr && "closure不能wrap多次\n");
		assert(Local_get_global_source() != nullptr && "Closure.wrap要与 create_closure配对使用\n");


		typedef decltype(helper(&F::operator())) Type;
		typedef decltype(helper2(&F::operator())) Type2;
		//Type* c = new Type();
		//Type2 fn = f;

		auto c = (Type *)this;
		//Local<Type> local_c = c;
		//this->call = fn;
		c->call = f;
		//memcpy(&c->call, &fn, sizeof(fn));
		Local_set_global_source(nullptr);
		//		return this;
		return Local<Type>(c);
	}

	/*const std::function<_Res(_ArgTypes...)>& get_fn() {
		return call;
	}*/
	//不要设置call函数，容易丢失引用
	std::function<_Res(_ArgTypes...)> call = nullptr;
};

/*创建closure. 你也可以使用: (new Closure<void()>)->wrap([](){});
但是使用此函数，可以少一对括号:
create_closure<void()>()->wrap([](){
});
为什么不能直接在new Closure将lambda传入呢。 因为这样就变成lambda先捕获局部变量，再set_global_source,
导致lambda对局部变量的引用关系失调，可能会导致循环引用，导致内存无法及时释放.
例:

Local<User> id = new User();
auto closure0 = create_closure<void()>()->wrap([=]() {
		printf("closure hi :%p\n", id.source);
	});
auto closure = create_closure<void()>()->wrap([=]() {
	printf("closure hi :%p\n", id.source);
});
*/
template <typename T>
Closure<T> *create_closure()
{
	Closure<T> *closure = new Closure<T>;
	return closure;
}

template<typename F>
auto closure_of(F f){
    typedef decltype( helper(&F::operator()) ) Type;
	Type *c = new Type();
	c->call = f;
	return c;
}

template<typename F>
auto closure_of2(Closure<void()> *c, F f) {
	typedef decltype(helper(&F::operator())) Type;
	typedef decltype(helper2(&F::operator())) Type2;
	//Type* c = new Type();
	Type2 fn = f;
	//return fn;
	//c->call = f;
	auto c2 = (Type*)c;
	size_t size = sizeof(fn);
	memcpy(&c2->call, &fn, sizeof(fn));
	return c2;
}

#define CLOSURE(fn) create_closure<void()>()->wrap2(fn)