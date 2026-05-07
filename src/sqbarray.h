// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#ifndef SQB_ARRAY_H
#define SQB_ARRAY_H

#include <stdexcept>
#include <string>
#include "sqbobject.h"
#include "sqbclass.h"
#include "sqbfunction.h"

namespace sqb {


class SQBArray : public SQBObject
{
public:
  class SQBArrayItem
  {
  public:
    SQBArrayItem(const SQBArray &array, int index)
        : array(array),
          index(index)
    {}

    template<typename T>
    T as() const
    {
      if(index < 0 || index >= array.size()) {
        throw std::runtime_error("Array index out of bounds");
      }

      array.push();
      sq_pushinteger(array.vm, index);

      if (SQ_FAILED(sq_get(array.vm, -2))) {
        sq_pop(array.vm, 2); // remove array, index
        throw std::runtime_error("Failed to get array element");
      }

      T val = types::pop<T>(array.vm, -1);
      sq_pop(array.vm, 2);  // remove array, val
      return val;
    }


    // Getter (implicit conversion)
    template<typename T>
    operator T() const
    {
      return as<T>();
    }

    // Setter
    template<typename T>
    SQBArrayItem& operator=(const T& value) {
      array.push(); // [array]

      int size = array.size();

      if(index >= size) {
        for(int i = size; i <= index; i++) {
          sq_pushnull(array.vm);
          sq_arrayappend(array.vm, -2);
        }
      }

      sq_pushinteger(array.vm, index); // [array, index]
      types::push(array.vm, value);    // [array, index, value]
      sq_set(array.vm, -3);            // array[index] = value, stack: [array]

      array.pop();
      return *this;
    }


    bool operator ==(const char* other)
    {
      std::string str(other);
      return operator==(str);
    }

    template<typename T>
    bool operator ==(const T& other)
    {
      if(index < 0 || index >= array.size()) {
        throw std::runtime_error("Array index out of bounds");
      }

      array.push();
      sq_pushinteger(array.vm, index);

      if (SQ_FAILED(sq_get(array.vm, -2))) {
        sq_pop(array.vm, 2); // remove array, index
        throw std::runtime_error("Failed to get array element");
      }

      T val = types::pop<T>(array.vm, -1);
      sq_pop(array.vm, 2);  // remove array, val

      return val == other;
    }

  private:
    const SQBArray &array;
    int index;
  };

public:
  ~SQBArray()
  {
  }


  /*
  // copy
  SQBArray(const SQBArray& other)
    : SQBObject(other.vm, other.hsqObject)
  {
  }
  */

  SQBArray(HSQUIRRELVM vm, int size=0)
    : SQBObject(vm)
  {
    sq_newarray(vm, size);

    sq_getstackobj(vm, -1, &hsqObject);
    sq_addref(vm, &hsqObject);

    sq_pop(vm, 1);

    needRelease = true;
  }

  template<typename T>
  SQBArray(HSQUIRRELVM vm, const std::vector<T>& val)
      : SQBArray(vm, 0)
  {
    append(val);
  }


  SQBArray(HSQUIRRELVM vm, HSQOBJECT obj)
      : SQBObject(vm, obj)
  {
    if (hsqObject._type != OT_ARRAY)
      throw std::runtime_error("HSQOBJECT not OT_ARRAY");
  }

  SQBArray(const SQBObject &parent, HSQOBJECT obj)
      : SQBObject(parent, obj)
  {
    if (hsqObject._type != OT_ARRAY)
      throw std::runtime_error("HSQOBJECT not OT_ARRAY");
  }


  template<typename T>
  SQBArray& append(const T& value)
  {
    push();
    types::push<T>(vm, value);
    if ( SQ_FAILED(sq_arrayappend(vm, -2)) )
      throw std::runtime_error("cannot append value to array");
    pop();

    return *this;
  }

  template<typename T>
  SQBArray& append(const std::vector<T>& value)
  {
    push();
    for (const auto &i : value) {
      types::push<T>(vm, i);
      if ( SQ_FAILED(sq_arrayappend(vm, -2)) )
        throw std::runtime_error("cannot append value to array");
    }
    pop();

    return *this;
  }

  template<typename T>
  std::vector<T> to_vector() const
  {
    std::vector<T> t;
    int c = size();
    t.reserve(c);

    for (int i=0; i<c; i++)
      t.push_back(SQBArrayItem(*this, i).as<T>());

    return t;
  }

  /*
   * bad idea!
  template<typename T>
  operator std::vector<T>()
  {
    std::vector<T> t;
    int c = size();
    t.reserve(c);

    for (int i=0; i<c; i++)
      t.push_back(SQBArrayItem(*this, i).as<T>());

    return t;
  }
  */


  int size() const
  {
    push();
    int len = sq_getsize(vm, -1);
    pop();
    return len;
  }

  void clear()
  {
    push();
    sq_arrayresize(vm, -1, 0);
    pop();
  }

  /*
  SQBArray clone() const
  {
    SQBArray new_arr(vm);
    push();
    new_arr.push();
    sq_clone(vm, -2);
    sq_remove(vm, -2);
    return new_arr;
  }
  */

  SQBArray clone() const
  {
    push();           // [original]
    sq_clone(vm, -1); // [original, clone]

    HSQOBJECT hObj;
    sq_getstackobj(vm, -1, &hObj);

    SQBArray new_arr(vm, hObj);

    sq_pop(vm, 2);
    return new_arr;
  }


  SQBArrayItem operator[](int index)
  {
    return SQBArrayItem(*this, index);
  }


};

namespace types {


template<>
inline SQBArray popValue<SQBArray>(HSQUIRRELVM vm, SQInteger idx) {
  HSQOBJECT obj;
  if (SQ_FAILED(sq_getstackobj(vm, idx, &obj))) {
    throw std::runtime_error("Failed to get sqobject");
  }
  return SQBArray(vm, obj);
}

template<>
inline void pushValue<SQBArray>(HSQUIRRELVM vm, const SQBArray& val) {
  val.push();
}

//////////////////////////////////////////////////

// array monotype
//template<typename A, typename T, typename = typename std::enable_if<std::is_same<A, std::vector<T>>::value>::type>
template<typename T>
inline void pushValue(HSQUIRRELVM vm, const std::vector<T>& val) {
  SQBArray ar(vm, val.size());
  ar.append(val);
  ar.push();
}


}

// end namespase types


}
#endif
