// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#ifndef SQB_DETAIL_H
#define SQB_DETAIL_H

#include <functional>
#include <squirrel.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <typeindex>
#include <cassert>

#include <tuple>
#include <type_traits>

#include "sqbtypes.h"
#include "sqbobject.h"




namespace sqb {

namespace detail {


static const char* getTypeName(SQObjectType type) {
  switch (type) {
  case OT_NULL:          return "OT_NULL"; break;
  case OT_INTEGER:       return "OT_INTEGER"; break;
  case OT_FLOAT:         return "OT_FLOAT"; break;
  case OT_BOOL:          return "OT_BOOL"; break;
  case OT_STRING:        return "OT_STRING"; break;
  case OT_TABLE:         return "OT_TABLE"; break;
  case OT_ARRAY:         return "OT_ARRAY"; break;
  case OT_USERDATA:      return "OT_USERDATA"; break;
  case OT_CLOSURE:       return "OT_CLOSURE"; break;
  case OT_NATIVECLOSURE: return "OT_NATIVECLOSURE"; break;
  case OT_GENERATOR:     return "OT_GENERATOR"; break;
  case OT_USERPOINTER:   return "OT_USERPOINTER"; break;
  case OT_THREAD:        return "OT_THREAD"; break;
  case OT_FUNCPROTO:     return "OT_FUNCPROTO"; break;
  case OT_CLASS:         return "OT_CLASS"; break;
  case OT_INSTANCE:      return "OT_INSTANCE"; break;
  case OT_WEAKREF:       return "OT_WEAKREF"; break;
  case OT_OUTER:         return "OT_OUTER"; break;
  default:               return "OT_UNKNOWN";
  }
}


static void debugStackShow(HSQUIRRELVM vm)
{
  int top = sq_gettop(vm);
  printf("--- stack top %i ---\n", top);
  for (int i=top; i>0; i--) {
    SQObjectType type = sq_gettype(vm, -i);
    const char *name = getTypeName(type);

    printf("%i %s\n", (-i), name);
  }

  fflush(stdout);
}


static std::string getCurrentPosition(HSQUIRRELVM vm)
{
  SQStackInfos stackInfo;
  SQInteger level = 1; // 0 - current function, 1 - challenging etc
  char buf[1024];

  if (SQ_SUCCEEDED(sq_stackinfos(vm, level, &stackInfo))) {
    if (stackInfo.source && stackInfo.funcname) {
      snprintf(buf, sizeof(buf), "%s:%lli, function %s", stackInfo.source, stackInfo.line, stackInfo.funcname);
    }
  }

  return buf;
}



// For generic types that are functors, delegate to its 'operator()'
template <typename T>
struct function_traits
    : public function_traits<decltype(&T::operator())>
{};

// for pointers to member function
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType(ClassType::*)(Args...) const> {
  //enum { arity = sizeof...(Args) };
  typedef std::function<ReturnType (Args...)> f_type;
};

// for pointers to member function
template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType(ClassType::*)(Args...) > {
  typedef std::function<ReturnType (Args...)> f_type;
};

// for function pointers
template <typename ReturnType, typename... Args>
struct function_traits<ReturnType (*)(Args...)>  {
  typedef std::function<ReturnType (Args...)> f_type;
};

template <typename L>
inline typename function_traits<L>::f_type make_function(L l){
  return static_cast<typename function_traits<L>::f_type>(l);
}


////////////////////////////////////////////////////////////////////////
/// Signature
struct  Signature {
  std::vector<size_t> arg_hashes;
  size_t return_hash;
  int number_required_arguments;

  Signature()
      : return_hash(0),
        number_required_arguments(0)
  {}

  // copy
  Signature(const Signature& other)
      : arg_hashes(other.arg_hashes),
        return_hash(other.return_hash),
        number_required_arguments(other.number_required_arguments)
  {}

  // move
  Signature(Signature&& other) noexcept
      : arg_hashes(std::move(other.arg_hashes)),
        return_hash(other.return_hash),
        number_required_arguments(other.number_required_arguments)
  {}

  Signature(std::vector<size_t> args, size_t ret = typeid(void).hash_code())
      : arg_hashes(std::move(args)),
        return_hash(ret),
        number_required_arguments(arg_hashes.size())
  {}

  /*
  // manual types and
  template <typename Ret, typename... Args, typename... Default>
  explicit Signature(Default... args)
  {
    init_types<Args..., Ret>();
  }
  */

  // from std::function
  template <typename Ret, typename... Args>
  explicit Signature(const std::function<Ret(Args...)> func)
  {
    init_types<Args..., Ret>();
  }

