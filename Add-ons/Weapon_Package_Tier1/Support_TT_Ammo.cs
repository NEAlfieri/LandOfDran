// Edit and rename of Support_AmmoGuns
// Includes a lot of other functions, such as giving ammo on spawn
package TT_Ammo
{
	///////////////////////////////////////////////////////////////////////////
	// Packaged default functions                                            //
	///////////////////////////////////////////////////////////////////////////

	function Player::pickup(%this, %item)
	{
		%data = %item.dataBlock;
		%ammo = %item.TT_weaponAmmoLoaded;

		// Resets the ammo of empty slots to deal with "remove item" events.
		if($Pref::Server::TT::RemoveItemBugfix)
		{
			for(%i = 0; %i < %this.dataBlock.maxTools; %i++)
			{
				if(!isObject(%this.tool[%i]))
				{
					%this.TT_toolAmmo[%i] = "";
					%this.TT_lastTool[%i] = "";
				}
			}
		}

		%val = Parent::pickup(%this,%item);
		if(%val == 1 && %data.TT_reloads && isObject(%this.client))
		{
			%slot = -1;
			for(%i = 0; %i < %this.dataBlock.maxTools; %i++)
			{
				if(isObject(%this.tool[%i]) && %this.tool[%i].getID() == %data.getID() && %this.TT_toolAmmo[%i] $= "")
				{
					%slot = %i;
					break;
				}
			}

			if(%slot == -1)
				return %val;

			if(%ammo $= "")
			{
				%this.TT_toolAmmo[%slot] = %data.TT_maxAmmo;
			}
			else
			{
				%this.TT_toolAmmo[%slot] = %ammo;
			}
			%this.TT_lastTool[%slot] = %data.getID();
		}
		return %val;
	}

	function servercmdDropTool(%client, %slot)
	{
		%pl = %client.player;
		if(!isObject(%pl))
			return Parent::servercmdDropTool(%client, %slot);

		%tool = %pl.tool[%slot];
		if(!isObject(%tool) || !%tool.TT_reloads)
			return Parent::servercmdDropTool(%client, %slot);

		// unmounts if the image is different from dropped item's image but image points to same item
		// intended for alt images such as zoomed in sniper carbine
		if(%pl.currTool == %slot)
		{
			%im = %pl.getMountedImage(0);
			if(isObject(%im) && %tool.image.getID() != %im && %tool == %im.item.getID())
				%pl.unmountImage(0);
		}

		$TT_weaponAmmoLoaded = %client.player.TT_toolAmmo[%slot];
		%client.player.TT_toolAmmo[%slot] = "";
		%obj.TT_lastTool[%slot] = "";

		return Parent::servercmdDropTool(%client,%slot);
	}

	function ItemData::onAdd(%this, %obj)
	{
		if($TT_weaponAmmoLoaded !$= "")
		{
			%obj.TT_weaponAmmoLoaded = $TT_weaponAmmoLoaded;
			$TT_weaponAmmoLoaded = "";
		}
		return Parent::onAdd(%this,%obj);
	}

	function Weapon::onUse(%this, %obj, %toolNum)
	{
		// For wielding multiple copies of a weapon
		// unused check: %obj.tool[%toolNum].image.getID() == %obj.tool[%obj.TT_lastToolNum].image.getID()
		if(%toolNum !$= %obj.TT_lastToolNum)
		{
			if(!%this.TT_reloads && %obj.TT_bottomPrint && isObject(%obj.tool[%toolNum].image))
			{
				%obj.TT_bottomPrint = "";
				clearBottomPrint(%obj.client);
			}
			if(%this.TT_reloads && %this.getID() == %obj.tool[%obj.TT_lastToolNum])
			{
				%obj.TT_lastToolNum = %toolNum;
				if($Pref::Server::TT::RemountDups)
				{
					cancel(%obj.TT_onUse);
					serverCmdUnUseTool(%obj.client);
					%obj.TT_onUse = schedule(33, 0, serverCmdUseTool, %obj.client, %toolNum);
					return;
				}
				else
				{
					if(%obj.TT_toolAmmo[%toolNum] $= "")
						%obj.TT_toolAmmo[%toolNum] = %this.TT_maxAmmo;
					%this.image.TT_displayAmmo(%obj);
				}
			}
		}
		return Parent::onUse(%this, %obj, %toolNum);
	}

	function serverCmdUnUseTool(%client)
	{
		if(%client.player.TT_bottomPrint)
		{
			%client.player.TT_bottomPrint = "";
			clearBottomPrint(%client);
		}
		return Parent::serverCmdUnUseTool(%client);
	}

	function gameConnection::onDeath(%client, %srcObj, %srcClient, %dmgType, %dmgLoc)
	{
		if(%client.player.TT_bottomPrint)
		{
			clearBottomPrint(%client);
		}
		return Parent::onDeath(%client, %srcObj, %srcClient, %dmgType, %dmgLoc);
	}

	function WeaponImage::onMount(%this, %obj, %slot)
	{
		if(%this.item.TT_reloads)
		{
			// If player has just spawned currTool will be ""
			if(%obj.currTool $= "")
			{
				%obj.currTool = -1;
			}
			%itemID = %this.item.getID();
			%toolNum = %obj.currTool;

			// Handles force mounted image when inventory slot is still selected
			if(%obj.tool[%toolNum] != %itemID && %obj.TT_lastTool[%toolNum] != %itemID)
			{
				%toolNum = -1;
			}

			if(!%obj.TT_retainImageAmmo)
			{
				// Check if inventory has updated without telling us (looking at you SetInventory event)
				if(%toolNum != -1 && %obj.TT_lastTool[%toolNum] != %itemID)
				{
					// Deal with SetInventory
					if($Pref::Server::TT::SetInvBugfix)
					{
						// What? You mean I can't load 70 shells in my shotgun?
						%obj.TT_toolAmmo[%toolNum] = %this.item.TT_maxAmmo;
					}
					%obj.TT_lastTool[%toolNum] = %itemID;
				}
				// If using one of the "force mount item" or addItem events, set ammo to maximum.
				if(%toolNum == -1 || %obj.TT_toolAmmo[%toolNum] $= "")
				{
					%obj.TT_toolAmmo[%toolNum] = %this.item.TT_maxAmmo;
				}
			}

			// For edge case where player has a weapon mounted but selects a tool with no image.
			//	Also for when an item is force mounted.
			%obj.TT_lastToolNum = %toolNum;

			// Last ditch check in case ammo gets screwed up in some way.
			if(%obj.TT_toolAmmo[%toolNum] < 0)
			{
				%obj.TT_toolAmmo[%toolNum] = 0;
			}
			else if($Pref::Server::TT::SetInvBugfix && %obj.TT_toolAmmo[%toolNum] > %this.item.TT_maxAmmo)
			{
				%obj.TT_toolAmmo[%toolNum] = %this.item.TT_maxAmmo;
			}
			%this.TT_displayAmmo(%obj);
		}
		else if(%this.item.TT_grenade)
		{
			%obj.TT_lastToolNum = %toolNum;
		}

		%obj.TT_bottomPrint = "";
		cancel(%obj.TT_onUse);
		%obj.TT_onUse = "";
		// Reset values used with alt image weapons.
		%obj.TT_retainImageAmmo = "";
		%obj.TT_forceToolReload = "";

		Parent::onMount(%this,%obj,%slot);
	}

	function WeaponImage::onUnMount(%this, %obj, %slot)
	{
		// Resets the ammo of empty slots to deal with "remove item" events.
		if($Pref::Server::TT::RemoveItemBugfix)
		{
			for(%i = 0; %i < %obj.dataBlock.maxTools; %i++)
			{
				if(!isObject(%obj.tool[%i]))
				{
					%obj.TT_toolAmmo[%i] = "";
					%obj.TT_lastTool[%i] = "";
				}
			}
		}

		if(%this.item.TT_reloads)
		{
			%this.TT_displayAmmo(%obj, -1);
		}
		else if(%this.item.TT_grenade)
		{
			if($Pref::Server::TT::RemoveOnNoNades && %this.TT_needsAmmo(%obj) && %obj.tool[%obj.TT_lastToolNum] == %this.item.getID())
			{
				%obj.tool[%obj.TT_lastToolNum] = 0;
				messageClient(%obj.client, 'MsgItemPickup', '', %obj.TT_lastToolNum, 0);
			}
			%this.TT_displayAmmo(%obj, -1);
		}

		Parent::onUnMount(%this,%obj,%slot);
	}

	function servercmdLight(%client)
	{
		if(isObject(%client.player) && isObject(%client.player.getMountedImage(0)))
		{
			%p = %client.player;
			%im = %p.getMountedImage(0);
			if(%im.item.TT_reloads && %im.TT_onUseLight(%p))
				return;
		}
		Parent::servercmdLight(%client);
	}

	// Remove ammo groups
	function onMissionEnded()
	{
		if(isObject(TT_allAmmoTypesGroup)) {
			TT_allAmmoTypesGroup.delete();
		}
		if(isObject(TT_ammoSetsGroup)) {
			TT_ammoSetsGroup.delete();
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Custom WeaponImage functions specific to Tier Tactical                //
	///////////////////////////////////////////////////////////////////////////

	function WeaponImage::TT_onUseLight(%this, %obj)
	{
		%toolNum = TT_getActiveTool(%obj);

		if(%obj.TT_toolAmmo[%toolNum] < %this.item.TT_maxAmmo)
		{
			if(%this.TT_canStartReload(%obj))
			{
				%state = %obj.getImageState(0);
				if(%state $= "Ready")
				{
					if(%obj.TT_toolAmmo[%toolNum] == 0)
						%obj.setImageAmmo(0, 1);
					else
						%obj.setImageAmmo(0, 0);
					%obj.setImageLoaded(0, 0);
				}
				else if(%state $= "Empty" || %state $= "EmptyFire")
				{
					%obj.setImageAmmo(0, 1);
				}
				return true;
			}
			else if($Pref::Server::TT::Ammo == 0 && !%this.TT_canReload(%obj))
			{
				%this.TT_displayAmmo(%obj);
			}
		}
		return false;
	}

	// optional: %duration (default $Pref::Server::TT::DisplayTime)
	function WeaponImage::TT_displayAmmo(%this, %obj, %duration)
	{
		if(%duration $= "")
		{
			%duration = $Pref::Server::TT::DisplayTime;
		}
		else if(%duration == -1)
		{
			clearBottomPrint(%obj.client);
			return;
		}

		if(%this.item.TT_reloads)
		{
			if($Pref::Server::TT::Ammo == 2 || !$Pref::Server::TT::DisplayAmmo)
				return;

			%toolNum = TT_getActiveTool(%obj);

			%desc = "<just:right><font:impact:24><color:fff000>" @ $TT_ammoType[%this.item.TT_ammoType].desc;
			if($Pref::Server::TT::Ammo == 0)
				%counter = "<font:impact:34>\c6" @ %obj.TT_toolAmmo[%toolNum] @ " / " @ %obj.quantity[%this.item.TT_ammoType];
			else if($Pref::Server::TT::Ammo == 1)
				%counter = "<font:impact:34>\c6" @ %obj.TT_toolAmmo[%toolNum] @ " / " @ %this.item.TT_maxAmmo;
			else if($Pref::Server::TT::Ammo == 3)
				%counter = "<font:impact:34>\c6" @ %obj.quantity[%this.item.TT_ammoType];
			bottomPrint(%obj.client, %desc SPC %counter, %duration, 1);
			%obj.TT_bottomPrint = 1;
		}
		else if(%this.item.TT_grenade)
		{
			if(!$Pref::Server::TT::DisplayNades)
				return;

			%desc = "<just:right><font:impact:24><color:fff000>" @ $TT_ammoType[%this.item.TT_ammoType].desc;
			if($Pref::Server::TT::PlayerInfNades && %obj.getClassName() $= "Player")
				%counter = "<font:impact:34>\c6 --";
			else
				%counter = "<font:impact:34>\c6 " @ %obj.quantity[%this.item.TT_ammoType];
			bottomPrint(%obj.client, %desc SPC %counter, %duration, 1);
			%obj.TT_bottomPrint = 1;
		}
	}

	// optional: %amount (default 1)
	function WeaponImage::TT_decrementAmmo(%this, %obj, %amount)
	{
		if(%amount $= "")
			%amount = 1;

		if(%this.item.TT_reloads)
		{
			%toolNum = TT_getActiveTool(%obj);
			%mandatory = %this.item.TT_alwaysReload || $Pref::Server::TT::AlwaysReload[%this.item.TT_alwaysReloadPref];

			if($Pref::Server::TT::Ammo == 0 || $Pref::Server::TT::Ammo == 1)
			{
				%obj.TT_toolAmmo[%toolNum] -=  %amount;
			}
			else if($Pref::Server::TT::Ammo == 2 && %mandatory)
			{
				%obj.TT_toolAmmo[%toolNum] -= %amount;
			}
			else if($Pref::Server::TT::Ammo == 3)
			{
				if(%mandatory)
					%obj.TT_toolAmmo[%toolNum] -= %amount;
				if(!$Pref::Server::TT::BotInfAmmo || !%obj.getClassName() $= "AIPlayer")
					%obj.quantity[%this.item.TT_ammoType] -= %amount;
			}
		}
		else if(%this.item.TT_grenade)
		{
			if($Pref::Server::TT::PlayerInfNades && %obj.getClassName() $= "Player"
			|| $Pref::Server::TT::BotInfNades && %obj.getClassName() $= "AIPlayer")
				return;
			%obj.quantity[%this.item.TT_ammoType] -= %amount;
		}
	}

	//
	//
	// RELOAD SEQUENCE LOL
	// now 50% more modular or so
	///////////////////////////////////////////////////////////////////////////////////
	// optional: %sound, %anim
	function WeaponImage::TT_reload(%this, %obj, %slot, %sound, %anim)
	{
		%mandatory = %this.item.TT_alwaysReload || $Pref::Server::TT::AlwaysReload[%this.item.TT_alwaysReloadPref];
		if(!%mandatory && ($Pref::Server::TT::Ammo == 2 || $Pref::Server::TT::Ammo == 3))
		{
			%obj.setImageLoaded(%slot, 1);
			return;
		}

		%toolNum = TT_getActiveTool(%obj);

		%obj.playThread(2, %anim);
		serverPlay3D(%sound,%obj.getPosition());

		if($Pref::Server::TT::Ammo == 0)
		{
			if($Pref::Server::TT::BotInfAmmo && %obj.getClassName() $= "AIPlayer")
			{
				%obj.TT_toolAmmo[%toolNum] = %this.item.TT_maxAmmo;
			}
			else
			{
				%obj.quantity[%this.item.TT_ammoType] += %obj.TT_toolAmmo[%toolNum];
				if(%obj.quantity[%this.item.TT_ammoType] > %this.item.TT_maxAmmo)
				{
					%obj.quantity[%this.item.TT_ammoType] -= %this.item.TT_maxAmmo;
					%obj.TT_toolAmmo[%toolNum] = %this.item.TT_maxAmmo;
				}
				else if(%obj.quantity[%this.item.TT_ammoType] <= %this.item.TT_maxAmmo)
				{
					%obj.TT_toolAmmo[%toolNum] = %obj.quantity[%this.item.TT_ammoType];
					%obj.quantity[%this.item.TT_ammoType] = 0;
				}
			}
		}
		else
		{
			%obj.TT_toolAmmo[%toolNum] = %this.item.TT_maxAmmo;
		}
		%obj.setImageLoaded(%slot, 1);
	}

	// optional: %amount (default 1), %sound, %anim
	function WeaponImage::TT_incrementReload(%this, %obj, %slot, %amount, %sound, %anim)
	{
		if($Pref::Server::TT::Ammo == 2 || $Pref::Server::TT::Ammo == 3)
			return;

		if(%amount $= "")
			%amount = 1;

		%toolNum = TT_getActiveTool(%obj);

		%obj.playThread(2, %anim);
		serverPlay3D(%sound, %obj.getPosition());

		if(%obj.TT_toolAmmo[%toolNum] + %amount > %this.item.TT_maxAmmo)
			%amount = %this.item.TT_maxAmmo - %obj.TT_toolAmmo[%toolNum];

		if($Pref::Server::TT::Ammo == 0)
		{
			if(!$Pref::Server::TT::BotInfAmmo || %obj.getClassName() !$= "AIPlayer")
			{
				if(%obj.quantity[%this.item.TT_ammoType] < %amount)
					%amount = %obj.quantity[%this.item.TT_ammoType];
				%obj.quantity[%this.item.TT_ammoType] -= %amount;
			}
			%obj.TT_toolAmmo[%toolNum] += %amount;
		}
		else if($Pref::Server::TT::Ammo == 1)
		{
			%obj.TT_toolAmmo[%toolNum] += %amount;
		}
	}

	// optional: %requiredAmmo (default 1)
	function WeaponImage::TT_canReload(%this, %obj, %requiredAmmo)
	{
		if(%requiredAmmo $= "")
			%requiredAmmo = 1;

		if($Pref::Server::TT::Ammo == 0)
		{
			if(%obj.quantity[%this.item.TT_ammoType] >= %requiredAmmo || $Pref::Server::TT::BotInfAmmo && %obj.getClassName() $= "AIPlayer")
				return true;
		}
		else if($Pref::Server::TT::Ammo == 1)
		{
			return true;
		}
		else if(%this.item.TT_alwaysReload || $Pref::Server::TT::AlwaysReload[%this.item.TT_alwaysReloadPref])
		{
			if($Pref::Server::TT::Ammo == 2)
			{
				return true;
			}
			else if($Pref::Server::TT::Ammo == 3)
			{
				if(%obj.quantity[%this.item.TT_ammoType] >= %requiredAmmo || $Pref::Server::TT::BotInfAmmo && %obj.getClassName() $= "AIPlayer")
					return true;
			}
		}
		return false;
	}

	// optional: %requiredAmmo (default 1)
	function WeaponImage::TT_needsAmmo(%this, %obj, %requiredAmmo)
	{
		if(%requiredAmmo $= "")
			%requiredAmmo = 1;

		if(%this.item.TT_reloads)
		{
			%toolNum = TT_getActiveTool(%obj);

			if(%this.item.TT_alwaysReload || $Pref::Server::TT::AlwaysReload[%this.item.TT_alwaysReloadPref])
			{
				if($Pref::Server::TT::Ammo == 3 && !($Pref::Server::TT::BotInfAmmo && %obj.getClassName() $= "AIPlayer"))
					return %obj.quantity[%this.item.TT_ammoType] < %requiredAmmo || %obj.TT_toolAmmo[%toolNum] < %requiredAmmo;
				return %obj.TT_toolAmmo[%toolNum] < %requiredAmmo;
			}

			if(($Pref::Server::TT::Ammo == 0 || $Pref::Server::TT::Ammo == 1) && %obj.TT_toolAmmo[%toolNum] >= %requiredAmmo)
			{
				return false;
			}
			else if($Pref::Server::TT::Ammo == 2)
			{
				return false;
			}
			else if($Pref::Server::TT::Ammo == 3)
			{
				if(%obj.quantity[%this.item.TT_ammoType] >= %requiredAmmo || $Pref::Server::TT::BotInfAmmo && %obj.getClassName() $= "AIPlayer")
					return false;
			}
			return true;
		}
		else if(%this.item.TT_grenade)
		{
			if($Pref::Server::TT::PlayerInfNades && %obj.getClassName() $= "Player"
			|| $Pref::Server::TT::BotInfNades && %obj.getClassName() $= "AIPlayer")
				return false;
			return %obj.quantity[%this.item.TT_ammoType] < %requiredAmmo;
		}
	}

	// optional: %requiredAmmo (default 1)
	function WeaponImage::TT_canFire(%this, %obj, %requiredAmmo)
	{
		if(%requiredAmmo $= "")
			%requiredAmmo = 1;

		return (!$Pref::Server::TT::DeathStopFiring || %obj.getDamagePercent() < 1.0) && !%this.TT_needsAmmo(%obj, %requiredAmmo);
	}

	// optional: %requiredAmmo (default 1)
	function WeaponImage::TT_canStartReload(%this, %obj, %requiredAmmo)
	{
		if(%requiredAmmo $= "")
			%requiredAmmo = 1;

		return %obj.getDamagePercent() < 1.0 && %this.TT_canReload(%obj, %requiredAmmo);
	}

	///////////////////////////////////////////////////////////////////////////
	// Custom WeaponImage functions specific to Tier Tactical                //
	// Functions below are intended to be used with StateScript              //
	// Primarily check functions that set ImageAmmo or ImageLoaded           //
	///////////////////////////////////////////////////////////////////////////

	function WeaponImage::TT_onLoadCheck(%this,%obj,%slot)
	{
		if(%this.TT_needsAmmo(%obj))
			%obj.setImageLoaded(%slot, 0);
		else
			%obj.setImageLoaded(%slot, 1);

		if(%this.TT_canStartReload(%obj))
			%obj.setImageAmmo(%slot, 1);
		else
			%obj.setImageAmmo(%slot, 0);
	}

	function WeaponImage::TT_onReloadCheck(%this,%obj,%slot)
	{
		if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		{
			%obj.setImageAmmo(%slot, 0);
			%obj.setImageLoaded(%slot, 0);
			return;
		}

		%toolNum = TT_getActiveTool(%obj);

		if(%this.TT_canReload(%obj))
		{
			if(%obj.TT_toolAmmo[%toolNum] < %this.item.TT_maxAmmo)
				%obj.setImageLoaded(%slot, 0);
			else
				%obj.setImageLoaded(%slot, 1);
			%obj.setImageAmmo(%slot, 1);
		}
		else
		{
			%obj.setImageAmmo(%slot, 0);
			%obj.setImageLoaded(%slot, 1);
		}
	}

	function WeaponImage::TT_onFireCheck(%this,%obj,%slot)
	{
		%toolNum = TT_getActiveTool(%obj);

		if(%this.TT_canFire(%obj))
			%obj.setImageLoaded(%slot, 1);
		else
			%obj.setImageLoaded(%slot, 0);

		if(%this.TT_canReload(%obj))
			%obj.setImageAmmo(%slot, 1);
		else
			%obj.setImageAmmo(%slot, 0);
	}

	// Sets ImageAmmo to 0 if player is dead to stop reload animations from playing
	function WeaponImage::TT_onDeathCheck(%this,%obj,%slot)
	{
		if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
			%obj.setImageAmmo(%slot, 0);
		else
			%obj.setImageAmmo(%slot, 1);
	}

	// dryfire but no clicking
	function WeaponImage::TT_onEmptyFire(%this, %obj, %slot)
	{
		%this.TT_displayAmmo(%obj);
	}
};
activatePackage(TT_Ammo);

///////////////////////////////////////////////////////////////////////////
// Functions to help with using ammo and ammo types                      //
///////////////////////////////////////////////////////////////////////////

// This deals with the case when you select an item that has no image but still have a TT weapon image out.
// Use whenever you want to determine which TT_toolAmmo slot to use (for example, decrementing ammo)
function TT_getActiveTool(%obj)
{
	if(isObject(%obj.tool[%obj.currTool].image) && %obj.tool[%obj.currTool].image.getID() == %obj.getMountedImage(0))
	{
		return %obj.currTool;
	}
	else
	{
		return %obj.TT_lastToolNum;
	}
}

function TT_initAmmo()
{
	if(!isObject(TT_allAmmoTypesGroup)) {
		new ScriptGroup(TT_allAmmoTypesGroup);
	}
	if(!isObject(TT_ammoSetsGroup)) {
		new ScriptGroup(TT_ammoSetsGroup);
	}
}

function TT_registerAmmoSet(%name, %longName, %isPack)
{
	if(!isObject($TT_ammoSet[%name]))
	{
		%set = new SimSet()
		{
			name = %name;
			longName = %longName;
			// does set contain all ammo types from a pack? e.g. all of t+t
			isPack = %isPack;
		};
		$TT_ammoSet[%name] = %set;
		TT_ammoSetsGroup.add(%set);
	}
}

function TT_registerAmmoType(%name, %longName, %desc, %canDrop, %set0, %set1, %set2, %set3)
{
	if(isObject($TT_ammoType[%name]))
		return;

	%type = new ScriptObject()
	{
		desc = %desc;
		canDrop = %canDrop;
		name = %name;
		longName = %longName;
	};
	$TT_ammoType[%name] = %type;
	TT_allAmmoTypesGroup.add(%type);

	for(%i = 0; %i < 4; %i++)
	{
		%setName = %set[%i];
		if(%setName !$= "" && isObject($TT_ammoSet[%setName]))
		{
			$TT_ammoSet[%setName].add(%type);
		}
	}
}

package TT_AmmoSpawn
{
	function Armor::onAdd(%this,%obj)
	{
		Parent::onAdd(%this,%obj);
		%count = TT_allAmmoTypesGroup.getCount();
		for(%i = 0; %i < %count; %i++)
		{
			%typeName = TT_allAmmoTypesGroup.getObject(%i).name;
			%obj.quantity[%typeName] = $Pref::Server::TT::Start[%typeName];
		}
	}
};
activatePackage(TT_AmmoSpawn);

// %ignoreMax (optional, default false)
function TT_addAmmo(%obj, %type, %amount, %ignoreMax)
{
	%currAmt = %obj.quantity[%type];
	%max = $Pref::Server::TT::Max[%type];
	if(%amount == -1)
	{
		// -1 turns into the max capacity for that ammo type
		%amount = %max;
	}
	else if(%amount < 0)
	{
		// other negative values aren't valid
		return 0;
	}

	if(!%ignoreMax)
	{
		// if %max - %currAmt < 0, mClamp should default to min value (0 in this case)
		%amount = mClamp(%amount, 0, %max - %currAmt);
	}
	%obj.quantity[%type] += %amount;
	if(%amount == 0)
	{
		// if nothing happened avoid notifying the player
		return 0;
	}

	if(isObject(%image = %obj.getMountedImage(0)) && %image.item.TT_ammoType $= %type)
	{
		if(%image.item.TT_reloads && ($Pref::Server::TT::Ammo == 0 || $Pref::Server::TT::Ammo == 3))
		{
			%image.TT_displayAmmo(%obj);

			%state = %obj.getImageState(0);
			if(%state $= "Empty" || %state $= "EmptyFire")
			{
				if($Pref::Server::TT::Ammo == 0 && %image.TT_canStartReload(%obj))
				{
					%obj.setImageAmmo(0, 1);
				}
				else if($Pref::Server::TT::Ammo == 3)
				{
					if(!%image.TT_needsAmmo(%obj))
						%obj.setImageLoaded(0, 1);
					else if(%image.TT_canStartReload(%obj))
						%obj.setImageAmmo(0, 1);
				}
			}
		}
		else if(!$Pref::Server::TT::PlayerInfNades && %image.item.TT_grenade)
		{
			%image.TT_displayAmmo(%obj, 0);
		}
	}
	else if(isObject(%toolID = %obj.tool[%obj.currTool]) && %toolID.TT_grenade && %toolID.TT_ammoType $= %type)
	{
		%image = %toolID.image;
		if(!%image.TT_needsAmmo(%obj))
		{
			%obj.updateArm(%image);
			%obj.mountImage(%image, 0);
		}
	}
	return %amount;
}
