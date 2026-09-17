//////////
// item //
//////////
datablock ItemData(AkimboPistolItem)
{
	category = "Weapon";  // Mission editor category
	className = "Weapon"; // For inventory system

	 // Basic Item Properties
	shapeFile = "./pistol_.dts";
	rotate = false;
	mass = 1;
	density = 0.2;
	elasticity = 0.2;
	friction = 0.6;
	emap = true;

	//gui stuff
	uiName = "Pistols Akimbo";
	iconName = "./pistolsakimbo";
	doColorShift = true;
	colorShiftColor = "0.7 0.7 0.74 1.000";

	 // Dynamic properties defined by the scripts
	image = AkimboPistolImage;
	canDrop = true;
	
	TT_ammoType = "9MM";
	TT_reloads = true;
	TT_maxAmmo = 30;
};

////////////////
//weapon image//
////////////////
AddDamageType("AkimboL4Pistol",   '<bitmap:add-ons/Weapon_Package_Tier1/CI_DualPistol> %1',    '%2 <bitmap:add-ons/Weapon_Package_Tier1/CI_DualPistol> %1',0.75,1);
datablock ShapeBaseImageData(AkimboPistolImage)
{
   // Basic Item properties
	shapeFile = "./pistol_.dts";
   emap = true;

   // Specify mount point & offset for 3rd person, and eye offset
   // for first person rendering.
   mountPoint = 0;
   offset = "0 0 0";
   eyeOffset = 0; //"0.7 1.2 -0.5";
   rotation = eulerToMatrix( "0 0 0" );

   // When firing from a point offset from the eye, muzzle correction
   // will adjust the muzzle vector to point to the eye LOS point.
   // Since this weapon doesn't actually fire from the muzzle point,
   // we need to turn this off.  
   correctMuzzleVector = true;

   // Add the WeaponImage namespace as a parent, WeaponImage namespace
   // provides some hooks into the inventory system.
   className = "WeaponImage";

   // Projectile && Ammo.
   item = AkimboPistolItem;
   ammo = " ";
   projectile = pistolTracerProjectile;
   projectileType = Projectile;

	casing = gunShellDebris;
	shellExitDir        = "1.0 -1.3 1.0";
	shellExitOffset     = "0 0 0";
	shellExitVariance   = 15.0;	
	shellVelocity       = 7.0;

   //melee particles shoot from eye node for consistancy
   melee = false;
   //raise your arm up or not
   armReady = true;

   doColorShift = true;
   colorShiftColor = AkimboPistolItem.colorShiftColor;//"0.400 0.196 0 1.000";

   //casing = " ";

   // Images have a state system which controls how the animations
   // are run, which sounds are played, script callbacks, etc. This
   // state system is downloaded to the client so that clients can
   // predict state changes and animate accordingly.  The following
   // system supports basic ready->fire->reload transitions as
   // well as a no-ammo->dryfire idle state.
   TT_raycastEnabled = true;
   TT_raycastWeaponRange = 200; //varies
   TT_raycastWeaponTargets =
                   $TypeMasks::PlayerObjectType |    //AI/Players
                   $TypeMasks::StaticObjectType |    //Static Shapes
                   $TypeMasks::TerrainObjectType |    //Terrain
                   $TypeMasks::VehicleObjectType |    //Terrain
                   $TypeMasks::FXBrickObjectType;    //Bricks
   TT_raycastExplosionProjectile = GunProjectile;
   TT_raycastExplosionBrickSound = bulletHitSound;
   TT_raycastExplosionPlayerSound = bulletHitSound;
   TT_raycastDirectDamage = 10; //10
   TT_raycastDirectDamageType = $DamageType::AkimboL4Pistol;
   TT_raycastSpreadAmt = 0.0001; //varies
   TT_raycastSpreadCount = 1;
   TT_raycastTracerProjectile = pistolTracerProjectile;
   TT_raycastFromMuzzle = true;

   // Initial start up state
	stateName[0]                    = "Activate";
	stateTimeoutValue[0]            = 0.15;
	stateTransitionOnTimeout[0]     = "LoadCheckA";
	stateSound[0]			  = weaponSwitchSound;

	stateName[1]                    = "Ready";
	stateTransitionOnNotLoaded[1]	= "ManualReload";
	stateTransitionOnTriggerDown[1] = "FireCheckA";

	stateName[2]                    = "Fire";
	stateTransitionOnTimeout[2]     = "Smoke";
	stateTimeoutValue[2]            = 0.02;
	stateFire[2]                    = true;
	stateAllowImageChange[2]        = false;
	stateScript[2]                  = "onFire";
	stateEjectShell[2]       = true;
	stateSequence[2]			  = "Fire";
	stateEmitter[2]			  = gunFlashEmitter;
	stateEmitterTime[2]		  = 0.05;
	stateEmitterNode[2]		  = "muzzleNode";
	stateSound[2]			  = PistolfireSound;

	stateName[3] 			  = "Smoke";
	stateEmitter[3]			  = gunSmokeEmitter;
	stateEmitterTime[3]		  = 0.1;
	stateEmitterNode[3]		  = "muzzleNode";
	stateTimeoutValue[3]            = 0.01;
	stateTransitionOnTimeout[3]     = "Wait";

	stateName[4]			  = "Wait";
	stateTimeoutValue[4]		  = 0.045;
	stateTransitionOnTimeout[4]	  = "AkimboFireCheckA";

	stateName[5]			  = "PistolsAkimbo";
	stateTimeoutValue[5]		  = 0.10;
	stateScript[5] = "onFireAkimbo";
	stateTransitionOnTimeout[5]	  = "LoadCheckA";
	
	stateName[6]				= "LoadCheckA";
	stateScript[6]				= "TT_onLoadCheck";
	stateTimeoutValue[6]			= 0.01;
	stateTransitionOnTimeout[6]		= "LoadCheckB";
	
	stateName[7]				= "LoadCheckB";
	stateTransitionOnLoaded[7]		= "Ready";
	stateTransitionOnNotLoaded[7]      = "Empty";
	
	stateName[8]				= "AkimboFireCheckA";
	stateScript[8]				= "TT_onFireCheck";
	stateTimeoutValue[8]			= 0.01;
	stateTransitionOnTimeout[8]		= "AkimboFireCheckB";
	
	stateName[9]				= "AkimboFireCheckB";
	stateTransitionOnLoaded[9]		= "PistolsAkimbo";
	stateTransitionOnNotLoaded[9]		= "EmptyFire";
	
	stateName[10]				= "ReloadWait";
	stateTimeoutValue[10]			= 0.3;
	stateTransitionOnTimeout[10]		= "ReloadStart";
	
	stateName[11]				= "ReloadStart";
	stateTimeoutValue[11]			= 1.6;
	stateScript[11]				= "onReloadStart";
	stateTransitionOnTimeout[11]		= "ReloadNext";
	
	stateName[13]				= "ReloadNext";
	stateTimeoutValue[13]			= 0.45;
	stateScript[13]				= "onReloadNext";
	stateTransitionOnTimeout[13]		= "Reloaded";
	
	stateName[14]				= "Reloaded";
	stateTimeoutValue[14]			= 0.3;
	stateScript[14]				= "onReloaded";
	stateTransitionOnTimeout[14]		= "Ready";

	stateName[15]                   = "ManualReload";
	stateTransitionOnAmmo[15]  = "ReloadWait";
	stateTransitionOnNoAmmo[15]     = "ReloadStart";

	stateName[16]                    = "FireCheckA";
	stateScript[16]                  = "TT_onFireCheck";
	stateTransitionOnTimeout[16]     = "FireCheckB";

	stateName[17]                    = "FireCheckB";
	stateTransitionOnLoaded[17]   = "Fire";
	stateTransitionOnNotLoaded[17]      = "EmptyFire";

	stateName[18]                    = "Empty";
	stateTransitionOnLoaded[18]      = "Ready";
	stateTransitionOnAmmo[18]        = "ReloadWait";
	stateTransitionOnTriggerDown[18] = "FireCheckA";

	stateName[19]                    = "EmptyFire";
	stateScript[19]                  = "TT_onEmptyFire";
	stateTransitionOnLoaded[19]      = "Ready";
	stateTransitionOnAmmo[19]        = "ReloadWait";
	stateTransitionOnTriggerUp[19]   = "Empty";
};

