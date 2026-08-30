--[[
    City Siege Addon - Minimap Button

    A self-contained Blizzard-style minimap button. The addon previously required
    LibDataBroker-1.1 and LibDBIcon-1.0, neither of which ships with it, so the
    button never appeared at all. LibDBIcon is still used when another addon has
    already loaded it; otherwise this draws its own.
]]

CitySiege_MinimapButton = {}
local MinimapButton = CitySiege_MinimapButton

local LDB = LibStub and LibStub("LibDataBroker-1.1", true)
local LDBIcon = LibStub and LibStub("LibDBIcon-1.0", true)

local button = nil
local BUTTON_RADIUS = 80

-- ---------------------------------------------------------------------------
-- Placement
-- ---------------------------------------------------------------------------

local function GetSavedAngle()
    local settings = CitySiege_Config:GetMinimapSettings()
    return settings and settings.minimapPos or 225
end

local function UpdatePosition(angle)
    if not button then return end

    button:ClearAllPoints()
    button:SetPoint("CENTER", Minimap, "CENTER",
        BUTTON_RADIUS * cos(angle), BUTTON_RADIUS * sin(angle))
end

--- Follows the cursor around the minimap edge while dragging.
local function OnDragUpdate(self)
    local mx, my = Minimap:GetCenter()
    local px, py = GetCursorPosition()
    local scale = Minimap:GetEffectiveScale()

    px, py = px / scale, py / scale

    local angle = math.deg(math.atan2(py - my, px - mx))
    UpdatePosition(angle)

    CitySiege_Config:SetMinimapPosition(angle)
end

-- ---------------------------------------------------------------------------
-- Construction
-- ---------------------------------------------------------------------------

local function BuildButton()
    button = CreateFrame("Button", "CitySiegeMinimapButton", Minimap)
    button:SetSize(31, 31)
    button:SetFrameStrata("MEDIUM")
    button:SetFrameLevel(8)
    button:RegisterForClicks("AnyUp")
    button:RegisterForDrag("LeftButton")
    button:SetMovable(true)

    -- Icon, trimmed so the stock icon border does not show through the ring.
    local icon = button:CreateTexture(nil, "BACKGROUND")
    icon:SetSize(20, 20)
    icon:SetPoint("TOPLEFT", 7, -6)
    icon:SetTexture(CitySiege_Icons.MINIMAP)
    icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)
    button.icon = icon

    -- The standard tracking ring every Blizzard minimap button uses.
    local border = button:CreateTexture(nil, "OVERLAY")
    border:SetSize(53, 53)
    border:SetPoint("TOPLEFT", 0, 0)
    border:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")

    button:SetHighlightTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight")

    button:SetScript("OnDragStart", function(self)
        local settings = CitySiege_Config:GetMinimapSettings()
        if settings and settings.lock then return end

        self:SetScript("OnUpdate", OnDragUpdate)
    end)

    button:SetScript("OnDragStop", function(self)
        self:SetScript("OnUpdate", nil)
    end)

    button:SetScript("OnClick", function(self, mouseButton)
        MinimapButton:OnClick(mouseButton)
    end)

    button:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_LEFT")
        MinimapButton:BuildTooltip(GameTooltip)
        GameTooltip:Show()
    end)

    button:SetScript("OnLeave", function() GameTooltip:Hide() end)

    UpdatePosition(GetSavedAngle())
end

function MinimapButton:Initialize()
    -- Prefer LibDBIcon when some other addon has already provided it, so the
    -- button lands in the user's usual place with their usual settings.
    if LDB and LDBIcon then
        self.dataObject = LDB:NewDataObject("CitySiege", {
            type = "launcher",
            text = "City Siege",
            icon = CitySiege_Icons.MINIMAP,
            OnClick = function(_, mouseButton) MinimapButton:OnClick(mouseButton) end,
            OnTooltipShow = function(tooltip) MinimapButton:BuildTooltip(tooltip) end,
        })

        LDBIcon:Register("CitySiege", self.dataObject, CitySiege_Config:GetMinimapSettings())
        self.usingLib = true
    else
        BuildButton()
    end

    if CitySiege_Config:IsMinimapShown() then
        self:Show()
    else
        self:Hide()
    end
end

-- ---------------------------------------------------------------------------
-- Behaviour
-- ---------------------------------------------------------------------------

function MinimapButton:OnClick(mouseButton)
    if mouseButton == "RightButton" then
        if CitySiege_SettingsPanel then
            CitySiege_SettingsPanel:Toggle()
        end
    else
        if CitySiege_MainFrame then
            CitySiege_MainFrame:Toggle()
        end
    end
end

function MinimapButton:BuildTooltip(tooltip)
    if not tooltip then return end

    tooltip:AddLine("City Siege")

    local active = CitySiege_SiegeTracker and CitySiege_SiegeTracker:GetAllSieges() or {}
    local count = 0
    for cityID, siege in pairs(active) do
        local cityData = CitySiege_CityData[cityID]
        if cityData then
            count = count + 1
            local remaining = tonumber(siege.remaining) or 0
            if siege.syncTime then
                remaining = math.max(0, remaining - (GetTime() - siege.syncTime))
            end
            tooltip:AddDoubleLine(
                CitySiege_GetCityColorString(cityID) .. cityData.displayName .. "|r",
                CitySiege_Utils:FormatTime(remaining), 1, 1, 1, 1, 1, 1)
        end
    end

    if count == 0 then
        tooltip:AddLine("No siege in progress.", 0.6, 0.6, 0.6)
    end

    tooltip:AddLine(" ")
    tooltip:AddLine("Left-click to open the siege window.", 0.8, 0.8, 0.8)
    tooltip:AddLine("Right-click for settings.", 0.8, 0.8, 0.8)
    tooltip:AddLine("Drag to move around the minimap.", 0.8, 0.8, 0.8)
end

function MinimapButton:Show()
    CitySiege_Config:SetMinimapHidden(false)

    if self.usingLib then
        LDBIcon:Show("CitySiege")
    elseif button then
        button:Show()
    end
end

function MinimapButton:Hide()
    CitySiege_Config:SetMinimapHidden(true)

    if self.usingLib then
        LDBIcon:Hide("CitySiege")
    elseif button then
        button:Hide()
    end
end

function MinimapButton:Toggle()
    if CitySiege_Config:IsMinimapShown() then
        self:Hide()
    else
        self:Show()
    end
end

function MinimapButton:Lock()
    CitySiege_Config:SetMinimapLocked(true)
    if self.usingLib then LDBIcon:Lock("CitySiege") end
end

function MinimapButton:Unlock()
    CitySiege_Config:SetMinimapLocked(false)
    if self.usingLib then LDBIcon:Unlock("CitySiege") end
end
