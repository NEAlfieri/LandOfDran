datablock DebrisData(biplaneTireDebris)
{
   emitters = "jeepTireDebrisTrailEmitter";

	shapeFile = "./biplanefrontwheel.dts";
	lifetime = 2.0;
	minSpinSpeed = -400.0;
	maxSpinSpeed = 200.0;
	elasticity = 0.5;
	friction = 0.2;
	numBounces = 3;
	staticOnMaxBounce = true;
	snapOnMaxBounce = false;
	fade = true;

	gravModifier = 2;
};


// Explosion
////////////
datablock ExplosionData(biplaneExplosion : vehicleExplosion)
{
   debris = biplaneTireDebris;
   debrisNum = 4;
   debrisNumVariance = 0;
   debrisPhiMin = 0;
   debrisPhiMax = 360;
   debrisThetaMin = 40;
   debrisThetaMax = 85;
   debrisVelocity = 14;
   debrisVelocityVariance = 3;
};


// Projectile - you can't spawn explosions on the server, so we use a short-lived projectile
/////////////////////////////////
datablock ProjectileData(biplaneExplosionProjectile : vehicleExplosionProjectile)
{
   directDamage        = 0;
   radiusDamage        = 0;
   damageRadius        = 0;
   explosion           = biplaneExplosion;

   directDamageType  = $DamageType::VehicleExplosion;
   radiusDamageType  = $DamageType::VehicleExplosion;

   explodeOnDeath		= 1;

   armingDelay         = 0;
   lifetime            = 0;

   uiName = "";
};