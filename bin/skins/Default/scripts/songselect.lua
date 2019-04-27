gfx.LoadSkinFont("segoeui.ttf");

function gfx_protected_call(f, ...)
    gfx.Save();
    f(...);
    gfx.Restore();
end

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

-- Timers
local totalTime = 0;

-- Update Functions

function update(deltaTime)
    totalTime = totalTime + deltaTime;
    
    screenWidth, screenHeight = game.GetResolution();
    adjust_layout();
    
    if layout == "Landscape" then
        infoPanel.width = screenHeight * 0.5;
        infoPanel.height = screenHeight;
    else -- Portrait
        infoPanel.width = screenWidth;
        infoPanel.height = screenWidth * 0.5;
    end
end

function key_pressed(key)
end

function key_released(key)
end

-- Render Functions

function render()
    gfx.ResetTransform();

    gfx_protected_call(render_info_panel, infoPanel.width, infoPanel.height);
end

function render_info_panel(width, height)
    gfx.BeginPath();
    gfx.Rect(20, 20, width - 40, height - 40);
    gfx.FillColor(50, 50, 50, 255);
    gfx.Fill();
end
