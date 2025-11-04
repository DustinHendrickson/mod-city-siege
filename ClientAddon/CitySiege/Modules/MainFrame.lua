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
    
    -- Title bar
    local titleBar = CreateFrame("Frame", nil, frame)
    titleBar:SetHeight(35)
    titleBar:SetPoint("TOPLEFT", 1, -1)
    titleBar:SetPoint("TOPRIGHT", -1, -1)
    CitySiege_Utils:SetBackdrop(titleBar, 0.09, 0.76, 0.95, 1)
    
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
    
    -- Title text
    local title = titleBar:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("CENTER")
    title:SetText("|cFFFFFFFFCity Siege|r")
    frame.titleText = title
    
    -- Close button
    local closeBtn = CreateFrame("Button", nil, titleBar, "UIPanelCloseButton")
    closeBtn:SetPoint("RIGHT", -5, 0)
    closeBtn:SetScript("OnClick", function() self:Hide() end)
    
    -- Settings button
    local settingsBtn = CreateFrame("Button", nil, titleBar, "UIPanelButtonTemplate")
    settingsBtn:SetSize(80, 22)
    settingsBtn:SetPoint("RIGHT", closeBtn, "LEFT", -5, 0)
    settingsBtn:SetText("Settings")
    settingsBtn:SetScript("OnClick", function()
        if CitySiege_SettingsPanel then
            CitySiege_SettingsPanel:Toggle()
        end
    end)
    
    -- City selection dropdown
    local cityLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    cityLabel:SetPoint("TOPLEFT", titleBar, "BOTTOMLEFT", 15, -15)
    cityLabel:SetText("City:")
    
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
    frame.tabs[4] = self:CreateTab(tabsFrame, "Stats", 4, function() self:ShowTab(4) end)
    
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
    tab:SetSize(110, 28)
    tab:SetPoint("LEFT", (index - 1) * 115, 0)
    
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
    tabText:SetText(text)
    tab.text = tabText
    
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
    if commandPanel then
        commandPanel:ClearAllPoints()
        commandPanel:SetAllPoints(frame.commandsContent)
    end
    
    -- Map tab (using MapDisplay)
    frame.mapContent = CreateFrame("Frame", nil, frame.contentFrame)
    frame.mapContent:SetPoint("TOPLEFT", 5, -5)
    frame.mapContent:SetPoint("BOTTOMRIGHT", -5, 5)
    frame.mapContent:Hide()
    mapDisplay = CitySiege_MapDisplay:Create(frame.mapContent)
    if mapDisplay then
        mapDisplay:ClearAllPoints()
        mapDisplay:SetAllPoints(frame.mapContent)
    end
    
    -- Info tab
    frame.infoContent = self:CreateInfoPanel(frame.contentFrame)
    frame.infoContent:SetPoint("TOPLEFT", 5, -5)
    frame.infoContent:SetPoint("BOTTOMRIGHT", -5, 5)
    frame.infoContent:Hide()
    
    -- Stats tab
    frame.statsContent = self:CreateStatsPanel(frame.contentFrame)
    frame.statsContent:SetPoint("TOPLEFT", 5, -5)
    frame.statsContent:SetPoint("BOTTOMRIGHT", -5, 5)
    frame.statsContent:Hide()
end

function MainFrame:CreateInfoPanel(parent)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetAllPoints()
    
    -- Add visible background
    CitySiege_Utils:SetBackdrop(panel, 0.1, 0.1, 0.1, 0.9)
    
    local scrollFrame = CreateFrame("ScrollFrame", nil, panel, "UIPanelScrollFrameTemplate")
    scrollFrame:SetPoint("TOPLEFT", 15, -15)
    scrollFrame:SetPoint("BOTTOMRIGHT", -30, 15)
    
    local content = CreateFrame("Frame", nil, scrollFrame)
    content:SetSize(750, 1000)
    scrollFrame:SetScrollChild(content)
    
    local infoText = content:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    infoText:SetPoint("TOPLEFT", 10, -10)
    infoText:SetWidth(730)
    infoText:SetJustifyH("LEFT")
    infoText:SetJustifyV("TOP")
    panel.infoText = infoText
    
    self:UpdateInfoText()
    
    return panel
end

