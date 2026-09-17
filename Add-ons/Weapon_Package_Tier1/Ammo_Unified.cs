datablock ItemData(ammoDroppedItem)
{
	shapeFile = "./ammo_player_drop.dts";
	mass = 1;
	density = 0.2;
	elasticity = 0.2;
	friction = 0.6;
	emap = true;
	uiName = "";
	doColorShift = false;
	colorShiftColor = "0.471 0.471 0.471 1.000";
	canDrop = true;
};

function ammoDroppedItem::onAdd(%this, %obj)
{
	%obj.rotate = true;
	Parent::onAdd(%this, %obj);
}

package BigAmmoPackage
{
	function Armor::onDisabled(%this, %obj, %state)
	{
		%pos = %obj.getPosition();
		%vec = %obj.getVelocity();
		%i = new Item()
		{
			minigame = getMiniGameFromObject(%obj);
			datablock = ammoDroppedItem;
			canPickup = true;

			position = getword(%pos, 0) SPC getword(%pos, 1) SPC getword(%pos, 2) + 1;
		};
		MissionCleanup.add(%i);

		%count = TT_allAmmoTypesGroup.getCount();
		%k = 0;
		for(%j = 0; %j < %count; %j++)
		{
			%type = TT_allAmmoTypesGroup.getObject(%j);

			// Check if drop prefs apply to the ammo type
			// Example: if ammo is for a weapon, then prefs only allow drop if
			// either PlayerAmmoDrop is true and %obj is a player or BotAmmoDrop
			// is true and %obj is a bot
			// Likewise for grenades but with PlayerNadeDrop and BotNadeDrop
			%prefsAllowDrop = (($TT_ammoSet["weps"].isMember(%type)
								&& (($Pref::Server::TT::PlayerAmmoDrop && %obj.getClassName() $= "Player")
									|| ($Pref::Server::TT::BotAmmoDrop && %obj.getClassName() $= "AIPlayer")))
							|| ($TT_ammoSet["nades"].isMember(%type)
								&& (($Pref::Server::TT::PlayerNadeDrop && %obj.getClassName() $= "Player")
									|| ($Pref::Server::TT::BotNadeDrop && %obj.getClassName() $= "AIPlayer"))));

			%typeName = %type.name;
			%ammoCount = %obj.quantity[%typeName];
			// check if ammo type should be dropped, also check if player has any
			if(%prefsAllowDrop && %type.canDrop && %ammoCount > 0)
			{
				%i.TT_ammoPickup = true;
				%i.TT_ammoPickup[%k] = %typeName SPC %ammoCount;
				%k++;
			}
		}

		if(%i.TT_ammoPickup)
		{
			%itemvec = vectorAdd(%vec, getRandom(-8, 8) SPC getRandom(-8, 8) SPC 4);
			%time = 12000;
			%i.schedule(%time - 500, fadeout);
			%i.schedule(%time, delete);
			%i.setVelocity(%itemVec);
		}
		else
		{
			%i.delete();
		}
		Parent::onDisabled(%this, %obj, %state);
	}
};
activatePackage(BigAmmoPackage);
