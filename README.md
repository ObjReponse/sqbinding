# SQBinding

C++11 binding to the scripting language [Squirrel](http://www.squirrel-lang.org /). Without any external dependencies except Squirrel itself.

This code was written as part of another major project. I thought some parts might be useful to other people.
I wanted to unify the C++ API and the API for writing scripts as much as possible. To make the same code in C++ and Squirrel look almost the same.

## Why Squirrel

At first, I used Lua. Objectively speaking, it's a great thing. But the syntax was completely unsuitable for my task. The existing bindings also did not solve all my needs.  Squirrel, with its classes, tables, and syntax, is much similar to C++.

The problem was that, once again, ready-made bindings did not provide what was needed.

**What can SQBinding do**

- Small size
- Easy embedding
- The simplest possible binding of everything
- Overloading of functions and methods
- Native support for smart pointers
- Search and call script objects
- Adding any custom types
- Transparent bidirectional transmission of any type
- Operators (I had to patch Squirrel here)

## What it looks like in a real project

The goal was to get a tool with which you can easily get the same API in C++ and Squirrel.

**Squirrel:**
```lua
od   <- ObjectDetection()
http <- HttpServer("0.0.0.0", 9876)

c <- Chain()
c << Demuxer(url, {rtsp_transport="tcp"}) << VIDEO << Decoder() 
  << Scale({width=640,format="bgr24"}) 
  << MotionDetector(od) 
  << Reemit({fps=1}) << Encoder("libx264", h264_par) 
  << Muxer({format="mpegts"}) << http.createResource("md")

c.start()
```

**C++:**
```cpp
ObjectDetection od;
HttpServer http("0.0.0.0", 9876);

Chain c;
c << make::Demuxer(url, {{"rtsp_transport","tcp"}}) << MTM::VIDEO << make::Decoder() 
  << make::Scale({{"width",640},{"format","bgr24"}}) 
  << make::MotionDetector(od) 
  << make::Reemit({{"fps",1}}) << make::Encoder("libx264", h264_par) 
  << make::Muxer({{"format","mpegts"}}) << http.createResource("md");

c.start();
```

The difference is minimal. This is exactly what was intended.

# Features

## Minimal examples

Here we call the Squirrel function from C++ and vice versa the C++ function from Squirrel.

```cpp
#include "sqbinding.h"

void hellocpp() {
  printf("hello C++\n");
}

int main(int argc, char **argv)
{
  sqb::SQBinding sqb;

  sqb.bindFunction("hellocpp", hellocpp);
  sqb.executeString("
    hellocpp()

    function hellosquirrel() {
      print("hello Squirrel\n")
    }
  ");

  auto hellosquirrel = sqb.getFunction("hellosquirrel");

  hellosquirrel();

  return 0;
}
```

Console output is disabled by default, you should have your own output function.


```cpp
void printfunc(HSQUIRRELVM v, const SQChar *s,...)
{
  va_list vl;
  va_start(vl, s);
  vfprintf(stdout, s, vl);
  fprintf(stdout, "\n");
  fflush(stdout);
  va_end(vl);
}

sqb::SQBinding sqb;
sq_setprintfunc(sqb.vm, printfunc, printfunc); // print func, error func
```

It is almost always better to run the code inside the try catch block, since SQBinding throws exceptions in case of errors.
In general, I recommend closing the entire code in a separate `{...}` block


```cpp

int main() {
  ...

  int result = 0;

  {
    sqb::SQBinding sqb;
    sq_setprintfunc(sqb.vm, printfunc, printfunc);

    try {
      result = sqb.executeFile("script.nut");
    }catch(const std::exception& e){
      fprintf(stderr, "Error %s : %s", e.what(), sqb::detail::getCurrentPosition(sq.sqb.vm).c_str());
      std::exit(EXIT_FAILURE);
    }

    result = Event::Loop();
  }

  ...

  return result;
}

```

Important!

All variables, declared types, classes will exist until we destroy `sqb`

This is convenient because you can call basic functions and classes from a file, then call another file that will use the already loaded data, or call small routines from a string.


## Binding of functions

There are two ways, through a pointer and through a lambda/functor.

```cpp

// 1. through the pointer
std::string func(std::string name) {
  return "Hello " + name;
}

sqb.bindFunction("func", func);

// 2. functors
std::function<std::string(std::string)> func;

func = [](std::string name) {
  return "Hello " + name;
};

sqb.bindFunction("func", func);

// 3. lambda
sqb.bindFunction("func", [](std::string name){
  return "Hello " + name;
});

```

Now you can call **func() from Squirrel.**

Often, when binding, the C++ API comes across, which contains many overloaded functions. This is exactly the trouble I'm facing. But unfortunately, SQBinding solves this simply.

```cpp
int test(int i) { return i; }
std::string test(std::string s) { return s; }

sqb.bindFunction("test", static_cast<int(*)(int)>(test));
sqb.bindFunction("test", static_cast<std::string(*)(std::string)>(test));
```

Yes, `static_cast` doesn't look good, you can write a macro to simplify it or some kind of wrapper, but this will be your decision.

The reverse situation is when there is a function in Squirrel that needs to be called from C++.

```lua
function func(name) {
  return "Hello " + name
}
```

First you need to execute the Squirrel code, then try to call the Squirrel function.

```cpp

sqb.executeFile("script.nut");
// or
sqb.executeString(scrip);

// Creating the SQBFunction object
auto func = sqb.getFunction("func");

// calling without taking the result
func("Ligverd");

// we automatically collect the result
std::string str = func("Ligverd");

// explicitly taking the result by specifying the type
std::string str = func("Ligverd").ret<std::string>(); 

```

## Working with variables

There are three main mechanisms: binding, taking, and obtaining meaning.

```cpp
std::string name;

sqb.bindValue("name", &name);
sqb.bindValue("name", &name, true); // read only

// get the value of a variable from Squirrel, if there is no variable, there will be an exception
auto value = sqb.getValue<std::string>("value");

// get if available
std::string value;
sqb.getValueIfExists("value", value);

// setting the value
std::string value = "val";
sqb.setValue("value", value);

```

## Custom types

All the basic scalar types are already described, but if you need to pass some type of your own, there is an easy way to add conversion to/from Squirrel.

```cpp

struct CustomType
{
  std::string name;
  int number;
  std::vector<int> ar;
};


namespace sqb {
namespace types {

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
```

I'll run a little ahead, if you have made a binding of the class, then you do not need to make such converters!_


SQBinding can automatically do a cast type, provided that you guarantee correctness. To do this, you need to explicitly specify which types can be explicitly or not explicitly converted to.

```cpp
types::Type::create<ClassType,  ClassType*, BaseClassType, BaseClassType*>(OT_INSTANCE);
```

In 99% of cases, this is not required. SQBinding itself manages type compatibility.

This may be necessary in some cases, for example, you have a base class, but you did not bind it, but only a child class was bound.
In order for a method or function that accepts the type of the base class to work correctly, you can create a manual compatibility table (as shown above)

## Working with tables as a separate type and as a namespace

Squirrel has no concept of structures, hashes, or namespaces, everything is replaced by tables.

all global functions, variables and types are stored in the root table, in fact, when we declare **sqb::SQBinding sqb;** we create an SQBTable object.

```cpp
sqb::SQBinding sqb; // <- this is the SQBTable on the Squirrel root_table

// create your own table in the roottable
auto my = sqb.newTable("my");

// create a free table in the roottable to which to bind and/or set variables and functions
std::string name;
int index;
int func(int i) {...}

sqb.newTable("my")
  .bindValue("name", &name)
  .bindValue("index", &index)
  .bindFunction("func", func)
  .bindFunction("lambda", [](int x) { return x*2; })
  .newTable("subtable")
    .bindValue("subname", &name)
  ;

```

В Squirrel это выглядит так

```lua
print(my.name)
my.index = 5;

my.func(8)
my.lambda(9)

my.subtable.name = "hello"
```

To get a table from Squirrel in C++, on the contrary

```cpp
sqb::SQBTable t(sqb.vm, sqb.find("my"));
```

In fact, **find** can be called for any `SQBinding` object, it returns `HSQOBJECT`, i.e. the native Squirrel type.


## Arrays

Array is the basic Squirrel type and is served by the SQBArray class

In most cases, it is not necessary to work with SQBArray specifically, it must be used either in pushValue/popValue converters or in rare cases of manual disassembly.


```cpp
// add a field in the table with the Array type
auto ar = sqb.newArray("name");
ar.append(1)
  .append(2)
  .append("Hello")
  .append(object)
  ;

// create an array in your table
auto ar = sqb.newTable("my").newArray("name");
```

You can also create a separate array without bindings, i.e. it won't be on the Squirrel stack.

```cpp
sqb::SQBArray ar(sqb.vm);

// add a data collection immediately
ar.append(std::vector<int>({1,73,3,4,5}));

// adding one element at a time
ar.append(42);

// accessing the index
ar[1] = "hello";

// conversion to type automatically
std::string s = ar[1];

// explicit type conversion
std::string s = ar[a].as<std::string>;

// number of elements
ar.size();

// clear the array
ar.clear();

```

In Squirrel, elements of the same array can have a mixed type. But the mono type is more often used. To do this, you can use the conversion.

```cpp
sqb::SQBArray ar(sqb.vm);
ar.append(std::vector<int>({1,73,3,4,5}));

auto v = ar.to_vector<int>();
```

As I did in C++11, there is no `std::variant` in it, but you can use C++17 and higher or write your own type analog `variant`

in this case, you will get more convenience.

```cpp
sqb::SQBArray ar(sqb.vm);

using Type = std::variant<int, std::string>;

std::vector<Type> v = {1, 2, "hello"};

ar.append(v);

auto v2 = ar.to_vector<Type>();

```

But as I said, you need to convert there more often./here is a structure that is more convenient to do via popValue/pushValue


## Classes

In some ways, working with classes is somewhat like working with tables.

Binding is supported:
- Class
- Inheritance
- Method (via pointer, via lambda)
- Static method (via pointer, via lambda)
- Property (via pointer, via setter/getter)
- Overloading constructors and methods

What is not supported by SQBinding due to Squirrel limitations:
- Static properties

It is common practice to bind two static setter/getter methods, in Squirrel it may look like this.

```lua
local obj = Object()

obj.setStaticName(name)
name = obj.getStaticName()
```
Yeah, I don't like it either. If I get my hands on it, maybe I'll make a patch for Squirrel, but I don't even know how much work is being done for this yet.

I have not allocated anything separate for the `enum`, and I usually do this

```cpp
enum Type {
  UNKNOWN,
  MOUSE,
  KEYBOARD,
  PRINTER
};

// define in the roottable
sqb.setValue("UNKNOWN",  Type::UNKNOWN);
sqb.setValue("MOUSE",    Type::MOUSE);
sqb.setValue("KEYBOADR", Type::KEYBOARD);
sqb.setValue("PRINTER",  Type::PRINTER);

// or wrap it in a table
sqb.newTable("Type")
   .setValue("UNKNOWN",  Type::UNKNOWN)
   .setValue("MOUSE",    Type::MOUSE)
   .setValue("KEYBOADR", Type::KEYBOARD)
   .setValue("PRINTER",  Type::PRINTER)
   ;

```

### Class binding


```cpp
class Base
{
public:
  int  id;
  Type type;

  std::string name;

  Base(Type type = UNKNOWN) : id(-1), type(type) {}
  Base(int id, Type type = UNKNOWN) : id(id), type(type) {}

  int getID() { return id; }

  std::string method() { return "ok"; }
  std::string method(int i) { return std::to_string(i); }
  std::string method(std::string n) { return n; }

  Base operator+(int i) { ... }

  static void resetAll() {}
  static int version;
};


sqb.bindClass<Base>("Base")
   .bindConstructor()
   .bindConstructor<Type>()
   .bindConstructor<int>()
   .bindConstructor<int, Type>(),
   .bindConstructor([](std::string x){ return new Base( std::stoi(x) ); }) // <-- non-existent custom constructor
   .bindMethod("getID", &Base::getID)
   .bindMethod("method", static_cast<std::string(Base::*)()>(&Test::method))
   .bindMethod("method", static_cast<std::string(Base::*)(int)>(&Test::method))
   .bindMethod("method", static_cast<std::string(Base::*)(std::string)>(&Test::method))
   .bindMethod("incID", [](Base *self){ self->id++; }) // <-- a custom method that does not exist
   .bindStaticMethod("resetAll", &Base::resetAll)
   .bindProp("id", &Base::id)
   .bindProp("type", &Base::type, true) // <-- true - readonly
   .bindProp("next", [](Base *self){ return self->id+1; }) // <-- getter only
   .bindProp("name", [](Base *self){ return self->name; }, [](Base *self, std::string n){ self->name = n; })
   .bindStaticMethod("getStaticVersion", [](){ return Base::version })
   .bindStaticMethod("setStaticVersion", [](int v){ Base::version = v; })
   .bindMethod("_add", &Base::operator+)

```

As I wrote above with the static ambush property, but they are rare.

'_add` is the metomethodes of Squirrel itself

**Basic metomethodes**

```
_add
_sub
_mul
_div
_unm
_modulo
_set
_get
_typeof
_nexti
_cmp
_call
_cloned
_newslot
_delslot
_tostring
_newmember
_inherited
```

**Those that are available after the patch** 01_meta_method.patch


```
_shiftl
_shiftr
_and
_or
```

### Inheritance

Everything is the same here as in any OOP, as I wrote above, type compatibility will be respected automatically.

```cpp
class Device : public Base {...};

sqb.bindClass<Device, Base>("Device", "Base")
  ...
  ;

```

Squirrel stores a pointer to a class, like a regular raw pointer, which is not always convenient. For example, we create a class in Squirrel that takes over C++ and vice versa.

In these cases, it is convenient to use std::shared_ptr / std::unique_ptr / or even some kind of custom ptr.

I hasten to reassure you that you will not have to do anything, do the binding **in the same way** you just need to call the smart method

```cpp
sqb.bindClass<Base>("Base")
   .smartSharedPtr()
   ... then its the same as in the example above

```

SQBInding will create std::shared_ptr<Base> and give it to Squirrel.

In fact, the smartSharedPtr() method is a wrapper as the most requested case.

What does it look like in the general case?


```cpp
sqb.bindClass<Base>("Base")
   .smart(
     [](Base* c) -> SQUserPointer { return new std::shared_ptr<Base>(c); },
     [](SQUserPointer p) -> Base* { return static_cast<std::shared_ptr<Base>*>(p)->get(); }
     )

```

The first argument is the container creation and object placement functor, the second argument is the object extraction functor from the container.

I specifically made it possible to use any container, not just the std::stared compatible ones.

### Important: pointers in methods are bound via lambdas and external functions

When a method is bound using a lambda expression, it always receives a raw pointer "T", regardless of whether the class is registered as a regular instance,
through "smartSharedPtr()" or through a custom "smart()".


```cpp
// It works the same for a regular class and for a class via shared_ptr !!!
.bindMethod("transform", [](Base* self, int i) {
    self->id = i;
})
```

But when passing an object to an external function, the type of pointer being passed depends on how the class is registered.:


```lua
local p <- Base("test")
externalFunction(p) -- passed to Base* (raw pointer)

local psp <- BaseSharedPtr("test")
externalFunction(psp) -- passed to std::shared_ptr<Base>

local pcc <- BaseCustomContainer("test")
externalFunction(pcc) -- passes the custom container specified in smart() MyPtr<Base> etc
```

Accordingly, the C++ function must be declared for the desired type.:

```cpp
// For a regular instance
void externalFunction(Base* p);

// For shared_ptr
void externalFunction(std::shared_ptr<Base> p);

// For a custom container
void externalFunction(MyPtr<Base> p);
```

## Building and running the example

The repository is self-sufficient: it downloads Squirrel, builds it statically, compiles SQBinding and runs tests.

```bash
cd example
mkdir .build && cd .build
cmake ..
make
./test
```

Test output:

```
>>> hello from script as string
[PASS ] execute script from string
[PASS ] execute script from file
[PASS ] bind function
[PASS ] bind table var
[PASS ] bind table function
[PASS ] bind multi(int)
[PASS ] bind multi(int, string)
[PASS ] bind static method
[PASS ] bind static prop
[PASS ] bind operator<<
[PASS ] bind operator|
[PASS ] check cross var
[PASS ] check object shared_ptr
[PASS ] check arg shared_ptr
[PASS ] array<int> set/get
[PASS ] array<int> send as argument
[PASS ] array<string> set/get
[PASS ] array<string> send as argument
[PASS ] array init on create(vector) set/get
[PASS ] array init on create(5) set/get
[PASS ] custom instanceof Base
[PASS ] custom has a Base method
[PASS ] calling an inherited method
[PASS ] custom type and call
```

The full code of the example and tests is in `example/test.cpp`.
