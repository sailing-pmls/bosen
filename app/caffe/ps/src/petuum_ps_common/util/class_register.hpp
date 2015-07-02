#pragma once

/*
 *
 * author: jinliang
 */


#include <map>
#include <stdint.h>

namespace petuum {
template<typename BaseClass>
class ClassRegistry {
public:
  typedef BaseClass* (*CreateFunc)();
  ClassRegistry():
    default_creator_(0) { };
  ~ClassRegistry() { };

  void SetDefaultCreator(CreateFunc creator) {
    default_creator_ = creator;
  }

  void AddCreator(int32_t key, CreateFunc creator) {
    creator_map_[key] = creator;
  }

  BaseClass *CreateObject(int32_t key){
    typename std::map<int32_t, CreateFunc>::const_iterator it =
        creator_map_.find(key);
    if (it == creator_map_.end()) {
      if(default_creator_ != 0) {
	return (*default_creator_)();
      }
      return 0;
    } else {
      CreateFunc creator = it->second;
      return (*creator)();
    }
  }

  static ClassRegistry<BaseClass> &GetRegistry() {
    static ClassRegistry<BaseClass> registry;
    return registry;
  }
private:

  CreateFunc default_creator_;
  std::map<int32_t, CreateFunc> creator_map_;
};

template<typename BaseClass, typename ImplClass>
BaseClass *CreateObj() {
  return dynamic_cast<BaseClass*>(new ImplClass);
}
}