  // from function ptr
  template <typename Ret, typename... Args>
  explicit Signature(Ret(*funcptr)(Args...))
  {
    init_types<Args..., Ret>();
  }

  // from lambda
  template <typename Func>
  explicit Signature(const Func &lambda)
      : Signature(detail::make_function(lambda))
  {
  }

  /*
  template <typename Ret, typename... Args>
  explicit Signature(Args&&..., Ret&&)
  {
    init_types<Args..., Ret>();
  }
  */


  //explicit Signature(HSQUIRRELVM vm, bool isMethod = false)
  //    : return_hash(typeid(void).hash_code())
  void get(HSQUIRRELVM vm, bool isMethod = false)
  {
    return_hash = typeid(void).hash_code();
    arg_hashes.clear();

    std::string sig;
    SQInteger numArgs = sq_gettop(vm);

    int skip = isMethod?1:2;
    number_required_arguments = numArgs - skip;

    for (SQInteger i = skip; i < numArgs; ++i) {
      SQObjectType  type = sq_gettype(vm, i);
      SQUserPointer typetagP;

      sq_gettypetag(vm, i, &typetagP);
      size_t typetag = reinterpret_cast<size_t>(typetagP);
      typetag = getHash(type, typetag);

      arg_hashes.push_back(typetag);
    }
  }

  static std::string getString(HSQUIRRELVM vm, bool isMethod = false)
  {
    SQInteger numArgs = sq_gettop(vm);
    int skip = isMethod?1:2;
    std::string sigString, separator;

    for (SQInteger i = skip; i < numArgs; ++i) {
      SQObjectType  type = sq_gettype(vm, i);
      sigString += separator + getTypeName(type);
      separator = ", ";
    }

    return sigString;
  }

  bool is_compatible(const Signature &other) const
  {
    //if (return_hash != other.return_hash)  // DEBUG!!!
    //  return false;

    int i;
    for (i=0; i<arg_hashes.size(); i++) {
      if (i>=other.arg_hashes.size())
        return false;
      if (!types::Type::can_convert(arg_hashes[i], other.arg_hashes[i]))
        return false;
    }

    if (i < other.number_required_arguments)
      return false;

    return true;
  }

  /*
  std::string to_string() const {
    std::ostringstream oss;
    oss << "Signature[ret=" << return_hash << ", args=";
    for (size_t h : arg_hashes) {
      oss << h << " ";
    }
    oss << "]";
    return oss.str();
  }
  */

  bool operator==(const Signature& other) const {
    return return_hash == other.return_hash && number_required_arguments == other.number_required_arguments && arg_hashes == other.arg_hashes;
  }

  void set_return(size_t hash) { return_hash = hash; }
  void add_arg(size_t hash)    { arg_hashes.push_back(hash); }

private:
  template <typename... Ts>
  void init_types()
  {
    if (sizeof...(Ts) > 0) {
      std::initializer_list<size_t> hashes = {typeid(Ts).hash_code()...};
      arg_hashes.assign(hashes.begin(), hashes.end()-1);
      return_hash = *(hashes.end()-1);
    }
    number_required_arguments = arg_hashes.size();
  }


