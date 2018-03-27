
local self = ...

print "Hello world"

print(self)
print(self.PropStr)
self.PropStr = "test"
print(self.PropStr)
local loc = self:K2_GetActorLocation()
print(loc)
print(loc.X)
print(loc.Y)
print(loc.Z)
self:testfunc(nil, 1, loc)