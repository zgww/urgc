// urgc.cpp
//

#include <iostream>
#include <algorithm>
#include <vector>
#include <functional>
#include <unordered_map>

#include <cassert>

#include "Urgc.h"
using namespace std;

//static unsigned long ROOT_REF = -1l;

/*
 入引用管理
 */

RefMgr::RefMgr()
{
	target = nullptr;
}
RefMgr::RefMgr(void *_target) : target(_target)
{
}
//设置强引用
void RefMgr::set_unit_source(void *obj)
{
	unit_source = obj;
	ghost_source = nullptr;
	// printf("\t设置强引用:%p=>%p\n", obj, target);
}

void RefMgr::ref(void *source)
{
	//第一次引用，直接
	if (all_sources.size() == 0)
	{
		unit_source = source;
		// printf("\t第一次引用。记录唯一引用:%p=>%p\n", source, target);
	}
	all_sources.push_back(source);
}
//从all_sources删除第一个source
void RefMgr::remove_first_matched_source_from_all_sources(void *source)
{
	// printf("删除出引用 s=%p\n", source);
	// printf("删除出引用 s=%p size=%d\n", source, all_sources.size());
	for (auto it = begin(all_sources); it != end(all_sources); it++)
	{
		if (*it == source)
		{
			all_sources.erase(it);
			break;
		}
	}
	// printf("\t删除出引用 s=%p\n", source);
}

void RefMgr::delete_target()
{
	if (deleter == nullptr)
	{
		delete target;
	}
	else
	{
		deleter(target);
	}
	//后面还要用target来查找
	//target = nullptr;
}

/*唯一强引用gc*/
//通过被引用者查找入引用管理
RefMgr *Urgc::get_ref_mgr_by_target(void *target)
{
	if (target_in_ref_mgr.find(target) != target_in_ref_mgr.end())
	{
		return &target_in_ref_mgr[target];
	}
	return nullptr;
}
//不存在就创建 入引用
RefMgr *Urgc::goc_ref_mgr_by_target(void *target, std::function<void(void *)> deleter)
{
	RefMgr *mgr = get_ref_mgr_by_target(target);
	if (mgr == nullptr)
	{
		RefMgr tmp(target);
		tmp.deleter = deleter;
		target_in_ref_mgr[target] = tmp;
	}
	return get_ref_mgr_by_target(target);
}
//source-target构成Key. 用来唯一引用一个RefMgr
string Urgc::fmt_key(void *source, void *target)
{
	char ckey[64];
	sprintf(ckey, "%p-%p", source, target);
	return string(ckey);
}

//记录引用
void Urgc::ref(void *source, void *target, std::function<void(void *)> deleter)
{
	if (target == nullptr)
	{
		return;
	}
	assert(source != nullptr && "source不能为nullptr");
	assert(target != nullptr && "target不能为nullptr");

	RefMgr *mgr = goc_ref_mgr_by_target(target, deleter);

	//记录出引用
	add_out_ref_mgr(source, mgr);

	// printf("\turgc:引用指针:source:%p => target:%p\n", source, target);
	//记录引用
	mgr->ref(source);
}

//记录出 引用管理器
void Urgc::add_out_ref_mgr(void *p, RefMgr *mgr)
{
	//不存在就创建关联
	auto it = out_ref_mgrs_map.find(p);
	if (it == out_ref_mgrs_map.end())
	{
		out_ref_mgrs_map[p] = {mgr};
	}
	else
	{
		(it->second).push_back(mgr);
	}
}
//移除出 引用管理器
void Urgc::remove_out_ref_mgr(void *p, RefMgr *mgr)
{
	auto it = out_ref_mgrs_map.find(p);
	if (it != out_ref_mgrs_map.end())
	{
		auto found = find(it->second.begin(), it->second.end(), mgr);
		//映射数组项
		if (found != it->second.end())
		{
			it->second.erase(found);
		}
		//数组空了，删除映射
		if (it->second.size() == 0)
		{
			out_ref_mgrs_map.erase(it);
		}
	}
}
/*
	 解引用:从target的被引用列表(all_sources)中删除，删除source->target的出引用记录
	 */
