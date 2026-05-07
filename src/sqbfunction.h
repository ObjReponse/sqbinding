// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#ifndef SQB_FUNCTION_H
#define SQB_FUNCTION_H

#include <stdexcept>
#include <string>

#include "sqbdetail.h"
#include "sqbobject.h"

namespace sqb {


class SQBFunction : public SQBObject {
public:
  struct CallResult {
    HSQUIRRELVM vm;
    int top;
    mutable bool processed = false;

    CallResult(HSQUIRRELVM v, int t) : vm(v), top(t) {}

    ~CallResult() {
      if (!processed && vm) {
        sq_settop(vm, top);
      }
    }

    CallResult(CallResult&& other) : vm(other.vm), top(other.top), processed(other.processed) {
      other.processed = true;
    }

    template<typename T>
    T ret() const {
      processed = true;
      T result = types::pop<T>(vm, -1);
      sq_settop(vm, top);
      return result;
    }

    template<typename T>
    operator T() const {
      return ret<T>();
    }

    CallResult(const CallResult&) = delete;
    CallResult& operator=(const CallResult&) = delete;
  };



  SQBFunction(HSQUIRRELVM vm, HSQOBJECT obj)
      : SQBObject(vm, obj)
  {
    if (hsqObject._type != OT_CLOSURE)
      throw std::runtime_error("HSQOBJECT not OT_CLOSURE");
  }

  SQBFunction(SQBObject &parent, HSQOBJECT obj)
      : SQBObject(parent, obj)
  {
    if (hsqObject._type != OT_CLOSURE)
      throw std::runtime_error("HSQOBJECT not OT_CLOSURE");
  }

  template<typename... Args>
  CallResult operator()(const Args&... args) const {
    int top = push();
    _call(args...);
    return CallResult(vm, top);
  }

private:

  template<typename... Args>
  void _call(Args... args) const
  {
    // push first argument parent table or instance or root table
    if (parentObject)
      parentObject->push();
    else
      sq_pushroottable(vm);

    // push another arguments
    types::pushArgs(vm, args...);

    if (SQ_FAILED(sq_call(vm, sizeof...(Args)+1, SQTrue, SQTrue))) {
      throw std::runtime_error("Squirrel call failed");
    }
  }

};


namespace types {


// get object SQBFunction
template<>
inline SQBFunction popValue<SQBFunction>(HSQUIRRELVM vm, SQInteger idx) {
  HSQOBJECT obj;
  if (SQ_FAILED(sq_getstackobj(vm, idx, &obj))) {
    throw std::runtime_error("Failed to get sqobject");
  }
  return SQBFunction(vm, obj);
}

template<>
inline void pushValue<SQBFunction>(HSQUIRRELVM vm, const SQBFunction& val) {
  val.push();
}


} // end namespace



}
#endif