function MainFrame:CreateStatsPanel(parent)
    local panel = CreateFrame("Frame", nil, parent)
    panel:SetAllPoints()
    
    -- Add visible background
    CitySiege_Utils:SetBackdrop(panel, 0.1, 0.1, 0.1, 0.9)
    
    local title = panel:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", 0, -25)
    title:SetText("Siege Statistics")
    
    local statsText = panel:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    statsText:SetPoint("TOPLEFT", 25, -65)
    statsText:SetWidth(750)
    statsText:SetJustifyH("LEFT")
    statsText:SetJustifyV("TOP")
    panel.statsText = statsText
    
    self:UpdateStatsText()
    
    return panel
end

function MainFrame:ShowTab(tabIndex)
    if not frame or not currentCityID then return end -- Don't show tabs without a city selected
    
    frame.currentTab = tabIndex
    
    -- Hide all content frames
    if frame.commandsContent then frame.commandsContent:Hide() end
    if frame.mapContent then frame.mapContent:Hide() end
    if frame.infoContent then frame.infoContent:Hide() end
    if frame.statsContent then frame.statsContent:Hide() end
    
    -- Update tab appearances
    for i, tab in ipairs(frame.tabs) do
        if i == tabIndex then
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
        if mapDisplay then
            mapDisplay:SetCity(currentCityID)
            mapDisplay:UpdateDisplay()
        end
    elseif tabIndex == 3 and frame.infoContent then
        frame.infoContent:Show()
        self:UpdateInfoText()
    elseif tabIndex == 4 and frame.statsContent then
        frame.statsContent:Show()
        self:UpdateStatsText()
    end
end

function MainFrame:SelectCity(cityID)
    currentCityID = cityID
    
    -- Request current siege data from server when city selected
    if cityID then
        SendChatMessage(".citysiege sync " .. cityID, "GUILD")
    end
    
    -- Show or hide tabs based on city selection
    if cityID then
        -- City selected - show all tabs
        for i, tab in ipairs(frame.tabs) do
            tab:Show()
        end
        
        -- Hide welcome message if it exists
        if frame.welcomeText then
            frame.welcomeText:Hide()
        end
        
        -- Show current tab
        if not frame.currentTab or frame.currentTab == 0 then
            self:ShowTab(1)
        end
    else
        -- No city selected - hide all tabs
        for i, tab in ipairs(frame.tabs) do
            tab:Hide()
        end
        
        -- Hide all content
        if frame.commandsContent then frame.commandsContent:Hide() end
        if frame.mapContent then frame.mapContent:Hide() end
        if frame.infoContent then frame.infoContent:Hide() end
        if frame.statsContent then frame.statsContent:Hide() end
        
        -- Show welcome message
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
    self:UpdateStatsText()
    
    if mapDisplay and mapDisplay.UpdateDisplay then
        mapDisplay:UpdateDisplay()
    end
end

