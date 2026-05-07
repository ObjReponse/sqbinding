// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#include "sqbobject.h"
#include <cassert>
#include <cstdarg>
#include <stdexcept>

using namespace sqb;



SQBObject::SQBObject(HSQUIRRELVM vm)
    : vm(vm),
      parentObject(nullptr),
      hsqObject({OT_NULL, nullptr}),
      needRelease(false)
{
  assert(vm!=nullptr);
}

SQBObject::SQBObject(HSQUIRRELVM vm, SQInteger idx)
  : vm(vm),
    parentObject(nullptr),
    needRelease(true)
{
  assert(vm!=nullptr);

  if (SQ_FAILED(sq_getstackobj(vm, idx, &hsqObject)))
    throw std::runtime_error("Failed to get sqobject");
  sq_addref(vm, &hsqObject);
}

SQBObject::SQBObject(HSQUIRRELVM vm, HSQOBJECT obj)
    : vm(vm),
      parentObject(nullptr),
      hsqObject(obj),
      needRelease(true)
{
  assert(vm!=nullptr);
  sq_addref(vm, &hsqObject);
}

SQBObject::SQBObject(const SQBObject &parent, HSQOBJECT obj)
    : vm(parent.vm),
      parentObject(&parent),
      hsqObject(obj),
      needRelease(true)
{
  assert(vm!=nullptr);
  sq_addref(vm, &hsqObject);
}

SQBObject::SQBObject(const SQBObject &other)
    : vm(other.vm),
      parentObject(other.parentObject),
      hsqObject(other.hsqObject),
      needRelease(other.needRelease)
{
  if (needRelease)
    sq_addref(vm, &hsqObject);
}

SQBObject::SQBObject(SQBObject &&other) noexcept
    : vm(other.vm),
      parentObject(other.parentObject),
      hsqObject(other.hsqObject),
      needRelease(other.needRelease)
{
  other.hsqObject = {};
  other.needRelease = false;
}

SQBObject &SQBObject::operator=(SQBObject &&other) noexcept {
  if (this != &other) {
    release();
    vm = other.vm;
    parentObject = other.parentObject;
    hsqObject = other.hsqObject;
    needRelease = other.needRelease;
    other.hsqObject = {};
    other.needRelease = false;
  }
  return *this;
}

SQBObject &SQBObject::operator=(const SQBObject &other) {
  if (this != &other) {
    release();
    vm = other.vm;
    parentObject = other.parentObject;
    hsqObject = other.hsqObject;
    needRelease = other.needRelease;
    if (needRelease && sq_isinstance(hsqObject)) {
      sq_addref(vm, &hsqObject);
    }
  }
  return *this;
}

SQBObject::~SQBObject()
{
  release();
}

void SQBObject::setParent(const SQBObject &parent) {
  parentObject = &parent;
}

HSQUIRRELVM SQBObject::getVM() const
{
  return vm;
}

HSQOBJECT SQBObject::find(const std::string name)
{
  if (name.empty())
    return {OT_NULL, nullptr}; // null object

  push();

  sq_pushstring(vm, name.c_str(), -1); // [parent, string]

  if(SQ_FAILED(sq_get(vm, -2))) { // [parent, object]
    sq_pop(vm, 1);
    return {OT_NULL, nullptr}; // null object
    //throw std::runtime_error("Could not get object " + name);
  }

  HSQOBJECT obj;
  sq_getstackobj(vm, -1, &obj);

  //freeStack(2);
  sq_pop(vm, 2);

  return obj;
}

int SQBObject::push() const
{
  sq_pushobject(vm, hsqObject);
  return sq_gettop(vm);
}

void SQBObject::pop(int stackSizeShouldBe) const
{
  int t = sq_gettop(vm);
  if (stackSizeShouldBe>-1 && t != stackSizeShouldBe)
    throw std::runtime_error("sq_gettop "+ std::to_string(t) +") != stackSizeShouldBe:" + std::to_string(stackSizeShouldBe));
  sq_pop(vm, 1);
}


void SQBObject::freeStack(int p) const
{
  //assert(sq_gettop(vm) == p);
  if (sq_gettop(vm) != p)
    throw std::runtime_error("freeStack("+ std::to_string(p) +") != top:" + std::to_string(sq_gettop(vm)));
  sq_pop(vm, p);
}


bool SQBObject::empty() {
  return (hsqObject._unVal.raw == 0);
}


void SQBObject::release()
{
  if (needRelease && vm)
    sq_release(vm, &hsqObject);
}
