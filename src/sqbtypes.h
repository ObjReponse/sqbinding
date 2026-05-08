// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#ifndef SQB_TYPES_H
#define SQB_TYPES_H

#include <algorithm>
#include <squirrel.h>

#include <stdexcept>
#include <string>
#include <typeinfo>
#include <unordered_set>
#include <vector>
#include <unordered_map>

//#include <sstream>

namespace sqb {
namespace types {


////////////////////////////////////////////////////////////////////////
/// Tyep
class Type {
public:
  SQObjectType        type;
  std::vector<size_t> hashes;

  Type() = delete;
  Type(const Type&) = delete;
  Type& operator=(const Type&) = delete;


  static const Type& get(size_t type_hash)
  {
    auto&  type_map  = get_type_map();
    auto it = type_map.find(type_hash);
    if (it != type_map.end())
      return it->second;

    static const Type t({0});
    return t;
  }

  template<typename T, typename... Bases>
  static const Type& create(SQObjectType type = OT_OUTER)
  {
    auto&  type_map  = get_type_map();
    size_t type_hash = typeid(T).hash_code();

    if (type == OT_OUTER)
      type = std::is_pointer<T>::value?OT_USERPOINTER:OT_USERDATA;

    std::vector<size_t> v_hashes = {type_hash};

    auto it = type_map.find(type_hash);

    if (it == type_map.end()) {
      it = type_map.emplace(
            std::piecewise_construct,
            std::forward_as_tuple(type_hash),
            std::forward_as_tuple(v_hashes, type)
            ).first;
    }

    Type &t = it->second;

    if (sizeof...(Bases) > 0) {
      // (..., hashes.push_back(typeid(Bases).hash_code())); // C++17
      add_base_types<Bases...>(t.hashes); // C++11 and add_base_types template
    }

    fill_convert_base_type<T,
                           bool,
                           char, signed char, unsigned char,
                           short, unsigned short,
                           int, unsigned int,
                           long, unsigned long,
                           long long, unsigned long long,
                           float, double, long double,
                           std::string>(t.hashes);

    // remove doubles
    std::unordered_set<size_t> seen;
    auto new_end = std::remove_if(t.hashes.begin(), t.hashes.end(),
            [&seen](const size_t& value) {
                return !seen.insert(value).second;
            });
    t.hashes.erase(new_end, t.hashes.end());

    return t;
  }

  bool can_convert_to(const Type& other) const
  {
    return can_convert_to(other.hashes.front());
  }

  bool can_convert_to(size_t type_hash) const
  {
    for (size_t h : hashes) {
      if (h == type_hash)
        return true;
    }
    return false;
  }

  template<typename T>
  bool can_convert_to() const
  {
    return can_convert_to(typeid(T).hash_code());
  }

  static bool can_convert(size_t from_hash, size_t to_hash)
  {
    if (from_hash == to_hash or from_hash == 0) // from_hash == 0 dont check
      return true;

    auto& type_map = get_type_map();
    auto it = type_map.find(from_hash);
    if (it == type_map.end()) return false;

    for (size_t h : it->second.hashes) {
      if (h == to_hash)
        return true;
    }
    return false;
  }

  template<typename From, typename To>
  static bool can_convert()
  {
    return can_convert(typeid(From).hash_code(), typeid(To).hash_code());
  }

  explicit Type(const std::vector<size_t>& hashes, const SQObjectType type = OT_OUTER)
      : type(type),
        hashes(hashes)
  {}

protected:
  static std::unordered_map<size_t, Type>& get_type_map() {
    static std::unordered_map<size_t, Type> type_map;
    return type_map;
  }


  template<typename T>
  static void fill_convert_base_type(std::vector<size_t>& hashes)
  {}

  template<typename T, typename Next, typename... Rest>
  static void fill_convert_base_type(std::vector<size_t>& hashes)
  {
    if (std::is_convertible<T, Next>::value)
      hashes.push_back(typeid(Next).hash_code());

    fill_convert_base_type<T, Rest...>(hashes);
  }

  template<typename... Bases>
  static void add_base_types(std::vector<size_t>& hashes, typename std::enable_if<sizeof...(Bases) == 0>::type* = nullptr)
  {}

