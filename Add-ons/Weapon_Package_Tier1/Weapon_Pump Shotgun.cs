//audio
datablock AudioProfile(PumpShotgunFireSound)
{
   filename    = "./pump_shotgun_fire.wav";
   description = AudioClose3d;
   preload = true;
};

datablock AudioProfile(PumpShotgunReloadSound)
{
   filename    = "./pump_shotgun_reload.wav";
   description = AudioClose3d;
   preload = true;
};

datablock AudioProfile(PumpShotgunJamSound)
{
   filename    = "./jam.wav";
   description = AudioClose3d;
   preload = true;
};

datablock ParticleData(shotgunExplosionParticle)
{
    dragCoefficient      = 8;
    gravityCoefficient   = 1;
    inheritedVelFactor   = 0.2;
    constantAcceleration = 0.0;
    lifetimeMS           = 700;
    lifetimeVarianceMS   = 400;
    textureName          = "base/data/particles/cloud";
    spinSpeed       = 10.0;
    spinRandomMin       = -50.0;
    spinRandomMax       = 50.0;
    colors[0]     = "0.9 0.9 0.9 0.3";
    colors[1]     = "0.9 0.5 0.6 0.0";
    sizes[0]      = 2.25;
    sizes[1]      = 2.75;

    useInvAlpha = true;
};
datablock ParticleEmitterData(shotgunExplosionEmitter)
{
   ejectionPeriodMS = 1;
   periodVarianceMS = 0;
   ejectionVelocity = 2;
   velocityVariance = 1.0;
   ejectionOffset   = 0.0;
   thetaMin         = 89;
   thetaMax         = 90;
   phiReferenceVel  = 0;
   phiVariance      = 360;
   overrideAdvance = false;
   particles = "shotgunExplosionParticle";

   useEmitterColors = true;
};


datablock ParticleData(shotgunExplosionRingParticle)
{
    dragCoefficient      = 8;
    gravityCoefficient   = -0.5;
    inheritedVelFactor   = 0.2;
    constantAcceleration = 0.0;
    lifetimeMS           = 50;
    lifetimeVarianceMS   = 35;
    textureName          = "base/data/particles/star1";
    spinSpeed       = 500.0;
    spinRandomMin       = -500.0;
    spinRandomMax       = 500.0;
    colors[0]     = "1 1 0.0 0.9";
    colors[1]     = "0.9 0.0 0.0 0.0";
    sizes[0]      = 3;
    sizes[1]      = 3;

    useInvAlpha = false;
};
datablock ParticleEmitterData(shotgunExplosionRingEmitter)
{
    lifeTimeMS = 50;

   ejectionPeriodMS = 3;
   periodVarianceMS = 0;
   ejectionVelocity = 0;
   velocityVariance = 0.0;
   ejectionOffset   = 0.0;
   thetaMin         = 89;
   thetaMax         = 90;
   phiReferenceVel  = 0;
   phiVariance      = 360;
   overrideAdvance = false;
   particles = "shotgunExplosionRingParticle";

   useEmitterColors = false;
};

datablock ExplosionData(shotgunExplosion)
{
   //explosionShape = "";
    soundProfile = bulletHitSound;

   lifeTimeMS = 150;

   particleEmitter = shotgunExplosionEmitter;
   particleDensity = 5;
   particleRadius = 0.2;

   emitter[0] = shotgunExplosionRingEmitter;

   faceViewer     = true;
   explosionScale = "1 1 1";

   shakeCamera = true;
   camShakeFreq = "10.0 11.0 10.0";
   camShakeAmp = "1.0 1.0 1.0";
   camShakeDuration = 0.5;
   camShakeRadius = 10.0;

   // Dynamic light
   lightStartRadius = 0;
   lightEndRadius = 2;
   lightStartColor = "0.3 0.6 0.7";
   lightEndColor = "0 0 0";

   impulseRadius = 2;
   impulseForce = 1000;
};

// datablock ExplosionData(shotgunFlashExplosion : shotgunExplosion)
// {
//    //explosionShape = "";
//  soundProfile = "";

//    lifeTimeMS = 150;

//    particleEmitter = shotgunExplosionRingEmitter;
//    particleDensity = 5;
//    particleRadius = 0.2;

//    emitter[0] = "";

//    faceViewer     = true;
//    explosionScale = "1 1 1";

//    shakeCamera = false;
//    camShakeFreq = "10.0 11.0 10.0";
//    camShakeAmp = "1.0 1.0 1.0";
//    camShakeDuration = 0.5;
//    camShakeRadius = 10.0;

