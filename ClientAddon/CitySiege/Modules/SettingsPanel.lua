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
    
    local UI = CitySiege_UI

    frame = UI:CreateWindow("CitySiegeSettingsPanel", "City Siege Settings", 460, 560)
    frame:SetFrameStrata("DIALOG")

    -- Recessed well, then a scroll frame inside it (named, as 3.3.5 requires).
    local inset = UI:CreateInset(frame, 16, 16, 40, 44)

    local scrollFrame = CreateFrame("ScrollFrame", "CitySiegeSettingsScrollFrame", inset, "UIPanelScrollFrameTemplate")
    scrollFrame:SetPoint("TOPLEFT", 8, -8)
    scrollFrame:SetPoint("BOTTOMRIGHT", -28, 8)

    local content = CreateFrame("Frame", "CitySiegeSettingsScrollChild", scrollFrame)
    content:SetSize(390, 1200)
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
    local resetBtn = UI:CreateButton(frame, "Reset All", 120, 22, function()
        StaticPopup_Show("CITYSIEGE_RESET_SETTINGS")
    end)
    resetBtn:SetPoint("BOTTOMLEFT", 18, 16)
    UI:SetTooltip(resetBtn, "Reset All", "Restore every addon setting to its default.")

    local closeBottomBtn = UI:CreateButton(frame, "Close", 120, 22, function() frame:Hide() end)
    closeBottomBtn:SetPoint("BOTTOMRIGHT", -18, 16)
    
    -- Load current settings
    self:LoadSettings()
    
    return frame
end

function SettingsPanel:AddSectionHeader(parent, text, yOffset)
    local header = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    header:SetPoint("TOPLEFT", 10, yOffset)
    header:SetText(text)
    header:SetTextColor(NORMAL_FONT_COLOR.r, NORMAL_FONT_COLOR.g, NORMAL_FONT_COLOR.b)

    local rule = CitySiege_UI:CreateDivider(parent, 360)
    rule:SetPoint("TOPLEFT", 10, yOffset - 20)

    return yOffset - 32
end

function SettingsPanel:AddCheckbox(parent, text, yOffset, onClick)
    local checkbox = CreateFrame("CheckButton", nil, parent, "UICheckButtonTemplate")
    checkbox:SetPoint("TOPLEFT", 20, yOffset)
    
    -- Get or create the text label (3.3.5 compatible)
    local label = checkbox:GetFontString() or checkbox:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    label:SetPoint("LEFT", checkbox, "RIGHT", 5, 0)
    label:SetText(text)
    checkbox.text = label
    
    checkbox:SetScript("OnClick", function(self)
        onClick(self:GetChecked())
    end)
    return checkbox
end

local sliderCount = 0

function SettingsPanel:AddSlider(parent, text, yOffset, minVal, maxVal, step, onChange)
    -- Sliders must be named in 3.3.5 so the template can find their labels.
    -- A counter is used rather than math.random so names cannot collide.
    sliderCount = sliderCount + 1
    local sliderName = "CitySiegeSlider" .. sliderCount

    local slider = CreateFrame("Slider", sliderName, parent, "OptionsSliderTemplate")
    slider:SetPoint("TOPLEFT", 24, yOffset)
    slider:SetMinMaxValues(minVal, maxVal)
    slider:SetValueStep(step)
    slider:SetWidth(330)
    
    -- SetObeyStepOnDrag may not exist in 3.3.5
    if slider.SetObeyStepOnDrag then
        slider:SetObeyStepOnDrag(true)
    end
    
    -- Get or create text elements (3.3.5 compatible)
    local titleText = _G[sliderName .. "Text"] or slider:CreateFontString(nil, "ARTWORK", "GameFontNormal")
    if not _G[sliderName .. "Text"] then
        titleText:SetPoint("BOTTOM", slider, "TOP", 0, 2)
    end
    titleText:SetText(text)
    
    local lowText = _G[sliderName .. "Low"] or slider:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
    if not _G[sliderName .. "Low"] then
        lowText:SetPoint("TOPLEFT", slider, "BOTTOMLEFT", 0, -2)
    end
    lowText:SetText(minVal)
    
    local highText = _G[sliderName .. "High"] or slider:CreateFontString(nil, "ARTWORK", "GameFontNormalSmall")
    if not _G[sliderName .. "High"] then
        highText:SetPoint("TOPRIGHT", slider, "BOTTOMRIGHT", 0, -2)
    end
    highText:SetText(maxVal)
    
    -- Create value display
    slider.valueText = slider:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    slider.valueText:SetPoint("TOP", slider, "BOTTOM", 0, -18)
    
    slider:SetScript("OnValueChanged", function(self, value)
        value = math.floor(value * 10 + 0.5) / 10 -- Round to 1 decimal
        self.valueText:SetText(string.format("%.1f", value))
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
