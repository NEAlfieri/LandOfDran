datablock DebrisData(biplaneDebris)
{
   emitters = "JeepDebrisTrailEmitter";

	shapeFile = "./biplaneWreckage.dts";
	lifetime = 3.0;
	minSpinSpeed = -500.0;
	maxSpinSpeed = 500.0;
	elasticity = 0.5;
	friction = 0.2;
	numBounces = 1;
	staticOnMaxBounce = true;
	snapOnMaxBounce = false;
	fade = true;

	gravModifier = 2;
};

datablock ExplosionData(BiPlaneFinalExplosion : vehicleFinalExplosion)
{
   debris = biplaneDebris;
   debrisNum = 1;
   debrisNumVariance = 0;
   debrisPhiMin = 0;
   debrisPhiMax = 360;
   debrisThetaMin = 0;
   debrisThetaMax = 20;
   debrisVelocity = 18;
   debrisVelocityVariance = 3;
};

datablock ProjectileData(biplaneFinalExplosionProjectile : vehicleFinalExplosionProjectile)
{
   directDamage        = 0;
   radiusDamage        = 0;
   damageRadius        = 0;
   explosion           = biplaneFinalExplosion;

   directDamageType  = $DamageType::VehicleExplosion;
   radiusDamageType  = $DamageType::VehicleExplosion;

   explodeOnDeath		= 1;

   armingDelay         = 0;
   lifetime            = 10;
   
   uiName = "";
};