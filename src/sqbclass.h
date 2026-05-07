// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#ifndef SQB_CLASS_H
#define SQB_CLASS_H

#include <stdexcept>
#include <string>
#include <memory>
#include "sqbdetail.h"
#include "sqbobject.h"

namespace sqb {


template<typename ClassType>
class SQBClass : public SQBObject {
protected:

  struct SmartStrategy {
    SQUserPointer       (*packInstance)(ClassType*);
    ClassType*          (*extractInstance)(SQUserPointer);
    SQRELEASEHOOK       releaseHook;
  };
  SmartStrategy _strategy;

public:

  using PackHook    = SQUserPointer(*)(ClassType*);
  using ExtractHook = ClassType*(*)(SQUserPointer);

  SQBClass(HSQUIRRELVM vm, HSQOBJECT obj)
      : SQBObject(vm, obj)
  {
    if (hsqObject._type != OT_CLASS)
      throw std::runtime_error("HSQOBJECT not OT_CLASS");
  }


  template<typename BaseClassType>
  SQBClass(const SQBObject &parent, const std::string &name, const SQBClass<BaseClassType>* baseClass=nullptr)
      : SQBObject(parent.vm)
  {
    if (name.empty())
      throw std::runtime_error("Failed create class with empty name");

    setParent(parent);
    int top = parent.push();

    sq_pushstring(vm, name.c_str(), -1);


    if (baseClass == nullptr) {
      types::Type::create<ClassType,  ClassType*>(OT_INSTANCE);
      types::Type::create<ClassType*, ClassType>(OT_INSTANCE);

      sq_newclass(vm, false); // [table, string] without inheritance
    }else{
      types::Type::create<ClassType,  ClassType*, BaseClassType, BaseClassType*>(OT_INSTANCE);
      types::Type::create<ClassType*, ClassType,  BaseClassType, BaseClassType*>(OT_INSTANCE);

      baseClass->push();
      sq_newclass(vm, true);  // [table, string, class]
    }

    size_t typetag;
    typetag  = typeid(ClassType).hash_code();
    sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(typetag));

    sq_getstackobj(vm, -1, &hsqObject);
    sq_addref(vm, &hsqObject);

    if(SQ_FAILED(sq_newslot(vm, -3, SQFalse))) {
      sq_settop(vm, 0);
      throw std::runtime_error("Failed to register class");
    }

    needRelease = true;
    pop(top);

    _getterMap = detail::initPropHook<ClassType>(this, false);
    _setterMap = detail::initPropHook<ClassType>(this, true);

    // default pack/extract
    _strategy.packInstance    = [](ClassType* i)    { return static_cast<SQUserPointer>(i); };
    _strategy.extractInstance = [](SQUserPointer p) { return static_cast<ClassType*>(p); };
    _strategy.releaseHook     = &types::release_hook_delete<ClassType>;
  }


  template<typename SmartPtr>
  SQBClass& smart(PackHook packHook, ExtractHook extractHook)
  {
    _strategy.packInstance    = packHook;
    _strategy.extractInstance = extractHook;
    _strategy.releaseHook     = &types::release_hook_delete<SmartPtr>;

    int top = push();
    size_t typetag;
    typetag  = typeid(SmartPtr).hash_code();
    sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(typetag));
    pop();

    types::Type::create<SmartPtr>(OT_INSTANCE);

