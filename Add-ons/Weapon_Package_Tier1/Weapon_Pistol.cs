//audio
datablock AudioProfile(PistolFireSound)
{
	filename    = "./Pistol_fire.1.wav";
	description = AudioClose3d;
	preload = true;
};

datablock AudioProfile(PistolClickSound)
{
	filename    = "./Pistol_click.wav";
	description = AudioClose3d;
	preload = true;
};

datablock ParticleData(pistolTrailParticle)
{
	dragCoefficient		= 3.0;
	windCoefficient		= 0.0;
	gravityCoefficient	= 0.0;
	inheritedVelFactor	= 0.0;
	constantAcceleration	= 0.0;
	lifetimeMS		= 120;
	lifetimeVarianceMS	= 0;
	spinSpeed		= 10.0;
	spinRandomMin		= -50.0;
	spinRandomMax		= 50.0;
	useInvAlpha		= false;
	animateTexture		= false;
	//framesPerSec		= 1;

	textureName		= "base/data/particles/dot";
	//animTexName		= "~/data/particles/dot";

	// Interpolation variables
	colors[0]	= "1 1 0 1";
	colors[1]	= "1 1 0.4 1";
	colors[2]	= "1 1 1 1";
	sizes[0]	= 0.2;
	sizes[1]	= 0.15;
	sizes[2]	= 0.0;
	times[0]	= 0.0;
	times[1]	= 0.5;
	times[2]	= 1.0;
};

datablock ParticleEmitterData(pistolTrailEmitter)
{
	ejectionPeriodMS = 2;
	periodVarianceMS = 0;

	ejectionVelocity = 0; //0.25;
	velocityVariance = 0; //0.10;

	ejectionOffset = 0;

	thetaMin         = 0.0;
	thetaMax         = 90.0;  

	particles = pistolTrailParticle;

	useEmitterColors = true;
};

datablock ProjectileData(pistolTracerProjectile)
{
	directDamage        = 0;
	directDamageType    = $DamageType::Gun;
	radiusDamageType    = $DamageType::Gun;

	brickExplosionRadius = 0;
	brickExplosionImpact = true;          //destroy a brick if we hit it directly?
	brickExplosionForce  = 10;
	brickExplosionMaxVolume = 1;          //max volume of bricks that we can destroy
	brickExplosionMaxVolumeFloating = 2;  //max volume of bricks that we can destroy if they aren't connected to the ground

	impactImpulse	     = 100;
	verticalImpulse	  = 50;
	//explosion           = gunExplosion;
	particleEmitter     = "pistolTrailEmitter";

	muzzleVelocity      = 200;
	velInheritFactor    = 0;

	armingDelay         = 0;
	lifetime            = 1000;
	fadeDelay           = 700;
	bounceElasticity    = 0.5;
	bounceFriction      = 0.10;
	isBallistic         = true;
	gravityMod = 0.0;

	hasLight    = false;
	lightRadius = 3.0;
	lightColor  = "0 0 0.5";
};

//////////
// item //
//////////
datablock ItemData(PistolItem)
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
	uiName = "Pistol";
	iconName = "./pistol";
	doColorShift = true;
	colorShiftColor = "0.73 0.73 0.73 1.000";

	 // Dynamic properties defined by the scripts
	image = PistolImage;
	canDrop = true;

	TT_ammoType = "9MM";
	TT_reloads = true;
	TT_maxAmmo = 12;
};

AddDamageType("L4Pistol",   '<bitmap:add-ons/Weapon_Package_Tier1/CI_Pistol> %1',    '%2 <bitmap:add-ons/Weapon_Package_Tier1/CI_Pistol> %1',0.75,1);

