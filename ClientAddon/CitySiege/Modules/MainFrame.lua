--[[
    City Siege Addon - Main Frame
    The main UI window that contains all other components
]]

CitySiege_MainFrame = {}
local MainFrame = CitySiege_MainFrame

local frame = nil
local currentCityID = nil
local commandPanel = nil
local mapDisplay = nil

function MainFrame:Initialize()
    if frame then return end
    
    self:CreateFrame()
    
    CitySiege_Utils:Debug("Main Frame initialized")
end

function MainFrame:CreateFrame()
    -- Create main frame
    frame = CreateFrame("Frame", "CitySiegeMainFrame", UIParent)
    frame:SetSize(850, 600)
    frame:SetPoint("CENTER")
    frame:SetFrameStrata("MEDIUM")
    frame:EnableMouse(true)
    frame:SetMovable(true)
    frame:RegisterForDrag("LeftButton")
    frame:SetClampedToScreen(true)
    frame:Hide()
    
    -- Store drag offset for smooth dragging
    frame.dragOffsetX = 0
    frame.dragOffsetY = 0
    frame.isDragging = false
    
    -- Background
    CitySiege_Utils:SetBackdrop(frame, 0, 0, 0, 0.95)
    
    -- Title bar with gradient
    local titleBar = CreateFrame("Frame", nil, frame)
    titleBar:SetHeight(40)
    titleBar:SetPoint("TOPLEFT", 1, -1)
    titleBar:SetPoint("TOPRIGHT", -1, -1)
    CitySiege_Utils:SetBackdrop(titleBar, 0.05, 0.15, 0.25, 1)
    
    -- Title bar gradient overlay
    local gradient = titleBar:CreateTexture(nil, "BORDER")
    gradient:SetAllPoints(titleBar)
    gradient:SetTexture("Interface\\AddOns\\CitySiege\\Media\\gradient", true, true)
    gradient:SetGradientAlpha("VERTICAL", 0.09, 0.76, 0.95, 0.6, 0.05, 0.4, 0.6, 0.9)
    gradient:SetBlendMode("ADD")
    
    -- Drag handler with custom mouse anchor
    titleBar:EnableMouse(true)
    titleBar:RegisterForDrag("LeftButton")
    titleBar:SetScript("OnDragStart", function(self) 
        if not CitySiege_Config:GetUISettings().locked then
            -- Get mouse position and frame position
            local scale = frame:GetEffectiveScale()
            local x, y = GetCursorPosition()
            x = x / scale
            y = y / scale
            
            local frameX = frame:GetLeft()
            local frameY = frame:GetTop()
            
            -- Calculate offset from frame origin to mouse
            frame.dragOffsetX = x - frameX
            frame.dragOffsetY = frameY - y
            frame.isDragging = true
            
            frame:SetScript("OnUpdate", function(self)
                if frame.isDragging then
                    local scale = frame:GetEffectiveScale()
                    local x, y = GetCursorPosition()
                    x = x / scale
                    y = y / scale
                    
                    -- Apply offset so frame moves with mouse at click point
                    frame:ClearAllPoints()
                    frame:SetPoint("TOPLEFT", UIParent, "BOTTOMLEFT", 
                        x - frame.dragOffsetX, 
                        y + frame.dragOffsetY)
                end
            end)
        end
    end)
    titleBar:SetScript("OnDragStop", function(self) 
        frame.isDragging = false
        frame:SetScript("OnUpdate", nil)
        MainFrame:SavePosition()
    end)
    
    -- Title text with shadow
    local title = titleBar:CreateFontString(nil, "OVERLAY", "GameFontNormalHuge")
    title:SetPoint("CENTER", 0, -2)
    title:SetFont("Fonts\\FRIZQT__.TTF", 16, "OUTLINE")
    title:SetShadowOffset(2, -2)
    title:SetShadowColor(0, 0, 0, 1)
    title:SetText("|cFF16C3F2City Siege|r")
    frame.titleText = title
    
    -- Close button
    local closeBtn = CreateFrame("Button", nil, titleBar, "UIPanelCloseButton")
    closeBtn:SetPoint("RIGHT", -5, 0)
    closeBtn:SetScript("OnClick", function() self:Hide() end)
    
    -- Settings button with tooltip
    local settingsBtn = CreateFrame("Button", nil, titleBar, "UIPanelButtonTemplate")
    settingsBtn:SetSize(85, 24)
    settingsBtn:SetPoint("RIGHT", closeBtn, "LEFT", -5, 0)
    settingsBtn:SetText("Settings")
    settingsBtn:SetScript("OnClick", function()
        if CitySiege_SettingsPanel then
            CitySiege_SettingsPanel:Toggle()
        end
    end)
    settingsBtn:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        GameTooltip:SetText("City Siege Settings", 1, 1, 1)
        GameTooltip:AddLine("Configure addon display, notifications, and map settings", 0.8, 0.8, 0.8, 1)
        GameTooltip:Show()
    end)
    settingsBtn:SetScript("OnLeave", function()
        GameTooltip:Hide()
    end)
    
    -- City selection dropdown with enhanced styling
    local cityLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    cityLabel:SetPoint("TOPLEFT", titleBar, "BOTTOMLEFT", 20, -18)
    cityLabel:SetFont("Fonts\\FRIZQT__.TTF", 13, "OUTLINE")
    cityLabel:SetTextColor(0.09, 0.76, 0.95)
    cityLabel:SetText("Select City:")
    
    local cityDropdown = self:CreateCityDropdown(frame)
    cityDropdown:SetPoint("LEFT", cityLabel, "RIGHT", 5, 0)
    frame.cityDropdown = cityDropdown
    
    -- Tabs
    local tabsFrame = CreateFrame("Frame", nil, frame)
    tabsFrame:SetSize(820, 30)
    tabsFrame:SetPoint("TOPLEFT", cityLabel, "BOTTOMLEFT", -5, -15)
    frame.tabsFrame = tabsFrame
    
    frame.tabs = {}
    frame.tabs[1] = self:CreateTab(tabsFrame, "Commands", 1, function() self:ShowTab(1) end)
    frame.tabs[2] = self:CreateTab(tabsFrame, "Map", 2, function() self:ShowTab(2) end)
    
    -- Hide all tabs initially until city is selected
    for i, tab in ipairs(frame.tabs) do
        tab:Hide()
    end
    
    -- Content area with padding from edges
    local contentFrame = CreateFrame("Frame", nil, frame)
    contentFrame:SetPoint("TOPLEFT", tabsFrame, "BOTTOMLEFT", 10, -10)
    contentFrame:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -10, 10)
    CitySiege_Utils:SetBackdrop(contentFrame, 0.05, 0.05, 0.05, 0.8)
    frame.contentFrame = contentFrame
    
    -- Create tab content frames
    self:CreateTabContent()
    
    -- Don't auto-select a city - let user choose
    -- Tabs will be hidden until city is selected
    
    -- Load saved position
    self:LoadPosition()
    
    -- Apply UI settings
    self:ApplySettings()
    
    return frame