datablock ShapeBaseImageData(LeftHandedPistolImage)
{
   // Basic Item properties
	shapeFile = "Add-Ons/Weapon_Package_Tier1/pistol_.dts";
   emap = true;

   // Specify mount point & offset for 3rd person, and eye offset
   // for first person rendering.
   mountPoint = 1;
   offset = "0 0 0";
   eyeOffset = 0; //"0.7 1.2 -0.5";
   rotation = eulerToMatrix( "0 0 0" );

   // When firing from a point offset from the eye, muzzle correction
   // will adjust the muzzle vector to point to the eye LOS point.
   // Since this weapon doesn't actually fire from the muzzle point,
   // we need to turn this off.  
   correctMuzzleVector = true;

   // Add the WeaponImage namespace as a parent, WeaponImage namespace
   // provides some hooks into the inventory system.
   className = "WeaponImage";

   // Projectile && Ammo.
   item = AkimboPistolItem;
   ammo = " ";
   projectile = pistolTracerProjectile;
   projectileType = Projectile;

	casing = gunShellDebris;
	shellExitDir        = "1.0 -1.3 1.0";
	shellExitOffset     = "0 0 0";
	shellExitVariance   = 15.0;	
	shellVelocity       = 7.0;

   //melee particles shoot from eye node for consistancy
   melee = false;
   //raise your arm up or not
   armReady = false;

   doColorShift = true;
   colorShiftColor = AkimboPistolItem.colorShiftColor;//"0.400 0.196 0 1.000";

   //casing = " ";

   // Images have a state system which controls how the animations
   // are run, which sounds are played, script callbacks, etc. This
   // state system is downloaded to the client so that clients can
   // predict state changes and animate accordingly.  The following
   // system supports basic ready->fire->reload transitions as
   // well as a no-ammo->dryfire idle state.
   TT_raycastEnabled = true;
   TT_raycastWeaponRange = 200; //varies
   TT_raycastWeaponTargets =
                   $TypeMasks::PlayerObjectType |    //AI/Players
                   $TypeMasks::StaticObjectType |    //Static Shapes
                   $TypeMasks::TerrainObjectType |    //Terrain
                   $TypeMasks::VehicleObjectType |    //Terrain
                   $TypeMasks::FXBrickObjectType;    //Bricks
   TT_raycastExplosionProjectile = GunProjectile;
   TT_raycastExplosionBrickSound = bulletHitSound;
   TT_raycastExplosionPlayerSound = bulletHitSound;
   TT_raycastDirectDamage = 10; //10
   TT_raycastDirectDamageType = $DamageType::AkimboL4Pistol;
   TT_raycastSpreadAmt = 0.0003; //varies
   TT_raycastSpreadCount = 1;
   TT_raycastTracerProjectile = pistolTracerProjectile;
   TT_raycastFromMuzzle = true;

   // Initial start up state
	stateName[0]                    = "Activate";
	stateTimeoutValue[0]            = 0.15;
	stateTransitionOnTimeout[0]     = "Ready";
	stateSound[0]			  = weaponSwitchSound;

	stateName[1]                    = "Ready";
	stateTransitionOnTriggerDown[1] = "Fire";

	stateName[2]                    = "Fire";
	stateTransitionOnTimeout[2]     = "Smoke";
	stateTimeoutValue[2]            = 0.02;
	stateFire[2]                    = true;
	stateAllowImageChange[2]        = false;
	stateScript[2]                  = "onFire";
	stateEjectShell[2]       = true;
	stateSequence[2]			  = "Fire";
	stateEmitter[2]			  = gunFlashEmitter;
	stateEmitterTime[2]		  = 0.05;
	stateEmitterNode[2]		  = "muzzleNode";
	stateSound[2]			  = PistolfireSound;

	stateName[3] 			  = "Smoke";
	stateEmitter[3]			  = gunSmokeEmitter;
	stateEmitterTime[3]		  = 0.1;
	stateEmitterNode[3]		  = "muzzleNode";
	stateTimeoutValue[3]            = 0.01;
	stateTransitionOnTimeout[3]     = "Wait";

	stateName[4]			  = "Wait";
	stateTimeoutValue[4]		  = 0.08;
	stateTransitionOnTimeout[4]	  = "Ready";
};