//    // Dynamic light
//    lightStartRadius = 0;
//    lightEndRadius = 2;
//    lightStartColor = "0.3 0.6 0.7";
//    lightEndColor = "0 0 0";

//    impulseRadius = 0;
//    impulseForce = 0;
// };

//shell
datablock DebrisData(PumpShotgunShellDebris)
{
    shapeFile = "./bushido's_shell_shotgun.dts";
    lifetime = 2.0;
    minSpinSpeed = -400.0;
    maxSpinSpeed = 200.0;
    elasticity = 0.5;
    friction = 0.2;
    numBounces = 3;
    staticOnMaxBounce = true;
    snapOnMaxBounce = false;
    fade = true;

    gravModifier = 4;
};

AddDamageType("PumpShotgun",   '<bitmap:add-ons/Weapon_Package_Tier1/CI_L4Shotgun> %1',    '%2 <bitmap:add-ons/Weapon_Package_Tier1/CI_L4Shotgun> %1',0.75,1);
datablock ProjectileData(PumpShotgunProjectile)
{
   projectileShapeName = "add-ons/Weapon_Gun/bullet.dts";
   directDamage        = 9; //14;
   directDamageType    = $DamageType::PumpShotgun;
   radiusDamageType    = $DamageType::PumpShotgun;

   brickExplosionRadius = 0.2;
   brickExplosionImpact = true;          //destroy a brick if we hit it directly?
   brickExplosionForce  = 15;
   brickExplosionMaxVolume = 20;          //max volume of bricks that we can destroy
   brickExplosionMaxVolumeFloating = 30;  //max volume of bricks that we can destroy if they aren't connected to the ground

   impactImpulse         = 200;
   verticalImpulse     = 100;
   explosion           = gunExplosion;

   muzzleVelocity      = 100;
   velInheritFactor    = 1;

   armingDelay         = 0;
   lifetime            = 4000;
   fadeDelay           = 3500;
   bounceElasticity    = 0.5;
   bounceFriction      = 0.20;
   isBallistic         = true;
   gravityMod = 0.4;

   hasLight    = false;
   lightRadius = 3.0;
   lightColor  = "0 0 0.5";
};

datablock ProjectileData(ShotgunBlastProjectile : PumpShotgunProjectile)
{
   projectileShapeName = "add-ons/Vehicle_Tank/tankbullet.dts";
   directDamage        = 20; //14;
   directDamageType    = $DamageType::PumpShotgun;
   radiusDamageType    = $DamageType::PumpShotgun;

   brickExplosionRadius = 0.4;
   brickExplosionImpact = true;          //destroy a brick if we hit it directly?
   brickExplosionForce  = 30;
   brickExplosionMaxVolume = 25;          //max volume of bricks that we can destroy
   brickExplosionMaxVolumeFloating = 35;  //max volume of bricks that we can destroy if they aren't connected to the ground

   impactImpulse         = 300;
   verticalImpulse     = 100;
   explosion           = shotgunExplosion;

   muzzleVelocity      = 100;
   velInheritFactor    = 1;

   armingDelay         = 0;
   lifetime            = 70;
   fadeDelay           = 0;
   isBallistic         = true;
   gravityMod = 0.0;
};

// datablock ProjectileData(ShotgunFlashProjectile : PumpShotgunProjectile)
// {
//    projectileShapeName = "";
//    directDamage        = 0; //14;
//    directDamageType    = $DamageType::PumpShotgun;
//    radiusDamageType    = $DamageType::PumpShotgun;

//    brickExplosionRadius = 0.4;
//    brickExplosionImpact = true;          //destroy a brick if we hit it directly?
//    brickExplosionForce  = 30;
//    brickExplosionMaxVolume = 25;          //max volume of bricks that we can destroy
//    brickExplosionMaxVolumeFloating = 35;  //max volume of bricks that we can destroy if they aren't connected to the ground

//    impactImpulse      = 300;
//    verticalImpulse     = 100;
//    explosion           = shotgunFlashExplosion;

//    muzzleVelocity      = 10;
//    velInheritFactor    = 1;

//    armingDelay         = 0;
//    lifetime            = 10;
//    fadeDelay           = 0;
//    isBallistic         = true;
//    gravityMod = 0.0;
// };

