# SQBinding

C++11 binding for the scripting language [Squirrel](http://www.squirrel-lang.org/). With no external dependencies except Squirrel itself.

I was writing for my project, where I wanted to make the C++ API and the scripting API as similar as possible. To make the same code in C++ and Squirrel look almost the same.

## Why Squirrel and why your own binding

Initially, I used Lua. Objectively, it's a great thing. But syntax and the stack machine created a permanent gap between C++ and scripting logic. Squirrel, with its classes, tables, and C++-like syntax, is much more natural.

The problem was that ready—made bindings did not provide what was needed.:
- **There is no overload.** If you want `doSomething(int)` and `doSomething(string)`, produce `doSomethingInt`, `doSomethingStr'.
- **There are no operators.** And the project actively uses `<<`, `|` and others.
- **Poor operation with smart pointers.** Passing `shared_ptr` to the script and back is another story.
- **There is no convenient registration of user types.** So that your type can be converted back and forth as a table, without manually parsing the stack.

SQBinding closes it all.

## How it works in a real project

The goal was to make the same pipeline in C++ and Squirrel look the same. Here is a real example:

**Squirrel:**
```lua
od   <- ObjectDetection()
http <- HttpServer("0.0.0.0", 9876)

c <- Chain()
c << Demuxer(url, {rtsp_transport="tcp"}) << VIDEO << Decoder() << Scale({width=640,format="bgr24"}) << MotionDetector(od) << Reemit({fps=1}) << Encoder("libx264", h264_par) << Muxer({format="mpegts"}) << http.createResource("md")
c.start()
```

**C++:**
```cpp
ObjectDetection od;
HttpServer http("0.0.0.0", 9876);

Chain c;
c << make::Demuxer(url, {{"rtsp_transport","tcp"}}) << MTM::VIDEO << make::Decoder() << make::Scale({{"width",640},{"format","bgr24"}}) << make::MotionDetector(od) << make::Reemit({{"fps",1}}) << make::Encoder("libx264", h264_par) << make::Muxer({{"format","mpegts"}}) << http.createResource("md");

c.start();
```

The difference is minimal. That's exactly what was intended.

## Features

### Global functions and data tables

Binding global functions, variables, and entire tables with their own methods in a few lines.

```cpp
int func_s(std::string name, int id)
{
  return name + "_" + std::to_string(id);
}

sqb.bindFunction("func_s", func_s);

sqb.bindFunction("make_l", [](std::string name, int id)
{
  return name + "_" + std::to_string(id);
});

std::string app_name = "MyApp";
int version = 1;

sqb.newTable("config")
    .bindValue("name", &app_name)
    .bindValue("version", &version)
    .bindFunction("format", [](const std::string& s) {
        return "[" + s + "]";
    });
```

```lua
local id1 <- make_s("object", 42)    -- "object_42"
local id2 <- make_l("object", 42)    -- "object_42"

config.name = "NewName"
config.version = 2
local tag <- config.format("release") -- "[release]"
```

### Classes: constructors and method overloads

Class registration is a chain of calls that does not depend on how the object is stored: by value, via `shared_ptr` or in a custom container. The syntax is the same.

```cpp
// Regular instance
sqb.bindClass<Printer>("Printer")
    .bindConstructor<std::string>()
    .bindMethod("print", static_cast<std::string(Printer::*)(int)>(&Printer::print))
    .bindMethod("print", static_cast<std::string(Printer::*)(const std::string&)>(&Printer::print));

// The same class via shared_ptr — differs only in the presence of .smartSharedPtr()
sqb.bindClass<Printer>("PrinterSP")
    .smartSharedPtr()
    .bindConstructor<std::string>()
    .bindMethod("print", static_cast<std::string(Printer::*)(int)>(&Printer::print))
    .bindMethod("print", static_cast<std::string(Printer::*)(const std::string&)>(&Printer::print));
```

```lua
local p <- Printer("Device")
p.print(100)           -- "Device: code=100"
p.print("error")       -- "Device: msg=error"

local psp <- PrinterSP("Server")
psp.print(200)         -- "Server: code=200"
```

### Properties and operators

Class properties are bound directly to C++ fields. Operators are used through special methods `_shiftl`, `_or` and others.

```cpp
sqb.bindClass<Processor>("Processor")
    .bindConstructor<std::string>()
    .bindProp("tag", &Processor::tag)
    .bindProp("counter", &Processor::counter, true)   // read-only
    .bindMethod("_shiftl", [](Processor* self, int value) {
        self->counter += value;
        return self;
    })
    .bindMethod("_or", [](Processor* self, const std::string& suffix) {
        self->tag += suffix;
        return self;
    });
```

```lua
local proc <- Processor("init")
proc << 10
proc << 5
proc.counter            -- 15

proc | "-final"
proc.tag                -- "init-final"
```

### Important: pointers in lambdas and external functions

When a method is bound via a lambda, it **always* receives a raw pointer `T*`, regardless of whether the class is registered as a regular instance, through `smartSharedPtr()` or through a custom `smart()'.

```cpp
// It works the same for a regular class and for a class via shared_ptr
.bindMethod("transform", [](Processor* self, int factor) {
    self->counter *= factor;
    return self;
})
```

But when passing an object to an external function, the type of pointer being passed depends on how the class is registered.:

```lua
local p <- Processor("test")
externalFunction(p) -- passed to Processor* (raw pointer)

local psp <- ProcessorSP("test")
externalFunction(psp) -- passed to std::shared_ptr<Processor>

local pcu <- ProcessorCU("test")
externalFunction(pcu) -- passes the custom container specified in smart()
```

Accordingly, the C++ function must be declared for the desired type.:

```cpp
// For a regular instance
void externalFunction(Processor* p);

// For shared_ptr
void externalFunction(std::shared_ptr<Processor> p);

// For a custom container
void externalFunction(const MyPtr<Processor>& p);
```

### Smart pointers and custom containers

`smartSharedPtr()` is a ready—made wrapper for `std::shared_ptr`. If a different container is used, there is a common `smart()` method.:

```cpp
// shared_ptr — one call
.bindClass<Resource>("Resource")
    .smartSharedPtr()
    .bindConstructor<std::string>()
    .bindMethod("info", &Resource::info);

// Any other container — via smart with extract and wrap functions
.bindClass<Resource>("ResourceCustom")
    .smart(
        [](Resource* p) { return new MyPtrContainer(p); },
        [](SQUserPointer* p) { return static_cast<MyPtrContainer<MyPtrContainer>*>(p)->get() }
    )
    .bindConstructor<std::string>()
    .bindMethod("info", &Resource::info);
```

### Inheritance

C++ hierarchies are saved in scripts: `instanceof', access to the methods of the base class — everything works.

```cpp
sqb.bindClass<Device>("Device")
    .bindMethod("status", &Device::status);

sqb.bindClass<Sensor, Device>("Sensor", "Device")
    .bindMethod("read", &Sensor::read);
```

```lua
local s <- Sensor()
s instanceof Device -- true
local st <- s.status() -- base class method
local val <- s.read() -- the method of the Sensor itself
```

### Arrays

Direct work with `std::vector` and other collections via `SQBArray`:

```cpp
// Creation from
the sqb vector::SQBArray arr(vm, std::vector<std::string>({"alpha", "beta", "gamma"}));

// Calling a script function with an array
auto process = sqb.getFunction("process_list");
std::string result = process(arr).ret<std::string>();  // "beta"
```

```lua
function process_list(list) {
    return list[1]; -- indexing from 0
}
```

### Custom types

For your own types, it is enough to specialize `popValue` and `pushValue`:

```cpp
struct ConfigEntry {
    std::string key;
    int value;
    std::vector<std::string> tags;
};

namespace sqb { namespace types {

template<>
inline ConfigEntry popValue<ConfigEntry>(HSQUIRRELVM vm, SQInteger idx) {
    SQBTable t = popValue<SQBTable>(vm, idx);
    ConfigEntry e;
    e.key   = t.getValue<std::string>("key");
    e.value = t.getValue<int>("value");
    e.tags  = t.getArray("tags").to_vector<std::string>();
    return e;
}

template<>
inline void pushValue<ConfigEntry>(HSQUIRRELVM vm, const ConfigEntry& e) {
    SQBTable t(vm);
    t.setValue("key", e.key);
    t.setValue("value", e.value);
    SQBArray ar(vm); ar.append(e.tags);
    t.setValue("tags", ar);
    t.push();
}
}}

// Using
sqb.bindFunction("updateConfig", [](ConfigEntry e) {
    e.value += 10;
    e.tags.push_back("modified");
    return e;
});
```

```lua
local cfg <- {
    key = "timeout",
    value = 100,
    tags = ["network", "critical"]
}
local updated <- updateConfig(cfg)
-- updated.value == 110
-- updated.tags == ["network", "critical", "modified"]
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
