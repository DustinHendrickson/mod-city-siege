--[[
    City Siege Addon - UI toolkit

    Blizzard-style widget factory. Everything the addon draws goes through here so
    the whole interface matches the 3.3.5 default UI: DialogBox frames with a
    header plaque, recessed insets for content, CharacterFrame tabs, UIPanel
    buttons and the standard font colours.
]]

CitySiege_UI = {}
local UI = CitySiege_UI

-- Blizzard's dialog chrome ---------------------------------------------------

local DIALOG_BACKDROP = {
    bgFile   = "Interface\\DialogFrame\\UI-DialogBox-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 32,
    insets = { left = 11, right = 12, top = 12, bottom = 11 },
}

local INSET_BACKDROP = {
    bgFile   = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true, tileSize = 16, edgeSize = 16,
    insets = { left = 4, right = 4, top = 4, bottom = 4 },
}

local BAR_BACKDROP = {
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    edgeSize = 12,
    insets = { left = 3, right = 3, top = 3, bottom = 3 },
}

UI.BAR_TEXTURE = "Interface\\TargetingFrame\\UI-StatusBar"

-- Shared colours, matched to the default UI where one exists.
UI.Colors = {
    title      = { r = 1.00, g = 0.82, b = 0.00 },  -- NORMAL_FONT_COLOR
    label      = { r = 1.00, g = 0.82, b = 0.00 },
    value      = { r = 1.00, g = 1.00, b = 1.00 },
    muted      = { r = 0.65, g = 0.65, b = 0.65 },
    alliance   = { r = 0.00, g = 0.44, b = 0.87 },
    horde      = { r = 0.77, g = 0.12, b = 0.23 },
    good       = { r = 0.10, g = 0.80, b = 0.10 },
    warning    = { r = 1.00, g = 0.82, b = 0.00 },
    danger     = { r = 0.90, g = 0.15, b = 0.15 },
}

function UI:Color(key, text)
    local c = self.Colors[key] or self.Colors.value
    return string.format("|cFF%02X%02X%02X%s|r", c.r * 255, c.g * 255, c.b * 255, text)
end

-- Tooltips -------------------------------------------------------------------

function UI:SetTooltip(frame, title, ...)
    local lines = { ... }

    frame:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText(title, 1, 1, 1)
        for _, line in ipairs(lines) do
            if line then
                GameTooltip:AddLine(line, 0.8, 0.8, 0.8, true)
            end
        end
        GameTooltip:Show()
    end)

    frame:SetScript("OnLeave", function() GameTooltip:Hide() end)
end

-- Windows --------------------------------------------------------------------

--- Creates a movable dialog window with Blizzard chrome.
-- @param name    global frame name (required for tabs and UISpecialFrames)
-- @param title   text shown on the header plaque
-- @param width   frame width
-- @param height  frame height
-- @param opts    optional { closeOnEscape = true, onHide = func, onShow = func }
function UI:CreateWindow(name, title, width, height, opts)
    opts = opts or {}

    local frame = CreateFrame("Frame", name, UIParent)
    frame:SetSize(width, height)
    frame:SetPoint("CENTER")
    frame:SetFrameStrata("MEDIUM")
    frame:SetToplevel(true)
    frame:SetClampedToScreen(true)
    frame:EnableMouse(true)
    frame:SetMovable(true)
    frame:RegisterForDrag("LeftButton")
    frame:SetBackdrop(DIALOG_BACKDROP)
    frame:Hide()

    frame:SetScript("OnDragStart", function(self)
        if not self.locked then
            self:StartMoving()
        end
    end)

    frame:SetScript("OnDragStop", function(self)
        self:StopMovingOrSizing()
        if self.onPositionChanged then
            self:onPositionChanged()
        end
    end)

    -- Header plaque, the same art the quest and merchant frames use.
    local header = frame:CreateTexture(nil, "ARTWORK")
    header:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
    header:SetTexCoord(0.2, 0.8, 0, 0.6)
    header:SetPoint("TOP", 0, 12)
    frame.header = header

    local headerText = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    headerText:SetPoint("TOP", header, "TOP", 0, -14)
    headerText:SetText(title)
    frame.headerText = headerText

    -- The plaque is sized to its text, exactly like StaticPopup does it.
    header:SetWidth(math.max(headerText:GetStringWidth() + 90, 160))
    header:SetHeight(48)

    local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", -6, -6)
    frame.closeButton = close

    frame:SetScript("OnShow", function(self)
        PlaySound("igCharacterInfoOpen")
        if opts.onShow then opts.onShow(self) end
    end)

    frame:SetScript("OnHide", function(self)
        PlaySound("igCharacterInfoClose")
        if opts.onHide then opts.onHide(self) end
    end)

    if opts.closeOnEscape ~= false and name then
        tinsert(UISpecialFrames, name)
    end

    function frame:SetTitle(text)
        self.headerText:SetText(text)
        self.header:SetWidth(math.max(self.headerText:GetStringWidth() + 90, 160))
    end

    return frame
end

--- Recessed content well, as used inside the character and quest frames.
function UI:CreateInset(parent, insetLeft, insetRight, insetTop, insetBottom)
    local inset = CreateFrame("Frame", nil, parent)
    inset:SetPoint("TOPLEFT", insetLeft or 14, -(insetTop or 60))
    inset:SetPoint("BOTTOMRIGHT", -(insetRight or 14), insetBottom or 14)
    inset:SetBackdrop(INSET_BACKDROP)
    inset:SetBackdropColor(0, 0, 0, 0.55)
    inset:SetBackdropBorderColor(0.5, 0.5, 0.5, 1)
    return inset
