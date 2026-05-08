#include <iostream>
#include <cstdarg>
#include <cassert>
#include <unistd.h>
#include "sqbinding.h"
#include "ut/check.h"


std::string f_str_int(std::string name, int i) {
  return name + std::to_string(i);
}

std::string myFuncDef(std::string name, int i=10) {
  return name + " " + std::to_string(i);
}


int test(int i) {
  return i+i;
}

std::string test(std::string s) {
  return "hello " + s;
}

float test(float f) {
  return f+1;
}


struct Test {
  std::string name;
  int idx;

  Test() : idx(0) {}
  Test(int i) : idx(i) {}
  Test(std::string s, int i) : name(s), idx(i) {}

  //~Test() { std::cout << "~Test" << std::endl; }

  std::string single(int i) {
    return "idx: "+ std::to_string(idx) +" name: " + name +"  single(" + std::to_string(i) + ")";
  }

  std::string multi(int i) {
    return "idx: "+ std::to_string(idx) +" test(" + std::to_string(i) + ")";
  }

  std::string multi(int i, std::string s) {
    return "idx: "+ std::to_string(idx) +" test(" + std::to_string(i) + ", " + s + ")";
  }

  Test& operator<< (int i) {
    idx+=i;
    return *this;
  }

  Test& operator| (std::string s) {
    name += s;
    return *this;
  }

  static std::string staicMethod(int x) {
    static std::string data = "static";
    data += " " + std::to_string(x);
    return data;
  }

  static int static_prop;
};

int Test::static_prop = 55;


int testArgPtr(std::shared_ptr<Test> ptr)
{
  return ptr->idx;
}


class Base
{
public:
  std::string baseMethod() {
    return "base";
  }
};

class Custom : public Base
{
public:
  std::string myMethod() {
    return "test";
  }
};



// custom type
struct CustomType
{
  std::string name;
  int number;
  std::vector<int> ar;
};

// register custom type in SQB
namespace sqb { namespace types {

template<>
inline CustomType popValue<CustomType>(HSQUIRRELVM vm, SQInteger idx) {
  if (sq_gettype(vm, idx) != OT_TABLE)
    throw std::runtime_error("can`t convert to CustomType");

  CustomType val;

  SQBTable t = popValue<SQBTable>(vm, idx);

  val.name   = t.getValue<std::string>("name");
  val.number = t.getValue<int>("number");
  val.ar     = t.getArray("ar").to_vector<int>();

  return val;
}

template<>
inline void pushValue<CustomType>(HSQUIRRELVM vm, const CustomType& val) {
  SQBTable t(vm);
  SQBArray ar(vm);

  ar.append(val.ar);

  t.setValue("name", val.name);
  t.setValue("number", val.number);
  t.setValue("ar", ar);

  t.push();
}

}}


/////////////////////////////////////////////////////////////
void printfunc(HSQUIRRELVM v, const SQChar *s,...)
{
  va_list vl;
  va_start(vl, s);
  vfprintf(stdout, s, vl);
  fprintf(stdout, "\n");
  fflush(stdout);
  va_end(vl);
}


enum AAA {
  A,
  B,
  C
};

