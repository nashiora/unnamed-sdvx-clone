local function print(...)
	local result = "";
	for _, v in next, { ... } do
		result = result .. " " .. tostring(v);
	end
	game.Log(result, game.LOGGER_NORMAL);
end

-- Configuration Stuff, all in one place

local CONFIG = {
	UseGridList = false,
};

-- Graphics Additions

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
local chartListPanel = { };

-- Timers

local totalTime = 0;

-- Miscelaneous

game.LoadSkinSample("menu_click")

local chartList = { };

local groupIndex = 1;
local setIndex = 1;
local chartIndex = 1;

local yPosWheel = 0;
local xPosCurHilite = 0;
local yPosCurHilite = 0;

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
    gfx.Rect(x, y, width, height);
    gfx.FillColor(20, 20, 20, 255);
    gfx.Fill();

    if #chartList == 0 then
        return;
    end

    if layout == "Landscape" then
        local groupHeaderHeight = screenHeight / 20;

        -- Left and right margin applied to the width.
        local xMargin = 25;
		
		local groupInfos = { };
        if CONFIG.UseGridList then
			for gi = 1, #chartList do
			end

			local function handle_group(groupIndex)
			end
        else -- wheel view
			local numVisibleEntries = 10;
			local entryHeight = height / numVisibleEntries;

			for gi = 1, #chartList do
				groupInfos[gi] = {
					TotalHeight = (0.5 + #chartList[gi].sets) * entryHeight,
				};
			end

			local function handle_group(groupIndex)
				gfx.BeginPath();
				gfx.Rect(x + width / 2, y, width / 2, entryHeight * 0.5);
				gfx.FillColor(80, 80, 150, 255);
				gfx.Fill();

				for si = 1, #chartList[groupIndex].sets do
					local yPos = (si - 0.5) * entryHeight + entryHeight * 0.5;
					local xPos = (1 - yPos / height) * width / 2;
					
					gfx.BeginPath();
					gfx.Rect(x + xPos, y + yPos - entryHeight * 0.5 + entryHeight * 0.05, width * 0.8, entryHeight * 0.9);
					if si == setIndex then
						gfx.FillColor(255, 255, 255, 255);
					else
						gfx.FillColor(200, 200, 255, 255);
					end
					gfx.Fill();
				end
			end

			local primaryGroup = 1;

			handle_group(primaryGroup);
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
        infoPanel.width = screenHeight * 0.75;
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

function menu_navigate(direction, byPage)
	print(direction, byPage);
	if direction < 0 then -- up
		game.PlaySample("menu_click");
		if setIndex > 1 then
			setIndex = setIndex - 1;
		else
			if groupIndex > 1 then
				groupIndex = groupIndex - 1;
			else
				groupIndex = #chartList;
			end
			setIndex = #chartList[groupIndex].sets;
		end
	else -- down
		game.PlaySample("menu_click");
		if setIndex < #chartList[groupIndex].sets then
			setIndex = setIndex + 1;
		else
			if groupIndex < #chartList then
				groupIndex = groupIndex + 1;
			else
				groupIndex = 1;
			end
			setIndex = 1;
		end
	end

	print("[", groupIndex .. ", " .. setIndex .. ", " .. chartIndex, "]");
end

function button_pressed(button)
end

function button_released(button)
end

function chart_list_changed()
	chartList = SONGSELECT.chartList;
end