end

-- Tabs -----------------------------------------------------------------------

--- Adds Blizzard character-frame tabs along the bottom of a window.
-- @param frame    a window created with a global name
-- @param labels   array of tab captions
-- @param onSelect function(index) called when a tab is chosen
function UI:CreateTabs(frame, labels, onSelect)
    local name = frame:GetName()
    local tabs = {}

    for index, label in ipairs(labels) do
        local tab = CreateFrame("Button", name .. "Tab" .. index, frame, "CharacterFrameTabButtonTemplate")
        tab:SetID(index)
        tab:SetText(label)

        if index == 1 then
            tab:SetPoint("TOPLEFT", frame, "BOTTOMLEFT", 14, 6)
        else
            tab:SetPoint("LEFT", tabs[index - 1], "RIGHT", -14, 0)
        end

        tab:SetScript("OnClick", function(self)
            PanelTemplates_SetTab(frame, self:GetID())
            PlaySound("igCharacterInfoTab")
            if onSelect then onSelect(self:GetID()) end
        end)

        PanelTemplates_TabResize(tab, 0)
        tabs[index] = tab
    end

    PanelTemplates_SetNumTabs(frame, #labels)
    PanelTemplates_SetTab(frame, 1)

    frame.tabs = tabs
    return tabs
end

-- Text -----------------------------------------------------------------------

function UI:CreateHeading(parent, text, point, relativeTo, relativePoint, x, y)
    local heading = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    heading:SetPoint(point or "TOPLEFT", relativeTo or parent, relativePoint or "TOPLEFT", x or 0, y or 0)
    heading:SetText(text)
    heading:SetTextColor(UI.Colors.title.r, UI.Colors.title.g, UI.Colors.title.b)
    return heading
end

--- Subtle horizontal rule between sections.
function UI:CreateDivider(parent, width)
    local divider = parent:CreateTexture(nil, "ARTWORK")
    divider:SetTexture(1, 1, 1, 0.12)
    divider:SetHeight(1)
    if width then
        divider:SetWidth(width)
    end
    return divider
end

--- A label on the left and a value on the right, the standard tooltip layout.
function UI:CreateStatLine(parent, label, yOffset, indent)
    indent = indent or 12

    local left = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    left:SetPoint("TOPLEFT", indent, yOffset)
    left:SetText(label)
    left:SetTextColor(UI.Colors.label.r, UI.Colors.label.g, UI.Colors.label.b)

    local right = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    right:SetPoint("TOPRIGHT", -indent, yOffset)
    right:SetJustifyH("RIGHT")
    right:SetText("-")

    return { label = left, value = right }
end

-- Controls -------------------------------------------------------------------

function UI:CreateButton(parent, text, width, height, onClick)
    local button = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
    button:SetSize(width or 120, height or 22)
    button:SetText(text)
    if onClick then
        button:SetScript("OnClick", function(self)
            PlaySound("igMainMenuOptionCheckBoxOn")
            onClick(self)
        end)
    end
    return button
end

function UI:CreateStatusBar(parent, width, height, r, g, b)
    local holder = CreateFrame("Frame", nil, parent)
    holder:SetSize(width, height)
    holder:SetBackdrop(BAR_BACKDROP)
    holder:SetBackdropBorderColor(0.6, 0.6, 0.6, 1)

    local bar = CreateFrame("StatusBar", nil, holder)
    bar:SetPoint("TOPLEFT", 3, -3)
    bar:SetPoint("BOTTOMRIGHT", -3, 3)
    bar:SetStatusBarTexture(UI.BAR_TEXTURE)
    bar:SetStatusBarColor(r or 0.1, g or 0.8, b or 0.1)
    bar:SetMinMaxValues(0, 100)
    bar:SetValue(0)

    local background = bar:CreateTexture(nil, "BACKGROUND")
    background:SetAllPoints()
    background:SetTexture(UI.BAR_TEXTURE)
    background:SetVertexColor(0.15, 0.15, 0.15, 0.9)

    local text = bar:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    text:SetPoint("CENTER")
    bar.text = text

    holder.bar = bar
    return holder
end

--- Recolours a bar green -> yellow -> red as its value drops.
function UI:SetBarHealth(holder, percent)
    local bar = holder.bar
    bar:SetValue(percent or 0)

    if percent > 60 then
        bar:SetStatusBarColor(0.10, 0.80, 0.10)
    elseif percent > 30 then
        bar:SetStatusBarColor(1.00, 0.82, 0.00)
    else
        bar:SetStatusBarColor(0.90, 0.15, 0.15)
    end
end

--- Standard Blizzard dropdown with a fixed list of choices.
-- @param entries array of { text = ..., value = ... }
function UI:CreateDropdown(parent, name, width, entries, onSelect, placeholder)
    local dropdown = CreateFrame("Frame", name, parent, "UIDropDownMenuTemplate")

    UIDropDownMenu_Initialize(dropdown, function(self, level)
        for _, entry in ipairs(entries) do
            local info = UIDropDownMenu_CreateInfo()
            info.text = entry.text
            info.value = entry.value
            info.checked = (UIDropDownMenu_GetSelectedValue(dropdown) == entry.value)
            info.func = function(button)
                UIDropDownMenu_SetSelectedValue(dropdown, button.value)
                UIDropDownMenu_SetText(dropdown, entry.text)
                if onSelect then onSelect(button.value) end
            end
            UIDropDownMenu_AddButton(info, level)
        end
    end)

    UIDropDownMenu_SetWidth(dropdown, width or 150)
    UIDropDownMenu_SetText(dropdown, placeholder or "")

    return dropdown
end