  template<typename First, typename... Rest>
  static void add_base_types(std::vector<size_t>& hashes, typename std::enable_if<sizeof...(Rest) >= 0>::type* = nullptr)
  {
      // registred Type and PointerType
      hashes.push_back(typeid(First).hash_code());
      hashes.push_back(typeid(First*).hash_code());
      add_base_types<Rest...>(hashes);
  }


};


//////////////////////////////////////////////////////////
/// release hook
template <typename T>
static SQInteger release_hook_destruct(SQUserPointer p, SQInteger size)
{
  if (p) static_cast<T*>(p)->~T();
  return 0;
}

template <typename T>
static SQInteger release_hook_delete(SQUserPointer p, SQInteger size)
{
  if (p) delete static_cast<T*>(p);
  return 0;
}


static bool findClassByTypetag(HSQUIRRELVM vm, SQUserPointer tag, HSQOBJECT* outClass) {
  // 1. Iterate through all registered classes (for example, in the root table)
  sq_pushroottable(vm);
  sq_pushnull(vm);  // iterator
  while (SQ_SUCCEEDED(sq_next(vm, -2))) {
    // Checking whether the element is a class
    if (sq_gettype(vm, -1) == OT_CLASS) {
      SQUserPointer classTag;
      sq_gettypetag(vm, -1, &classTag);
      if (classTag == tag) {
        sq_getstackobj(vm, -1, outClass);
        sq_pop(vm, 4);  // Removing the iterator, key, value, and roottable
        return true;
      }
    }
    sq_pop(vm, 2);  // Removing the key and value
  }
  sq_pop(vm, 2);  // Removing the iterator, roottable
  return false;
}


//////////////////////////////////////////////////////////
/// pop

inline SQUserPointer popValuePointer(HSQUIRRELVM vm, SQInteger idx) {
  SQUserPointer userPtr = nullptr;
  SQObjectType  type = sq_gettype(vm, idx);

  if (type == OT_INSTANCE) {
    sq_getinstanceup(vm, idx, &userPtr, nullptr, false);
  } else if (type == OT_USERPOINTER) {
    sq_getuserpointer(vm, idx, &userPtr);
  } else if (type == OT_USERDATA) {
    sq_getuserdata(vm, idx, &userPtr, nullptr);
  } else {
    throw std::runtime_error("Expected instance, userpointer or userdata");
  }

  if (!userPtr)
    throw std::runtime_error("Pointer is null");

  return userPtr;
}




// POINTER
template <typename T, typename std::enable_if< std::is_pointer<T>::value, int>::type = 0>
inline T popValue(HSQUIRRELVM vm, SQInteger idx) {
  SQUserPointer userPtr = popValuePointer(vm, idx);

  SQUserPointer typetag;

  if (SQ_SUCCEEDED(sq_gettypetag(vm, idx, &typetag))) {
    if (!Type::can_convert(reinterpret_cast<size_t>(typetag), typeid(T).hash_code())) {
    //if (reinterpret_cast<size_t>(typetag) != typeid(T).hash_code()) {
      throw std::runtime_error("Type mismatch in userpointer");
    }
  }

  return static_cast<T>(userPtr);
}


// DATA
template <typename T, typename std::enable_if<!std::is_pointer<T>::value, int>::type = 0>
inline T popValue(HSQUIRRELVM vm, SQInteger idx)
{
  //using Tc = typename std::remove_cv<T>::type;
  using Tc = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
  const size_t expectedHash = typeid(Tc).hash_code();

  SQObjectType type = sq_gettype(vm, idx);
  if (type != OT_USERDATA && type != OT_USERPOINTER && type != OT_INSTANCE && type != OT_ARRAY) {
    throw std::runtime_error("Expected userdata type, need custom popValue");
  }

  SQUserPointer typetag;
  if (SQ_FAILED(sq_gettypetag(vm, idx, &typetag))) {
    throw std::runtime_error("Failed to get typetag");
  }

  if (!Type::can_convert(reinterpret_cast<size_t>(typetag), expectedHash)) {
    throw std::runtime_error("Type mismatch in userdata");
  }

  Tc* data = nullptr;

  if (type == OT_USERDATA) {
    if (SQ_FAILED(sq_getuserdata(vm, idx, (SQUserPointer*)&data, nullptr))) {
      throw std::runtime_error("Failed to get userdata");
    }
  }else if (type == OT_USERPOINTER) {
    if (SQ_FAILED(sq_getuserpointer(vm, idx, (SQUserPointer*)&data))) {
      throw std::runtime_error("Failed to get userpointer");
    }
  }else if (type == OT_INSTANCE) {
    sq_getinstanceup(vm, idx, (SQUserPointer*)&data, nullptr, true);
  }

  if (data == nullptr)
    throw std::runtime_error("cannot popValue, data is null");

  return *data;
}

// Specializations for supported types

template<>
inline size_t popValue<size_t>(HSQUIRRELVM vm, SQInteger idx) {
  SQInteger val;
  sq_getinteger(vm, idx, &val);
  return static_cast<size_t>(val);
}

template<>
inline int popValue<int>(HSQUIRRELVM vm, SQInteger idx) {
  SQInteger val;
  sq_getinteger(vm, idx, &val);
  return static_cast<int>(val);
}

template<>
inline long long popValue<long long>(HSQUIRRELVM vm, SQInteger idx) {
  SQInteger val;
  sq_getinteger(vm, idx, &val);
  return static_cast<int>(val);
}

template<>
inline float popValue<float>(HSQUIRRELVM vm, SQInteger idx) {
  SQFloat val;
  sq_getfloat(vm, idx, &val);
  return static_cast<float>(val);
}

template<>
inline bool popValue<bool>(HSQUIRRELVM vm, SQInteger idx) {
  SQBool val;
  sq_getbool(vm, idx, &val);
  return static_cast<bool>(val);
}


#ifdef SQUNICODE
template<>
inline std::wstring popValue(HSQUIRRELVM vm, SQInteger index){
  //checkType(vm, index, OT_STRING);
  const SQChar* val;
  if (SQ_FAILED(sq_getstring(vm, index, &val))) {
    val = nullptr;
    //throw std::runtime_error("Could not get string from squirrel stack");
  }

  if(val == nullptr)
    return std::wstring(L"");

  SQInteger len = sq_getsize(vm,index);
  return std::wstring(val,len);
}
#else
template<>
inline std::string popValue(HSQUIRRELVM vm, SQInteger idx) {
  SQObjectType objType = sq_gettype(vm, idx);

  switch (objType) {
  case OT_STRING:
    const SQChar* ch;
    if (SQ_FAILED(sq_getstring(vm, idx, &ch)))
      throw std::runtime_error("Could not get string from squirrel stack");
    return (ch==nullptr)?"":std::string(ch, sq_getsize(vm,idx));
  case OT_INTEGER:
    return std::to_string( popValue<int>(vm, idx) );
  case OT_FLOAT:
    return std::to_string( popValue<double>(vm, idx) );
  case OT_BOOL:
    return popValue<bool>(vm, idx)?"true":"false";
  case OT_NULL:
    return "NULL";
  default:
    return "";
  }
}
#endif



/////////////////////////////
/// interfcae pop

template <typename T>
inline typename std::enable_if<std::is_pointer<T>::value, T>::type
pop(HSQUIRRELVM vm, SQInteger idx)
{
  return popValue<typename std::remove_cv<T>::type>(vm, idx);
}

template <typename T>
inline typename std::enable_if<!std::is_pointer<T>::value, T>::type
pop(HSQUIRRELVM vm, SQInteger idx)
{
  return popValue<typename std::remove_cv<T>::type>(vm, idx);
}










//////////////////////////////////////////////////////////
/// push


// USERPOINTER
template <typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
inline void pushValue(HSQUIRRELVM vm, const T& val) {
  size_t hash = typeid(T).hash_code();
  const Type &t = Type::get(hash);

  if (t.type == OT_INSTANCE) {
    // search exists instance in stack
    int top = sq_gettop(vm);
    for (int idx=1; idx<=top; idx++) {
      SQUserPointer existingPtr;
      if (SQ_SUCCEEDED(sq_getinstanceup(vm, idx, &existingPtr, nullptr, SQFalse))) {
        if (existingPtr == val && sq_gettype(vm, idx) == t.type) {
          sq_push(vm, idx);
          return;
        }
      }
    }

    // create new instance
    using NakedT = typename std::remove_pointer<T>::type;
    sq_setinstanceup(vm, -1, val);
    sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(typeid(NakedT).hash_code()));
  }else{
    sq_pushuserpointer(vm, reinterpret_cast<SQUserPointer>(val));
    sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(typeid(T).hash_code()));
  }