function AkimboPistolImage::onMount(%this, %obj, %slot)
{
	Parent::onMount(%this, %obj, %slot);
	%obj.mountImage(LeftHandedPistolImage, 1);
}

function AkimboPistolImage::onUnMount(%this, %obj, %slot)
{
	Parent::onUnMount(%this, %obj, %slot);
	%obj.unMountImage(1);
}

function AkimboPistolImage::onFire(%this,%obj,%slot)
{
	if(vectorLen(%obj.getVelocity()) > 0.1)
	{
		%this.TT_raycastSpreadAmt = 0.0018;
		%this.TT_raycastWeaponRange = 85;
	}
	else
	{
		%this.TT_raycastSpreadAmt = 0.0006;
		%this.TT_raycastWeaponRange = 200;
	}
	
	if($Pref::Server::TT::Recoil)
		%obj.spawnExplosion(TTLittleRecoilProjectile,"1 1 1");

	Parent::onFire(%this,%obj,%slot);
	%this.TT_decrementAmmo(%obj);
	%this.TT_displayAmmo(%obj);
	%obj.playThread(2, shiftAway);
}

function AkimboPistolImage::onFireAkimbo(%this,%obj,%slot)
{
	%obj.setImageTrigger(1,1);
	if($Pref::Server::TT::DeathStopFiring)
		%obj.setImageTrigger(1,0);
	%this.TT_displayAmmo(%obj);
}

