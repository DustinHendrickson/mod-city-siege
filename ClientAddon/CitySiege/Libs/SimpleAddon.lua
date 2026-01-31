--[[
    SimpleAddon - Lightweight addon framework for City Siege
    Replaces Ace3 with a minimal implementation
]]

local MAJOR, MINOR = "SimpleAddon-1.0", 1
local lib = LibStub:NewLibrary(MAJOR, MINOR)

if not lib then return end

lib.addons = lib.addons or {}

-- Simple addon registration
function lib:NewAddon(name)
    local addon = {
        name = name,
        modules = {},
        events = {},
        db = {},
    }
    
    -- Event handling
    addon.frame = CreateFrame("Frame")
    
    function addon:RegisterEvent(event)
        self.frame:RegisterEvent(event)
    end
    
    function addon:UnregisterEvent(event)
        self.frame:UnregisterEvent(event)
    end
    
    function addon:UnregisterAllEvents()
        self.frame:UnregisterAllEvents()
    end
    
    -- Frame event handler
    addon.frame:SetScript("OnEvent", function(frame, event, ...)
        if addon[event] then
            addon[event](addon, event, ...)
        end
    end)
    
    lib.addons[name] = addon
    return addon
end

-- Get addon
function lib:GetAddon(name)
    return lib.addons[name]
end
