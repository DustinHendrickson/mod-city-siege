--[[
    City Siege Addon - Main Window

    A standard Blizzard dialog frame with three tabs: a live Overview of the
    selected city's siege, the Battle Map, and the GM Commands panel.
]]

CitySiege_MainFrame = {}
local MainFrame = CitySiege_MainFrame

local UI = nil
local frame = nil
local currentCityID = nil
local overview = nil
local mapPanel = nil
local commandPanel = nil

local TAB_OVERVIEW, TAB_MAP, TAB_COMMANDS = 1, 2, 3

local PHASE_NAMES = {
    [1] = "Muster",
    [2] = "Assault",
    [3] = "Breach",
    [4] = "Throne",
}

local PHASE_DESCRIPTIONS = {
    [1] = "The war host is forming up outside the gates.",
    [2] = "The army is marching on the city.",
    [3] = "The gates are breached - fighting in the streets.",
    [4] = "The enemy has reached the throne room!",
}

-- ---------------------------------------------------------------------------
-- Construction
-- ---------------------------------------------------------------------------

function MainFrame:Initialize()
    if frame then return end
    UI = CitySiege_UI
    self:CreateFrame()
end

function MainFrame:CreateFrame()
    if frame then return frame end
    UI = UI or CitySiege_UI

    frame = UI:CreateWindow("CitySiegeMainFrame", "City Siege", 720, 520, {
        onShow = function() MainFrame:Refresh() end,
        onHide = function() MainFrame:SavePosition() end,
    })

    frame.onPositionChanged = function() MainFrame:SavePosition() end

    -- Selection row -------------------------------------------------------
    local cityLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    cityLabel:SetPoint("TOPLEFT", 22, -42)
    cityLabel:SetText("City")

    local entries = {}
    for cityID, cityData in pairs(CitySiege_CityData) do
        table.insert(entries, { text = cityData.displayName, value = cityID, order = cityID })
    end
    table.sort(entries, function(a, b) return a.order < b.order end)

    frame.cityDropdown = UI:CreateDropdown(frame, "CitySiegeMainCityDropdown", 160, entries,
        function(value) MainFrame:SelectCity(value) end, "Select a city...")
    frame.cityDropdown:SetPoint("LEFT", cityLabel, "RIGHT", -8, -2)

    -- Live status pill on the right of the selection row.
    frame.statusText = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    frame.statusText:SetPoint("TOPRIGHT", -26, -46)
    frame.statusText:SetJustifyH("RIGHT")
    frame.statusText:SetText(UI:Color("muted", "No siege in progress"))

    -- Content -------------------------------------------------------------
    frame.inset = UI:CreateInset(frame, 16, 16, 74, 18)

    frame.pages = {}
    for i = 1, 3 do
        local page = CreateFrame("Frame", nil, frame.inset)
        page:SetPoint("TOPLEFT", 8, -8)
        page:SetPoint("BOTTOMRIGHT", -8, 8)
        page:Hide()
        frame.pages[i] = page
    end

    overview = self:BuildOverview(frame.pages[TAB_OVERVIEW])

    if CitySiege_MapDisplay then
        local ok = pcall(function() CitySiege_MapDisplay:Create(frame.pages[TAB_MAP]) end)
        if ok then mapPanel = CitySiege_MapDisplay end
    end

    if CitySiege_CommandPanel then
        local ok, panel = pcall(function() return CitySiege_CommandPanel:Create(frame.pages[TAB_COMMANDS]) end)
        if ok then commandPanel = panel end
    end

    UI:CreateTabs(frame, { "Overview", "Battle Map", "Commands" }, function(index)
        MainFrame:ShowTab(index)
    end)

    self:ShowTab(TAB_OVERVIEW)
    self:LoadPosition()
    self:ApplySettings()

    -- Drive the countdown without waiting on a server packet.
    frame:SetScript("OnUpdate", function(self, elapsed)
        self.sinceRefresh = (self.sinceRefresh or 0) + elapsed
        if self.sinceRefresh >= 0.5 then
            self.sinceRefresh = 0
            MainFrame:RefreshOverview()
        end
    end)

    return frame
end

-- ---------------------------------------------------------------------------
-- Overview tab
-- ---------------------------------------------------------------------------

