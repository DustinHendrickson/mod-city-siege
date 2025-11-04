--[[
    City Siege Addon - Settings Panel
    Provides interface options for configuring the addon
]]

CitySiege_SettingsPanel = {}
local SettingsPanel = CitySiege_SettingsPanel

local frame = nil

function SettingsPanel:Create()
    if frame then 
        -- Frame already exists, just return it
        return frame
    end
    
    frame = CreateFrame("Frame", "CitySiegeSettingsPanel", UIParent)
    frame:SetSize(500, 600)
    frame:SetPoint("CENTER")
    frame:SetFrameStrata("DIALOG")
    frame:EnableMouse(true)
    frame:SetMovable(true)
    frame:RegisterForDrag("LeftButton")
    frame:SetScript("OnDragStart", frame.StartMoving)
    frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
    frame:Hide()
    
    -- Background
    CitySiege_Utils:SetBackdrop(frame, 0, 0, 0, 0.9)
    
    -- Title
    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", 0, -15)
    title:SetText("|cFF16C3F2City Siege|r Settings")
    
    -- Close button
    local closeBtn = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
    closeBtn:SetPoint("TOPRIGHT", -5, -5)
    closeBtn:SetScript("OnClick", function() frame:Hide() end)
    
    -- Scroll frame for settings
    local scrollFrame = CreateFrame("ScrollFrame", nil, frame, "UIPanelScrollFrameTemplate")
    scrollFrame:SetPoint("TOPLEFT", 10, -50)
    scrollFrame:SetPoint("BOTTOMRIGHT", -30, 50)
    
    local content = CreateFrame("Frame", nil, scrollFrame)
    content:SetSize(450, 1200)
    scrollFrame:SetScrollChild(content)
    
    local yOffset = -10
    
    -- Minimap Settings
    yOffset = self:AddSectionHeader(content, "Minimap Button", yOffset)
    
    frame.minimapHide = self:AddCheckbox(content, "Hide minimap button", yOffset, function(checked)
        CitySiege_Config:SetMinimapHidden(checked)
        if checked then
            CitySiege_MinimapButton:Hide()
        else
            CitySiege_MinimapButton:Show()
        end
    end)
    yOffset = yOffset - 30
    
    frame.minimapLock = self:AddCheckbox(content, "Lock minimap button", yOffset, function(checked)
        CitySiege_Config:SetMinimapLocked(checked)
        if checked then
            CitySiege_MinimapButton:Lock()
        else
            CitySiege_MinimapButton:Unlock()
        end
    end)
    yOffset = yOffset - 40
    
    -- UI Settings
    yOffset = self:AddSectionHeader(content, "User Interface", yOffset)
    
    frame.showInCombat = self:AddCheckbox(content, "Show UI in combat", yOffset, function(checked)
        CitySiege_Config:SetShowInCombat(checked)
    end)
    yOffset = yOffset - 30
    
    frame.autoHide = self:AddCheckbox(content, "Auto-hide when leaving siege zone", yOffset, function(checked)
        CitySiege_Config:GetUISettings().autoHide = checked
    end)
    yOffset = yOffset - 30
    
    frame.uiLocked = self:AddCheckbox(content, "Lock main window", yOffset, function(checked)
        CitySiege_Config:SetUILocked(checked)
    end)
    yOffset = yOffset - 30
    
    -- UI Scale slider
    yOffset = yOffset - 10
    frame.scaleSlider = self:AddSlider(content, "UI Scale", yOffset, 0.5, 2.0, 0.1, function(value)
        CitySiege_Config:SetUIScale(value)
        if CitySiege_MainFrame and CitySiege_MainFrame.GetFrame then
            local mainFrame = CitySiege_MainFrame:GetFrame()
            if mainFrame then
                mainFrame:SetScale(value)
            end
        end
    end)
    yOffset = yOffset - 60
    
    -- UI Alpha slider
    frame.alphaSlider = self:AddSlider(content, "UI Transparency", yOffset, 0.3, 1.0, 0.1, function(value)
        CitySiege_Config:SetUIAlpha(value)
        if CitySiege_MainFrame and CitySiege_MainFrame.GetFrame then
            local mainFrame = CitySiege_MainFrame:GetFrame()
            if mainFrame then
                mainFrame:SetAlpha(value)
            end
        end
    end)
    yOffset = yOffset - 70
    
    -- Notification Settings
    yOffset = self:AddSectionHeader(content, "Notifications", yOffset)
    
    frame.notifySiegeStart = self:AddCheckbox(content, "Notify on siege start", yOffset, function(checked)
        CitySiege_Config:SetNotification("siegeStart", checked)
    end)
    yOffset = yOffset - 30
    
    frame.notifySiegeEnd = self:AddCheckbox(content, "Notify on siege end", yOffset, function(checked)
        CitySiege_Config:SetNotification("siegeEnd", checked)
    end)
    yOffset = yOffset - 30
    
    frame.notifyPhaseChange = self:AddCheckbox(content, "Notify on phase change", yOffset, function(checked)
        CitySiege_Config:SetNotification("phaseChange", checked)
    end)
    yOffset = yOffset - 30
    
    frame.soundEnabled = self:AddCheckbox(content, "Enable sounds", yOffset, function(checked)
        CitySiege_Config:SetNotification("soundEnabled", checked)
    end)
    yOffset = yOffset - 40
    
    -- Map Display Settings
    yOffset = self:AddSectionHeader(content, "Map Display", yOffset)
    
    frame.showPlayers = self:AddCheckbox(content, "Show player positions", yOffset, function(checked)
        CitySiege_Config:SetMapSetting("showPlayers", checked)
    end)
    yOffset = yOffset - 30
    
    frame.showNPCs = self:AddCheckbox(content, "Show NPC positions", yOffset, function(checked)
        CitySiege_Config:SetMapSetting("showNPCs", checked)
    end)
    yOffset = yOffset - 30
    
    frame.showWaypoints = self:AddCheckbox(content, "Show waypoint paths", yOffset, function(checked)
        CitySiege_Config:SetMapSetting("showWaypoints", checked)
    end)
    yOffset = yOffset - 30
    
    -- Map update interval slider
    yOffset = yOffset - 10
    frame.updateInterval = self:AddSlider(content, "Map Update Interval", yOffset, 0.5, 5.0, 0.5, function(value)
        CitySiege_Config:SetMapSetting("updateInterval", value)
    end)
    yOffset = yOffset - 60
    
    -- Map icon scale slider
    frame.iconScale = self:AddSlider(content, "Map Icon Scale", yOffset, 0.5, 2.0, 0.1, function(value)
        CitySiege_Config:SetMapSetting("iconScale", value)
    end)
    yOffset = yOffset - 60
    
    -- Bottom buttons
    local resetBtn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    resetBtn:SetSize(120, 25)
    resetBtn:SetPoint("BOTTOMLEFT", 10, 10)
    resetBtn:SetText("Reset All")
    resetBtn:SetScript("OnClick", function()
        StaticPopup_Show("CITYSIEGE_RESET_SETTINGS")
    end)
    
    local closeBottomBtn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    closeBottomBtn:SetSize(120, 25)
    closeBottomBtn:SetPoint("BOTTOMRIGHT", -10, 10)
    closeBottomBtn:SetText("Close")
    closeBottomBtn:SetScript("OnClick", function() frame:Hide() end)
    
    -- Load current settings
    self:LoadSettings()
    
    return frame