int main(int argc, char **argv)
{
  ut::Check check;


  {
    sqb::SQBinding sqb;
    sq_setprintfunc(sqb.vm, printfunc, printfunc);

    std::string my_name = "text";
    int my_i = 55;

    sqb.setValue("A", AAA::A)
       .setValue("B", AAA::B)
       .setValue("C", AAA::C)
       ;

    sqb.newTable("my")
        .bindValue("name", &my_name)
        .bindValue("i", &my_i)
        .bindFunction("func", [](int x) { return x*2; })
        //.newTable("sub")
        ;


    sqb.bindFunction("test", static_cast<int(*)(int)>(test));
    sqb.bindFunction("test", static_cast<std::string(*)(std::string)>(test));
    //sqb.bindFunction("test", static_cast<float(*)(float)>(test));

    sqb.bindFunction("f_str_int", f_str_int);

    sqb.bindClass<Test>("Test")
        .bindConstructor()
        .bindConstructor<int>()
        .bindConstructor<std::string, int>()
        .bindConstructor([](std::string s) { return new Test(s, 55); })
        .bindMethod("single", &Test::single)
        .bindMethod("lambda", [](Test *self, int i, std::string s) { return self->multi(i,s); } )
        .bindMethod("multi", static_cast<std::string(Test::*)(int)>(&Test::multi))
        .bindMethod("multi", static_cast<std::string(Test::*)(int,std::string)>(&Test::multi))
        .bindProp("name", &Test::name)
        .bindProp("idx", &Test::idx, true)
        //.bindMethod("_shiftl", [](Test *self, int i) { *self << i; return *self; })
        .bindMethod("_shiftl", &Test::operator<<)
        .bindMethod("_or", [](Test *self, std::string s) { *self | s; return self; })
        .bindStaticMethod("staticMethod", &Test::staicMethod)
        /////.bindStaticProp("static_prop", &Test::static_prop)
        ;

    // :(
    sqb.newTable("TestStatic")
        .bindValue("static_prop", &Test::static_prop)
        ;

    sqb.bindClass<Test>("TestP")
        .smartSharedPtr()
        .bindConstructor()
        .bindConstructor<int>()
        .bindConstructor<std::string, int>()
        .bindConstructor([](std::string s) { return new Test(s, 55); })
        .bindMethod("single", &Test::single)
        .bindMethod("lambda", [](Test *self, int i, std::string s) { return self->multi(i,s); } )
        .bindMethod("multi", static_cast<std::string(Test::*)(int)>(&Test::multi))
        .bindMethod("multi", static_cast<std::string(Test::*)(int,std::string)>(&Test::multi))
        .bindProp("name", &Test::name)
        .bindProp("idx", &Test::idx, true)
        .bindMethod("_shiftl", [](Test *self, int i) { *self << i; return self; })
        //.bindMethod("_shiftl", &Test::operator<<)
        .bindMethod("_or", [](Test *self, std::string s) { *self | s; return self; })
        .bindStaticMethod("staticMethod", &Test::staicMethod)
        /////.bindStaticProp("static_prop", &Test::static_prop)
        ;

    sqb.bindFunction("testArgPtr", &testArgPtr);


    check.test("execute script from string", sqb.executeString(R"(print(">>> hello from script as string"))")==0);
    check.test("execute script from file", sqb.executeFile(ut::Check::getPath(__FILE__)+"/test.nut")==0);

    check.test("bind function", sqb.getValue<std::string>("stest5") == "test5" );
    check.test("bind table var", (my_name == "Hello" && my_i == 100) );
    check.test("bind table function", sqb.getValue<int>("i88") == 88 );
    check.test("bind multi(int)", sqb.getValue<std::string>("smulti1") == "idx: 55 test(7)");
    check.test("bind multi(int, string)", sqb.getValue<std::string>("smulti2") == "idx: 55 test(8, data)");
    check.test("bind static method", (Test::staicMethod(15) == "static 12 15"));
    check.test("bind static prop", (Test::static_prop == 567));
    check.test("bind operator<<", sqb.getValue<int>("opshiftl") == 50);
    check.test("bind operator|", sqb.getValue<std::string>("opor") == "Hello Ligverd" && sqb.getValue<std::string>("oporor") == "Hello Ligverd and Lana");
    check.test("check cross var", sqb.executeString("crossValue <- 555")==0 && sqb.getValue<int>("crossValue") == 555);
    check.test("check object shared_ptr", sqb.getValue<int>("idx_TestP") == 345);
    check.test("check arg shared_ptr", sqb.getValue<int>("arg_shared_ptr") == 345);

    // Array
    sqb.executeString(R"(function testArray(ar) {return ar[1];})");
    auto testArray = sqb.getFunction("testArray");

    sqb::SQBArray ar(sqb.vm);
    ar.append(std::vector<int>({1,73,3,4,5}));
    check.test("array<int> set/get", ar[1] == 73 );
    check.test("array<int> send as argument", testArray(ar).ret<int>() == 73 );

    ar.clear();
    ar.append(std::vector<std::string>({"abc","def","ghj"}));
    check.test("array<string> set/get", ar[1] == "def" );
    check.test("array<string> send as argument", testArray(ar).ret<std::string>() == "def" );

    sqb::SQBArray ar1(sqb.vm, std::vector<int>({1,73,3,4,5}));
    check.test("array init on create(vector) set/get", ar1[2] == 3 );

    sqb::SQBArray ar2(sqb.vm, 5);
    ar2.append(36);
    check.test("array init on create(5) set/get", ar2[5] == 36 );


    sqb.bindClass<Base>("Base")
        .bindMethod("baseMethod", &Base::baseMethod);

    sqb.bindClass<Custom, Base>("Custom", "Base")
        .bindMethod("myMethod", &Custom::myMethod);

    sqb.executeString(R"(custom <- Custom();)");
    check.test("custom instanceof Base", sqb.executeString(R"(if (custom instanceof Base) iii <- 1; else iii <- 0;)")==0 && sqb.getValue<int>("iii")==1);
    check.test("custom has a Base method", sqb.executeString(R"(if ("baseMethod" in custom) bbb <- 1; else bbb <- 0;)")==0 && sqb.getValue<int>("bbb")==1);
    check.test("calling an inherited method", sqb.executeString(R"(cbase <- custom.baseMethod();)")==0 && sqb.getValue<std::string>("cbase")=="base");


    CustomType ct;
    ct.name = "Hello";
    ct.number = 42;
    ct.ar = {1,2,3};

    auto func = sqb.getFunction("customTypeFunction");
    CustomType ct2 = func(ct);
    //func(ct);

    check.test("custom type and call", ct2.name=="Hello world" && ct2.number==43 && ct2.ar == std::vector<int>({1,2,3,9}));

  }

  assert(check.all_tests_passed);
  return check.all_tests_passed ? 0:1;
}


