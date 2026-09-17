datablock AudioProfile(ammogetSound)
{
   filename = "./ammoget.wav";
   description = AudioClosest3d;
   preload = false;
};

package AmmoStaticPackage
{
	function ItemData::TT_initAmmoPickup(%this, %obj)
	{
		%obj.TT_ammoPickup = %this.TT_ammoPickup;
		%i = 0;
		while((%pickup = %this.TT_ammoPickup[%i]) !$= "")
		{
			%obj.TT_ammoPickup[%i] = %pickup;
			%i++;
		}
	}
	// For compatibility with add-ons that hook pickup like the event onItemPickup
	// Ammo items usually aren't supposed to go in your inventory but this makes it seem like they did
	function ShapeBase::pickup(%this, %obj, %amount)
	{
		if(%obj.TT_ammoPickup)
			return 1;
		return Parent::pickup(%this, %obj, %amount);
	}
	// Why onCollision and not ItemData::onPickup? onPickup is not triggered if you already have the item in inventory
	function Armor::onCollision(%this, %obj, %col, %vec, %force)
	{
		if(%col.TT_ammoPickup && %col.canPickup && %obj.getDamagePercent() < 1.0 && minigameCanUse(%obj, %col))
		{
			if(!$Pref::Server::TT::BotAmmoPickUp && %obj.getClassName() $= "AIPlayer")
				return;

			%consumePickup = false;
			%i = 0;
			while((%pickup = %col.TT_ammoPickup[%i]) !$= "")
			{
				%i++;
				%type = getWord(%pickup, 0);
				%amount = getWord(%pickup, 1);

				%pickedUp = TT_addAmmo(%obj, %type, %amount);
				if($Pref::Server::TT::FullAmmoPickUp || %pickedUp > 0)
					%consumePickup = true;
			}
			if(%consumePickup)
			{
				%sound = %col.getDatablock().TT_pickupSound;
				if(%sound $= "")
					%sound = AmmoGetSound;
				serverPlay3D(%sound, %obj.getPosition());
				if(isObject(%col.spawnBrick))
				{
					%col.fadeOut();
					%col.schedule(%col.spawnBrick.itemRespawnTime, fadeIn);
				}
				else
				{
					%col.schedule(10, delete);
				}
				%obj.pickup(%col);
			}
			return;
		}
		Parent::onCollision(%this, %obj, %col, %vec, %force);
	}
};
activatePackage(AmmoStaticPackage);

if($Pref::Server::TT::DisableTier1)
{
	return;
}
else if($Pref::Server::TT::DisableAmmoItems)
{
	warn("WARNING: Weapon_Package_Tier1 - ammo pickup items disabled");
	return;
}

/////////////////////////////////

datablock ItemData(rocketstaticItem)
{
	category = "Weapon";  // Mission editor category
	className = "Weapon"; // For inventory system
	shapeFile = "./ammo_rocket.dts";
	mass = 1;
	density = 0.2;
	elasticity = 0.2;
	friction = 0.6;
	emap = true;
	uiName = "Ammo, Rocket";
	doColorShift = false;
	colorShiftColor = "0.471 0.471 0.471 1.000";
	canDrop = true;

	TT_ammoPickup = true;
	TT_ammoPickup[0] = "rocket 1";
};

function rocketstaticItem::onAdd(%this, %obj)
{
	%this.TT_initAmmoPickup(%obj);
	%obj.rotate = true;
	%obj.setShapeName(getWord(%obj.TT_ammoPickup[0], 1));
	Parent::onAdd(%this, %obj);
}

/////////////////////////////////

datablock ItemData(bigstaticItem : rocketstaticItem)
{
	shapeFile = "./ammo_group.dts";
	uiName = "Ammo Pile";

	TT_ammoPickup = true;
	TT_ammoPickup[0] = "9MM -1"; // -1 for max ammo. makes sense
	TT_ammoPickup[1] = "556 -1";
	TT_ammoPickup[2] = "shotgun -1";
	TT_ammoPickup[3] = "270 -1";
	TT_ammoPickup[4] = "708 -1";
	TT_ammoPickup[5] = "880 -1";
	TT_ammoPickup[6] = "bomb -1";
	TT_ammoPickup[7] = "rocket -1";
	TT_ammoPickup[8] = "bolt -1";
};

function bigstaticItem::onAdd(%this, %obj)
{
	%this.TT_initAmmoPickup(%obj);
	Parent::onAdd(%this, %obj);
}

/////////////////////////////////

