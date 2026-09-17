//audio
datablock AudioProfile(SubmachineGunFire1Sound)
{
   filename    = "./submachinegun_lowdamage.wav";
   description = AudioClose3d;
   preload = true;
};

AddDamageType("SubmachineGun",   '<bitmap:add-ons/Weapon_Package_Tier1/ci_smg1> %1',    '%2 <bitmap:add-ons/Weapon_Package_Tier1/ci_smg1> %1',0.75,1);
datablock ProjectileData(SubmachineGunProjectile1)
{
   projectileShapeName = "add-ons/Weapon_Gun/bullet.dts";
   directDamage        = 8;
   directDamageType    = $DamageType::SubmachineGun;
   radiusDamageType    = $DamageType::SubmachineGun;

   brickExplosionRadius = 0;
   brickExplosionImpact = true;          //destroy a brick if we hit it directly?
   brickExplosionForce  = 10;
   brickExplosionMaxVolume = 1;          //max volume of bricks that we can destroy
   brickExplosionMaxVolumeFloating = 2;  //max volume of bricks that we can destroy if they aren't connected to the ground

   impactImpulse	     = 100;
   verticalImpulse     = 20;
   explosion           = gunExplosion;

   muzzleVelocity      = 100;
   velInheritFactor    = 1;

   armingDelay         = 0;
   lifetime            = 4000;
   fadeDelay           = 3500;
   bounceElasticity    = 0.5;
   bounceFriction      = 0.20;
   isBallistic         = true;
   gravityMod = 0.2;

   hasLight    = false;
   lightRadius = 3.0;
   lightColor  = "0 0 0.5";
};

//////////
// item //
//////////
datablock ItemData(SubmachineGunItem)
{
	category = "Weapon";  // Mission editor category
	className = "Weapon"; // For inventory system

	 // Basic Item Properties
	shapeFile = "./submachinegun.dts";
	rotate = false;
	mass = 1;
	density = 0.2;
	elasticity = 0.2;
	friction = 0.6;
	emap = true;

	//gui stuff
	uiName = "Submachine Gun";
	iconName = "./submachinegun";
	doColorShift = true;
	colorShiftColor = "0.4 0.4 0.4 1.000";

	 // Dynamic properties defined by the scripts
	image = SubmachineGunImage;
	canDrop = true;

	TT_ammoType = "9MM";
	TT_reloads = true;
	TT_maxAmmo = 35;
};

