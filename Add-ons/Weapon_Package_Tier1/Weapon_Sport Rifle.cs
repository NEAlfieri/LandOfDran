//audio
datablock AudioProfile(SportRifleFireSound)
{
   filename    = "./Sport_Rifle_fire.wav";
   description = AudioClose3d;
   preload = true;
};

//datablock AudioProfile(SportRifleReloadSound)
//{
//   filename    = "./pump_shotgun_reload.wav";
//   description = AudioClose3d;
//   preload = true;
//};


datablock ParticleData(SportRifleCritTrailParticle)
{
	dragCoefficient		= 3.0;
	windCoefficient		= 0.0;
	gravityCoefficient	= 0.0;
	inheritedVelFactor	= 0.0;
	constantAcceleration	= 0.0;
	lifetimeMS		= 150;
	lifetimeVarianceMS	= 0;
	spinSpeed		= 10.0;
	spinRandomMin		= -50.0;
	spinRandomMax		= 50.0;
	useInvAlpha		= false;
	animateTexture		= false;
	//framesPerSec		= 1;

	textureName		= "base/data/particles/thinring";
	//animTexName		= "~/data/particles/dot";

	// Interpolation variables
	colors[0]	= "1 1 0.7 0.5";
	colors[1]	= "1 1 1 0.25";
	colors[2]	= "1 1 1 0";
	sizes[0]	= 0.5;
	sizes[1]	= 0.75;
	sizes[2]	= 0.0;
	times[0]	= 0.0;
	times[1]	= 0.5;
	times[2]	= 1.0;
};

datablock ParticleEmitterData(SportRifleCritTrailEmitter)
{
   ejectionPeriodMS = 10;
   periodVarianceMS = 0;

   ejectionVelocity = 0; //0.25;
   velocityVariance = 0;

   ejectionOffset = 0;

   thetaMin         = 0.0;
   thetaMax         = 90.0;  

   particles = SportRifleCritTrailParticle;

   useEmitterColors = true;
};

datablock ParticleData(SportRifleCritTrail2Particle)
{
	dragCoefficient		= 3.0;
	windCoefficient		= 0.0;
	gravityCoefficient	= 0.0;
	inheritedVelFactor	= 0.0;
	constantAcceleration	= 0.0;
	lifetimeMS		= 100;
	lifetimeVarianceMS	= 0;
	spinSpeed		= 10.0;
	spinRandomMin		= -50.0;
	spinRandomMax		= 50.0;
	useInvAlpha		= false;
	animateTexture		= false;
	//framesPerSec		= 1;

	textureName		= "base/data/particles/thinring";
	//animTexName		= "~/data/particles/dot";

	// Interpolation variables
	colors[0]	= "1 1 0.7 0.5";
	colors[1]	= "1 1 1 0.25";
	colors[2]	= "1 1 1 0";
	sizes[0]	= 0.5;
	sizes[1]	= 0.75;
	sizes[2]	= 0.0;
	times[0]	= 0.0;
	times[1]	= 0.5;
	times[2]	= 1.0;
};

datablock ParticleEmitterData(SportRifleCritTrail2Emitter)
{
   ejectionPeriodMS = 10;
   periodVarianceMS = 0;

   ejectionVelocity = 0; //0.25;
   velocityVariance = 0;

   ejectionOffset = 0;

   thetaMin         = 0.0;
   thetaMax         = 90.0;  

   particles = SportRifleCritTrail2Particle;

   useEmitterColors = true;
};


