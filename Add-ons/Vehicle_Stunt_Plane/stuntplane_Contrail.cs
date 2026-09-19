datablock ParticleData(contrailParticle)
{
   dragCoefficient      = 0;
   windCoefficient     = 0;
   gravityCoefficient   = 0;
   inheritedVelFactor   = 0.0;
   constantAcceleration = 0.0;
   spinRandomMin = -90;
   spinRandomMax = 90;
   lifetimeMS           = 500;
   lifetimeVarianceMS   = 0;
   textureName          = "base/data/particles/cloud";
   colors[0]     = "1 1 1 1";
   colors[1]     = "0.2 0.2 1 0";
   sizes[0]      = 0.1;
   sizes[1]      = 0.05;
};

datablock ParticleEmitterData(contrailEmitter)
{
   ejectionPeriodMS = 1;
   periodVarianceMS = 0;
   ejectionVelocity = 0;
   velocityVariance = 0.0;
   ejectionOffset   = 0.0;
   thetaMin         = 0;
   thetaMax         = 180;
   phiReferenceVel  = 0;
   phiVariance      = 360;
   overrideAdvance = false;
   particles = "contrailParticle";
};

datablock ShapeBaseImageData(ContrailImage1)
{
   shapeFile = "base/data/shapes/empty.dts";
	emap = false;

	mountPoint = 3;
   rotation = "1 0 0 -90";

	stateName[0]					= "Ready";
	stateTransitionOnTimeout[0]		= "FireA";
	stateTimeoutValue[0]			= 0.01;

	stateName[1]					= "FireA";
	stateTransitionOnTimeout[1]		= "Done";
	stateWaitForTimeout[1]			= True;
	stateTimeoutValue[1]			= 10000;
	stateEmitter[1]					= ContrailEmitter;
	stateEmitterTime[1]				= 10000;

	stateName[2]					= "Done";
	stateScript[2]					= "onDone";
};
function ContrailImage1::onDone(%this,%obj,%slot)
{
	%obj.unMountImage(%slot);
}
datablock ShapeBaseImageData(ContrailImage2)
{
   	shapeFile = "base/data/shapes/empty.dts";
	emap = false;

	mountPoint = 4;
   	rotation = "1 0 0 -90";

	stateName[0]					= "Ready";
	stateTransitionOnTimeout[0]		= "FireA";
	stateTimeoutValue[0]			= 0.01;

	stateName[1]					= "FireA";
	stateTransitionOnTimeout[1]		= "Done";
	stateWaitForTimeout[1]			= True;
	stateTimeoutValue[1]			= 10000;
	stateEmitter[1]					= ContrailEmitter;
	stateEmitterTime[1]				= 10000;

	stateName[2]					= "Done";
	stateScript[2]					= "onDone";
};
function ContrailImage2::onDone(%this,%obj,%slot)
{
	%obj.unMountImage(%slot);
}

function contrailCheck(%obj)
{
//	return;
	if(!isObject(%obj))
		return;

	%speed = vectorLen(%obj.getVelocity());
	if(%speed < %obj.dataBlock.minContrailSpeed)
	{
		if(%obj.getMountedImage(3) !$= "")
		{
			%obj.unMountImage(2);
			%obj.unMountImage(3);
		}
	}
	else
	{
		if(%obj.getMountedImage(3) $= 0)
		{
			%obj.mountImage(contrailImage1,2);
			%obj.mountImage(contrailImage2,3);
		}
	}

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

	schedule(2000,0,"contrailCheck",%obj);
}