function MainFrame:BuildOverview(parent)
    local page = {}
    local width = 640

    page.cityName = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    page.cityName:SetPoint("TOPLEFT", 12, -10)
    page.cityName:SetText("Select a city")

    page.subtitle = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    page.subtitle:SetPoint("TOPLEFT", 12, -32)
    page.subtitle:SetTextColor(0.7, 0.7, 0.7)
    page.subtitle:SetText("Choose a city above to see its siege status.")

    local divider = UI:CreateDivider(parent, width)
    divider:SetPoint("TOPLEFT", 12, -50)

    -- Phase ---------------------------------------------------------------
    page.phaseLabel = parent:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    page.phaseLabel:SetPoint("TOPLEFT", 12, -64)
    page.phaseLabel:SetText("Stage")

    page.phaseValue = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    page.phaseValue:SetPoint("LEFT", page.phaseLabel, "RIGHT", 12, 0)
    page.phaseValue:SetText("-")

    page.phaseBar = UI:CreateStatusBar(parent, width, 16, 0.85, 0.4, 0.1)
    page.phaseBar:SetPoint("TOPLEFT", 12, -84)

    page.phaseDesc = parent:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    page.phaseDesc:SetPoint("TOPLEFT", 12, -104)
    page.phaseDesc:SetText("")

    -- City leader ---------------------------------------------------------
    page.leaderLabel = parent:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    page.leaderLabel:SetPoint("TOPLEFT", 12, -130)
    page.leaderLabel:SetText("City Leader")

    page.leaderName = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
    page.leaderName:SetPoint("LEFT", page.leaderLabel, "RIGHT", 12, 0)
    page.leaderName:SetText("-")

    page.leaderBar = UI:CreateStatusBar(parent, width, 20, 0.1, 0.8, 0.1)
    page.leaderBar:SetPoint("TOPLEFT", 12, -150)

    local divider2 = UI:CreateDivider(parent, width)
    divider2:SetPoint("TOPLEFT", 12, -182)

    -- Numbers -------------------------------------------------------------
    page.stats = {}
    local rows = {
        { key = "time",      label = "Time remaining" },
        { key = "attackers", label = "Enemy forces standing" },
        { key = "defenders", label = "City garrison holding" },
        { key = "faction",   label = "Attacking faction" },
        { key = "route",     label = "Marching route" },
    }

    local y = -196
    for _, row in ipairs(rows) do
        page.stats[row.key] = UI:CreateStatLine(parent, row.label, y, 12)
        y = y - 20
    end

    -- Call to arms --------------------------------------------------------
    page.callToArms = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    page.callToArms:SetPoint("BOTTOMLEFT", 12, 12)
    page.callToArms:SetPoint("BOTTOMRIGHT", -12, 12)
    page.callToArms:SetJustifyH("LEFT")
    page.callToArms:SetText("")

    return page
end

--- Live siege data for the selected city, or nil.
local function GetSelectedSiege()
    if not currentCityID or not CitySiege_SiegeTracker then return nil end
    return CitySiege_SiegeTracker:GetSiege(currentCityID)
end