//////////
// item //
//////////
datablock ItemData(PumpShotgunItem)
{
    category = "Weapon";  // Mission editor category
    className = "Weapon"; // For inventory system

     // Basic Item Properties
    shapeFile = "./Pump_Shotgun.dts";
    rotate = false;
    mass = 1;
    density = 0.2;
    elasticity = 0.2;
    friction = 0.6;
    emap = true;

    //gui stuff
    uiName = "Pump Shotgun";
    iconName = "./pumpshotgun";
    doColorShift = true;
    colorShiftColor = "0.3 0.3 0.31 1.000";

     // Dynamic properties defined by the scripts
    image = PumpShotgunImage;
    canDrop = true;

    TT_ammoType = "shotgun";
    TT_reloads = true;
    TT_maxAmmo = 6;
};

////////////////
//weapon image//
////////////////
datablock ShapeBaseImageData(PumpShotgunImage)
{
   // Basic Item properties
   shapeFile = "./Pump_Shotgun.dts";
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
   item = PumpShotgunItem;
   ammo = " ";
   projectile = PumpShotgunProjectile;
   projectileType = Projectile;

   casing = PumpShotgunShellDebris;
   shellExitDir        = "1.0 0.1 1.0";
   shellExitOffset     = "0 0 0";
   shellExitVariance   = 10.0;  
   shellVelocity       = 5.0;

   //melee particles shoot from eye node for consistancy
   melee = false;
   //raise your arm up or not
   armReady = true;
   minShotTime = 1000;

   doColorShift = true;
   colorShiftColor = PumpShotgunItem.colorShiftColor;

   // Images have a state system which controls how the animations
   // are run, which sounds are played, script callbacks, etc. This
   // state system is downloaded to the client so that clients can
   // predict state changes and animate accordingly.  The following
   // system supports basic ready->fire->reload transitions as
   // well as a no-ammo->dryfire idle state.

   // Initial start up state
    stateName[0]                    = "Activate";
    stateTimeoutValue[0]            = 0.128; //0.15
    stateTransitionOnTimeout[0]     = "DeathCheckA";
    stateSound[0]             = weaponSwitchSound;

    stateName[1]                        = "Ready";
    stateTransitionOnTriggerDown[1]     = "FireDelay";
    stateTransitionOnNotLoaded[1]       = "ReloadCheckA";
    stateScript[1]                  = "onReady";

    stateName[2]                    = "Fire";
    stateTransitionOnTimeout[2]     = "Smoke";
    stateTimeoutValue[2]            = 0.1;
    stateFire[2]                    = true;
    stateAllowImageChange[2]        = false;
    stateScript[2]                  = "onFire";

    stateName[3]              = "Smoke";
    stateTimeoutValue[3]            = 0.192; //0.2
    stateTransitionOnTimeout[3]     = "DeathCheckA";

    // When splitting up the Timeout of a state only 32 ms ticks really count, rounded up
    // 0.01 gets rounded up to 0.032 ms or 1 tick
    // 0.5 / 0.032 = 15.625 -> 16 ticks
    // 15 + 1 -> 15 * 0.032 + 1 * 0.032 = 0.48 + 0.032 ms
    stateName[4]              = "Eject";
    stateTimeoutValue[4]          = 0.48; //0.5
    stateTransitionOnTimeout[4]   = "LoadCheckA";
    stateEjectShell[4]            = true;
    stateSequence[4]              = "fire";
    stateSound[4]             = PumpShotgunReloadSound;
    stateScript[4]                  = "onEject";

    // Would run LoadCheck during Eject but that broke the eject animation for TTAmmo 2/3
    // Some weird interaction with DeathCheck?
    stateName[5]            = "LoadCheckA";
    stateTimeoutValue[5]    = 0.01;
    stateScript[5]          = "TT_onLoadCheck";
    stateTransitionOnTimeout[5] = "WaitForTriggerUp";

    // Loaded - gun has ammo, doesn't need reload
    // NotLoaded - empty gun, needs reload
    // Ammo - reserve ammo available, can reload
    // NoAmmo - no reserve ammo left, can't reload
    // Loaded/NotLoaded goes before Ammo/NoAmmo transitions
    stateName[6]                = "LoadCheckB";
    stateTransitionOnLoaded[6]      = "Ready";
    stateTransitionOnAmmo[6]        = "Reload";
    stateTransitionOnNoAmmo[6]    = "Empty";
    
    stateName[7]                = "ReloadCheckA";
    stateScript[7]              = "TT_onReloadCheck";
    stateTimeoutValue[7]            = 0.01;
    stateTransitionOnTimeout[7]     = "ReloadCheckB";
                        
    stateName[8]                = "ReloadCheckB";
    stateTransitionOnLoaded[8]      = "CompleteReload";
    stateTransitionOnNotLoaded[8]       = "LoadCheckB";

    stateName[9]                  = "Empty";
    stateTransitionOnLoaded[9]      = "Ready";
    stateTransitionOnAmmo[9]       = "Reload";
    stateTransitionOnTriggerDown[9] = "Fire";

    stateName[10]               = "Reload";
    stateTransitionOnTimeout[10]        = "Reloaded";
    stateTransitionOnTriggerDown[10]    = "Fire";
    stateTimeoutValue[10]           = 0.25;
    stateScript[10]             = "onReloadStart";

    stateName[11]               = "Reloaded";
    stateTransitionOnTimeout[11]        = "ReloadCheckA";
    stateTimeoutValue[11]           = 0.2;
    stateScript[11]             = "onReloaded";

    stateName[12]             = "CompleteReload";
    stateTimeoutValue[12]         = 0.5;
    stateTransitionOnTimeout[12]        = "Ready";
    stateSequence[12]             = "fire";
    stateSound[12]            = PumpShotgunReloadSound;
    stateScript[12]                  = "onEject";

    // This state doesn't do anything
    // It's just to keep timing consistent with original T+T
    // FireCheck not needed since onFire does the check
    stateName[13]               = "FireDelay";
    stateTimeoutValue[13]         = 0.01;
    stateTransitionOnTimeout[13]        = "Fire";

    stateName[14]                  = "WaitForTriggerUp";
    stateTransitionOnTriggerUp[14]   = "LoadCheckB";

    // Don't know of a way to stop state animations/sounds from playing so let's add more checks
    // If player is dead this should stop them from completing the reload and playing an animation while dead
    // So much effort just to stop a dead guy from pumping their shotgun
    stateName[15]                  = "DeathCheckA";
    stateTimeoutValue[15]          = 0.032; // necessary to sync state with animation and sound
    stateScript[15]                = "TT_onDeathCheck";
    stateTransitionOnTimeout[15]   = "DeathCheckB";

    stateName[16]                  = "DeathCheckB";
    stateTransitionOnAmmo[16]      = "Eject";
    // stateTransitionOnNoAmmo[16]    = "Empty";
    // uncomment above line for potentially funny bug with setTrigger and bots when
    // $Pref::Server::TT::DeathStopFiring = 0 and $Pref::Server::TT::DeathStopAnims = 1
};