end

function MainFrame:CreateCityDropdown(parent)
    local dropdown = CreateFrame("Frame", "CitySiegeMainCityDropdown", parent, "UIDropDownMenuTemplate")
    
    UIDropDownMenu_Initialize(dropdown, function(self, level)
        local info = UIDropDownMenu_CreateInfo()
        
        -- Add "Pick a City" placeholder option
        info.text = "Pick a City..."
        info.value = -1
        info.func = function(self)
            UIDropDownMenu_SetSelectedValue(dropdown, self.value)
            MainFrame:SelectCity(nil)
        end
        info.checked = (UIDropDownMenu_GetSelectedValue(dropdown) == -1)
        info.disabled = false
        info.notCheckable = false
        UIDropDownMenu_AddButton(info, level)
        
        -- Add separator
        info = UIDropDownMenu_CreateInfo()
        info.disabled = true
        info.notCheckable = true
        info.text = " "
        UIDropDownMenu_AddButton(info, level)
        
        -- Add each city
        for cityID, cityData in pairs(CitySiege_CityData) do
            info = UIDropDownMenu_CreateInfo()
            info.text = cityData.displayName
            info.value = cityID
            info.func = function(self)
                UIDropDownMenu_SetSelectedValue(dropdown, self.value)
                MainFrame:SelectCity(self.value)
            end
            info.checked = (UIDropDownMenu_GetSelectedValue(dropdown) == cityID)
            UIDropDownMenu_AddButton(info, level)
        end
    end)
    
    -- Default to placeholder
    UIDropDownMenu_SetSelectedValue(dropdown, -1)
    UIDropDownMenu_SetText(dropdown, "Pick a City...")
    UIDropDownMenu_SetWidth(dropdown, 150)
    
    return dropdown
end

