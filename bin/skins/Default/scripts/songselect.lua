gfx.LoadSkinFont("segoeui.ttf");

function gfx_protected_call(f, ...)
    gfx.Save();
    f(...);
    gfx.Restore();
end

-- Configurable Settings

local useGridList = false;

-- Responsive Layout

local cachedLayoutInputWidth = 0;
local cachedLayoutInputHeight = 0;

-- target layout resolution, maintains display aspect ratio
local screenWidth = 0;
local screenHeight = 0;
-- Landscape or Portrait
local layout = "Landscape";
-- Font sizes
local fontSizeSmall  = 20;
local fontSizeMedium = 40;
local fontSizeLarge  = 60;

function adjust_layout()
    if cachedLayoutInputWidth == screenWidth and cachedLayoutInputHeight == screenHeight then
        return;
    end

    local aspect = screenWidth / screenHeight;

    if screenWidth < screenHeight then
        layout = "Portrait";

        local tScale = 0.75;
        fontSizeSmall  = math.floor(screenHeight * tScale * 0.02);
        fontSizeMedium = math.floor(screenHeight * tScale * 0.03);
        fontSizeLarge  = math.floor(screenHeight * tScale * 0.04);
    else
        layout = "Landscape";
        
        fontSizeSmall  = math.floor(screenHeight * 0.02);
        fontSizeMedium = math.floor(screenHeight * 0.03);
        fontSizeLarge  = math.floor(screenHeight * 0.04);
    end
end

-- Interface Dimensions

local infoPanel = { };
local chartListPanel = { };

-- Timers

local totalTime = 0;

-- Miscelaneous

local chartList = { };
local listIndex = 0;

local yPosWheel = 0
local xPosCurHilite = 0
local yPosCurHilite = 0

-- Render Functions

local testIndex = 1;

function render()
    gfx.ResetTransform();

    gfx_protected_call(render_chart_info, infoPanel.x, infoPanel.y, infoPanel.width, infoPanel.height);
    gfx_protected_call(render_chart_list, chartListPanel.x, chartListPanel.y, chartListPanel.width, chartListPanel.height);

    if SONGSELECT.searchStatus then
        gfx.BeginPath();
        gfx.FontSize(fontSizeSmall);
        gfx.TextAlign(gfx.TEXT_ALIGN_TOP, gfx.TEXT_ALIGN_LEFT);
        gfx.FillColor(255, 255, 255, 255);
        gfx.FastText(SONGSELECT.searchStatus, 0, 0);
    end
end

function render_chart_info(x, y, width, height)
    gfx.BeginPath();
    gfx.Rect(x + 20, y + 20, width - 40, height - 40);
    gfx.FillColor(20, 20, 20, 255);
    gfx.Fill();
end

function render_chart_list(x, y, width, height)
    gfx.BeginPath();
    gfx.Rect(x + 20, y + 20, width - 40, height - 40);
    gfx.FillColor(20, 20, 20, 255);
    gfx.Fill();

    if #chartList == 0 then
        return;
    end

    -- for the grid view
    local function get_abs_index(i, nCols)
        local listIndexAbs = -1;
        local listIndexAccum = 0;
        for gi = 1, #chartList do
            local cGroup = chartList[gi];

            if i <= listIndexAccum + #cGroup then
                listIndexAbs = listIndexAbs + i - listIndexAccum;
            else
                listIndexAccum = listIndexAccum + #cGroup;
                listIndexAbs = listIndexAbs + math.ceil(#cGroup / nCols) * nCols;
            end
        end
        return listIndexAbs;
    end

    if layout == "Landscape" then
        local groupHeaderHeight = screenHeight / 20;

        -- Left and right margin applied to the width.
        local xMargin = 25;

        if useGridList then
            -- grid view
        else
            -- wheel view
        end
    end
end

-- Update Functions

function update(deltaTime)
    totalTime = totalTime + deltaTime;
    
    screenWidth, screenHeight = game.GetResolution();
    adjust_layout();
    
    if layout == "Landscape" then
        infoPanel.x = 0;
        infoPanel.y = 0;
        infoPanel.width = screenHeight * 0.5;
        infoPanel.height = screenHeight;
        
        chartListPanel.x = infoPanel.width;
        chartListPanel.y = 0;
        chartListPanel.width = screenWidth - infoPanel.width;
        chartListPanel.height = screenHeight;
    else -- Portrait
        infoPanel.x = 0;
        infoPanel.y = 0;
        infoPanel.width = screenWidth;
        infoPanel.height = screenWidth * 0.5;
        
        chartListPanel.x = 0;
        chartListPanel.y = infoPanel.height;
        chartListPanel.width = screenWidth;
        chartListPanel.height = screenHeight - infoPanel.height;
    end
end

function key_pressed(key)
end

function key_released(key)
end

function chart_list_changed()
end
