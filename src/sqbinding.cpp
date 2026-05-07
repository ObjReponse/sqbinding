// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#include "sqbinding.h"

#include <squirrel.h>
#include <sqstdaux.h>    // error handler and base functions
#include <sqstdio.h>
#include <sqstdsystem.h>
#include <sqstdmath.h>
#include <sqstdstring.h>
#include <sqstdblob.h>

using namespace sqb;

SQBinding::SQBinding(size_t stackSize)
    : SQBinding(sq_open(stackSize))
{
  isInternalVM = true;

  /*
  sq_setprintfunc(vm, SQBinding::printfunc, SQBinding::errorfunc);
  */

  sqstd_seterrorhandlers(vm);

  // get root table
  sq_pushroottable(vm);

  // registred librarys
  sqstd_register_systemlib(vm);    // print, type, etc
  sqstd_register_stringlib(vm);
  sqstd_register_mathlib(vm);
  sqstd_register_bloblib(vm);
  sqstd_register_iolib(vm);

  sq_settop(vm, 0); // clean stack
}

SQBinding::SQBinding(HSQUIRRELVM vm)
    : SQBTable(vm, true), // create root table
      isInternalVM(false)
{
  // registred base types
  types::Type::create<bool>(OT_BOOL);
  types::Type::create<char>(OT_INTEGER);
  types::Type::create<short>(OT_INTEGER);
  types::Type::create<int>(OT_INTEGER);
  types::Type::create<long>(OT_INTEGER);
  types::Type::create<float>(OT_FLOAT);
  types::Type::create<double>(OT_FLOAT);
}

SQBinding::~SQBinding()
{
  if (isInternalVM && vm!=nullptr) {
    sq_close(vm);
    vm = nullptr;
  }
}

int SQBinding::executeFile(const std::string &scriptFile)
{
  if (SQ_SUCCEEDED(sqstd_loadfile(vm, scriptFile.c_str(), SQTrue))) {
    sq_pushroottable(vm);
    if (!SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue))) {
      error(vm, "Error execute script '%s'", scriptFile);
      return -1;
    }

    return 0;
  }

  error(vm, "Can`t load file '%s'", scriptFile);

  return -1;
}

int SQBinding::executeString(const std::string &scriptCode, const std::string &scriptName)
{
  if (SQ_SUCCEEDED(sq_compilebuffer(vm, scriptCode.c_str(), scriptCode.size(), scriptName.c_str(), SQTrue))) {
    sq_pushroottable(vm);
    if (!SQ_SUCCEEDED(sq_call(vm, 1, SQFalse, SQTrue))) {
      error(vm, "Error execute script '%s'", scriptName);
      return -1;
    }
    return 0;
  }

  error(vm, "Can`t compile script '%s'", scriptName);

  return -1;
}