function MainFrame:CreateTab(parent, text, index, onClick)
    local tab = CreateFrame("Button", nil, parent)
    tab:SetSize(130, 32)
    tab:SetPoint("LEFT", (index - 1) * 135 + 5, 0)
    
    -- Use simple button template instead of character tab textures
    tab:SetNormalTexture("Interface\\Buttons\\UI-Panel-Button-Up")
    tab:SetPushedTexture("Interface\\Buttons\\UI-Panel-Button-Down")
    tab:SetHighlightTexture("Interface\\Buttons\\UI-Panel-Button-Highlight")
    tab:SetDisabledTexture("Interface\\Buttons\\UI-Panel-Button-Disabled")
    
    -- Make textures fit properly
    tab:GetNormalTexture():SetTexCoord(0, 0.625, 0, 0.6875)
    tab:GetPushedTexture():SetTexCoord(0, 0.625, 0, 0.6875)
    tab:GetHighlightTexture():SetTexCoord(0, 0.625, 0, 0.6875)
    tab:GetHighlightTexture():SetBlendMode("ADD")
    
    local tabText = tab:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    tabText:SetPoint("CENTER", 0, 0)
    tabText:SetFont("Fonts\\FRIZQT__.TTF", 12, "OUTLINE")
    tabText:SetText(text)
    tab.text = tabText
    
    -- Hover tooltip
    tab:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_TOP")
        GameTooltip:SetText(text .. " Tab", 1, 1, 1)
        GameTooltip:Show()
    end)
    tab:SetScript("OnLeave", function()
        GameTooltip:Hide()
    end)
    
    tab:SetScript("OnClick", onClick)
    
    -- Store original textures for state changes
    tab.normalTexture = "Interface\\Buttons\\UI-Panel-Button-Up"
    tab.selectedTexture = "Interface\\Buttons\\UI-Panel-Button-Down"
    
    return tab
end

function MainFrame:CreateTabContent()
    -- Commands tab (using CommandPanel)
    frame.commandsContent = CreateFrame("Frame", nil, frame.contentFrame)
    frame.commandsContent:SetPoint("TOPLEFT", 5, -5)
    frame.commandsContent:SetPoint("BOTTOMRIGHT", -5, 5)
    frame.commandsContent:Hide()
    commandPanel = CitySiege_CommandPanel:Create(frame.commandsContent)
    
    -- Map tab (using MapDisplay)
    frame.mapContent = CreateFrame("Frame", nil, frame.contentFrame)
    frame.mapContent:SetPoint("TOPLEFT", 5, -5)
    frame.mapContent:SetPoint("BOTTOMRIGHT", -5, 5)
    frame.mapContent:Hide()
    
    if CitySiege_MapDisplay then
        local success, result = pcall(function()
            return CitySiege_MapDisplay:Create(frame.mapContent)
        end)
        
        if success then
            -- Store reference to the module, not just the frame
            mapDisplay = CitySiege_MapDisplay
        end
    end
end



function MainFrame:ShowTab(tabIndex)
    if not frame then return end
    
    self.currentTab = tabIndex
    frame.currentTab = tabIndex
    
    -- Hide all content frames
    if frame.commandsContent then frame.commandsContent:Hide() end
    if frame.mapContent then frame.mapContent:Hide() end
    
    -- Update tab appearances with enhanced styling
    for i, tab in ipairs(frame.tabs) do
        if i == tabIndex then
            -- Selected tab
            tab:SetAlpha(1)
            tab.text:SetTextColor(1, 1, 1)
            tab.text:SetFont("Fonts\\FRIZQT__.TTF", 13, "OUTLINE")
            if tab.selectedTexture then
                tab:SetNormalTexture(tab.selectedTexture)
                tab:GetNormalTexture():SetTexCoord(0, 0.625, 0, 0.6875)
            end
        else
            -- Unselected tabs
            tab:SetAlpha(0.75)
            tab.text:SetTextColor(0.65, 0.65, 0.7)
            tab.text:SetFont("Fonts\\FRIZQT__.TTF", 12, "OUTLINE")
            if tab.normalTexture then
                tab:SetNormalTexture(tab.normalTexture)
                tab:GetNormalTexture():SetTexCoord(0, 0.625, 0, 0.6875)
            end
        end
    end
    
    -- Show selected content and update it
    if tabIndex == 1 and frame.commandsContent then
        frame.commandsContent:Show()
    elseif tabIndex == 2 and frame.mapContent then
        frame.mapContent:Show()
        if mapDisplay then
            CitySiege_MapDisplay:SetCity(currentCityID)
            
            -- Request map data from server if a city is selected
            if currentCityID and currentCityID > 0 then
                -- Trigger local REQUEST_MAP handling which will execute the server command
                -- This allows non-GM players to request map data
                if CitySiege_EventHandler then
                    CitySiege_EventHandler:ParseAddonMessage("REQUEST_MAP:" .. currentCityID)
                end
            end
        end
    end