function AkimboPistolImage::onReloadStart(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return;
	%this.TT_displayAmmo(%obj);
	%obj.playThread(2, activate2);
	serverPlay3D(block_moveBrick_Sound,%obj.getPosition());
}

function AkimboPistolImage::onReloadNext(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return;
	%this.TT_displayAmmo(%obj);
	%obj.playThread(2, plant);
	serverPlay3D(block_plantBrick_Sound,%obj.getPosition());
}

function AkimboPistolImage::onReloaded(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return %obj.setImageLoaded(%slot, 1);
	%this.TT_reload(%obj, %slot, pistolClickSound, plant);
	%this.TT_displayAmmo(%obj);
}

function LeftHandedPistolImage::onMount(%this, %obj, %slot)
{
	Parent::onMount(%this, %obj, %slot);
	%obj.playThread(1, armreadyboth);
}

function LeftHandedPistolImage::onFire(%this, %obj, %slot)
{
	if(vectorLen(%obj.getVelocity()) > 0.1)
	{
		%this.TT_raycastSpreadAmt = 0.0027;
		%this.TT_raycastWeaponRange = 85;
	}
	else
	{
		%this.TT_raycastSpreadAmt = 0.0012;
		%this.TT_raycastWeaponRange = 200;
	}
	
	if($Pref::Server::TT::Recoil)
		%obj.spawnExplosion(TTLittleRecoilProjectile,"1 1 1");

	Parent::onFire(%this,%obj,%slot);
	%this.TT_decrementAmmo(%obj);
	%this.TT_displayAmmo(%obj);
	%obj.playThread(2, leftrecoil);
}
