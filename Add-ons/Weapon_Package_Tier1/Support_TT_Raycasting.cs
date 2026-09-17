////////////////////////////////////////////////////////////////////////////////////////////////////
//                                Support_TT_Raycasting.cs                                        //
// Creator: Space Guy, Iban, edits by Panopticon                                                  //
// Allows you to create weapons that function by instant raycasts rather than projectiles         //
// Shameless copy-paste of Support_RaycastingWeapons.cs, variables renamed to avoid conflicts     //
// Minor changes to accomodate bots, among other bug fixes                                        //
// It also makes it easier to swap out raycast code if I ever decide to do that                   //
// Set these fields in the datablock:                                                             //
// TT_raycastEnabled:              Boolean (must be true for script to hook into weapon)          //
// TT_raycastWeaponRange:          Range of weapon (> 0)                                          //
// TT_raycastWeaponTargets:        Typemasks                                                      //
// TT_raycastWeaponPierceTargets:  Typemasks (weapon fires through these targets, hitting each)   //
// TT_raycastDirectDamage:         Direct Damage                                                  //
// TT_raycastDirectDamageType:     Damage Type ID                                                 //
// TT_raycastCritDirectDamageType: Critical Hit Damage Type ID                                    //
// TT_raycastExplosionProjectile:  Creates this projectile on impact                              //
// TT_raycastExplosionSound:       AudioProfile of sound to play on impact                        //
// TT_raycastExplosionPlayerSound: AudioProfile of sound to play on hitting a player              //
// TT_raycastExplosionBrickSound:  AudioProfile of sound to play on hitting a brick               //
// TT_raycastExplosionCritSound:   AudioProfile of sound to play a critical impact                //
// TT_raycastSpreadAmt:            Spread radius of weapon                                        //
// TT_raycastSpreadCount:          Number of spreading projectiles                                //
// TT_raycastTracerProjectile:     Fires tracer projectiles along the spread path                 //
// TT_raycastCritTracerProjectile: Fires tracer projectiles along the spread path if critical hit //
// TT_raycastFromMuzzle:           Fire from muzzle point to muzzle vector instead of eye         //
//                                                                                                //
// Radius and Brick Damage can be done through the projectile and explosion.                      //
////////////////////////////////////////////////////////////////////////////////////////////////////

