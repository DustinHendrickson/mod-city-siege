--[[
    SimpleDataBroker - Minimal LibDataBroker-1.1 implementation
]]

local MAJOR, MINOR = "LibDataBroker-1.1", 1
local lib = LibStub:NewLibrary(MAJOR, MINOR)

if not lib then return end

lib.objects = lib.objects or {}
lib.callbacks = lib.callbacks or {}

function lib:NewDataObject(name, dataobj)
    if not name or self.objects[name] then return end
    
    self.objects[name] = dataobj or {}
    return self.objects[name]
end

function lib:GetDataObjectByName(name)
    return self.objects[name]
end

function lib:DataObjectIterator()
    return pairs(self.objects)
end
