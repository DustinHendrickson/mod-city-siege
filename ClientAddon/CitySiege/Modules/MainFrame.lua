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
local infoPanel = nil

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
    frame.tabs[3] = self:CreateTab(tabsFrame, "Info", 3, function() self:ShowTab(3) end)
    
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
    
    print("CitySiege MainFrame: About to create MapDisplay")
    print("CitySiege MainFrame: CitySiege_MapDisplay exists: " .. tostring(CitySiege_MapDisplay ~= nil))
    
    if CitySiege_MapDisplay then
        local success, result = pcall(function()
            return CitySiege_MapDisplay:Create(frame.mapContent)
        end)
        
        if success then
            -- Store reference to the module, not just the frame
            mapDisplay = CitySiege_MapDisplay
            print("CitySiege MainFrame: MapDisplay created successfully, result: " .. tostring(result))
        else
            print("CitySiege MainFrame: ERROR creating MapDisplay: " .. tostring(result))
        end
    else
        print("CitySiege MainFrame: ERROR - CitySiege_MapDisplay module not loaded!")
    end
    
    -- Info tab
    frame.infoContent = self:CreateInfoPanel(frame.contentFrame)
    frame.infoContent:SetPoint("TOPLEFT", 5, -5)
    frame.infoContent:SetPoint("BOTTOMRIGHT", -5, 5)
    frame.infoContent:Hide()
end

function MainFrame:CreateInfoPanel(parent)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetAllPoints()
    panel:Show()
    
    -- Title
    local title = panel:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", 0, -25)
    title:SetText("Siege Information")
    
    -- Info text directly on panel
    local infoText = panel:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    infoText:SetPoint("TOPLEFT", 25, -65)
    infoText:SetPoint("BOTTOMRIGHT", -25, 25)
    infoText:SetJustifyH("LEFT")
    infoText:SetJustifyV("TOP")
    infoText:SetSpacing(2)
    panel.infoText = infoText
    
    -- Set initial text immediately
    self:UpdateInfoText()
    
    return panel
end



function MainFrame:ShowTab(tabIndex)
    if not frame then return end
    
    self.currentTab = tabIndex
    frame.currentTab = tabIndex
    
    -- Hide all content frames
    if frame.commandsContent then frame.commandsContent:Hide() end
    if frame.mapContent then frame.mapContent:Hide() end
    if frame.infoContent then frame.infoContent:Hide() end
    
    -- Update tab appearances with enhanced styling
    for i, tab in ipairs(frame.tabs) do
        if i == tabIndex then
            -- Selected tab
            tab:SetAlpha(1)
            tab.text:SetTextColor(1, 1, 1)
            tab.text:SetFont("Fonts\\FRIZQT__.TTF", 13, "OUTLINE")
            if tab.selectedTexture then
                tab:SetNormalTexture(tab.selectedTexture)
            end
        else
            -- Unselected tabs
            tab:SetAlpha(0.75)
            tab.text:SetTextColor(0.65, 0.65, 0.7)
            tab.text:SetFont("Fonts\\FRIZQT__.TTF", 12, "OUTLINE")
            if tab.normalTexture then
                tab:SetNormalTexture(tab.normalTexture)
            end
        end
    end
            -- Selected tab
            tab:SetNormalTexture(tab.selectedTexture)
            tab:GetNormalTexture():SetTexCoord(0, 0.625, 0, 0.6875)
            tab.text:SetTextColor(1, 1, 1) -- White
        else
            -- Inactive tab  
            tab:SetNormalTexture(tab.normalTexture)
            tab:GetNormalTexture():SetTexCoord(0, 0.625, 0, 0.6875)
            tab.text:SetTextColor(0.8, 0.8, 0.8) -- Light grey
        end
    end
    
    -- Show selected content and update it
    if tabIndex == 1 and frame.commandsContent then
        frame.commandsContent:Show()
    elseif tabIndex == 2 and frame.mapContent then
        frame.mapContent:Show()
        print("CitySiege: Showing Map tab, currentCityID: " .. tostring(currentCityID))
        if mapDisplay then
            print("CitySiege: mapDisplay frame exists, calling CitySiege_MapDisplay:SetCity")
            CitySiege_MapDisplay:SetCity(currentCityID)
            
            -- Request map data from server if a city is selected
            if currentCityID then
                print("CitySiege: Requesting map data for city " .. currentCityID)
                -- Trigger local REQUEST_MAP handling which will execute the server command
                -- This allows non-GM players to request map data
                if CitySiege_EventHandler then
                    CitySiege_EventHandler:ParseAddonMessage("REQUEST_MAP:" .. currentCityID)
                end
            end
        else
            print("CitySiege: ERROR - mapDisplay frame is nil!")
        end
    elseif tabIndex == 3 and frame.infoContent then
        frame.infoContent:Show()
        self:UpdateInfoText()
    end
end

function MainFrame:SelectCity(cityID)
    print("CitySiege: SelectCity called with cityID: " .. tostring(cityID))
    
    currentCityID = cityID
    
    print("CitySiege: currentCityID set to: " .. tostring(currentCityID))
    
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
        self:ShowTab(3) -- Default to Info tab which has placeholder content
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
    
    -- Update info
    self:UpdateInfoText()
    self:UpdateSiegeDisplay()
