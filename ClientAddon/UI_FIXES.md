# City Siege Addon - UI Fixes

## Issues Fixed

### 1. Frame Overlap and Padding Issues
**Problem:** Inner frames were overlapping the outer frame borders, creating a messy appearance.

**Solution:**
- Added proper padding to the main content area (10px from all edges)
- Content frames now use `SetPoint("TOPLEFT", 5, -5)` and `SetPoint("BOTTOMRIGHT", -5, 5)` for internal padding
- Increased main frame size to 850x600 for better layout space

### 2. Tab System - All Frames Showing at Once
**Problem:** All tab content frames were visible simultaneously when the main frame opened.

**Solution:**
- Modified `CreateTabContent()` to hide all frames by default using `:Hide()`
- Updated `ShowTab()` function to properly hide all content frames before showing the selected one
- Only one tab content is visible at a time now
- Default tab changed to "Commands" (tab 1) instead of "Map"

### 3. Map Display Issues
**Problem:** 
- Map had fixed positioning (TOPRIGHT) and fixed size
- Map wasn't filling the content area
- No indication when no siege was active

**Solution:**
- Changed map to use `SetAllPoints(parent)` to fill entire content area
- Removed fixed size constraints (450x450)
- Added proper padding from content frame edges (20px)
- Added "No active siege" message that shows when no city is selected or siege isn't active
- Map container now has visible border and darker background for better visibility
- Improved legend positioning at bottom of map

### 4. Command Panel Layout
**Problem:**
- Fixed positioning (TOPLEFT with offset)
- Fixed size (350x400)
- Buttons were positioned with negative Y offsets

**Solution:**
- Changed to `SetAllPoints(parent)` to fill content area
- Removed fixed positioning
- Created a centered button container for better layout
- Improved spacing between elements (40px between buttons)
- Better visual hierarchy with title, city selector, and buttons

### 5. Info and Stats Tabs Not Visible
**Problem:** Info and Stats tabs had no visible background and appeared empty.

**Solution:**
- Added visible backgrounds using `CitySiege_Utils:SetBackdrop()` with grey color
- Increased padding for text content (15-25px from edges)
- Both tabs now have clear visual presence when selected

## Layout Structure

```
Main Frame (850x600)
├── Title Bar (35px height)
│   ├── Title Text (center)
│   ├── Settings Button (right)
│   └── Close Button (far right)
├── City Dropdown (below title bar, 15px padding)
├── Tab Bar (4 tabs, 15px below dropdown)
└── Content Area (fills remaining space with 10px padding)
    ├── Inner padding: 5px on all sides
    └── Tab Content (one visible at a time):
        ├── Commands Tab (centered layout, fills area)
        ├── Map Tab (fills area with 20px internal padding)
        ├── Info Tab (scrollable, grey background)
        └── Stats Tab (grey background, centered title)
```

## Visual Improvements

1. **Consistent Backgrounds:**
   - Main frame: Black (0, 0, 0, 0.95)
   - Content area: Dark grey (0.05, 0.05, 0.05, 0.8)
   - Tab panels: Medium grey (0.1, 0.1, 0.1, 0.9)
   - Map container: Dark blue-grey (0.1, 0.1, 0.15, 1)

2. **Better Spacing:**
   - Title bar: 35px (increased from 30px)
   - Between sections: 15px
   - Button spacing: 40px (increased from 35px)
   - Edge padding: 10-20px depending on context

3. **Proper Tab Behavior:**
   - Selected tab uses "UI-Character-Tab-Selected" texture
   - Inactive tabs use "UI-Character-Tab-Inactive" texture
   - Text position adjusts based on tab state
   - Only one tab content visible at a time

## Testing Checklist

- [x] Main frame opens without overlapping borders
- [x] Only Commands tab shows by default
- [x] Clicking each tab switches content properly
- [x] Map tab fills entire content area
- [x] Map shows "No active siege" message when appropriate
- [x] Info tab has visible background and scrollable content
- [x] Stats tab has visible background and displays text
- [x] Command panel fills content area with centered buttons
- [x] All frames have proper padding from edges
- [x] No content overlaps outer frame borders

## Future Enhancements

1. **Map Improvements:**
   - Add actual city map textures (currently using colored background)
   - Implement proper coordinate calibration per city
   - Add zoom functionality
   - Add click-to-waypoint feature

2. **Command Panel:**
   - Add command history
   - Add quick action macros
   - Add siege template presets

3. **Info Tab:**
   - Add real-time updates
   - Add detailed participant list
   - Add timeline of siege events

4. **Stats Tab:**
   - Add graphs/charts for statistics
   - Add leaderboards
   - Add achievement tracking
