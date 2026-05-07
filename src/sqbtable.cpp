// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#include "sqbtable.h"

using namespace sqb;

void SQBTable::initDelegate()
{
  delegateObj._type = OT_NULL;

  if (isRoot)
    return;

  if (SQ_SUCCEEDED(sq_getdelegate(vm, -1))) {
    sq_getstackobj(vm, -1, &delegateObj);
  }else{
    return;
  }

  if (!sq_isnull(delegateObj)) {
    sq_addref(vm, &delegateObj);
    sq_pop(vm, 1);
  }else{
    sq_pop(vm, 1);
    sq_newtable(vm);
    sq_getstackobj(vm, -1, &delegateObj);
    sq_addref(vm, &delegateObj);
    SQBObject dt(vm, delegateObj);
    _getterMap = detail::initPropHook<SQBTable>(&dt, false);
    _setterMap = detail::initPropHook<SQBTable>(&dt, true);
    sq_setdelegate(vm, -2);
  }
}
