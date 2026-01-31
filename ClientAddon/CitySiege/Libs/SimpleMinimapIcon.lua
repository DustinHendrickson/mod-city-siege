--[[
    SimpleMinimapIcon - Minimal minimap button implementation
    Replaces LibDBIcon-1.0
]]

local MAJOR, MINOR = "LibDBIcon-1.0", 1
local lib = LibStub:NewLibrary(MAJOR, MINOR)

if not lib then return end

lib.buttons = lib.buttons or {}

local function UpdatePosition(button, settings)
    local angle = math.rad(settings.minimapPos or 220)
    local x = math.cos(angle) * 80
    local y = math.sin(angle) * 80
    button:SetPoint("CENTER", Minimap, "CENTER", x, y)
end

function lib:Register(name, dataObj, settings)
    if self.buttons[name] then return end
    
    local button = CreateFrame("Button", "CitySiegeMinimapButton_" .. name, Minimap)
    button:SetSize(31, 31)
    button:SetFrameStrata("MEDIUM")
    button:SetFrameLevel(8)
    
    -- Icon
    local icon = button:CreateTexture(nil, "BACKGROUND")
    icon:SetSize(20, 20)
    icon:SetPoint("CENTER")
    icon:SetTexture(dataObj.icon)
    button.icon = icon
    
    -- Border
    local overlay = button:CreateTexture(nil, "OVERLAY")
    overlay:SetSize(53, 53)
    overlay:SetPoint("TOPLEFT")
    overlay:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")
    
    -- Click handler
    button:RegisterForClicks("LeftButtonUp", "RightButtonUp", "MiddleButtonUp")
    button:SetScript("OnClick", function(self, btn)
        if dataObj.OnClick then
            dataObj.OnClick(self, btn)
        end
    end)
    
    -- Tooltip
    button:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_LEFT")
        if dataObj.OnTooltipShow then
            dataObj.OnTooltipShow(GameTooltip)
        end
        GameTooltip:Show()
    end)
    
    button:SetScript("OnLeave", function(self)
        GameTooltip:Hide()
    end)
    
    -- Dragging
    button:RegisterForDrag("LeftButton")
    button:SetScript("OnDragStart", function(self)
        if not settings.lock then
            self:LockHighlight()
            self.isMoving = true
        end
    end)
    
    button:SetScript("OnDragStop", function(self)
        self:UnlockHighlight()
        self.isMoving = false
    end)
    
    button:SetScript("OnUpdate", function(self)
        if self.isMoving then
            local mx, my = Minimap:GetCenter()
            local px, py = GetCursorPosition()
            local scale = Minimap:GetEffectiveScale()
            px, py = px / scale, py / scale
            
            local angle = math.deg(math.atan2(py - my, px - mx))
            settings.minimapPos = angle
            UpdatePosition(button, settings)
        end
    end)
    
    -- Store
    self.buttons[name] = button
    button.dataObj = dataObj
    button.settings = settings
    
    UpdatePosition(button, settings)
    
    if not settings.hide then
        button:Show()
    else
        button:Hide()
    end
    
    return button
end

function lib:Hide(name)
    local button = self.buttons[name]
    if button then
        button:Hide()
    end
end

function lib:Show(name)
    local button = self.buttons[name]
    if button then
        button:Show()
    end
end

function lib:Lock(name)
    local button = self.buttons[name]
    if button then
        button.settings.lock = true
    end
end

function lib:Unlock(name)
    local button = self.buttons[name]
    if button then
        button.settings.lock = false
    end
end

function lib:Refresh(name, settings)
    local button = self.buttons[name]
    if button and settings then
        button.settings = settings
        UpdatePosition(button, settings)
    end
end
