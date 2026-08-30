--[[
    City Siege Addon - Command Panel

    Buttons for the server's .citysiege commands. The server consumes any chat
    message beginning with a dot before it reaches other players, so nothing is
    ever leaked to a channel - but a player without the required rank simply gets
    an error back, which is why every button says what it needs.
]]

CitySiege_CommandPanel = {}
local CommandPanel = CitySiege_CommandPanel

local UI = nil
local frame = nil

--- Sends a server command. SAY is used rather than GUILD because the server
--- intercepts dot-commands on any channel and GUILD errors out for guildless
--- characters.
local function RunCommand(command)
    SendChatMessage(command, "SAY")
end

StaticPopupDialogs["CITYSIEGE_CONFIRM"] = {
    text = "%s",
    button1 = YES,
    button2 = NO,
    OnAccept = function(self) if self.data then self.data() end end,
    timeout = 0,
    whileDead = true,
    hideOnEscape = true,
    preferredIndex = 3,
}

local function Confirm(message, action)
    local dialog = StaticPopup_Show("CITYSIEGE_CONFIRM", message)
    if dialog then
        dialog.data = action
    end
end

-- ---------------------------------------------------------------------------

function CommandPanel:Create(parent)
    if frame then return frame end
    UI = CitySiege_UI

    frame = CreateFrame("Frame", "CitySiegeCommandPanel", parent)
    frame:SetAllPoints(parent)
    frame.currentCity = nil

    local heading = UI:CreateHeading(frame, "Siege Commands", "TOPLEFT", frame, "TOPLEFT", 12, -10)

    frame.subtitle = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    frame.subtitle:SetPoint("TOPLEFT", heading, "BOTTOMLEFT", 0, -6)
    frame.subtitle:SetTextColor(0.7, 0.7, 0.7)
    frame.subtitle:SetText("Select a city above to enable these controls.")

    local divider = UI:CreateDivider(frame, 640)
    divider:SetPoint("TOPLEFT", 12, -52)

    -- Two columns of buttons -------------------------------------------------
    local buttons = CreateFrame("Frame", nil, frame)
    buttons:SetPoint("TOPLEFT", 12, -66)
    buttons:SetPoint("BOTTOMRIGHT", -12, 60)
    frame.buttons = buttons

    local defs = {
        {
            key = "start", text = "Start Siege", rank = "Game Master",
            tip = "Begin a siege in the selected city.",
            run = function(city) RunCommand(".citysiege start " .. city.name) end,
        },
        {
            key = "stop", text = "Stop Siege", rank = "Game Master",
            tip = "End the active siege. The natural result decides the winner.",
            confirm = "End the siege of %s now?",
            run = function(city) RunCommand(".citysiege stop " .. city.name) end,
        },
        {
            key = "cleanup", text = "Clear Forces", rank = "Game Master",
            tip = "Remove every siege unit with no winner and no rewards.",
            confirm = "Remove all siege forces from %s?\n\nNobody will be rewarded.",
            run = function(city) RunCommand(".citysiege cleanup " .. city.name) end,
        },
        {
            key = "route", text = "Show Route", rank = "Game Master",
            tip = "Print the generated marching route for this city.",
            run = function(city) RunCommand(".citysiege route " .. city.name) end,
        },
        {
            key = "repath", text = "Rebuild Route", rank = "Game Master",
            tip = "Regenerate the route from the server navmesh. Use after moving a muster point.",
            run = function(city) RunCommand(".citysiege repath " .. city.name) end,
        },
        {
            key = "beacons", text = "Toggle Beacons", rank = "Game Master",
            tip = "Place visible markers along the route in the world. Run again to remove them.",
            run = function(city) RunCommand(".citysiege showroute " .. city.name) end,
        },
    }

    frame.cityButtons = {}

    for index, def in ipairs(defs) do
        local button = UI:CreateButton(buttons, def.text, 200, 24, function()
            local city = frame.currentCity
            if not city then return end

            if def.confirm then
                Confirm(string.format(def.confirm, city.displayName), function() def.run(city) end)
            else
                def.run(city)
            end
        end)

        local column = (index - 1) % 2
        local row = math.floor((index - 1) / 2)
        button:SetPoint("TOPLEFT", column * 216, -row * 32)

        UI:SetTooltip(button, def.text, def.tip, " ", "Requires: " .. def.rank)

        frame.cityButtons[def.key] = button
    end

    -- Server-wide controls ---------------------------------------------------
    local globalDivider = UI:CreateDivider(frame, 640)
    globalDivider:SetPoint("BOTTOMLEFT", 12, 48)

    local statusButton = UI:CreateButton(frame, "Siege Status", 200, 24, function()
        RunCommand(".citysiege status")
    end)
    statusButton:SetPoint("BOTTOMLEFT", 12, 16)
    UI:SetTooltip(statusButton, "Siege Status", "Print every running siege to chat.", " ", "Requires: Player")

    local reloadButton = UI:CreateButton(frame, "Reload Config", 200, 24, function()
        Confirm("Reload the City Siege configuration from disk?", function()
            RunCommand(".citysiege reload")
        end)
    end)
    reloadButton:SetPoint("BOTTOMLEFT", 228, 16)
    UI:SetTooltip(reloadButton, "Reload Config",
        "Re-read mod_city_siege.conf and clear cached routes.", " ", "Requires: Administrator")

    self:SetCity(nil)

    return frame
end

--- Enables or disables the per-city controls.
function CommandPanel:SetCity(cityID)
    if not frame then return end

    local cityData = cityID and CitySiege_CityData[cityID] or nil
    frame.currentCity = cityData
    frame.currentCityID = cityID

    for _, button in pairs(frame.cityButtons or {}) do
        if cityData then
            button:Enable()
        else
            button:Disable()
        end
    end

    if cityData then
        frame.subtitle:SetText(string.format("Acting on %s%s|r.",
            CitySiege_GetCityColorString(cityID), cityData.displayName))
    else
        frame.subtitle:SetText("Select a city above to enable these controls.")
    end
end

-- Backwards-compatible alias.
function CommandPanel:SetSelectedCity(cityID)
    self:SetCity(cityID)
end

function CommandPanel:GetSelectedCity()
    return frame and frame.currentCity or nil
end

function CommandPanel:GetFrame()
    return frame
end