  static size_t getHash(const SQObjectType &type, const size_t &typetag)
  {
    switch (type) {
    case OT_NULL:    return 0;
    case OT_INTEGER: return typeid(int).hash_code();
    case OT_FLOAT:   return typeid(float).hash_code();
    case OT_BOOL:    return typeid(bool).hash_code();
    case OT_STRING:  return typeid(std::string).hash_code();
    case OT_TABLE:   return 0;
    case OT_ARRAY:   return 0;
    case OT_USERDATA:return typetag;
    case OT_CLOSURE: return 0;
    case OT_NATIVECLOSURE: return 0;
    case OT_GENERATOR: return 0;
    case OT_USERPOINTER: return 0;
    case OT_THREAD:  return 0;
    case OT_FUNCPROTO: return 0;
    case OT_CLASS:   return 0;
    case OT_INSTANCE: return typetag;
    case OT_WEAKREF: return 0;
    default:         return 0;
    }

    return 0;
  }


};



typedef void* (*ExtractFunc)(void*);


template<typename T>
struct ArgExtractor {
  static T extract(HSQUIRRELVM vm, int idx, ExtractFunc) {
    return types::pop<T>(vm, idx);
  }
};

// remove T& -> T
template<typename T>
struct ArgExtractor<T&> {
  //static T& extract(HSQUIRRELVM vm, int idx, ExtractFunc ext) {
  static T extract(HSQUIRRELVM vm, int idx, ExtractFunc ext) {
    return *ArgExtractor<T*>::extract(vm, idx, ext);
  }
};

template<typename T>
struct ArgExtractor<const T&> {
  static T extract(HSQUIRRELVM vm, int idx, ExtractFunc ext) {
    return *ArgExtractor<const T*>::extract(vm, idx, ext);
  }
};

template<typename T>
struct ArgExtractor<T*> {
  static T* extract(HSQUIRRELVM vm, int idx, ExtractFunc ext) {
    if (ext && idx == 1) { // if first argument
      SQUserPointer up;
      sq_getinstanceup(vm, 1, &up, nullptr, false);
      return static_cast<T*>(ext(up));
    }
    return types::pop<T*>(vm, idx);
  }
};

///////////////////////////////////////////////


// Recursive unpacking of arguments
template <typename... Args>
struct ArgumentUnpacker;

// Base case: all arguments are extracted
template <>
struct ArgumentUnpacker<>
{
  template <typename Func>
  static typename std::result_of<Func()>::type
  call(HSQUIRRELVM, Func&& func, const int argsCount, ExtractFunc ext = nullptr)
  {
    return func();
  }
};

// Recursive case: we extract the arguments one at a time
template <typename T, typename... Rest>
struct ArgumentUnpacker<T, Rest...>
{
  template <typename Func>
  static typename std::result_of<Func(T, Rest...)>::type
  call(HSQUIRRELVM vm, Func&& func, const int argsCount, ExtractFunc ext = nullptr)
  {
    SQInteger argIndex = argsCount - sizeof...(Rest);
    int argShift = ext ? 0 : 1;

    typedef typename std::remove_reference<T>::type BaseType;
    BaseType arg = detail::ArgExtractor<T>::extract(vm, argIndex + argShift, ext);

    return ArgumentUnpacker<Rest...>::call(vm, 
      [&func, &arg](Rest... rest) -> decltype(func(arg, rest...)) {
        return func(arg, rest...);
      }, 
      argsCount, ext);
  }
};


template<typename T>
void pushCallResult(HSQUIRRELVM vm, T& result, std::true_type) {
  types::pushValue(vm, &result);
}

template<typename T>
void pushCallResult(HSQUIRRELVM vm, const T& result, std::false_type) {
  types::pushValue(vm, result);
}

// return Ret
template <typename Ret, typename... Args>
typename std::enable_if<!std::is_void<Ret>::value, SQInteger>::type
callFunction(HSQUIRRELVM vm, const std::function<Ret(Args...)>& func, ExtractFunc ext)
{
  int argsCount = sizeof...(Args);

  Ret result = ArgumentUnpacker<Args...>::call(vm, [&func](Args... args) -> Ret {
        return func(args...);
      }, argsCount, ext);

  // Type& -> Type*
  // Type* -> Type*
  // Type  -> Type

  pushCallResult(vm, result, typename std::is_lvalue_reference<Ret>::type());
  return 1;
}


// return void
template <typename Ret, typename... Args>
typename std::enable_if<std::is_void<Ret>::value, SQInteger>::type
callFunction(HSQUIRRELVM vm, const std::function<void(Args...)>& func, ExtractFunc ext)
{
  int argsCount = sizeof...(Args);

  ArgumentUnpacker<Args...>::call(vm, [&func](Args... args) {
        func(args...);
      }, argsCount, ext);
  return 0;
}




using Function = std::function<SQInteger(HSQUIRRELVM)>;

// structure referenced by squirrel when calling the function, it contains all overloaded functions
struct FunctionVariant {
  ExtractFunc extractor;

  std::vector<std::pair<Signature, Function>> functions;

  FunctionVariant(ExtractFunc ext = nullptr)
      : extractor(ext)
  {}

  bool isMethod() const { return extractor != nullptr; }

  template <typename Ret, typename... Args>
  void append(const std::string& name, const std::function<Ret(Args...)> &func)
  {
    Signature signature({typeid(Args).hash_code()...}, typeid(Ret).hash_code());

    for (auto& item : functions) {
      if (item.first == signature)
        throw std::runtime_error("Overload already exists: " + name); // TODO Signature create serialization + " " + signature);
    }

    auto ex = extractor;

    functions.push_back(std::make_pair(
      std::move(signature),
      [func, ex](HSQUIRRELVM vm) {
        return callFunction<Ret, Args...>(vm, func, ex);
      }
    ));
  }