  //sq_setreleasehook(vm, -1, &release_hook<T>);
}


// USERDATA
template <typename T, typename std::enable_if<!std::is_pointer<T>::value, int>::type = 0>
inline void pushValue(HSQUIRRELVM vm, const T& val) {
  using Tc = typename std::remove_cv<T>::type;
  const size_t typeHash = typeid(Tc).hash_code();
  const Type &t = Type::get(typeHash);

  if (t.type == OT_INSTANCE) {
    // search exists instance in stack
    int top = sq_gettop(vm);
    for (int idx=1; idx<=top; idx++) {
      SQUserPointer existingPtr;
      if (SQ_SUCCEEDED(sq_getinstanceup(vm, idx, &existingPtr, nullptr, SQFalse))) {
        if (existingPtr == &val && sq_gettype(vm, idx) == t.type) {
          sq_push(vm, idx);
          return;
        }
      }
    }

    HSQOBJECT classObj;
    if (!findClassByTypetag(vm, reinterpret_cast<SQUserPointer>(typeHash), &classObj)) {
      throw std::runtime_error("Class not registered in Squirrel");
    }

    sq_pushobject(vm, classObj);
    sq_createinstance(vm, -1);
    sq_remove(vm, -2);
    sq_setinstanceup(vm, -1, new Tc(val));
    sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(typeHash));
    sq_setreleasehook(vm, -1, &release_hook_delete<Tc>);
  }else{
    Tc* data = static_cast<Tc*>(sq_newuserdata(vm, sizeof(Tc)));
    new (data) Tc(val); // placement new

    sq_setreleasehook(vm, -1, &release_hook_destruct<Tc>);
    sq_settypetag(vm, -1, reinterpret_cast<SQUserPointer>(typeHash));
  }

}


