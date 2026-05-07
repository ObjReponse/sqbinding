// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#ifndef SQB_TABLE_H
#define SQB_TABLE_H

#include <stdexcept>
#include <string>
#include "sqbobject.h"
#include "sqbclass.h"
#include "sqbfunction.h"
#include "sqbarray.h"

namespace sqb {


class SQBTable : public SQBObject {

public:
  ~SQBTable()
  {
    if (delegateObj._type != OT_NULL) {
      sq_release(vm, &delegateObj);
    }
  }

  // copy
  SQBTable(const SQBTable& other)
    : SQBObject(other.vm, other.hsqObject),
      delegateObj(other.delegateObj),
      _setterMap(other._setterMap),
      _getterMap(other._getterMap),
      isRoot(other.isRoot)
  {
    if (delegateObj._type != OT_NULL)
      sq_addref(vm, &delegateObj);
  }


  SQBTable(HSQUIRRELVM vm, bool root = false)
    : SQBObject(vm),
      isRoot(root)
  {
    if (root)
      sq_pushroottable(vm); // [root]
    else
      sq_newtable(vm);

    sq_getstackobj(vm, -1, &hsqObject);
    sq_addref(vm, &hsqObject);

    initDelegate();
    sq_pop(vm, 1);

    needRelease = !root;
  }


  SQBTable(HSQUIRRELVM vm, HSQOBJECT obj)
      : SQBObject(vm, obj)
  {
    if (hsqObject._type != OT_TABLE)
      throw std::runtime_error("HSQOBJECT not OT_TABLE");

    // push this
    int top = push();
    initDelegate();
    pop(top);
  }

  SQBTable(const SQBObject &parent, HSQOBJECT obj)
      : SQBObject(parent, obj)
  {
    if (hsqObject._type != OT_TABLE)
      throw std::runtime_error("HSQOBJECT not OT_TABLE");

    // push this
    int top = push();
    initDelegate();
    pop(top);
  }


  template<typename T>
  SQBTable& setValue(const std::string &name, const T& value) {
    int top = push();

    sq_pushstring(vm, name.c_str(), -1);
    types::push<T>(vm, value);

    //detail::debugStackShow(vm);

    sq_newslot(vm, -3, SQFalse);

    //detail::debugStackShow(vm);


    pop();
    //sq_pop(vm,top);
    return *this;
  }

  template<typename T>
  bool getValueIfExists(const std::string &name, T &value) {
    int top = push();

    sq_pushstring(vm, name.c_str(), -1);

    if(SQ_FAILED(sq_get(vm, -2))) {
      sq_settop(vm, top);
      return false;
    }

    value = types::pop<T>(vm, -1);
    sq_pop(vm,top);

    return true;
  }


  template<typename T>
  T getValue(const std::string &name) {
    T value;

    if (!getValueIfExists(name, value)) {
      error(vm, "Variable '%s' not found", name);
      throw std::runtime_error("Variable '"+name+"' not found");
    }

    return value;
  }

  SQBArray getArray(const std::string &name) {
    int top = push();
    sq_pushstring(vm, name.c_str(), -1);

    if (SQ_FAILED(sq_get(vm, -2))) {
      sq_settop(vm, top);
      error(vm, "Variable '%s' not found", name);
      throw std::runtime_error("Variable '"+name+"' not found");
    }

    SQBArray ar = types::popValue<SQBArray>(vm, -1);

    sq_pop(vm, top);

    return ar;
  }



  template<typename PropType>
  SQBTable& bindValue(const std::string &name, PropType *prop, bool readOnly = false) {
    if (_getterMap == nullptr || _setterMap == nullptr) {
      error(vm, "setter/getter not init TODO detail::initPropHook");
      return *this;
    }

    (*_getterMap)[name] = [prop](HSQUIRRELVM vm) {
      types::pushValue<PropType>(vm, *prop);
      return 1;
    };

    (*_setterMap)[name] = [prop, name, readOnly](HSQUIRRELVM vm) {
      if (readOnly) {
        error(vm, "property '%s' read only!", name);
      }else{
        *prop = types::popValue<PropType>(vm, -2);
      }
      return 0;
    };

    return *this;
  }

