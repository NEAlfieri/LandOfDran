datablock WheeledVehicleData(BiplaneVehicle)
{
	category = "Vehicles";
	displayName = " ";
	shapeFile = "./biplane.dts";
	emap = true;
	minMountDist = 3;
   
   numMountPoints = 4;
   mountThread[0] = "sit";
   mountThread[1] = "sit";
   mountThread[2] = "sit";
   mountThread[3] = "sit";


	maxDamage = 100.00;
	destroyedLevel = 100.00;
	energyPerDamagePoint = 160;
	speedDamageScale = 1.04;
	collDamageThresholdVel = 20.0;
	collDamageMultiplier   = 0.02;

	massCenter = "0 0 0";
   //massBox = "2 5 1";

	maxSteeringAngle = 0.9785;  // Maximum steering angle, should match animation
	integration = 4;           // Force integration time: TickSec/Rate
	tireEmitter = VehicleTireEmitter; // All the tires use the same dust emitter

	// 3rd person camera settings
	cameraRoll = false;         // Roll the camera with the vehicle
	cameraMaxDist = 10;         // Far distance from vehicle
	cameraOffset = 7.5;        // Vertical offset from camera mount point
	cameraLag = 0.0;           // Velocity lag of camera
	cameraDecay = 0.75;        // Decay per sec. rate of velocity lag
	cameraTilt = 0.4;
   collisionTol = 0.1;        // Collision distance tolerance
   contactTol = 0.1;

	useEyePoint = false;	

	// defaultTire	= biplaneTire;
	// defaultSpring	= biplaneSpring;
	// flatTire	= biplaneFlatTire;
	// flatSpring	= biplaneFlatSpring;

   //numWheels = 3;

	// Rigid Body
	mass = 200;
	density = 5.0;
	drag = 0.6;
	bodyFriction = 0.6;
	bodyRestitution = 0.6;
	minImpactSpeed = 10;        // Impacts over this invoke the script callback
	softImpactSpeed = 10;       // Play SoftImpact Sound
	hardImpactSpeed = 15;      // Play HardImpact Sound
	groundImpactMinSpeed    = 10.0;

	// Engine
	engineTorque = 4000; //4000;       // Engine power
	engineBrake = 2000;         // Braking when throttle is 0
	brakeTorque = 4000;        // When brakes are applied
	maxWheelSpeed = 20;        // Engine scale by current speed / max speed

	rollForce		= 900;
	yawForce		= 600;
	pitchForce		= 1000;
	rotationalDrag		= 0.2;

	// Energy
	maxEnergy = 100;
	jetForce = 3000;
	minJetEnergy = 30;
	jetEnergyDrain = 2;

   isSled = false;

   forwardThrust		= 3000;
	reverseThrust		= 2000;
	lift			= 100;
	maxForwardVel		= 40;
	maxReverseVel		= 40;
	horizontalSurfaceForce	= 130;   // Horizontal center "wing" (provides "bite" into the wind for climbing/diving and turning)
	verticalSurfaceForce	= 130; 
	rollForce		= 4000;
	yawForce		= 6000;
	pitchForce		= 6000;
	rotationalDrag		= 0.5;
	stallSpeed		= 10;

//   forwardThrust		= 2000;
//	reverseThrust		= 2000;
//	lift			= 100;
//	maxForwardVel		= 70;
//	maxReverseVel		= 70;
//	horizontalSurfaceForce	= 100;   // Horizontal center "wing" (provides "bite" into the wind for climbing/diving and turning)
//	verticalSurfaceForce	= 100; 
//	rollForce		= 6000;
//	yawForce		= 6000;
//	pitchForce		= 4000;
//	rotationalDrag		= 0.5;
//	stallSpeed		= 10;
//
//   forwardThrust		= 2500;
//	reverseThrust		= 500;
//	lift			= 15000;
//	maxForwardVel		= 60;
//	maxReverseVel		= 10;
//	horizontalSurfaceForce	= 500;   // Horizontal center "wing" (provides "bite" into the wind for climbing/diving and turning)
//	verticalSurfaceForce	= 500; 
//	rollForce		= 5000;
//	yawForce		= 5000;
//	pitchForce		= 5000;
//	rotationalDrag		= 0.1;
//	stallSpeed		= 10;

	splash = vehicleSplash;
	splashVelocity = 4.0;
	splashAngle = 67.0;
	splashFreqMod = 300.0;
	splashVelEpsilon = 0.60;
	bubbleEmitTime = 1.4;
	splashEmitter[0] = vehicleFoamDropletsEmitter;
	splashEmitter[1] = vehicleFoamEmitter;
	splashEmitter[2] = vehicleBubbleEmitter;
	mediumSplashSoundVelocity = 10.0;   
	hardSplashSoundVelocity = 20.0;   
	exitSplashSoundVelocity = 5.0;
		
	//mediumSplashSound = "";
	//hardSplashSound = "";
	//exitSplashSound = "";
	
	// Sounds
	//   jetSound = ScoutThrustSound;
	//engineSound = idleSound;
	//squealSound = skidSound;
	softImpactSound = slowImpactSound;
	hardImpactSound = fastImpactSound;
	//wheelImpactSound = slowImpactSound;

	//   explosion = VehicleExplosion;
	justcollided = 0;

   uiName = "Biplane ";
	rideable = true;
		lookUpLimit = 0.50;
		lookDownLimit = 0.50;

	paintable = true;
   
   damageEmitter[0] = VehicleBurnEmitter;
	damageEmitterOffset[0] = "0.0 0.0 0.0 ";
	damageLevelTolerance[0] = 0.99;

   damageEmitter[1] = VehicleBurnEmitter;
	damageEmitterOffset[1] = "0.0 0.0 0.0 ";
	damageLevelTolerance[1] = 1.0;

   numDmgEmitterAreas = 1;

   initialExplosionProjectile = biplaneExplosionProjectile;
   initialExplosionOffset = 0;         //offset only uses a z value for now

   burnTime = 4000;

   finalExplosionProjectile = biplaneFinalExplosionProjectile;
   finalExplosionOffset = 0.5;          //offset only uses a z value for now


   minRunOverSpeed    = 2;   //how fast you need to be going to run someone over (do damage)
   runOverDamageScale = 5;   //when you run over someone, speed * runoverdamagescale = damage amt
   runOverPushScale   = 1.2; //how hard a person you're running over gets pushed
   
      // Advanced Steering
   steeringAutoReturn = false;
   // steeringAutoReturnRate = 0.9;
   // steeringAutoReturnMaxSpeed = 10;
   steeringUseStrafeSteering = false;
   // steeringStrafeSteeringRate = 0.1;
   
   // minContrailSpeed = 30;
};