  Function* getCompatibleFunction(const Signature &signatureCall)
  {
    for (auto& item : functions) {
      auto& signature = item.first;

      if (signatureCall.is_compatible(signature))
        return &item.second;
    }

    return nullptr;
  }
};



using FunctionMap = std::map<std::string, Function>;

template <typename TObject>
FunctionMap* initPropHook(SQBObject *obj, bool isSetter) {
  auto &vm = obj->vm;

  const char* name = isSetter?"_set":"_get";
  auto f = obj->find(name);
  if ( f._type != OT_NULL ) {
    return nullptr; // TODO get FunctionMap if available
  }

  int top  = obj->push();
  sq_pushstring(vm, name, -1);

  FunctionMap *m = new FunctionMap();
  types::pushValue(vm, m);
  sq_setreleasehook(vm, -1, &types::release_hook_delete<FunctionMap>);

  sq_newclosure(vm, [](HSQUIRRELVM vm) -> SQInteger {
        int count = sq_gettop(vm);

        FunctionMap *m  = types::popValue<FunctionMap*>(vm, -1);
        std::string key = types::popValue<std::string>(vm, -(count-1));

        auto it = m->find(key);
        if (it == m->end())
          throw std::runtime_error(std::string("undefined symbol ") + key);

        return it->second(vm);
      }, 1); // capture userdata
  sq_newslot(vm, -3, isSetter); // !!! setter req true

  obj->pop(top);

  return m;
}


inline FunctionVariant* findFunctionVariant(SQBObject *obj, const std::string& name)
{
  if (obj->functionVariant.find(name) != obj->functionVariant.end()) {
    return obj->functionVariant[name];
  }

  return nullptr;
}


inline std::string getCurrentFunctionSignature(HSQUIRRELVM vm, bool isMethod = false, int depth=0)
{
  SQStackInfos si;
  std::string name;

  // 0 — current function, 1 — challenging etc
  if (SQ_SUCCEEDED(sq_stackinfos(vm, depth, &si))) {
    if (si.funcname) {
      name = si.funcname;
    } else {
      name = "NONE";
    }
  }else{
    return "";
  }

  name += "(" + Signature::getString(vm, isMethod)  + ")";
  return name;
}


template <typename Ret, typename... Args>
void registerFunction(SQBObject *obj, const std::string& name, const std::function<Ret(Args...)> func, ExtractFunc extractor = nullptr, bool isStatic=false)
{
  HSQUIRRELVM vm = obj->getVM();

  if (isStatic)
    extractor = nullptr;

  if (vm == nullptr)
    throw std::runtime_error("squirrel vm not set");

  FunctionVariant* fv = findFunctionVariant(obj, name);

  if (fv == nullptr) {
    int top = obj->push(); //sq_pushroottable(vm);

    sq_pushstring(vm, name.c_str(), name.size());

    fv = new FunctionVariant(extractor);
    types::pushValue(vm, fv);
    sq_setreleasehook(vm, -1, &types::release_hook_delete<FunctionVariant>);

    obj->functionVariant[name] = fv;

    sq_newclosure(vm, [](HSQUIRRELVM vm) -> SQInteger {
          FunctionVariant *fv = types::popValue<FunctionVariant*>(vm, -1);

          Signature signature;
          signature.get(vm, fv->isMethod());

          Function *f = fv->getCompatibleFunction(signature);
          if (f==nullptr) {
            throw std::runtime_error(getCurrentFunctionSignature(vm, fv->isMethod()) + " signature not found"); // TODO + signature);
          }

          return (*f)(vm);
        }, 1); // 1 capturing one object on the stack before newclosure, the object is removed from the stack

    sq_setnativeclosurename(vm, -1, name.c_str());

    SQObjectType type = sq_gettype(vm, -3);
    assert(type == OT_TABLE || type == OT_CLASS);
    if (SQ_FAILED(sq_newslot(vm, -3, isStatic))) { // SQFalse as Instance , SQTrue as Class (static)
      throw std::runtime_error("failed bind function " + name);
    }

    obj->freeStack(top);
  }

  fv->append(name, func);
}


template <typename Ret, typename... Args>
static void registerFunction(SQBObject *obj, const std::string& name, Ret(*funcptr)(Args...), ExtractFunc extractor = nullptr, bool isStatic=false)
{
  auto func = std::function<Ret(Args...)>(funcptr);
  registerFunction(obj, name, func, extractor, isStatic);
}


template <typename Func>
static void registerFunction(SQBObject *obj, const std::string& name, const Func &lambda, ExtractFunc extractor = nullptr, bool isStatic=false)
{
  registerFunction(obj, name, detail::make_function(lambda), extractor, isStatic);
}

}}


#endif