function PumpShotgunImage::onFire(%this,%obj,%slot)
{
    if(%this.TT_canFire(%obj))
    {
        serverPlay3D(PumpShotgunfireSound,%obj.getPosition());
        %obj.playThread(2, activate);

        %this.TT_decrementAmmo(%obj);

        if($Pref::Server::TT::Recoil)
            %obj.spawnExplosion(TTBigRecoilProjectile,"1 1 1");

        %projectile = %this.projectile;
        %spread = 0.0032;
        %shellCount = 9;

        %p = TT_createProjectile(%this, %obj, %slot, %projectile, %shellCount, %spread);

        //just the muzzleflash
        //
        //nothing really special about this
        ///////////////////////////////////////////////////////////
    
        // doesn't actually show up, the projectile lifetime of 10 ms isn't enough to hit anything
        // also it doesn't explode on death so it just fades away into the abyss
        //TT_createProjectile(%this, %obj, %slot, shotgunFlashProjectile, 1);

        //shotgun blast projectile: only effective at point blank, sends targets flying off into the distance
        //
        //more or less represents the concussion blast. i can only assume such a thing exists because
        // i've never stood infront of a fucking shotgun before
        ///////////////////////////////////////////////////////////
    
        TT_createProjectile(%this, %obj, %slot, ShotgunBlastProjectile, 1);
    }
    else if(!$Pref::Server::TT::DeathStopFiring || %obj.getDamagePercent() < 1.0)
    {
        serverPlay3D(PumpShotgunJamSound,%obj.getPosition());
    }
    %this.TT_displayAmmo(%obj);
    return %p;
}

function PumpShotgunImage::onEject(%this,%obj,%slot)
{
    %obj.playThread(2, plant);
    %this.TT_displayAmmo(%obj);
}

function PumpShotgunImage::onReloadStart(%this,%obj,%slot)
{
    if($Pref::Server::TT::DeathStopAnims && %obj.getDamagePercent() >= 1.0)
        return;
    %obj.playThread(2, shiftto);
    serverPlay3D(block_MoveBrick_Sound,%obj.getPosition());
    %this.TT_displayAmmo(%obj);
}

function PumpShotgunImage::onReloaded(%this,%obj,%slot)
{
    // No check for DeathStopAnims since no anims/sounds here and it goes to ReloadCheckA after
    %this.TT_incrementReload(%obj, %slot);
    %this.TT_displayAmmo(%obj);
}
