-- This Lua code is executed every frame on the client,
-- and is responsible for drawing and handling the in-game user interface.
-- This way, the UI can be easily modded.

-- Global variable
frame = 0

-- This function is executed every frame
function CCUI_PerFrame()

	frame = frame + 1

	-- Draw the panels

	if getWKVersion() == GSVERSION_WKONE then
		local tpm = getWindowWidth() / 1024.0
		drawImage(11*tpm, 16, 1002*tpm, 45, "Interface/InGame/Top Panel/Standard Panel.tga")
	else
		drawImage(0, 0, getWindowWidth(), 85, "Interface/InGame/Top Panel/Standard Panel.tga")
	end
	drawImage(0, getWindowHeight()-326, 330, 326, "Interface/InGame/Bottom_Left_Panel.tga")
	drawImage(getWindowWidth()-345, getWindowHeight()-343, 345, 343, "Interface/InGame/Bottom_Right_Panel.tga")


	-- Top Panel Information

	player = getPlayerID()
	if player ~= 0 then
		local tpm = getWindowWidth() / 1024.0
		drawText(674*tpm, 24, tostring(math.floor(getItem(player, "Population Count"))))
		drawText(674*tpm, 42, tostring(math.floor(getItem(player, "Total Population Count"))))
		drawText(748*tpm, 24, tostring(getItem(player, "Food")))
		drawText(838*tpm, 24, tostring(getItem(player, "Wood")))
		drawText(930*tpm, 24, tostring(getItem(player, "Gold")))
		local color = getColor(player) % 8
		if color == 0 then color = 8 end
		drawImage((650+8) * tpm - 8, 22, 16, 36, "Interface/InGame/Top Panel/Player" .. tostring(color) .. ".tga")

		local foodCons = getItem(player, "Food Consumption")
		local foodProd = getItem(player, "Food Production")
		if foodProd > 0 then
			local foodRatio = math.min(1, foodCons / foodProd)
			if foodRatio < 1 then
				local leftWidth = math.floor(64*foodRatio + 0.5)
				drawRect(744*tpm, 42, leftWidth*tpm, 13, 0xFF004A7F)
				drawRect((744+leftWidth)*tpm, 42, (64-leftWidth)*tpm, 13, 0xFF007817)
			else
				local t = math.floor(getSystemTime() * 2)
				if (t & 1) == 0 then
					drawRect(744*tpm, 42, 64*tpm, 13, 0xFF0000FF)
				end
			end
		end
		local foodText = string.format("%i/%i", foodCons, foodProd)
		drawText(748*tpm, 42, foodText)
	end


	-- Selection

	sel = getFirstSelection()
	if sel ~= 0 then
		local ypos = getWindowHeight() - 190
		local h = 16
		local n = 0
		local bp = getBlueprint(sel)

	
		-- Selection information

		drawText(70, ypos, bp.name)
		drawText(71, ypos, bp.name)
		drawText(70, ypos + h, "ID: " .. tostring(sel))
		drawText(71, ypos + h, "ID: " .. tostring(sel))

		n = 2
		if bp.bpClass == GAMEOBJCLASS_CHARACTER then
			drawText(70, ypos + n*h, "HP: " .. tostring(getItem(sel, "Hit Points")) .. "/" .. tostring(getItem(sel, "Hit Point Capacity")))
			n = n+1
		elseif bp.bpClass == GAMEOBJCLASS_BUILDING then
			drawText(70, ypos + n*h, "SI: " .. tostring(getItem(sel, "Structural Integrity")) .. "/" .. tostring(getItem(sel, "Structural Integrity Capacity")))
			n = n+1
		end
		drawText(70, ypos + n*h, "Food: " .. tostring(getItem(sel, "Food"))); n=n+1
		drawText(70, ypos + n*h, "Wood: " .. tostring(getItem(sel, "Wood"))); n=n+1
		drawText(70, ypos + n*h, "Gold: " .. tostring(getItem(sel, "Gold"))); n=n+1


		-- Commands

		local commandButton = function (x, y, w, h, cs, assignMode, count)
			local mx = getMouseX()
			local my = getMouseY()
			local tex = cs.texpath
			if x <= mx and mx < x+w and y <= my and my < y+h then
				if cs.info == CSInfo.ENABLED then
					if isMouseDown(1) and cs.buttonDepressed ~= "" then
						tex = cs.buttonDepressed
					elseif cs.buttonHighlighted ~= "" then
						tex = cs.buttonHighlighted
					end
				end
				setTooltip(getCommandHint(cs.command, sel))
				local clicked = handleButton()
				if clicked then
					print("Clicked!")
					launchCommand(cs.command, assignMode, count)
				end
			end
			drawImage(x, y, w, h, tex)
		end

		-- Ordinary commands
		local x = 0
		local y = 0
		local vec = listSelectionCommands(function (x)
			return string.find(x, "Build ") ~= 1 and string.find(x, "Spawn ") ~= 1 and string.find(x, "Research ") ~= 1 and
			x ~= "Finish Building" and x ~= "Build"
		end )
		local vecLen = #vec
		-- Knuckles button positions, in pixel relative to top-left corner of Bottom_Left_Panel.tga
		local knucklesButtonX = { 14,  93, 173, 232, 251, 228 }
		local knucklesButtonY = { 37,  13,  31,  87, 167, 247 }
		local numKnucklesButtons = #knucklesButtonX
		for i = 1,vecLen do
			local cs = vec[i]
			local hi = 64
			if #cs.texpath > 0 then
				if i <= numKnucklesButtons then
					x = knucklesButtonX[i]
					y = getWindowHeight() - 326 + knucklesButtonY[i]
				end
				local assignMode = ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE
				if isCtrlHeld() then assignMode = ORDERASSIGNMODE_DO_FIRST
				elseif isShiftHeld() then assignMode = ORDERASSIGNMODE_DO_LAST end
				commandButton(x, y, hi, hi, cs, assignMode, 1)
				x = x+hi+2
			end
		end

		if bp.bpClass == GAMEOBJCLASS_CHARACTER then
			-- Build commands
			local vec = listSelectionCommands(function (x) return string.find(x, "Build ") == 1 end )
			for i = 1,#vec do
				local cs = vec[i]
				local hi = 64
				if #cs.texpath > 0 then
					x = 24 + ((i-1) % 5) * (hi+2)
					y = 64 + ((i-1) // 5) * (hi+2)
					local assignMode = ORDERASSIGNMODE_FORGET_EVERYTHING_ELSE
					if isCtrlHeld() then assignMode = ORDERASSIGNMODE_DO_FIRST
					elseif isShiftHeld() then assignMode = ORDERASSIGNMODE_DO_LAST end
					commandButton(x, y, hi, hi, cs, assignMode, 1)
				end
			end
		elseif bp.bpClass == GAMEOBJCLASS_BUILDING then
			-- Spawn commands
			local vec = listSelectionCommands(function (x) return string.find(x, "Spawn ") == 1 end )
			for i = 1,#vec do
				local cs = vec[i]
				local hi = 64
				if #cs.texpath > 0 then
					x = 24
					y = 64 + (i-1) * (hi+2)
					local count = 1
					if isShiftHeld() then count = 10 end
					commandButton(x, y, hi, hi, cs, ORDERASSIGNMODE_DO_LAST, count)
				end
			end

			-- Research commands
			local vec = listSelectionCommands(function (x) return string.find(x, "Research ") == 1 end )
			for i = 1,#vec do
				local cs = vec[i]
				local hi = 64
				if #cs.texpath > 0 then
					x = 24 + hi+2
					y = 64 + (i-1) * (hi+2)
					commandButton(x, y, hi, hi, cs, ORDERASSIGNMODE_DO_LAST, 1)
				end
			end
		end

	end

end -- function CCUI_PerFrame