end

function MainFrame:SelectCity(cityID)
    currentCityID = cityID
    
    -- Update CommandPanel with the selected city
    if commandPanel then
        CitySiege_CommandPanel:SetSelectedCity(cityID)
    end
    
    -- Always show all tabs
    for i, tab in ipairs(frame.tabs) do
        tab:Show()
    end
    
    -- Hide welcome message if it exists
    if frame.welcomeText then
        frame.welcomeText:Hide()
    end
    
    -- Refresh current tab display
    if frame.currentTab and frame.currentTab > 0 then
        self:ShowTab(frame.currentTab)
    else
        self:ShowTab(1) -- Default to Commands tab
        if not frame.welcomeText then
            frame.welcomeText = frame.contentFrame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
            frame.welcomeText:SetPoint("CENTER", frame.contentFrame, "CENTER", 0, 0)
            frame.welcomeText:SetJustifyH("CENTER")
        end
        frame.welcomeText:SetText("|cFFFFFF00Welcome to City Siege|r\n\n|cFFFFFFFFSelect a city from the dropdown above\nto view siege information and commands|r")
        frame.welcomeText:Show()
    end
    
    -- Update title
    if cityID then
        local cityData = CitySiege_CityData[cityID]
        if cityData then
            local color = CitySiege_GetCityColorString(cityID)
            frame.titleText:SetText(string.format("City Siege - %s%s|r", color, cityData.displayName))
        end
    else
        frame.titleText:SetText("|cFFFFFFFFCity Siege|r")
    end
    
    -- Update map display
    if mapDisplay then
        mapDisplay:SetCity(cityID)
    end
    
    -- Update command panel
    if commandPanel and CitySiege_CommandPanel.SetSelectedCity then
        CitySiege_CommandPanel:SetSelectedCity(cityID)
    end
    
    self:UpdateSiegeDisplay()
end

function MainFrame:UpdateSiegeDisplay()
    if not frame then return end
    
    if mapDisplay and mapDisplay.UpdateDisplay then
        mapDisplay:UpdateDisplay()
    end
end

function MainFrame:Show()
    if not frame then
        self:CreateFrame()
    end
    
    if frame then
        frame:Show()
        frame:Raise() -- Bring to front
        
        -- Show all tabs
        for i, tab in ipairs(frame.tabs) do
            tab:Show()
        end
        
        -- Ensure all child frames are visible
        if frame.contentFrame then
            frame.contentFrame:Show()
        end
        
        self:UpdateSiegeDisplay()
        
        -- Show the current tab or default to Info (has placeholder content)
        if self.currentTab and self.currentTab > 0 then
            self:ShowTab(self.currentTab)
        else
            self:ShowTab(3) -- Default to Info tab
        end
    end
end

function MainFrame:Hide()
    if frame then
        frame:Hide()
    end
end

function MainFrame:Toggle()
    if not frame then
        self:Show()
        return
    end
    
    if frame:IsShown() then
        self:Hide()
    else
        self:Show()
    end
end

function MainFrame:IsShown()
    return frame and frame:IsShown()
end

function MainFrame:SavePosition()
    if not frame then return end
    
    local point, _, relativePoint, x, y = frame:GetPoint()
    CitySiege_Config:SetFramePosition("MainFrame", point, "UIParent", relativePoint, x, y)
end

function MainFrame:LoadPosition()
    if not frame then return end
    
    local pos = CitySiege_Config:GetFramePosition("MainFrame")
    if pos then
        frame:ClearAllPoints()
        frame:SetPoint(pos.point, UIParent, pos.relativePoint, pos.x, pos.y)
    end
end

function MainFrame:ApplySettings()
    if not frame then return end
    
    local settings = CitySiege_Config:GetUISettings()
    
    frame:SetScale(settings.scale or 1.0)
    frame:SetAlpha(settings.alpha or 1.0)
end

function MainFrame:GetFrame()
    return frame
end