void Urgc::deref(void *source, void *target)
{
	assert(source != nullptr && "source不能为nullptr");
	assert(target != nullptr && "target不能为nullptr");
	// printf("deref :%p=>%p\n", source, target);

	RefMgr *mgr = get_ref_mgr_by_target(target);
	if (mgr == nullptr)
	{
		//target未受urgc管理
		return;
	}
	//移除出引用
	remove_out_ref_mgr(source, mgr);
	// printf("\tderef:remove_out_ref_mgr :%p=>%p\n", source, target);

	//判断:TODO 实际上，有可能该强引用失效，但是子树有其他可能的强引用转正，进而导致当前项无须删除。这需要进一步处理，留待之后.

	if (_deref_then_check_need_delete(mgr, source))
	{
		// printf("\tderef:deref_then_check :%p=>%p\n", source, target);

		// printf("\t\t解引用导致需要删除:%p=>%p %s\n", source, mgr->target, typeid(mgr->target).name());
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
			 最后再次遍历”待删除节点列表“，删除节点
			 */
		vector<RefMgr *> will_deletes{mgr};
		children_find_new_chance(mgr->target, will_deletes);
		// printf("\tchildren_find_new_chance :deref_then_check :%p=>%p\n", source, target);
		recover_will_deletes(will_deletes);
		// printf("\trecover will deletes :deref_then_check :%p=>%p\n", source, target);
		delete_will_deletes(will_deletes);
		// printf("\tdelete  will deletes :deref_then_check :%p=>%p\n", source, target);
	}
	// printf("\tderef:return :%p=>%p\n", source, target);
}

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
void Urgc::delete_will_deletes(vector<RefMgr *> &will_deletes)
{
	//提前删除所有出引用和出引用列表记录. 
	// 与下文的erase mgr分开处理。 不然可能导致，mgr已释放，但是出引用还依赖着，导致崩溃
	// printf("删除 出引用 \n");
	for (auto mgr : will_deletes)
	{
		if (mgr->unit_source == nullptr){ //未能恢复成功
			//删除所有出引用和出引用列表记录,
			auto out_it = out_ref_mgrs_map.find(mgr->target);
			// printf("\t\t结点最终删除2:%p %s\n", mgr->target, typeid(mgr->target).name());
			if (out_it != out_ref_mgrs_map.end())
			{
				auto kid_mgrs = out_it->second;
				//删除出引用
				for (auto kid_mgr : kid_mgrs)
				{
					kid_mgr->remove_first_matched_source_from_all_sources(
						mgr->target);
				}
				//删除出引用列表记录
				out_ref_mgrs_map.erase(out_it);
			}
		}
	}
	// printf("delete_will_deletes \n");
	for (auto mgr : will_deletes)
	{
		if (mgr->unit_source == nullptr)
		{ //未能恢复成功
			// printf("\t\t结点最终删除:%p %s\n", mgr->target, typeid(mgr->target).name());

			// printf("\t\turgc:释放指针:%p\n", mgr->target);
			//需要释放
			//delete mgr->target;
			mgr->delete_target();
			// printf("\t\t\turgc:释放指针完成:%p\n", mgr->target);

			//删除mgr. 此处删除了，有可能后续delete依赖了此mgr,导致访问被释放的内存
			auto it = target_in_ref_mgr.find(mgr->target);
			if (it != target_in_ref_mgr.end())
			{
				target_in_ref_mgr.erase(it);
			}
			// printf("\t\t\turgc:释放指针完成2:%p\n", mgr->target);
		}
	}
	// printf("\t\t\turgc:完释放:\n");
}

//子树出路寻找完毕后，有些待删除节点，可以回归. 此时的节点，unit_source=null, ghost_source=原强引用。
void Urgc::recover_will_deletes(vector<RefMgr *> &will_deletes)
{
	// printf("recorver_will_delets\n");
	for (auto mgr : will_deletes)
	{
		for (auto source : mgr->all_sources)
		{
			//重新查找强引用。 如果有找到，就归位
			// printf("recover_will_deletes:%p=>%p\n", source, mgr->target);
			if (check_ref(source, mgr->target))
			{
				//恢复子树
				recover_node(mgr, source);
				break; //处理下一个will_delete
			}
		}
	}
}
//子树恢复
void Urgc::recover_node(RefMgr *mgr, void *source)
{
	// printf("\t\t结点恢复:%p=>%p\n", source, mgr->target);
	mgr->set_unit_source(source);

	//子树处理
	auto it = out_ref_mgrs_map.find(mgr->target);
	if (it == out_ref_mgrs_map.end())
	{ //未找到出引用
		return;
	}
	auto &out_ref_mgrs = it->second;
	for (auto kid_mgr : out_ref_mgrs)
	{
		if (kid_mgr->unit_source == nullptr)
		{ //该结点，没有强引用
			//递归处理子树
			recover_node(kid_mgr, mgr->target);
		}
	}
}
//子树寻找新的出路
void Urgc::children_find_new_chance(void *parent,
									vector<RefMgr *> &will_deletes)
{
	// printf("children find new chance\n");
	auto it = out_ref_mgrs_map.find(parent);
	if (it == out_ref_mgrs_map.end())
	{ //未找到出引用
		return;
	}
	auto &out_ref_mgrs = it->second;
	for (auto mgr : out_ref_mgrs)
	{
		if (mgr->unit_source == parent)
		{ //只找子树(唯一强引用关联的）.
			child_find_new_chance(parent, mgr, will_deletes);
		}
	}
}
void Urgc::child_find_new_chance(void *parent, RefMgr *mgr,
								 vector<RefMgr *> &will_deletes)
{
	// printf("child find new chance\n");
	//遍历子节点，置空unit_source, 转入ghost_source, （备用，有可能回复）
	mgr->ghost_source = mgr->unit_source;
	mgr->unit_source = nullptr;
	//查找新的强引用
	for (auto source : mgr->all_sources)
	{
		if (source != parent)
		{
			//新的强引用
			// printf("child_find_new_chance:%p=>%p\n", source, mgr->target);
			if (check_ref(source, mgr->target))
			{
				// printf("\t\t找到新生机:%p=>%p\n", source, mgr->target);
				mgr->set_unit_source(source); //使用新的强引用
				return;						  //该子结点处理完成
			}
		}
	}
	//未找到新的强引用：如果没有找到，记录该子节点到“待删除节点列表”中，然后递归处理该子节点的子节点。
	will_deletes.push_back(mgr);
	children_find_new_chance(mgr->target, will_deletes);
}