    return *this;
  }

  SQBClass& smartSharedPtr()
  {
    _strategy.packInstance    = [](ClassType* c) -> SQUserPointer { return new std::shared_ptr<ClassType>(c); };
    _strategy.extractInstance = [](SQUserPointer p) -> ClassType* { return static_cast<std::shared_ptr<ClassType>*>(p)->get(); };
    _strategy.releaseHook     = &types::release_hook_delete<std::shared_ptr<ClassType>>;

    int top = push();
    size_t typetag;
    typetag  = typeid(std::shared_ptr<ClassType>).hash_code();
    sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(typetag));
    pop();

    types::Type::create<std::shared_ptr<ClassType>>(OT_INSTANCE);

    return *this;
  }

  template <typename... Args>
  SQBClass& bindConstructor() {
    //auto func = [](Args... args){ return new ClassType(args...); };
    //instanceAllocator(func);
    HSQUIRRELVM v = vm;
    SmartStrategy h = _strategy;

    detail::registerFunction(this, "constructor", [v, h](Args... args) -> void {
      auto obj = h.packInstance( new ClassType(args...) );
      sq_setinstanceup(v, 1, obj );
      sq_setreleasehook(v, 1, h.releaseHook);
      sq_pop(v, 2);
    });
    return *this;
  }

  template <typename Func>
  SQBClass& bindConstructor(const Func lambda) {
    auto func = detail::make_function(lambda);
    instanceAllocator(func);
    return *this;
  }


  template <typename Ret, typename... Args>
  SQBClass& bindMethod(const char* name, Ret (ClassType::*method)(Args...))
  {
    detail::registerFunction(this, name, [method](ClassType *self, Args... args) -> Ret {
      return (self->*method)(args...);
    }, (detail::ExtractFunc)_strategy.extractInstance);
    return *this;
  }

  template <typename Func>
  SQBClass& bindMethod(const std::string &name, Func func) {
    detail::registerFunction(this, name, func, (detail::ExtractFunc)_strategy.extractInstance);
    return *this;
  }

  template <typename Ret, typename... Args>
  SQBClass& bindStaticMethod(const char* name, Ret (ClassType::*method)(Args...))
  {
    detail::registerFunction(this, name, [method](Args... args) -> Ret {
      return (*method)(args...);
    }, nullptr, true);
    return *this;
  }

  template <typename Func>
  SQBClass& bindStaticMethod(const std::string &name, Func func) {
    detail::registerFunction(this, name, func, nullptr, true);
    return *this;
  }


  /*
    getter
    --- stack top 3 ---
    -3 OT_INSTANCE    this
    -2 OT_STRING      key
    -1 OT_USERPOINTER map

    setter
    --- stack top 4 ---
    -4 OT_INSTANCE    this
    -3 OT_STRING      key
    -2 OT_STRING      value
    -1 OT_USERPOINTER map
  */
  template<typename PropType>
  SQBClass& bindProp(const std::string &name, PropType ClassType::* prop, bool readOnly = false) {
    auto strategy = _strategy;

    (*_getterMap)[name] = [prop, strategy](HSQUIRRELVM vm) {
      SQUserPointer up = types::popValuePointer(vm, -3);
      ClassType *c = strategy.extractInstance(up);
      types::pushValue<PropType>(vm, c->*prop);
      return 1;
    };

    (*_setterMap)[name] = [prop, name, readOnly, strategy](HSQUIRRELVM vm) {
      if (readOnly) {
        error(vm, "property '%s' read only!", name);
      }else{
        SQUserPointer up = types::popValuePointer(vm, -3);
        ClassType *c = strategy.extractInstance(up);
        c->*prop = types::popValue<PropType>(vm, -2);
      }
      return 0;
    };

    return *this;
  }

  template<typename PropType>
  SQBClass& bindProp(const std::string &name, std::function<PropType(ClassType*)> getter, std::function<void(ClassType*, PropType)> setter = nullptr) {
    auto strategy = _strategy;

    if (getter) {
      (*_getterMap)[name] = [getter, strategy](HSQUIRRELVM vm) {
        SQUserPointer up = types::popValuePointer(vm, -3);
        ClassType *c = strategy.extractInstance(up);
        types::pushValue<PropType>(vm, getter(c));
        return 1;
      };
    }

    if (setter) {
      (*_setterMap)[name] = [setter, strategy](HSQUIRRELVM vm) {
        SQUserPointer up = types::popValuePointer(vm, -4);
        ClassType *c = strategy.extractInstance(up);
        setter(c, types::popValue<PropType>(vm, -2));
        return 0;
      };
    }

    return *this;
  }

  /*
  template<typename PropType>
  SQBClass& bindStaticProp(const std::string &name, PropType *prop, bool readOnly = false) {

    Scurrel not support this

    use bindStaticMethod(name, getter, setter)

    Class.setProp(value)
    value = Class.getProp()

    fuck :(

    return *this;
  }
  */

private:
  template <typename Ret, typename... Args, typename std::enable_if<!std::is_pointer<Ret>::value, Ret>::type* = nullptr>
  void instanceAllocator(const std::function<Ret(Args...)> func)
  {
    HSQUIRRELVM v = vm;
    SmartStrategy h = _strategy;

    detail::registerFunction(this, "constructor", [v, h, func](Args... args) -> void {
      auto data = h.packInstance(new ClassType( func(args...) ));
      sq_setinstanceup(v, 1, data);
      sq_setreleasehook(v, 1, h.releaseHook);
    });
  }

  template <typename Ret, typename... Args, typename std::enable_if<std::is_pointer<Ret>::value, Ret>::type* = nullptr>
  void instanceAllocator(const std::function<Ret(Args...)> func)
  {
    HSQUIRRELVM v = vm;
    SmartStrategy h = _strategy;
    detail::registerFunction(this, "constructor", [v, h, func](Args... args) -> void {
      auto obj = h.packInstance( func(args...) );
      sq_setinstanceup(v, 1, obj);
      sq_setreleasehook(v, 1, h.releaseHook);
    });
  }


  detail::FunctionMap *_setterMap;
  detail::FunctionMap *_getterMap;

};

}
#endif