function MainFrame:RefreshOverview()
    if not frame or not frame:IsShown() or not overview then return end

    local cityData = currentCityID and CitySiege_CityData[currentCityID]
    local siege = GetSelectedSiege()

    if not cityData then
        overview.cityName:SetText("Select a city")
        overview.subtitle:SetText("Choose a city above to see its siege status.")
        frame.statusText:SetText(UI:Color("muted", "No city selected"))
        return
    end

    local cityColor = CitySiege_GetCityColorString(currentCityID)
    overview.cityName:SetText(cityColor .. cityData.displayName .. "|r")

    if not siege then
        overview.subtitle:SetText("This city is at peace.")
        overview.phaseValue:SetText(UI:Color("muted", "Not under siege"))
        overview.phaseDesc:SetText("")
        overview.phaseBar.bar:SetValue(0)
        overview.leaderName:SetText("-")
        UI:SetBarHealth(overview.leaderBar, 100)
        overview.leaderBar.bar.text:SetText("")

        for _, stat in pairs(overview.stats) do
            stat.value:SetText("-")
        end

        overview.callToArms:SetText("")
        frame.statusText:SetText(UI:Color("muted", "No siege in progress"))
        return
    end

    -- Stage ---------------------------------------------------------------
    local phase = siege.phase or 1
    local phaseName = PHASE_NAMES[phase] or "Unknown"
    overview.subtitle:SetText(string.format("Under siege - stage %d of 4", math.min(phase, 4)))
    overview.phaseValue:SetText(UI:Color("title", phaseName))
    overview.phaseDesc:SetText(PHASE_DESCRIPTIONS[phase] or "")
    overview.phaseBar.bar:SetValue(math.min(phase, 4) / 4 * 100)
    overview.phaseBar.bar.text:SetText(string.format("%d / 4", math.min(phase, 4)))

    -- Leader --------------------------------------------------------------
    overview.leaderName:SetText(siege.leaderName or "Unknown")
    local health = tonumber(siege.leaderHealth) or 0
    UI:SetBarHealth(overview.leaderBar, health)
    overview.leaderBar.bar.text:SetText(string.format("%.0f%%", health))

    -- Numbers -------------------------------------------------------------
    local remaining = tonumber(siege.remaining) or 0
    if siege.syncTime then
        remaining = math.max(0, remaining - (GetTime() - siege.syncTime))
    end

    overview.stats.time.value:SetText(CitySiege_Utils:FormatTime(remaining))
    overview.stats.attackers.value:SetText(tostring(siege.attackerCount or 0))
    overview.stats.defenders.value:SetText(tostring(siege.defenderCount or 0))

    local attackingFaction = siege.attackingFaction
        or (cityData.faction == "Alliance" and "Horde" or "Alliance")
    overview.stats.faction.value:SetText(
        CitySiege_GetFactionColorString(attackingFaction) .. attackingFaction .. "|r")

    local waypoints = siege.waypoints or {}
    overview.stats.route.value:SetText(#waypoints > 0
        and string.format("%d waypoints", #waypoints)
        or UI:Color("muted", "not received"))

    -- Call to arms --------------------------------------------------------
    local playerFaction = CitySiege_Utils:GetPlayerFaction()
    if playerFaction == cityData.faction then
        overview.callToArms:SetText(UI:Color("good",
            "Your faction holds this city. Defend it - every level can contribute."))
    elseif playerFaction == attackingFaction then
        overview.callToArms:SetText(UI:Color("danger",
            "Your faction is leading this assault. Join the war host at the muster point."))
    else
        overview.callToArms:SetText(UI:Color("muted", "You have no stake in this siege."))
    end

    frame.statusText:SetText(string.format("%s  |cFFFFFFFF%s|r",
        UI:Color("danger", "UNDER SIEGE"), CitySiege_Utils:FormatTime(remaining)))
end

-- ---------------------------------------------------------------------------
-- Tabs and selection
-- ---------------------------------------------------------------------------

function MainFrame:ShowTab(index)
    if not frame then return end

    for i, page in ipairs(frame.pages) do
        if i == index then page:Show() else page:Hide() end
    end

    PanelTemplates_SetTab(frame, index)

    if index == TAB_MAP then
        self:UpdateMapDisplay()
    elseif index == TAB_OVERVIEW then
        self:RefreshOverview()
    end
end

function MainFrame:SelectCity(cityID)
    currentCityID = cityID

    local cityData = cityID and CitySiege_CityData[cityID]
    if cityData then
        frame:SetTitle(string.format("City Siege - %s", cityData.displayName))
    else
        frame:SetTitle("City Siege")
    end

    if CitySiege_CommandPanel and CitySiege_CommandPanel.SetCity then
        CitySiege_CommandPanel:SetCity(cityID)
    end

    self:UpdateMapDisplay()
    self:RefreshOverview()
end

function MainFrame:UpdateMapDisplay()
    if not mapPanel or not currentCityID then return end

    CitySiege_MapDisplay:SetCity(currentCityID)

    -- Ask the server for this city's route so the map has something to draw.
    if CitySiege_EventHandler then
        CitySiege_EventHandler:ParseAddonMessage("REQUEST_MAP:" .. currentCityID)
    end
end

function MainFrame:Refresh()
    self:RefreshOverview()
    self:UpdateMapDisplay()
end

-- Kept for compatibility with callers elsewhere in the addon.
function MainFrame:UpdateSiegeDisplay()
    self:RefreshOverview()
    if mapPanel and mapPanel.UpdateDisplay then
        mapPanel:UpdateDisplay()
    end
end

function MainFrame:UpdateInfo()
    self:RefreshOverview()
end

-- ---------------------------------------------------------------------------
-- Window plumbing
-- ---------------------------------------------------------------------------

function MainFrame:Show()
    if not frame then self:CreateFrame() end
    frame:Show()
    frame:Raise()
    self:Refresh()
end

function MainFrame:Hide()
    if frame then frame:Hide() end
end

function MainFrame:Toggle()
    if not frame then
        self:Show()
    elseif frame:IsShown() then
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
    if point then
        CitySiege_Config:SetFramePosition("MainFrame", point, "UIParent", relativePoint, x, y)
    end
end

function MainFrame:LoadPosition()
    if not frame then return end
    local pos = CitySiege_Config:GetFramePosition("MainFrame")
    if pos and pos.point then
        frame:ClearAllPoints()
        frame:SetPoint(pos.point, UIParent, pos.relativePoint, pos.x, pos.y)
    end
end

function MainFrame:ApplySettings()
    if not frame then return end

    local settings = CitySiege_Config:GetUISettings()
    frame:SetScale(settings.scale or 1.0)
    frame:SetAlpha(settings.alpha or 1.0)
    frame.locked = settings.locked
end

function MainFrame:GetFrame()
    return frame
end
