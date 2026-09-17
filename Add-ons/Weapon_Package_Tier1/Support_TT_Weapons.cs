// Support functions so that you don't have to copy-paste Ephialtes' shotgun code over and over
function TT_createProjectile(%this, %obj, %slot, %projectile, %shellCount, %spread)
{
   for(%shell = 0; %shell < %shellCount; %shell++)
   {
      %vector = %obj.getMuzzleVector(%slot);
      %objectVelocity = %obj.getVelocity();
      %vector1 = VectorScale(%vector, %projectile.muzzleVelocity);
      %vector2 = VectorScale(%objectVelocity, %projectile.velInheritFactor);
      %velocity = VectorAdd(%vector1,%vector2);
      if(%spread)
      {
         %x = (getRandom() - 0.5) * 10 * 3.1415926 * %spread;
         %y = (getRandom() - 0.5) * 10 * 3.1415926 * %spread;
         %z = (getRandom() - 0.5) * 10 * 3.1415926 * %spread;
         %mat = MatrixCreateFromEuler(%x @ " " @ %y @ " " @ %z);
         %velocity = MatrixMulVector(%mat, %velocity);
      }

      %p = new (%this.projectileType)()
      {
         dataBlock = %projectile;
         initialVelocity = %velocity;
         initialPosition = %obj.getMuzzlePoint(%slot);
         sourceObject = %obj;
         sourceSlot = %slot;
         client = %obj.client;
      };
      MissionCleanup.add(%p);

      %scale = getWord(%obj.getScale(), 2);
      %p.setScale(%scale SPC %scale SPC %scale);
   }
   return %p;
}

// Knocks player back in the direction opposite where they are aiming
function TT_knockback(%obj, %scaleX, %scaleY, %scaleZ)
{
   %fvec = %obj.getForwardVector();
   %fX = getWord(%fvec,0);
   %fY = getWord(%fvec,1);

   %evec = %obj.getEyeVector();
   %eX = getWord(%evec,0);
   %eY = getWord(%evec,1);
   %eZ = getWord(%evec,2);

   %eXY = mSqrt(%eX*%eX+%eY*%eY);

   %scaledAimVec = %fX*%eXY*%scaleX SPC %fY*%eXY*%scaleY SPC %eZ*%scaleZ;

   %obj.setVelocity(VectorAdd(%obj.getVelocity(),%scaledAimVec));
}

// This is that strange slow down effect SMGs do
function TT_dampenVelocity(%obj, %divisor)
{
   if($Pref::Server::TT::DisableBulletSlow)
      return;

   %vel = %obj.getVelocity();

   // TT original code was:
   // %obj.setVelocity(getWord(%obj.getVelocity(),0)/1.1 SPC getWord(%obj.getVelocity(),1)/1.1 SPC getWord(%obj.getVelocity(),1.1));
   // getWord(%col.getVelocity(),1.1) is pretty weird and evaluates to getWord(%col.getVelocity(),1) so it just gets the y component
   // without scaling. Probably a typo

   // Fixed version (what was probably intended) below:
   // %obj.setVelocity(vectorScale(%vel, 1 / 2));
   // Nevermind, the above does not preserve the slow down functionality when player is on the ground

   // This version is a bit different but tries to be less wonky
   // It first divides the velocity and then temporarily reduces the move speed of the player
   // After a very short period the slowdown completely disappears
   // Multiple shots can increase the slowdown up to a point and reset the time until slowdown
   // goes away
   %obj.setVelocity(vectorScale(%vel, 1 / %divisor));
   %obj.TT_slow(%divisor, 1 / (3 * %divisor), 200);

   // Equivalent of original version below:
   // %x = getWord(%vel, 0);
   // %y = getWord(%vel, 1);
   // %obj.setVelocity(%x / %divisor SPC %y / %divisor SPC %y);

   // I considered adding a pref to toggle which version to use, but it should be clear which is superior
   // Go ahead and change it if you crave original functionality
}

