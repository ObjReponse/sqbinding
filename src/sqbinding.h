// SQBinding - C++11 binding for Squirrel
// Copyright (c) 2025 Ligverd
// Licensed under the MIT License. See LICENSE file for details.

#ifndef SQB_BINDING_H
#define SQB_BINDING_H

#include "sqbtable.h"

namespace sqb {


class SQBinding : public SQBTable {
public:
  SQBinding(size_t stackSize=1024);
  SQBinding(HSQUIRRELVM vm);

  ~SQBinding();

  int executeFile(const std::string &scriptFile);
  int executeString(const std::string &scriptCode, const std::string &scriptName="script");

private:
  bool isInternalVM;
};


}
#endif