////////////////
//weapon image//
////////////////
datablock ShapeBaseImageData(PistolImage)
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
	item = PistolItem;
	ammo = " ";
	projectile = pistolTracerProjectile;
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
	colorShiftColor = PistolItem.colorShiftColor;

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
	TT_raycastDirectDamage = 12; //10
	TT_raycastDirectDamageType = $DamageType::L4Pistol;
	TT_raycastSpreadAmt = 0.0006; //varies
	TT_raycastSpreadCount = 1;
	TT_raycastTracerProjectile = pistolTracerProjectile;
	TT_raycastFromMuzzle = true;

	// Initial start up state
	stateName[0]                     = "Activate";
	stateTimeoutValue[0]             = 0.15;
	stateTransitionOnTimeout[0]      = "LoadCheckA";
	stateSound[0]                    = weaponSwitchSound;

	stateName[1]                     = "Ready";
	stateTransitionOnNotLoaded[1]    = "ManualReload";
	stateTransitionOnTriggerDown[1]  = "FireCheckA";

	// Switching from TTAmmo 1/2 to other modes and back may leave the gun in "Ready" when it is empty
	// Fire check prevents the gun from firing in this case
	// For some reason we can get away with no timeout here
	stateName[2]                     = "FireCheckA";
	stateScript[2]                   = "TT_onFireCheck";
	stateTransitionOnTimeout[2]      = "FireCheckB";

	stateName[3]                     = "FireCheckB";
	stateTransitionOnLoaded[3]       = "Fire";
	stateTransitionOnNotLoaded[3]    = "EmptyFire";

	stateName[4]                     = "Fire";
	stateTransitionOnTimeout[4]      = "Smoke";
	stateTimeoutValue[4]             = 0.05;
	stateFire[4]                     = true;
	stateAllowImageChange[4]         = false;
	stateEjectShell[4]               = true;
	stateScript[4]                   = "onFire";
	stateSequence[4]                 = "Fire";
	stateEmitter[4]                  = gunFlashEmitter;
	stateEmitterTime[4]              = 0.05;
	stateEmitterNode[4]              = "muzzleNode";
	stateSound[4]                    = PistolfireSound;

	stateName[5]                     = "Smoke";
	stateEmitter[5]                  = gunSmokeEmitter;
	stateEmitterTime[5]              = 0.2;
	stateEmitterNode[5]              = "muzzleNode";
	stateTransitionOnTriggerUp[5]    = "Wait";

	stateName[6]                     = "Wait";
	stateTimeoutValue[6]             = 0.001;
	stateScript[6]                   = "onBounce";
	stateTransitionOnTimeout[6]      = "LoadCheckA";
	stateSound[6]                    = pistolClickSound;

	stateName[7]                     = "LoadCheckA";
	stateTimeoutValue[7]             = 0.01;
	stateScript[7]                   = "TT_onLoadCheck";
	stateTransitionOnTimeout[7]      = "LoadCheckB";

	stateName[8]                     = "LoadCheckB";
	stateTransitionOnLoaded[8]       = "Ready";
	stateTransitionOnNotLoaded[8]    = "Empty";
	
	//Torque switches states instantly if there is an ammo/noammo state, regardless of stateWaitForTimeout
	
	// Loaded - gun has ammo, doesn't need reload
	// NotLoaded - empty gun, needs reload
	// Ammo - reserve ammo available, can reload
	// NoAmmo - no reserve ammo left, can't reload
	stateName[9]                     = "ReloadWait";
	stateTimeoutValue[9]             = 0.3;
	stateScript[9]                   = "onReloadWait";
	stateTransitionOnTimeout[9]      = "ReloadStart";

	// When splitting up the Timeout of a state only 32 ms ticks really count, rounded up
	// 0.01 gets rounded up to 0.032 ms or 1 tick
	// 0.3 / 0.032 = 9.375 -> 10 ticks
	// 9 + 1 -> 9 * 0.032 + 1 * 0.032 = 0.288 + 0.032 ms
	stateName[10]                    = "ReloadStart";
	stateTimeoutValue[10]            = 0.288; //0.3
	stateScript[10]                  = "onReloadStart";
	stateTransitionOnTimeout[10]     = "DeathCheck";
	
	stateName[11]                    = "Reloaded";
	stateTimeoutValue[11]            = 0.4;
	stateScript[11]                  = "onReloaded";
	stateTransitionOnTimeout[11]     = "Ready";
	stateSequence[11]                = "Fire";
	stateSound[11]                   = pistolClickSound;
	stateTransitionOnNoAmmo[11]      = "Empty";

	// Don't know of a way to stop state animations/sounds from playing so let's add more checks
	// If player is dead this should stop them from completing the reload and playing an animation while dead
	stateName[12]                    = "DeathCheck";
	stateScript[12]                  = "TT_onDeathCheck";
	stateTimeoutValue[12]            = 0.032; // necessary to sync state with animation and sound
	stateTransitionOnTimeout[12]     = "Reloaded";

	// Ammo - gun is completely empty, full reload
	// NoAmmo - some ammo left in gun, partial reload
	// ImageAmmo is set through TT_onUseLight when light key is pressed
	// Transitions are opposite of expected since onAmmo transition to ReloadWait has to be consistent with Empty
	// Otherwise FireCheck may desync the state with animation and sound if gun is in Ready while empty
	stateName[13]                    = "ManualReload";
	stateTransitionOnAmmo[13]        = "ReloadWait";
	stateTransitionOnNoAmmo[13]      = "ReloadStart";

	stateName[14]                    = "Empty";
	stateTransitionOnLoaded[14]      = "Ready";
	stateTransitionOnAmmo[14]        = "ReloadWait";
	stateTransitionOnTriggerDown[14] = "FireCheckA";

	stateName[15]                    = "EmptyFire";
	stateScript[15]                  = "TT_onEmptyFire";
	stateTransitionOnLoaded[15]      = "Ready";
	stateTransitionOnAmmo[15]        = "ReloadWait";
	stateTransitionOnTriggerUp[15]   = "Empty";
};

function PistolImage::onFire(%this,%obj,%slot)
{
	if($Pref::Server::TT::Recoil)
		%obj.spawnExplosion(TTRecoilProjectile,"1 1 1");
	
	if(vectorLen(%obj.getVelocity()) > 0.1)
	{
		%this.TT_raycastSpreadAmt = 0.0018;
		%this.TT_raycastWeaponRange = 85;
	}
	else
	{
		%this.TT_raycastSpreadAmt = 0.0009;
		%this.TT_raycastWeaponRange = 200;
	}
	
	%this.TT_decrementAmmo(%obj);
	%this.TT_displayAmmo(%obj);
	%obj.playThread(2, shiftAway);
	return Parent::onFire(%this,%obj,%slot);
}

function PistolImage::onReloadWait(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return;
	%this.TT_displayAmmo(%obj);
	%obj.playThread(2, shiftUp);
	serverPlay3D(block_PlantBrick_Sound,%obj.getPosition());
}

function PistolImage::onReloadStart(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return;
	%this.TT_displayAmmo(%obj);
	%obj.playThread(2, shiftLeft);
	serverPlay3D(block_MoveBrick_Sound,%obj.getPosition());
}

function PistolImage::onBounce(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return;
	%this.TT_displayAmmo(%obj);
	%obj.playThread(2, plant);
}

function PistolImage::onReloaded(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return %obj.setImageLoaded(%slot, 1);
	%this.TT_reload(%obj, %slot, pistolClickSound, plant);
	%this.TT_displayAmmo(%obj);
}