function MainFrame:UpdateInfoText()
    if not frame or not frame.infoContent or not frame.infoContent.infoText then return end
    
    local text = "|cFFFFFF00=== City Siege Information ===|r\n\n"
    
    local activeSieges = CitySiege_Config:GetActiveSieges()
    local siegeCount = CitySiege_Utils:TableSize(activeSieges)
    
    if siegeCount == 0 then
        text = text .. "|cFFFF6600No active sieges at this time.|r\n\n"
        
        -- Show available cities
        text = text .. "|cFFFFFF00Available Cities for Siege:|r\n"
        text = text .. "├─ |cFF0088FFAlliance Cities:|r\n"
        text = text .. "│  ├─ Stormwind City\n"
        text = text .. "│  ├─ Ironforge\n"
        text = text .. "│  ├─ Darnassus\n"
        text = text .. "│  └─ The Exodar\n"
        text = text .. "└─ |cFFFF0000Horde Cities:|r\n"
        text = text .. "   ├─ Orgrimmar\n"
        text = text .. "   ├─ Undercity\n"
        text = text .. "   ├─ Thunder Bluff\n"
        text = text .. "   └─ Silvermoon City\n\n"
        
        text = text .. "|cFFFFFF00How City Sieges Work:|r\n\n"
        text = text .. "A City Siege is a large-scale battle where one faction\n"
        text = text .. "attempts to capture an enemy capital city.\n\n"
        
        text = text .. "|cFF00FF00Siege Phases:|r\n"
        text = text .. "• Phase 1: Initial assault begins\n"
        text = text .. "• Phase 2: Breach the outer defenses\n"
        text = text .. "• Phase 3: Fight through the city\n"
        text = text .. "• Phase 4: Assault the city leader\n\n"
        
        text = text .. "|cFF00FF00Participation:|r\n"
        text = text .. "• Join as attacker or defender\n"
        text = text .. "• Earn kills and honor\n"
        text = text .. "• Defend your faction's honor\n"
        text = text .. "• Strategic waypoint system\n\n"
        
    else
        text = text .. string.format("|cFF00FF00Active Sieges: %d|r\n\n", siegeCount)
        
        for cityID, siegeData in pairs(activeSieges) do
            local cityData = CitySiege_CityData[cityID]
            if cityData then
                local color = CitySiege_GetCityColorString(cityID)
                text = text .. string.format("%s╔═══ %s ═══╗|r\n", color, cityData.displayName)
                text = text .. string.format("║ Status: %s\n", siegeData.status or "Active")
                text = text .. string.format("║ Phase: %d/4\n", siegeData.phase or 1)
                text = text .. string.format("║ Faction: %s\n", cityData.faction)
                
                if siegeData.elapsedTime then
                    text = text .. string.format("║ Duration: %s\n", CitySiege_Utils:FormatTime(siegeData.elapsedTime))
                end
                
                if siegeData.remaining then
                    text = text .. string.format("║ |cFFFFFF00Time Remaining:|r %s\n", CitySiege_Utils:FormatTime(siegeData.remaining))
                end
                
                if siegeData.leaderHealth then
                    local healthColor = "00FF00"
                    if siegeData.leaderHealth < 50 then healthColor = "FFFF00" end
                    if siegeData.leaderHealth < 25 then healthColor = "FF0000" end
                    text = text .. string.format("║ |cFF%sLeader Health:|r %.1f%%\n", healthColor, siegeData.leaderHealth)
                end
                
                if siegeData.attackingFaction then
                    text = text .. string.format("║ Attacking: %s\n", siegeData.attackingFaction)
                end
                
                text = text .. "║\n"
                text = text .. string.format("║ |cFFFF0000⚔ Attackers:|r %d players\n", siegeData.attackerCount or 0)
                text = text .. string.format("║ |cFF0088FF🛡 Defenders:|r %d players\n", siegeData.defenderCount or 0)
                
                if siegeData.stats then
                    text = text .. "║\n"
                    text = text .. string.format("║ Attacker Kills: %d\n", siegeData.stats.attackerKills or 0)
                    text = text .. string.format("║ Defender Kills: %d\n", siegeData.stats.defenderKills or 0)
                end
                
                text = text .. string.format("%s╚═══════════════╝|r\n\n", color)
            end
        end
    end
    
    text = text .. "|cFF808080═══════════════════════════════|r\n"
    text = text .. "|cFF808080Use Commands tab to start/manage sieges|r\n"
    text = text .. "|cFF808080Use Map tab to view siege battlefield|r"
    
    frame.infoContent.infoText:SetText(text)
end