end

function SettingsPanel:AddSectionHeader(parent, text, yOffset)
    local header = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    header:SetPoint("TOPLEFT", 10, yOffset)
    header:SetText(text)
    header:SetTextColor(0.09, 0.76, 0.95) -- City Siege blue
    return yOffset - 30
end

function SettingsPanel:AddCheckbox(parent, text, yOffset, onClick)
    local checkbox = CreateFrame("CheckButton", nil, parent, "UICheckButtonTemplate")
    checkbox:SetPoint("TOPLEFT", 20, yOffset)
    checkbox.text:SetText(text)
    checkbox:SetScript("OnClick", function(self)
        onClick(self:GetChecked())
    end)
    return checkbox
end

function SettingsPanel:AddSlider(parent, text, yOffset, minVal, maxVal, step, onChange)
    local slider = CreateFrame("Slider", nil, parent, "OptionsSliderTemplate")
    slider:SetPoint("TOPLEFT", 20, yOffset)
    slider:SetMinMaxValues(minVal, maxVal)
    slider:SetValueStep(step)
    slider:SetWidth(400)
    slider:SetObeyStepOnDrag(true)
    
    slider.Text:SetText(text)
    slider.Low:SetText(minVal)
    slider.High:SetText(maxVal)
    
    slider.Value = slider:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    slider.Value:SetPoint("TOP", slider, "BOTTOM", 0, 0)
    
    slider:SetScript("OnValueChanged", function(self, value)
        value = math.floor(value * 10 + 0.5) / 10 -- Round to 1 decimal
        self.Value:SetText(string.format("%.1f", value))
        onChange(value)
    end)
    
    return slider