end

function MainFrame:UpdateSiegeDisplay()
    if not frame then return end
    
    self:UpdateInfoText()
    
    if mapDisplay and mapDisplay.UpdateDisplay then
        mapDisplay:UpdateDisplay()
    end
end

function MainFrame:UpdateInfoText()
    if not frame or not frame.infoContent or not frame.infoContent.infoText then 
        CitySiege_Utils:Debug("UpdateInfoText: frame or infoText not found")
        return 
    end
    
    local text = "|cFFFFFF00=== City Siege Information ===|r\n\n"
    
    local activeSieges = CitySiege_Config:GetActiveSieges()
    local siegeCount = CitySiege_Utils:TableSize(activeSieges)
    
    if siegeCount == 0 then
        text = text .. "|cFFFF6600No active sieges at this time.|r\n\n"
        
        -- Show available cities
        text = text .. "|cFFFFFF00Available Cities for Siege:|r\n\n"
        text = text .. "|cFF0088FFAlliance Cities:|r\n"
        text = text .. "  - Stormwind City\n"
        text = text .. "  - Ironforge\n"
        text = text .. "  - Darnassus\n"
        text = text .. "  - The Exodar\n\n"
        text = text .. "|cFFFF0000Horde Cities:|r\n"
        text = text .. "  - Orgrimmar\n"
        text = text .. "  - Undercity\n"
        text = text .. "  - Thunder Bluff\n"
        text = text .. "  - Silvermoon City\n\n"
        
        text = text .. "|cFFFFFF00How City Sieges Work:|r\n\n"
        text = text .. "A City Siege is a large-scale battle where one faction\n"
        text = text .. "attempts to capture an enemy capital city.\n\n"
        
        text = text .. "|cFF00FF00Siege Phases:|r\n"
        text = text .. "  Phase 1: Initial assault begins\n"
        text = text .. "  Phase 2: Breach the outer defenses\n"
        text = text .. "  Phase 3: Fight through the city\n"
        text = text .. "  Phase 4: Assault the city leader\n\n"
        
        text = text .. "|cFF00FF00Participation:|r\n"
        text = text .. "  - Join as attacker or defender\n"
        text = text .. "  - Earn kills and honor\n"
        text = text .. "  - Defend your faction's honor\n"
        text = text .. "  - Strategic waypoint system\n\n"
        
    else
        text = text .. string.format("|cFF00FF00Active Sieges: %d|r\n\n", siegeCount)
        
        for cityID, siegeData in pairs(activeSieges) do
            local cityData = CitySiege_CityData[cityID]
            if cityData then
                local color = CitySiege_GetCityColorString(cityID)
                text = text .. string.format("%s=== %s ===|r\n", color, cityData.displayName)
                text = text .. string.format("Status: %s\n", siegeData.status or "Active")
                text = text .. string.format("Phase: %d/4\n", siegeData.phase or 1)
                text = text .. string.format("Faction: %s\n", cityData.faction)
                
                if siegeData.elapsedTime then
                    text = text .. string.format("Duration: %s\n", CitySiege_Utils:FormatTime(siegeData.elapsedTime))
                end
                
                if siegeData.remaining then
                    text = text .. string.format("|cFFFFFF00Time Remaining:|r %s\n", CitySiege_Utils:FormatTime(siegeData.remaining))
                end
                
                if siegeData.leaderHealth then
                    local healthColor = "00FF00"
                    if siegeData.leaderHealth < 50 then healthColor = "FFFF00" end
                    if siegeData.leaderHealth < 25 then healthColor = "FF0000" end
                    text = text .. string.format("|cFF%sLeader Health:|r %.1f%%\n", healthColor, siegeData.leaderHealth)
                end
                
                if siegeData.attackingFaction then
                    text = text .. string.format("Attacking: %s\n", siegeData.attackingFaction)
                end
                
                text = text .. "\n"
                text = text .. string.format("|cFFFF0000Attackers:|r %d players\n", siegeData.attackerCount or 0)
                text = text .. string.format("|cFF0088FFDefenders:|r %d players\n", siegeData.defenderCount or 0)
                
                if siegeData.stats then
                    text = text .. "\n"
                    text = text .. string.format("Attacker Kills: %d\n", siegeData.stats.attackerKills or 0)
                    text = text .. string.format("Defender Kills: %d\n", siegeData.stats.defenderKills or 0)
                end
                
                text = text .. "==================\n\n"
            end
        end
    end
    
    text = text .. "|cFF808080==============================|r\n"
    text = text .. "|cFF808080Use Commands tab to start/manage sieges|r\n"
    text = text .. "|cFF808080Use Map tab to view siege battlefield|r"
    
    if frame.infoContent and frame.infoContent.infoText then
        frame.infoContent.infoText:SetText(text)
        CitySiege_Utils:Debug("UpdateInfoText: Text set successfully, length: " .. string.len(text))
    else
        CitySiege_Utils:Debug("UpdateInfoText: infoText not found!")
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