datablock ItemData(grenadestaticItem : rocketstaticItem)
{
	shapeFile = "./ammo_grenade.dts";
	uiName = "Ammo, Grenade";

	TT_ammoPickup = true;
	TT_ammoPickup[0] = "bomb 1";
};

function grenadestaticItem::onAdd(%this, %obj)
{
	%this.TT_initAmmoPickup(%obj);
	%obj.rotate = true;
	%obj.setShapeName(getWord(%obj.TT_ammoPickup[0], 1));
	Parent::onAdd(%this, %obj);
}

/////////////////////////////////

datablock ItemData(boltstaticItem : rocketstaticItem)
{
	shapeFile = "./ammo_bolt_.dts";
	uiName = "Ammo, Bolt";

	TT_ammoPickup = true;
	TT_ammoPickup[0] = "bolt 6";
};

function boltstaticItem::onAdd(%this, %obj)
{
	%this.TT_initAmmoPickup(%obj);
	%obj.rotate = true;
	%obj.setShapeName(getWord(%obj.TT_ammoPickup[0], 1));
	Parent::onAdd(%this, %obj);
}

/////////////////////////////////

datablock ItemData(threestaticItem : rocketstaticItem)
{
	shapeFile = "./ammo_rifle.dts";
	uiName = "Ammo, .270";

	TT_ammoPickup = true;
	TT_ammoPickup[0] = "270 8";
};

function threestaticItem::onAdd(%this, %obj)
{
	%this.TT_initAmmoPickup(%obj);
	%obj.rotate = true;
	%obj.setShapeName(getWord(%obj.TT_ammoPickup[0], 1));
	Parent::onAdd(%this, %obj);
}

/////////////////////////////////

datablock ItemData(fivestaticItem : rocketstaticItem)
{
	shapeFile = "./ammo_556.dts";
	uiName = "Ammo, 5.56";

	TT_ammoPickup = true;
	TT_ammoPickup[0] = "556 45";
};

function fivestaticItem::onAdd(%this, %obj)
{
	%this.TT_initAmmoPickup(%obj);
	%obj.rotate = true;
	%obj.setShapeName(getWord(%obj.TT_ammoPickup[0], 1));
	Parent::onAdd(%this, %obj);
}

/////////////////////////////////

datablock ItemData(sevenstaticItem : rocketstaticItem)
{
	shapeFile = "./ammo_708.dts";
	uiName = "Ammo, 7.08";

	TT_ammoPickup = true;
	TT_ammoPickup[0] = "708 48";
};

function sevenstaticItem::onAdd(%this, %obj)
{
	%this.TT_initAmmoPickup(%obj);
	%obj.rotate = true;
	%obj.setShapeName(getWord(%obj.TT_ammoPickup[0], 1));
	Parent::onAdd(%this, %obj);
}

/////////////////////////////////

datablock ItemData(ShotgunStaticItem : rocketstaticItem)
{
	shapeFile = "./ammo_shotgun.dts";
	uiName = "Ammo, Buckshot";

	TT_ammoPickup = true;
	TT_ammoPickup[0] = "shotgun 18";
};

function ShotgunStaticItem::onAdd(%this, %obj)
{
	%this.TT_initAmmoPickup(%obj);
	%obj.rotate = true;
	%obj.setShapeName(getWord(%obj.TT_ammoPickup[0], 1));
	Parent::onAdd(%this, %obj);
}

/////////////////////////////////

datablock ItemData(ninestaticItem : rocketstaticItem)
{
	shapeFile = "./ammo_9MM.dts";
	uiName = "Ammo, 9mm";

	TT_ammoPickup = true;
	TT_ammoPickup[0] = "9mm 70";
};

function ninestaticItem::onAdd(%this, %obj)
{
	%this.TT_initAmmoPickup(%obj);
	%obj.rotate = true;
	%obj.setShapeName(getWord(%obj.TT_ammoPickup[0], 1));
	Parent::onAdd(%this, %obj);
}

/////////////////////////////////

datablock ItemData(eightstaticItem : rocketstaticItem)
{
	shapeFile = "./ammo_MAGNUM.dts";
	uiName = "Ammo, .880M";

	TT_ammoPickup = true;
	TT_ammoPickup[0] = "880 12";
};

function eightstaticItem::onAdd(%this, %obj)
{
	%this.TT_initAmmoPickup(%obj);
	%obj.rotate = true;
	%obj.setShapeName(getWord(%obj.TT_ammoPickup[0], 1));
	Parent::onAdd(%this, %obj);
}

/////////////////////////////////