end

function SettingsPanel:LoadSettings()
    if not frame then return end
    
    local minimapSettings = CitySiege_Config:GetMinimapSettings()
    local uiSettings = CitySiege_Config:GetUISettings()
    local notifSettings = CitySiege_Config:GetNotificationSettings()
    local mapSettings = CitySiege_Config:GetMapSettings()
    
    -- Load checkbox states
    frame.minimapHide:SetChecked(minimapSettings.hide)
    frame.minimapLock:SetChecked(minimapSettings.lock)
    frame.showInCombat:SetChecked(uiSettings.showInCombat)
    frame.autoHide:SetChecked(uiSettings.autoHide)
    frame.uiLocked:SetChecked(uiSettings.locked)
    frame.notifySiegeStart:SetChecked(notifSettings.siegeStart)
    frame.notifySiegeEnd:SetChecked(notifSettings.siegeEnd)
    frame.notifyPhaseChange:SetChecked(notifSettings.phaseChange)
    frame.soundEnabled:SetChecked(notifSettings.soundEnabled)
    frame.showPlayers:SetChecked(mapSettings.showPlayers)
    frame.showNPCs:SetChecked(mapSettings.showNPCs)
    frame.showWaypoints:SetChecked(mapSettings.showWaypoints)
    
    -- Load slider values
    frame.scaleSlider:SetValue(uiSettings.scale)
    frame.alphaSlider:SetValue(uiSettings.alpha)
    frame.updateInterval:SetValue(mapSettings.updateInterval)
    frame.iconScale:SetValue(mapSettings.iconScale)
end

function SettingsPanel:Show()
    if not frame then
        self:Create()
    end
    if frame then
        frame:Show()
        self:LoadSettings()
    end
end

function SettingsPanel:Hide()
    if frame then
        frame:Hide()
    end
end

function SettingsPanel:Toggle()
    -- Create frame if it doesn't exist
    if not frame then
        self:Create()
    end
    
    -- NOW check if frame was successfully created
    if not frame then
        CitySiege_Utils:Print("|cFFFF0000ERROR:|r Failed to create settings panel!")
        return
    end
    
    -- Toggle visibility
    if frame:IsShown() then
        frame:Hide()
    else
        frame:Show()
        self:LoadSettings()
    end
end

-- Static popup for reset confirmation
StaticPopupDialogs["CITYSIEGE_RESET_SETTINGS"] = {
    text = "Are you sure you want to reset all settings to default?",
    button1 = "Yes",
    button2 = "No",
    OnAccept = function()
        CitySiege_Config:ResetProfile()
        SettingsPanel:LoadSettings()
        CitySiege_Utils:Print("Settings reset to defaults.")
    end,
    timeout = 0,
    whileDead = true,
    hideOnEscape = true,
    preferredIndex = 3,
}