AddDamageType("SportRifle",   '<bitmap:add-ons/Weapon_Package_Tier1/CI_SportRifle> %1',    '%2 <bitmap:add-ons/Weapon_Package_Tier1/CI_SportRifle> %1',0.75,1);
AddDamageType("SportRifleHeadshot",   '<bitmap:add-ons/Weapon_Package_Tier1/CI_SportRifle>  <bitmap:add-ons/Weapon_Package_Tier1/CI_tactheadshot>%1',    '%2 <bitmap:add-ons/Weapon_Package_Tier1/CI_SportRifle> <bitmap:add-ons/Weapon_Package_Tier1/CI_tactheadshot>%1',0.75,1);
datablock ProjectileData(SportRifleProjectile)
{
   projectileShapeName = "./rifle_round.dts";
   directDamage        = 24; //90
   directDamageType    = $DamageType::SportRifle;
   radiusDamageType    = $DamageType::SportRifle;

   brickExplosionRadius = 0.2;
   brickExplosionImpact = true;          //destroy a brick if we hit it directly?
   brickExplosionForce  = 15;
   brickExplosionMaxVolume = 20;          //max volume of bricks that we can destroy
   brickExplosionMaxVolumeFloating = 30;  //max volume of bricks that we can destroy if they aren't connected to the ground

   particleEmitter     = sportRifleCritTrailEmitter;
   impactImpulse	     = 800;
   verticalImpulse     = 700;
   explosion           = gunExplosion;

   muzzleVelocity      = 190;
   velInheritFactor    = 0;

   armingDelay         = 0;
   lifetime            = 9000;
   fadeDelay           = 9500;
   bounceElasticity    = 0.0;
   bounceFriction      = 0.00;
   isBallistic         = true;
   gravityMod          = 0.02;
   
   explodeOnDeath = true;
   explodeOnPlayerImpact = true;

   hasLight    = false;
   lightRadius = 3.0;
   lightColor  = "0 0 0.5";
};

datablock ProjectileData(SportRifleWeakProjectile)
{
   projectileShapeName = "./rifle_round.dts";
   directDamage        = 24; //90
   directDamageType    = $DamageType::SportRifle;
   radiusDamageType    = $DamageType::SportRifle;

   brickExplosionRadius = 0.2;
   brickExplosionImpact = true;          //destroy a brick if we hit it directly?
   brickExplosionForce  = 15;
   brickExplosionMaxVolume = 20;          //max volume of bricks that we can destroy
   brickExplosionMaxVolumeFloating = 30;  //max volume of bricks that we can destroy if they aren't connected to the ground

   particleEmitter     = sportRifleCritTrail2Emitter;
   impactImpulse	     = 600;
   verticalImpulse     = 500;
   explosion           = gunExplosion;

   muzzleVelocity      = 175;
   velInheritFactor    = 0;

   armingDelay         = 10;
   lifetime            = 9000;
   fadeDelay           = 9500;
   bounceElasticity    = 0.0;
   bounceFriction      = 0.00;
   isBallistic         = true;
   gravityMod          = 0.04;
   
   explodeOnDeath = true;
   explodeOnPlayerImpact = true;

   hasLight    = false;
   lightRadius = 3.0;
   lightColor  = "0 0 0.5";
};

//////////
// item //
//////////
datablock ItemData(SportRifleItem)
{
	category = "Weapon";  // Mission editor category
	className = "Weapon"; // For inventory system

	 // Basic Item Properties
	shapeFile = "./Sport_Rifle.3.dts";
	rotate = false;
	mass = 1;
	density = 0.2;
	elasticity = 0.2;
	friction = 0.6;
	emap = true;

	//gui stuff
	uiName = "Sport Rifle";
	iconName = "./sportrifle";
	doColorShift = true;
	colorShiftColor = "0.4 0.4 0.43 1.000";

	 // Dynamic properties defined by the scripts
	image = SportRifleImage;
	canDrop = true;

	TT_ammoType = "556";
	TT_reloads = true;
	TT_maxAmmo = 10;
};