template<>
inline void pushValue<size_t>(HSQUIRRELVM vm, const size_t& val) {
  sq_pushinteger(vm, val);
}

template<>
inline void pushValue<int>(HSQUIRRELVM vm, const int& val) {
  sq_pushinteger(vm, val);
}

template<>
inline void pushValue<float>(HSQUIRRELVM vm, const float& val) {
  sq_pushfloat(vm, val);
}

template<>
inline void pushValue<double>(HSQUIRRELVM vm, const double& val) {
  sq_pushfloat(vm, val);
}

template<>
inline void pushValue<bool>(HSQUIRRELVM vm, const bool& val) {
  sq_pushbool(vm, val);
}


#ifdef SQUNICODE
template<>
inline void pushValue(HSQUIRRELVM vm, const std::wstring& value) {
  sq_pushstring(vm, value.c_str(), value.size());
}
#else
template<>
inline void pushValue(HSQUIRRELVM vm, const std::string& val) {
  sq_pushstring(vm, val.c_str(), val.size());
}
#endif



/////////////////////////////
/// interfcae push

// For string literals and pointers to char
inline void push(HSQUIRRELVM vm, const char* val)
{
  sq_pushstring(vm, val ? val : "", val ? -1 : 0);
}

template <typename T> //, typename std::enable_if<!std::is_pointer<T>::value, int>::type = 0>
typename std::enable_if<!std::is_pointer<T>::value, void>::type
inline push(HSQUIRRELVM vm, const T& val)
{
  pushValue<typename std::remove_pointer<typename std::remove_cv<T>::type>::type>(vm, val);
  //pushValue<typename std::remove_cv<T>::type>(vm, val);
}

// for pointer and not char*
//template <typename T, typename std::enable_if<std::is_pointer<T>::value, T>::type* = nullptr>
template <typename T>
typename std::enable_if<
    std::is_pointer<T>::value &&
        !std::is_same<typename std::remove_cv<typename std::remove_pointer<T>::type>::type, char>::value,
    void
    >::type
inline push(HSQUIRRELVM vm, const T& val)
{
  pushValue<typename std::remove_pointer<typename std::remove_cv<T>::type>::type>(vm, val);
}

// multiple args base
template<typename... Args>
typename std::enable_if<sizeof...(Args) == 0>::type
pushArgs(HSQUIRRELVM vm) {}

// multiple args recursion
template<typename T, typename... Args>
typename std::enable_if<sizeof...(Args) >= 0>::type
pushArgs(HSQUIRRELVM vm, T first, Args... args) {
  push(vm, first);
  pushArgs(vm, args...);
}



} // end namespace types

} // end namespace sqb


#endif
