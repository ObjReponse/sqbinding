// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#ifndef SQB_OBJECT_H
#define SQB_OBJECT_H

#include <map>
#include <string>

extern "C" {
  #include <squirrel.h>
}

namespace sqb {


// std::string -> const char*
template<typename T>
typename std::enable_if<std::is_same<typename std::decay<T>::type, std::string>::value, const char*>::type
make_printable(const T& arg) {
  return arg.c_str();
}

// Type -> Type
template<typename T>
typename std::enable_if<!std::is_same<typename std::decay<T>::type, std::string>::value,T>::type
make_printable(T arg) {
  return arg;
}



namespace detail {
struct FunctionVariant;
}



class SQBObject {
public:
  HSQUIRRELVM        vm;
  const   SQBObject *parentObject;
  mutable HSQOBJECT  hsqObject;
  mutable      bool  needRelease;

  std::map<std::string, detail::FunctionVariant*> functionVariant;

public:
  SQBObject(HSQUIRRELVM vm);
  SQBObject(HSQUIRRELVM vm, SQInteger idx);
  SQBObject(HSQUIRRELVM vm, HSQOBJECT obj);
  SQBObject(const SQBObject &parent, HSQOBJECT obj);


  // copy
  SQBObject(const SQBObject& other);

  // move
  SQBObject(SQBObject&& other) noexcept;

  // copy set
  SQBObject& operator=(const SQBObject& other);

  // move set
  SQBObject& operator=(SQBObject&& other) noexcept;


  virtual ~SQBObject();

  void setParent(const SQBObject& parent);

  HSQUIRRELVM getVM() const;

  HSQOBJECT find(const std::string name);

  // push to stack this object hsqObject (table, func, etc)
  virtual int push() const;
  virtual void pop(int stackSizeShouldBe=-1) const;

  void freeStack(int p) const;

  bool empty();


  template<typename... Args>
  static void print(HSQUIRRELVM vm, const char *format, Args... args)
  {
    SQPRINTFUNCTION printfunc = sq_getprintfunc(vm);
    if (printfunc) {
      printfunc(vm, format, make_printable(args)...);
    }
  }

  template<typename... Args>
  static void error(HSQUIRRELVM vm, const char *format, Args... args)
  {
    SQPRINTFUNCTION errorfunc = sq_geterrorfunc(vm);
    if (errorfunc) {
      errorfunc(vm, format, make_printable(args)...);
    }
  }

protected:

  void release();
};


}
#endif
