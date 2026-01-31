--[[
    City Siege Addon - Command Panel
    Provides buttons for executing siege commands
]]

CitySiege_CommandPanel = {}
local CommandPanel = CitySiege_CommandPanel

-- Frame reference
local frame = nil

function CommandPanel:Create(parent)
    if frame then return frame end
    
    frame = CreateFrame("Frame", "CitySiegeCommandPanel", parent)
    frame:SetAllPoints(parent)
    frame.currentCity = nil
    
    -- Title
    local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", 0, -25)
    title:SetText("Siege Commands")
    
    -- Instruction text (shown when no city selected)
    local instructionText = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    instructionText:SetPoint("TOP", title, "BOTTOM", 0, -30)
    instructionText:SetText("|cFFFFFF00Select a city from the dropdown above\nto access siege commands|r")
    instructionText:SetJustifyH("CENTER")
    frame.instructionText = instructionText
    
    -- Buttons section
    local buttonsFrame = CreateFrame("Frame", nil, frame)
    buttonsFrame:SetSize(240, 280)
    buttonsFrame:SetPoint("TOP", title, "BOTTOM", 0, -20)
    buttonsFrame:Hide() -- Hidden until city selected
    frame.buttonsFrame = buttonsFrame
    
    local buttonY = 0
    local buttonSpacing = 42
    local buttonWidth = 220
    local buttonHeight = 28
    
    -- Start Siege button
    frame.startButton = self:CreateCommandButton(buttonsFrame, "Start Siege", function()
        if frame.currentCity then
            SendChatMessage(".citysiege start " .. frame.currentCity.name, "GUILD")
            CitySiege_Utils:Print("Starting siege in " .. frame.currentCity.displayName)
        end
    end, buttonWidth, buttonHeight)
    frame.startButton:SetPoint("TOP", 0, buttonY)
    buttonY = buttonY - buttonSpacing
    
    -- Stop Siege button
    frame.stopButton = self:CreateCommandButton(buttonsFrame, "Stop Siege", function()
        if frame.currentCity then
            local faction = CitySiege_Utils:GetPlayerFaction()
            SendChatMessage(string.format(".citysiege stop %s %s", frame.currentCity.name, faction), "GUILD")
            CitySiege_Utils:Print("Stopping siege in " .. frame.currentCity.displayName)
        end
    end, buttonWidth, buttonHeight)
    frame.stopButton:SetPoint("TOP", 0, buttonY)
    buttonY = buttonY - buttonSpacing
    
    -- Cleanup button
    frame.cleanupButton = self:CreateCommandButton(buttonsFrame, "Cleanup NPCs", function()
        if frame.currentCity then
            SendChatMessage(".citysiege cleanup " .. frame.currentCity.name, "GUILD")
            CitySiege_Utils:Print("Cleaning up siege NPCs in " .. frame.currentCity.displayName)
        end
    end, buttonWidth, buttonHeight)
    frame.cleanupButton:SetPoint("TOP", 0, buttonY)
    buttonY = buttonY - buttonSpacing
    
    -- Status button
    frame.statusButton = self:CreateCommandButton(buttonsFrame, "Show Status", function()
        SendChatMessage(".citysiege status", "GUILD")
    end, buttonWidth, buttonHeight)
    frame.statusButton:SetPoint("TOP", 0, buttonY)
    buttonY = buttonY - buttonSpacing
    
    -- Info button
    frame.infoButton = self:CreateCommandButton(buttonsFrame, "Detailed Info", function()
        SendChatMessage(".citysiege info", "GUILD")
    end, buttonWidth, buttonHeight)
    frame.infoButton:SetPoint("TOP", 0, buttonY)
    buttonY = buttonY - buttonSpacing
    
    -- Reload button
    frame.reloadButton = self:CreateCommandButton(buttonsFrame, "Reload Config", function()
        SendChatMessage(".citysiege reload", "GUILD")
    end, buttonWidth, buttonHeight)
    frame.reloadButton:SetPoint("TOP", 0, buttonY)
    
    -- Warning text
    local warning = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    warning:SetPoint("BOTTOM", 0, 25)
    warning:SetText("|cFFFF8800Note: GM/Admin commands only|r")
    
    return frame
end

function CommandPanel:CreateCommandButton(parent, text, onClick, width, height)
    local button = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    button:SetSize(width or 200, height or 25)
    button:SetText(text)
    button:SetScript("OnClick", onClick)
    
    -- Make button text more readable
    local fontString = button:GetFontString()
    if fontString then
        fontString:SetFont("Fonts\\FRIZQT__.TTF", 12, "OUTLINE")
    end
    
    return button
end

function CommandPanel:CreateCityDropdown(parent)
    -- Removed - using global city dropdown instead
    return nil
end

function CommandPanel:OnCitySelected(cityID)
    -- No longer used - global dropdown handles this
end

function CommandPanel:GetSelectedCity()
    -- Return currently selected city from frame
    return frame and frame.currentCity or nil
end

function CommandPanel:SetSelectedCity(cityID)
    if not frame then return end
    
    local cityData = CitySiege_CityData[cityID]
    frame.currentCity = cityData
    
    -- Show or hide buttons based on city selection
    if cityData then
        if frame.buttonsFrame then
            frame.buttonsFrame:Show()
        end
        if frame.instructionText then
            frame.instructionText:Hide()
        end
        CitySiege_Utils:Debug("Command panel updated for city: " .. cityData.displayName)
    else
        if frame.buttonsFrame then
            frame.buttonsFrame:Hide()
        end
        if frame.instructionText then
            frame.instructionText:Show()
        end
    end
end

function CommandPanel:Show()
    if frame then
        frame:Show()
    end
end

function CommandPanel:Hide()
    if frame then
        frame:Hide()
    end
end

function CommandPanel:GetFrame()
    return frame
end