function biplanecontrailCheck(%obj)
{
	//return;
	if(!isObject(%obj))
		return;

	%speed = vectorLen(%obj.getVelocity());
	// if(%speed < %obj.dataBlock.minContrailSpeed)
	// {
		// if(%obj.getMountedImage(3) !$= "")
		// {
			// %obj.unMountImage(2);
			// %obj.unMountImage(3);
		// }
	// }
	// else
	// {
		// if(%obj.getMountedImage(3) $= 0)
		// {
			// %obj.mountImage(contrailImage1,2);
			// %obj.mountImage(contrailImage2,3);
		// }
	// }

	if(%speed < 5)
	{
		if(%obj.prop !$= "slow")
		{
			%obj.playThread(0,propslow);
			%obj.prop = "slow";
		}
	}
	else
	{
		if(%obj.prop !$= "fast")
		{
			%obj.playThread(0,propfast);
			%obj.prop = "fast";
		}
	}

	schedule(2000,0,"biplanecontrailCheck",%obj);
   
}

function Biplanevehicle::onadd(%this,%obj)
{ 
	parent::onadd(%this,%obj);
		
	%obj.setWheelTire(0, biplanetire);
	%obj.setWheelTire(1, biplanetire);
	%obj.setWheelTire(2, biplanebacktire);

	%obj.setWheelSpring(0, biplaneSpring);
	%obj.setWheelSpring(1, biplaneSpring);
	%obj.setWheelSpring(2, biplanebackSpring);
	
	%obj.setWheelSteering(0,0);
	%obj.setWheelSteering(1,0);
	%obj.setWheelSteering(2,-1);

	%obj.setWheelPowered(0,true);
    %obj.setWheelPowered(1,true);
	%obj.setWheelPowered(2,true);
	
	%obj.playthread(0,propslow);
	
	biplanecontrailCheck(%obj);
	    
}
