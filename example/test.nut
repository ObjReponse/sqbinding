
my.name = "Hello"
my.i = 100

i88 <- my.func(44)
stest5 <- f_str_int("test",5)

Test.staticMethod(12)
TestStatic.static_prop = 567

local c = Test("Hello")
smulti1 <- c.multi(7)
smulti2 <- c.multi(8,"data")

local test = Test(44)
test << 6
opshiftl <- test.idx

c | " Ligverd"
opor <- c.name

c | " and" | " Lana"
oporor <- c.name

function customTypeFunction(t) {
  t.name += " " + "world"
  t.number += 1
  t.ar.append(9)

  return t
}

p <- TestP(345)
idx_TestP <- p.idx

arg_shared_ptr <- testArgPtr(p)