--[[
    City Siege Addon - Minimap Button
    Creates and manages the minimap button using LibDBIcon
]]

CitySiege_MinimapButton = {}
local MinimapButton = CitySiege_MinimapButton

local LDB = LibStub("LibDataBroker-1.1", true)
local icon = LibStub("LibDBIcon-1.0", true)

function MinimapButton:Initialize()
    if not LDB or not icon then
        CitySiege_Utils:Print("LibDataBroker or LibDBIcon not found. Minimap button disabled.")
        return
    end
    
    -- Create LibDataBroker object
    self.dataObject = LDB:NewDataObject("CitySiege", {
        type = "launcher",
        text = "City Siege",
        icon = CitySiege_Icons.MINIMAP,
        OnClick = function(clickedframe, button)
            MinimapButton:OnClick(button)
        end,
        OnTooltipShow = function(tooltip)
            MinimapButton:OnTooltipShow(tooltip)
        end,
    })
    
    -- Register with LibDBIcon
    icon:Register("CitySiege", self.dataObject, CitySiege_Config:GetMinimapSettings())
    
    -- Show/hide based on settings
    if CitySiege_Config:GetMinimapSettings().hide then
        icon:Hide("CitySiege")
    else
        icon:Show("CitySiege")
    end
end

function MinimapButton:OnClick(button)
    if button == "LeftButton" then
        -- Left click: Toggle main frame
        if CitySiege_MainFrame then
            CitySiege_MainFrame:Toggle()
        end
    elseif button == "RightButton" then
        -- Right click: Open settings
        if CitySiege_SettingsPanel then
            CitySiege_SettingsPanel:Show()
        end
    elseif button == "MiddleButton" then
        -- Middle click: Request status update
        if CitySiege_SiegeTracker then
            CitySiege_SiegeTracker:RequestStatusUpdate()
        end
    end
end

function MinimapButton:OnTooltipShow(tooltip)
    if not tooltip then return end
    
    tooltip:AddLine("|cFF16C3F2City Siege|r")
    tooltip:AddLine(" ")
    tooltip:AddDoubleLine("|cFFFFFFFF Left-Click:|r", "Toggle main window")
    tooltip:AddDoubleLine("|cFFFFFFFF Right-Click:|r", "Open settings")
end

function MinimapButton:Show()
    if icon then
        icon:Show("CitySiege")
    end
end

function MinimapButton:Hide()
    if icon then
        icon:Hide("CitySiege")
    end
end

function MinimapButton:Lock()
    if icon then
        icon:Lock("CitySiege")
    end
end

function MinimapButton:Unlock()
    if icon then
        icon:Unlock("CitySiege")
    end
end

function MinimapButton:SetIcon(iconPath)
    if self.dataObject then
        self.dataObject.icon = iconPath
    end
end

function MinimapButton:UpdateTooltip()
    -- Force tooltip update
    if icon then
        icon:Refresh("CitySiege", CitySiege_Config:GetMinimapSettings())
    end
end