//设置指针。
void Urgc::set_pointer(void *source, void **ptr, void *target)
{
	*ptr = target;
	ref(source, target);
}
//判断是不是循环引用
//进行环识别：唯一强引用构成树，所以，构成唯一强引用环的唯一可能，是新的强引用来自子树
//所以，通过上溯父，到根，如果不会找到自己，就说明该引用来自子树外。所以可用。
bool Urgc::is_cycle_ref(void *source, void *target)
{
	// printf("is_cycle_ref\n");
	while (source && (unsigned long)source != ROOT_REF)
	{ //有源，且源非根引用
		// printf("\t check is_cycle_ref : %p=>%p\n", source, target);
		if (source == target)
		{
			return true;
		}
		auto mgr = get_ref_mgr_by_target(source);
		if (mgr == nullptr)
		{
			break;
		}
		source = mgr->unit_source;
	}
	return false;
}
//向上判断引用是否可用：非环，也不会到unit_source==nullptr(无效的引用)的情况
bool Urgc::check_ref(void *source, void *target)
{
	// printf("check_ref\n");
	while (true)
	{ //有源，且源非根引用
		// printf("\t检查引用可用:%p=>%p\n", source, target);
		if (source == nullptr)
		{ //无效的引用
			return false;
		}
		if ((unsigned long)source == ROOT_REF)
		{ //根引用
			return true;
		}
		if (source == target)
		{ //ring 环. 该引用会产生环，不能用
			// printf("\tcheck_ref遇到环 source == target:%p=>%p\n", source, target);
			return false;
		}
		auto mgr = get_ref_mgr_by_target(source);
		if (mgr == nullptr)
		{ //未记录的source
			return true;
		}
		source = mgr->unit_source;
	}
}
//解引用。返回是否需要释放
bool Urgc::_deref_then_check_need_delete(RefMgr *mgr, void *source)
{
	// printf("_deref_then_check_need_delete");
	//当unit_source为空时
	//当要解引用的，是唯一强引用，需要进行环识别：唯一强引用构成树，所以，构成唯一强引用环的唯一可能，是新的强引用来自子树
	//所以，通过上溯父，到根，如果不会找到自己，就说明该引用来自子树外。所以可用。
	if (mgr->unit_source && mgr->unit_source == source)
	{
		//先从all_sources中删除该强引用
		mgr->remove_first_matched_source_from_all_sources(mgr->unit_source);
		mgr->unit_source = nullptr;

		//寻找新的唯一强引用
		for (auto cur_source : mgr->all_sources)
		{
			if (!is_cycle_ref(cur_source, mgr->target))
			{
				// printf("\t找到新的唯一强引用\n");
				mgr->set_unit_source(cur_source); //更新唯一引用
				return false;
			}
		}
		//未找到唯一强引用,需要释放
		return true;
	}
	else
	{ //解非强引用,直接移除即可
		mgr->remove_first_matched_source_from_all_sources(source);
		return false;
	}
}
//报告
void Urgc::report(string title)
{
	if (1) return;
	printf("=========%s=================\n", title.c_str());
	int ref_cnt = 0;
	for (auto &it : target_in_ref_mgr)
	{
		auto target = it.first;
		printf("\t唯一强引用关系:%p => %p. ghost:%p\n", it.second.unit_source,
			   target, it.second.ghost_source);
		ref_cnt += it.second.all_sources.size();
		for (auto source : it.second.all_sources)
		{
			printf("\t\t所有引用:%p(%s)=>%p(%s)\n", source,
				   typeid(source).name(), target, typeid(target).name());
		}
	}
	int ungc_size = target_in_ref_mgr.size();
	int out_size = out_ref_mgrs_map.size();
	printf("$$$$$urgc:未回收指针数量: %d. 出引用数量: %d. 引用数:%d\n", ungc_size, out_size, ref_cnt);
}

Urgc urgc;

//局部变量引用指针

static GcObj *_global_source = nullptr;

void Local_set_global_source(GcObj *source)
{
	_global_source = source;
}
GcObj *Local_get_global_source()
{
	return _global_source;
}

//#define set(source, expr, target) \
//    expr = target;\
//    urgc.ref(source, target)