package TT_Raycasting
{
	function WeaponImage::onFire(%this,%obj,%slot)
	{
		if(!%this.TT_raycastEnabled || %this.TT_raycastWeaponRange <= 0)
			return Parent::onFire(%this,%obj,%slot);

		%targets = %this.TT_raycastWeaponTargets;
		%ptargets = %this.TT_raycastWeaponPierceTargets;

		if(%this.TT_raycastFromMuzzle)
		{
			%start = %obj.getMuzzlePoint(%slot);
			%aimVec = %obj.getMuzzleVector(%slot);
		}
		else
		{
			%start = %obj.getEyePoint();

			%fvec = %obj.getForwardVector();
			%fX = getWord(%fvec,0);
			%fY = getWord(%fvec,1);

			%evec = %obj.getEyeVector();
			%eX = getWord(%evec,0);
			%eY = getWord(%evec,1);
			%eZ = getWord(%evec,2);

			%eXY = mSqrt(%eX*%eX+%eY*%eY);

			%aimVec = %fX*%eXY SPC %fY*%eXY SPC %eZ;
		}

		if(%this.TT_raycastSpreadCount <= 0)
			%shellcount = 1;
		else
			%shellcount = %this.TT_raycastSpreadCount;

		%hit = 0;

		%fireCrit = 0;

		for(%i = 0; %i < %shellcount; %i++)
		{
			%pierced = "";
			%shellCrit = 0;

			if(%this.TT_raycastSpreadAmt > 0)
			{
				%spread = %this.TT_raycastSpreadAmt;

				%vector = VectorScale(%aimVec, 100);
				%x = (getRandom() - 0.5) * 10 * 3.1415926 * %spread;
				%y = (getRandom() - 0.5) * 10 * 3.1415926 * %spread;
				%z = (getRandom() - 0.5) * 10 * 3.1415926 * %spread;
				%mat = MatrixCreateFromEuler(%x @ " " @ %y @ " " @ %z);
				%shotVec = vectorNormalize(MatrixMulVector(%mat, %vector));
			}
			else
			{
				%shotVec = %aimVec;
			}

			%rangeRem = %this.TT_raycastWeaponRange * getWord(%obj.getScale(), 2);

			while(%rangeRem > 0)
			{
				if(%rangeRem > 100)
					%range = 100;
				else
					%range = %rangeRem;
				%rangeRem -= %range;

				%end = vectorAdd(%start, vectorScale(%shotVec, %range));

				%ray = containerRayCast(%start, %end, %targets, %obj, %pierced);

				%col = getWord(%ray, 0);
				%crit = 0;
				if(isObject(%col))
				{
					%colType = %col.getType();
					%end = posFromRaycast(%ray);
					%normal = normalFromRaycast(%ray);
					%hit++;

					if(isObject(CritProjectile))
					{
						if(%this.TT_isRaycastCritical(%obj, %slot, %col, %end, %normal, %hit))
						{
							%crit = 1;
							%shellCrit = 1;
							%fireCrit = 1;
						}
					}

					%this.TT_onRaycastHit(%obj, %slot, %col, %end, %normal, %shotVec, %crit);

					if(%colType & %ptargets)
					{
						%rangeRem += %range - vectorLen(vectorSub(%end, %start));
						%pierced = %col;
					}
					else
					{
						%rangeRem = 0;
					}
				}
				else
				{
					if(isObject(CritProjectile))
					{
						if(%this.TT_isRaycastCritical(%obj, %slot, -1, "0 0 0", "0 0 1", %hit))
						{
							%crit = 1;
							%shellCrit = 1;
							%fireCrit = 1;
						}
					}
				}
				%start = %end;
			}

			if(isObject(%this.TT_raycastTracerProjectile))
			{
				%projectile = %this.TT_raycastTracerProjectile;
				if(%shellCrit && isObject(%this.TT_raycastCritTracerProjectile))
					%projectile = %this.TT_raycastCritTracerProjectile;

				%muzzle = %obj.getMuzzlePoint(%slot);
				%scaleFactor = getWord(%obj.getScale(), 2);

				%p = new (%this.projectileType)()
				{
					dataBlock = %projectile;
					initialVelocity = vectorScale(vectorNormalize(vectorSub(%end, %muzzle)), %projectile.muzzleVelocity);
					initialPosition = %muzzle;
					sourceObject = %obj;
					sourceSlot = %slot;
					client = %obj.client;
				};
				%p.setScale(%scaleFactor SPC %scaleFactor SPC %scaleFactor);
				MissionCleanup.add(%p);
			}
		}

		if(%obj.getType() & $TypeMasks::PlayerObjectType && %fireCrit)
		{
			serverplay3d(critFireSound,%obj.getHackPosition());
			if(isObject(%obj.client) && %obj.getClassName() $= "Player")
				%obj.client.play2d(critHitSound);
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// Custom WeaponImage functions specific to Tier Tactical                //
	///////////////////////////////////////////////////////////////////////////

	function WeaponImage::TT_onRaycastHit(%this, %obj, %slot, %col, %pos, %normal, %shotVec, %crit)
	{
		if(!isObject(%col))
			return;

		%colType = %col.getType(); //%col changes to CorpseObjectType if you kill a player

		if(!isObject(CritProjectile))
			%crit = 0;

		if(%this.TT_raycastDirectDamage > 0 && %colType & ($TypeMasks::PlayerObjectType | $TypeMasks::VehicleObjectType))
		{
			if(!isObject(%obj.miniGame) && isObject(%col.spawnBrick))
			{
				if(%col.spawnBrick.getGroup().bl_id == getBL_IDFromObject(%obj) || %col.spawnBrick.getGroup().bl_id == 888888)
					%dmg = 1;
			}
			// This bit makes sure that raycasts hitting yourself count as self damage for
			// the purposes of miniGameCanDamage
			if(isObject(%obj.client))
				%attacker = %obj.client;
			else
				%attacker = %obj;
			if(miniGameCanDamage(%attacker, %col) == 1 || %dmg)
				%this.TT_onRaycastDamage(%obj, %slot, %col, %pos, %normal, %shotVec, %crit);
		}

		if(isObject(%this.TT_raycastExplosionProjectile))
		{
			%scaleFactor = getWord(%obj.getScale(), 2);
			%p = new Projectile()
			{
				dataBlock = %this.TT_raycastExplosionProjectile;
				initialPosition = %pos;
				initialVelocity = %normal;
				sourceObject = %obj;
				client = %obj.client;
				sourceSlot = %slot;
				originPoint = %pos;
			};
			MissionCleanup.add(%p);
			%p.setScale(%scaleFactor SPC %scaleFactor SPC %scaleFactor);
			%p.explode();
		}

		if(isObject(%this.TT_raycastExplosionSound))
			serverplay3d(%this.TT_raycastExplosionSound,%pos);

		if(isObject(%this.TT_raycastExplosionCritSound) && %crit)
			serverplay3d(%this.TT_raycastExplosionCritSound,%pos);

		%colPlayer = (%colType & $TypeMasks::PlayerObjectType);

		if(%colPlayer && isObject(%this.TT_raycastExplosionPlayerSound))
			serverplay3d(%this.TT_raycastExplosionPlayerSound,%pos);
		else if(!%colPlayer && isObject(%this.TT_raycastExplosionBrickSound))
			serverplay3d(%this.TT_raycastExplosionBrickSound,%pos);
	}

	function WeaponImage::TT_onRaycastDamage(%this,%obj,%slot,%col,%pos,%normal,%shotVec,%crit)
	{
		%damageType = $DamageType::Direct;
		if(%this.TT_raycastDirectDamageType)
			%damageType = %this.TT_raycastDirectDamageType;

		%scale = getWord(%obj.getScale(), 2);
		%directDamage = mClampF(%this.TT_raycastDirectDamage, -100, 100) * %scale;

		if(%crit)
		{
			if(%this.TT_raycastCritDirectDamageType)
				%damageType = %this.TT_raycastCritDirectDamageType;

			%directDamage = %directDamage * 3;

			%colscale = getWord(%col.getScale(),2);
			%col.spawnExplosion(critProjectile,%colscale);
			if(isObject(%col.client) && %col.getClassName() $= "Player")
				%col.client.play2d(critRecieveSound);
		}

		if(%this.TT_raycastImpactImpulse > 0)
			%col.applyImpulse(%pos,vectorScale(%shotVec,%this.TT_raycastImpactImpulse * %scale));

		if(%this.TT_raycastVerticalImpulse > 0)
			%col.applyImpulse(%pos,"0 0" SPC %this.TT_raycastVerticalImpulse * %scale);

		if(isObject(%obj.client))
		{
			%col.lastPusher = %obj.client;
			%col.lastPushTime = getSimTime();
		}

		// Originally used %col.client if that existed but that breaks point scoring when killing bots
		if(%obj.getClassName() $= "AIPlayer")
		{
			%p = new ScriptObject()
			{
				sourceObject = %obj;
				client = %obj.client;
			};
			MissionCleanup.add(%p);
			%col.damage(%p, %pos, %directDamage, %damageType);
			%p.delete();
		}
		else
		{
			%col.damage(%obj, %pos, %directDamage, %damageType);
		}
	}

	function WeaponImage::TT_isRaycastCritical(%this,%obj,%slot,%col,%pos,%normal,%hit)
	{
		return 0;
	}
};
activatePackage(TT_Raycasting);
