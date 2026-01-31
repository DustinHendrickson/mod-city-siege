--[[
    SimpleDB - Lightweight database for saved variables
    Replaces AceDB with minimal implementation
]]

local MAJOR, MINOR = "SimpleDB-1.0", 1
local lib = LibStub:NewLibrary(MAJOR, MINOR)

if not lib then return end

function lib:New(name, defaults)
    -- Initialize saved variable if it doesn't exist
    if not _G[name] then
        _G[name] = {}
    end
    
    local db = _G[name]
    
    -- Apply defaults
    if defaults then
        if defaults.profile and not db.profile then
            db.profile = {}
            for k, v in pairs(defaults.profile) do
                db.profile[k] = type(v) == "table" and CitySiege_Utils:CopyTable(v) or v
            end
        end
        if defaults.global and not db.global then
            db.global = {}
            for k, v in pairs(defaults.global) do
                db.global[k] = type(v) == "table" and CitySiege_Utils:CopyTable(v) or v
            end
        end
    end
    
    return db
end