////////////////
//weapon image//
////////////////
datablock ShapeBaseImageData(SportRifleImage)
{
   // Basic Item properties
   shapeFile = "./Sport_Rifle.3.dts";
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
   item = SportRifleItem;
   ammo = " ";
   projectile = SportRifleProjectile;
   projectileType = Projectile;

   //melee particles shoot from eye node for consistancy
   melee = false;
   //raise your arm up or not
   armReady = true;

   doColorShift = true;
   colorShiftColor = SportRifleItem.colorShiftColor;

   // Images have a state system which controls how the animations
   // are run, which sounds are played, script callbacks, etc. This
   // state system is downloaded to the client so that clients can
   // predict state changes and animate accordingly.  The following
   // system supports basic ready->fire->reload transitions as
   // well as a no-ammo->dryfire idle state.

   // Initial start up state
	stateName[0]                    = "Activate";
	stateTimeoutValue[0]            = 0.5;
	stateTransitionOnTimeout[0]     = "LoadCheckA";
	stateSound[0]			  = weaponSwitchSound;

	stateName[1]                    = "Ready";
	stateTransitionOnNotLoaded[1] = "Reload";
	stateTransitionOnTriggerDown[1] = "FireCheckA";

	stateName[2]                    = "Fire";
	stateTransitionOnTriggerUp[2] = "Smoke";
	stateFire[2]                    = true;
	stateAllowImageChange[2]        = false;
	stateScript[2]                  = "onFire";
	stateEmitter[2]			  = gunFlashEmitter;
	stateEmitterTime[2]		  = 0.05;
	stateEmitterNode[2]		  = "muzzleNode";
	stateSound[2]			  = SportRiflefireSound;

	stateName[3] 			  = "Smoke";
	stateEmitterTime[3]		  = 0.2;
	stateSequence[3]			  = "fire";
	stateEmitterNode[3]		  = "muzzleNode";
	stateSound[3]			  = PistolClickSound;
	stateTimeoutValue[3]            = 0.2;
	stateTransitionOnTimeout[3]     = "LoadCheckA";

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
	
	stateName[7]				= "Wait";
	stateTimeoutValue[7]			= 0.5;
	stateScript[7]				= "onReloadWait";
	stateTransitionOnTimeout[7]		= "Reloaded";
	
	stateName[8]				= "Reloaded";
	stateTimeoutValue[8]			= 0.1;
	stateScript[8]				= "onReloaded";
	stateTransitionOnTimeout[8]		= "Ready";

	stateName[9]                    = "FireCheckA";
	stateScript[9]                  = "TT_onFireCheck";
	stateTransitionOnTimeout[9]     = "FireCheckB";

	stateName[10]                    = "FireCheckB";
	stateTransitionOnLoaded[10]   = "Fire";
	stateTransitionOnNotLoaded[10]      = "EmptyFire";

	stateName[11]                    = "Empty";
	stateTransitionOnLoaded[11]      = "Ready";
	stateTransitionOnAmmo[11]        = "Reload";
	stateTransitionOnTriggerDown[11] = "FireCheckA";

	stateName[12]                    = "EmptyFire";
	stateScript[12]                  = "TT_onEmptyFire";
	stateTransitionOnLoaded[12]      = "Ready";
	stateTransitionOnAmmo[12]        = "Reload";
	stateTransitionOnTriggerUp[12]   = "Empty";
};

function SportRifleImage::onFire(%this,%obj,%slot)
{
	%obj.playThread(2, plant);
	
	if(vectorLen(%obj.getVelocity()) < 4 && (getSimTime() - %obj.lastShotTime) > 1000)
	{
		%projectile = %this.projectile;
		%spread = 0.00001;
	}
	else
	{
		%projectile = SportRifleWeakProjectile;
		%spread = 0.0001;
	}
	%shellCount = 1;
	
	%this.TT_decrementAmmo(%obj);
	%this.TT_displayAmmo(%obj);

	if($Pref::Server::TT::Recoil)
		%obj.spawnExplosion(TTRecoilProjectile,"1 1 1");

	return TT_createProjectile(%this, %obj, %slot, %projectile, %shellCount, %spread);
}

function SportRifleImage::onReloadStart(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return;
	%this.TT_displayAmmo(%obj);
	%obj.playThread(2, shiftRight);
	serverPlay3D(block_MoveBrick_Sound, %obj.getPosition());
}

function SportRifleImage::onReloadWait(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return;
	%this.TT_displayAmmo(%obj);
	%obj.playThread(2, plant);
	serverPlay3D(magazineOutSound, %obj.getPosition());
}

function SportRifleImage::onReloaded(%this,%obj,%slot)
{
	if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
		return %obj.setImageLoaded(%slot, 1);
	%this.TT_reload(%obj, %slot, reloadClick8Sound, plant);
	%this.TT_displayAmmo(%obj);
}

function SportRifleProjectile::damage(%this,%obj,%col,%fade,%pos,%normal)
{
	%multiplier = 3; // multiply damage by this on headshot
	TT_processHeadshotDamage(%this, %obj, %col, %pos, %this.directDamage, %multiplier, $DamageType::SportRifleHeadshot);
}

function SportRifleWeakProjectile::damage(%this,%obj,%col,%fade,%pos,%normal)
{
	TT_processHeadshotDamage(%this, %obj, %col, %pos, %this.directDamage, 2, $DamageType::SportRifleHeadshot);
}