  template <typename Func>
  SQBTable& bindFunction(const std::string &name, Func func) {
    detail::registerFunction(this, name, func);
    return *this;
  }


  SQBTable newTable(const std::string &name)
  {
    if (name.empty())
      throw std::runtime_error("Failed create table with empty name");

    int top = push();

    sq_pushstring(vm, name.c_str(), -1);

    SQBTable tbl(vm);
    tbl.setParent(*this);

    tbl.push();
    if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
      sq_settop(vm, 0);
      throw std::runtime_error("Failed to register table");
    }

    /////needRelease = true;
    pop(top); // pop parent

    return tbl;
  }

  SQBArray newArray(const std::string &name)
  {
    if (name.empty())
      throw std::runtime_error("Failed create array with empty name");

    int top = push();

    sq_pushstring(vm, name.c_str(), -1);

    SQBArray ar(vm);
    ar.setParent(*this);

    ar.push();
    if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
      sq_settop(vm, 0);
      throw std::runtime_error("Failed to register array");
    }

    pop(top);

    return ar;
  }


  template <typename T>
  SQBClass<T> bindClass(const std::string &name) {
    SQBClass<T> *baseClass = nullptr;
    return SQBClass<T>(*this, name, baseClass);
  }

  template <typename T, typename B>
  SQBClass<T> bindClass(const std::string &name, const SQBClass<B> &baseClass) {
    return SQBClass<T>(*this, name, &baseClass);
  }

  template <typename T, typename B>
  SQBClass<T> bindClass(const std::string &name, const std::string& baseClassName) {
    SQBClass<B> baseClass(vm, find(baseClassName));
    return SQBClass<T>(*this, name, &baseClass);
  }

  SQBFunction getFunction(const std::string &name)
  {
    return SQBFunction(*this, find(name));
  }

  SQBTable getTable(const std::string &name)
  {
    return SQBTable(*this, find(name));
  }

private:
  HSQOBJECT delegateObj;
  detail::FunctionMap *_setterMap;
  detail::FunctionMap *_getterMap;
  bool isRoot;

  void initDelegate();
};



namespace types {

template<>
inline SQBTable popValue<SQBTable>(HSQUIRRELVM vm, SQInteger idx) {
  HSQOBJECT obj;
  if (SQ_FAILED(sq_getstackobj(vm, idx, &obj))) {
    throw std::runtime_error("Failed to get sqobject");
  }
  return SQBTable(vm, obj);
}

template<>
inline void pushValue<SQBTable>(HSQUIRRELVM vm, const SQBTable& val) {
  val.push();
}

/////////////////////////////////////////////

/*
template<>
inline std::unordered_map<std::string, std::string> popValue<std::unordered_map<std::string, std::string>>(HSQUIRRELVM vm, SQInteger idx) {
  HSQOBJECT obj;
  if (SQ_FAILED(sq_getstackobj(vm, idx, &obj))) {
    throw std::runtime_error("Failed to get sqobject");
  }

  SQBTable tbl(vm, obj);
  //tbl.
}
*/



template<>
inline void pushValue<std::map<std::string, std::string>>(HSQUIRRELVM vm, const std::map<std::string, std::string>& val) {
  SQBTable tbl(vm);

  for (const auto& e : val) {
    tbl.setValue(e.first, e.second);
  }

  tbl.push();
}


template<>
inline void pushValue<std::unordered_map<std::string, std::string>>(HSQUIRRELVM vm, const std::unordered_map<std::string, std::string>& val) {
  SQBTable tbl(vm);

  for (const auto& e : val) {
    tbl.setValue(e.first, e.second);
  }

  tbl.push();
}



}


}
#endif