package TT_Weapons
{
   function Player::TT_setSpeedMultiplier(%this, %m)
   {
      %db = %this.getDatablock();
      %this.TT_speedMultiplier = %m;
      %this.setMaxBackwardSpeed(%db.maxBackwardSpeed * %m);
      %this.setMaxCrouchBackwardSpeed(%db.maxBackwardCrouchSpeed * %m);
      %this.setMaxCrouchForwardSpeed(%db.maxForwardCrouchSpeed * %m);
      %this.setMaxCrouchSideSpeed(%db.maxSideCrouchSpeed * %m);
      %this.setMaxForwardSpeed(%db.maxForwardSpeed * %m);
      %this.setMaxSideSpeed(%db.maxSideSpeed * %m);
      %this.setMaxUnderwaterBackwardSpeed(%db.maxUnderwaterBackwardSpeed * %m);
      %this.setMaxUnderwaterForwardSpeed(%db.maxUnderwaterForwardSpeed * %m);
      %this.setMaxUnderwaterSideSpeed(%db.maxUnderwaterSideSpeed * %m);
   }

   function Player::TT_resetSpeed(%this)
   {
      %this.TT_setSpeedMultiplier(1);
      %this.TT_speedMultiplier = "";
      %this.TT_resetSpeedSched = "";
   }

   // 0 <= %min <= 1
   function Player::TT_slow(%this, %divisor, %min, %time)
   {
      %m = %this.TT_speedMultiplier;
      if(%m $= "")
      {
         // The first shot does a significant slowdown
         // Brings us to halfway from original speed to minimum speed
         %this.TT_setSpeedMultiplier((1 + %min) / 2);
      }
      else if(%m > %min)
      {
         // Exponential decay, gets clamped if it falls below %min
         // Note: mClamp doesn't work with floats, mClampF does but isn't needed here
         %this.TT_setSpeedMultiplier(getMax(%m / %divisor, %min));
      }
      cancel(%this.TT_resetSpeedSched);
      %this.TT_resetSpeedSched = %this.schedule(%time, TT_resetSpeed);
   }
};

activatePackage(TT_Weapons);

function TT_isRaycastHeadshot(%this, %obj, %slot, %col, %pos, %normal, %hit)
{
   // not sure why spread needs to be 0 to headshot
   // if(%this.TT_raycastSpreadAmt > 0)
   //    return 0;
   if(!isObject(%col))
      return 0;
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
   if(miniGameCanDamage(%attacker, %col) != 1 && !%dmg)
      return 0;
   if(%col.getType() & $TypeMasks::PlayerObjectType)
   {
      %colscale = getWord(%col.getScale(),2);
      return (getword(%pos, 2) > getword(%col.getWorldBoxCenter(), 2) - 3.3*%colscale);
   }
   return 0;
}

// %dmg should typically be %this.directDamage (the damage defined by the projectile)
// if headshot, new damage is %dmg * %multiplier
function TT_processHeadshotDamage(%this, %obj, %col, %pos, %dmg, %multiplier, %headshotDmgType)
{
   if(%dmg <= 0)
      return;

   %damageType = $DamageType::Direct;
   if(%this.DirectDamageType)
      %damageType = %this.DirectDamageType;

   %scale = getWord(%obj.getScale(), 2);
   %dmg *= %scale;

   %sobj = %obj.sourceObject;
   if(%sobj.getType() & $TypeMasks::PlayerObjectType)
   {
      if(isObject(%sobj.client) && %sobj.getClassName() $= "Player")
         %sobj.client.play2D(bulletHitSound);
   }

   if(%col.getType() & $TypeMasks::PlayerObjectType)
   {
      %colscale = getWord(%col.getScale(),2);
      if(getword(%pos, 2) > getword(%col.getWorldBoxCenter(), 2) - 3.3*%colscale)
      {
         %dmg *= %multiplier;
         %damageType = %headshotDmgType;
      }
      %col.damage(%obj, %pos, %dmg, %damageType);
   }
   else
   {
      %col.damage(%obj, %pos, %dmg, %damageType);
   }
}

package TT_EmoteCritical
{
   function TT_processHeadshotDamage(%this, %obj, %col, %pos, %dmg, %multiplier, %headshotDmgType)
   {
      if(%dmg <= 0)
         return;

      %damageType = $DamageType::Direct;
      if(%this.DirectDamageType)
         %damageType = %this.DirectDamageType;

      %scale = getWord(%obj.getScale(), 2);
      %dmg *= %scale;

      %sobj = %obj.sourceObject;
      if(%sobj.getType() & $TypeMasks::PlayerObjectType)
      {
         if(isObject(%sobj.client) && %sobj.getClassName() $= "Player")
            %sobj.client.play2D(bulletHitSound);
      }

      if(%col.getType() & $TypeMasks::PlayerObjectType)
      {
         %colscale = getWord(%col.getScale(),2);
         if(getword(%pos, 2) > getword(%col.getWorldBoxCenter(), 2) - 3.3*%colscale)
         {
            %dmg *= %multiplier;
            %damageType = %headshotDmgType;

            %col.spawnExplosion(critProjectile,%colscale);
            if(isObject(%col.client) && %col.getClassName() $= "Player")
               %col.client.play2D(critRecieveSound);

            if(%sobj.getType() & $TypeMasks::PlayerObjectType)
            {
               serverPlay3D(critFireSound,%sobj.getHackPosition());
               if(isObject(%sobj.client) && %sobj.getClassName() $= "Player")
                  %sobj.client.play2D(critHitSound);
            }
         }
         %col.damage(%obj, %pos, %dmg, %damageType);
      }
      else
      {
         %col.damage(%obj, %pos, %dmg, %damageType);
      }
   }
};

// If Emote_Critical is enabled, overwrite the projectile headshot function to use it
if($AddOn__Emote_Critical == 1)
{
   activatePackage(TT_EmoteCritical);
}