function MainFrame:UpdateStatsText()
    if not frame or not frame.statsContent or not frame.statsContent.statsText then return end
    
    local stats = CitySiege_Config:GetStatistics()
    
    local text = "|cFFFFFF00═══ Personal Statistics ═══|r\n\n"
    
    -- Check if player has any siege experience
    local hasExperience = (stats.siegesParticipated and stats.siegesParticipated > 0)
    
    if not hasExperience then
        text = text .. "|cFFFF6600You haven't participated in any sieges yet!|r\n\n"
        text = text .. "Start your siege career by:\n"
        text = text .. "• Using Commands tab to start a siege (GM/Admin)\n"
        text = text .. "• Joining an active siege as attacker or defender\n"
        text = text .. "• Helping your faction defend or attack cities\n\n"
        text = text .. "|cFF808080Your statistics will appear here once you participate.|r\n\n"
    end
    
    -- Siege Participation
    text = text .. "|cFF00FF00━━━ Siege Participation ━━━|r\n"
    text = text .. string.format("Total Sieges: |cFFFFFFFF%s|r\n", CitySiege_Utils:FormatNumber(stats.siegesParticipated or 0))
    text = text .. string.format("├─ Victories: |cFF00FF00%s|r\n", CitySiege_Utils:FormatNumber(stats.siegesWon or 0))
    text = text .. string.format("├─ Defeats: |cFFFF0000%s|r\n", CitySiege_Utils:FormatNumber(stats.siegesLost or 0))
    text = text .. string.format("└─ Ongoing: |cFFFFFF00%s|r\n\n", CitySiege_Utils:FormatNumber((stats.siegesParticipated or 0) - (stats.siegesWon or 0) - (stats.siegesLost or 0)))
    
    -- Win Rate
    if stats.siegesParticipated and stats.siegesParticipated > 0 then
        local winRate = (stats.siegesWon / stats.siegesParticipated) * 100
        local color = winRate >= 60 and "|cFF00FF00" or (winRate >= 40 and "|cFFFFFF00" or "|cFFFF0000")
        text = text .. string.format("Win Rate: %s%.1f%%|r\n\n", color, winRate)
    else
        text = text .. "Win Rate: |cFF808080N/A|r\n\n"
    end
    
    -- Combat Statistics
    text = text .. "|cFF00FF00━━━ Combat Statistics ━━━|r\n"
    text = text .. string.format("Total Kills: |cFF00FF00%s|r\n", CitySiege_Utils:FormatNumber(stats.totalKills or 0))
    text = text .. string.format("Total Deaths: |cFFFF0000%s|r\n", CitySiege_Utils:FormatNumber(stats.totalDeaths or 0))
    
    -- K/D Ratio
    if stats.totalDeaths and stats.totalDeaths > 0 then
        local kd = (stats.totalKills or 0) / stats.totalDeaths
        local color = kd >= 2.0 and "|cFF00FF00" or (kd >= 1.0 and "|cFFFFFF00" or "|cFFFF0000")
        text = text .. string.format("K/D Ratio: %s%.2f|r\n\n", color, kd)
    else
        text = text .. "K/D Ratio: |cFF808080N/A|r\n\n"
    end
    
    -- Averages
    if stats.siegesParticipated and stats.siegesParticipated > 0 then
        local avgKills = (stats.totalKills or 0) / stats.siegesParticipated
        local avgDeaths = (stats.totalDeaths or 0) / stats.siegesParticipated
        text = text .. "|cFF00FF00━━━ Averages Per Siege ━━━|r\n"
        text = text .. string.format("Avg Kills: |cFFFFFFFF%.1f|r\n", avgKills)
        text = text .. string.format("Avg Deaths: |cFFFFFFFF%.1f|r\n\n", avgDeaths)
    end
    
    -- Role Performance (if data available)
    if stats.attackerSieges or stats.defenderSieges then
        text = text .. "|cFF00FF00━━━ Role Performance ━━━|r\n"
        text = text .. string.format("As Attacker: |cFFFF0000%d|r sieges\n", stats.attackerSieges or 0)
        text = text .. string.format("As Defender: |cFF0088FF%d|r sieges\n\n", stats.defenderSieges or 0)
    end
    
    -- City-specific stats (if available)
    if stats.citiesAttacked or stats.citiesDefended then
        text = text .. "|cFF00FF00━━━ City Experience ━━━|r\n"
        text = text .. string.format("Cities Attacked: %d\n", stats.citiesAttacked or 0)
        text = text .. string.format("Cities Defended: %d\n\n", stats.citiesDefended or 0)
    end
    
    -- Performance Rating
    if hasExperience then
        local rating = "Recruit"
        local ratingColor = "|cFF808080"
        
        if stats.siegesParticipated >= 50 then
            rating = "Veteran"
            ratingColor = "|cFFFF6600"
        elseif stats.siegesParticipated >= 20 then
            rating = "Experienced"
            ratingColor = "|cFFFFFF00"
        elseif stats.siegesParticipated >= 10 then
            rating = "Regular"
            ratingColor = "|cFFFFFFFF"
        elseif stats.siegesParticipated >= 5 then
            rating = "Initiate"
            ratingColor = "|cFF00FF00"
        end
        
        text = text .. "|cFF00FF00━━━━━━━━━━━━━━━━━━━━|r\n"
        text = text .. string.format("Rank: %s%s|r\n", ratingColor, rating)
    end
    
    text = text .. "\n|cFF808080Stats update in real-time during sieges|r"
    
    frame.statsContent.statsText:SetText(text)
end

function MainFrame:Show()
    if not frame then
        self:CreateFrame()
    end
    
    if frame then
        frame:Show()
        self:UpdateSiegeDisplay()
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