////////////////
//weapon image//
////////////////
datablock ShapeBaseImageData(SubmachineGunImage)
{
   // Basic Item properties
   shapeFile = "./submachinegun.dts";
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
   item = SubmachineGunItem;
   ammo = " ";
   projectile = SubmachineGunProjectile1;
   projectileType = Projectile;

   casing = GunShellDebris;
   shellExitDir        = "1.0 0.1 1.0";
   shellExitOffset     = "0 0 0";
   shellExitVariance   = 10.0;	
   shellVelocity       = 5.0;

   //melee particles shoot from eye node for consistancy
   melee = false;
   //raise your arm up or not
   armReady = true;

   doColorShift = true;
   colorShiftColor = SubmachineGunItem.colorShiftColor;

   // Images have a state system which controls how the animations
   // are run, which sounds are played, script callbacks, etc. This
   // state system is downloaded to the client so that clients can
   // predict state changes and animate accordingly.  The following
   // system supports basic ready->fire->reload transitions as
   // well as a no-ammo->dryfire idle state.

   // Initial start up state
	stateName[0]                     = "Activate";
	stateTimeoutValue[0]             = 0.05;
	stateTransitionOnTimeout[0]       = "LoadCheckA";
	stateSound[0]					= weaponSwitchSound;

	stateName[1]                     = "Ready";
	stateTransitionOnNotLoaded[1]       = "Reload";
	stateTransitionOnTriggerDown[1]  = "FireCheckA";
	stateAllowImageChange[1]         = true;

	stateName[2]                    = "Fire";
	stateTransitionOnTimeout[2]     = "Delay";
	stateTimeoutValue[2]            = 0;
	stateFire[2]                    = true;
	stateAllowImageChange[2]        = false;
	stateSequence[2]                = "Fire";
	stateScript[2]                  = "onFire";
	stateEjectShell[2]       	  = true;
	stateEmitter[2]					= gunFlashEmitter;
	stateEmitterTime[2]				= 0.05;
	stateEmitterNode[2]				= "muzzleNode";
	stateSound[2]					= submachinegunFire1Sound;

	stateName[3]			= "Delay";
	stateTransitionOnTimeout[3]     = "FireLoadCheckA";
	stateTimeoutValue[3]            = 0.01;
	stateEmitter[3]					= gunSmokeEmitter;
	stateEmitterTime[3]				= 0.01;
	stateEmitterNode[3]				= "muzzleNode";
	
	stateName[4]				= "LoadCheckA";
	stateScript[4]				= "TT_onLoadCheck";
	stateTimeoutValue[4]			= 0.01;
	stateTransitionOnTimeout[4]		= "LoadCheckB";
	
	stateName[5]				= "LoadCheckB";
	stateTransitionOnLoaded[5]		= "Ready";
	stateTransitionOnNotLoaded[5]  = "Empty";

	stateName[6]				= "Reload";
	stateTimeoutValue[6]			= 1.2;
	stateScript[6]				= "onReloadStart";
	stateTransitionOnTimeout[6]		= "Wait";
	stateWaitForTimeout[6]			= true;
	
	stateName[7]				= "Wait";
	stateTimeoutValue[7]			= 0.8;
	stateScript[7]				= "onReloadWait";
	stateTransitionOnTimeout[7]		= "Reloaded";
	
	stateName[8]				= "FireLoadCheckA";
	stateScript[8]				= "TT_onLoadCheck";
	stateTimeoutValue[8]			= 0.01;
	stateTransitionOnTimeout[8]		= "FireLoadCheckB";
	
	stateName[9]				= "FireLoadCheckB";
	stateTransitionOnLoaded[9]		= "Smoke";
	stateTransitionOnNotLoaded[9]		= "ReloadSmoke";
	
	stateName[10] 				= "Smoke";
	stateEmitter[10]			= gunSmokeEmitter;
	stateEmitterTime[10]			= 0.3;
	stateEmitterNode[10]			= "muzzleNode";
	stateTimeoutValue[10]			= 0.2;
	stateTransitionOnTimeout[10]		= "Ready";
	stateTransitionOnTriggerDown[10]	= "FireCheckA";

	stateName[11] 				= "ReloadSmoke";
	stateEmitter[11]			= gunSmokeEmitter;
	stateEmitterTime[11]			= 0.3;
	stateEmitterNode[11]			= "muzzleNode";
	stateTimeoutValue[11]			= 0.2;
	stateTransitionOnTimeout[11]		= "Empty";
	
	stateName[12]				= "Reloaded";
	stateTimeoutValue[12]			= 0.01;
	stateScript[12]				= "onReloaded";
	stateTransitionOnTimeout[12]		= "Ready";

	stateName[13]                    = "FireCheckA";
	stateScript[13]                  = "TT_onFireCheck";
	stateTransitionOnTimeout[13]     = "FireCheckB";

	stateName[14]                    = "FireCheckB";
	stateTransitionOnLoaded[14]   = "Fire";
	stateTransitionOnNotLoaded[14]      = "EmptyFire";

	stateName[15]                    = "Empty";
	stateTransitionOnLoaded[15]      = "Ready";
	stateTransitionOnAmmo[15]        = "Reload";
	stateTransitionOnTriggerDown[15] = "FireCheckA";

	stateName[16]                    = "EmptyFire";
	stateScript[16]                  = "TT_onEmptyFire";
	stateTransitionOnLoaded[16]      = "Ready";
	stateTransitionOnAmmo[16]        = "Reload";
	stateTransitionOnTriggerUp[16]   = "Empty";
};

function SubmachineGunImage::onFire(%this,%obj,%slot)
{
	%projectile = %this.projectile;
	%spread = 0.0015;
	%shellCount = 1;

	%obj.playThread(2, plant);
	
	%this.TT_decrementAmmo(%obj);
	%this.TT_displayAmmo(%obj);

	if($Pref::Server::TT::Recoil)
		%obj.spawnExplosion(TTLittleRecoilProjectile,"1 1 1");

	return TT_createProjectile(%this, %obj, %slot, %projectile, %shellCount, %spread);
}

function SubmachineGunImage::onReloadStart(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return;
	%this.TT_displayAmmo(%obj);
	%obj.playThread(2, wrench);
	serverPlay3D(block_MoveBrick_Sound,%obj.getPosition());
}

function SubmachineGunImage::onReloadWait(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return;
	%this.TT_displayAmmo(%obj);
	serverPlay3D(magazineOutSound,%obj.getPosition());
	%obj.playThread(2, activate);
}

function SubmachineGunImage::onReloaded(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return %obj.setImageLoaded(%slot, 1);
	%this.TT_reload(%obj, %slot, reloadClick6Sound, plant);
	%this.TT_displayAmmo(%obj);
}

function SubmachinegunProjectile1::damage(%this,%obj,%col,%fade,%pos,%normal)
{
	if(%col.getType() & $TypeMasks::PlayerObjectType)
	{
		TT_dampenVelocity(%col, 1.1);
	}
	Parent::damage(%this,%obj,%col,%fade,%pos,%normal);
}