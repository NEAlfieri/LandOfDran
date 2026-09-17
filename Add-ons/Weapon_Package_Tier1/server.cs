function TT_defaultIfUnset(%pref, %default, %category)
{
   if(%category $= "")
      %category = "TT";
   if($Pref::Server["::" @ %category @ "::" @ %pref] $= "")
      $Pref::Server["::" @ %category @ "::" @ %pref] = %default;
}

if($RTB::Hooks::ServerControl)
{
	// ammo system changer. 0 is T+T2 style, 1 is T+T style, 2 is bullet-hose classic blockland style, 3 (new!) is classic arena shooter style
	RTB_registerPref("Ammo System","Tier+Tactical | Ammo","$Pref::Server::TT::Ammo","list T+T2 0 T+T1 1 Classic 2 Arena 3","Weapon_Package_Tier1",0,0,1);
   RTB_registerPref("Display Ammo","Tier+Tactical | Ammo","$Pref::Server::TT::DisplayAmmo","bool","Weapon_Package_Tier1",1,0,1);
   // how long ammo bottomprints last in seconds. 0 is forever (originally was 4 for most weapons)
   RTB_registerPref("Display Duration","Tier+Tactical | Ammo","$Pref::Server::TT::DisplayTime","int 0 10","Weapon_Package_Tier1",4,0,1);
   RTB_registerPref("Infinite Ammo for Bots","Tier+Tactical | Ammo","$Pref::Server::TT::BotInfAmmo","bool","Weapon_Package_Tier1",0,0,1);
   // fixes the bug where changing your weapon with SetInventory event transfers over the ammo from the old weapon over to the new one
   RTB_registerPref("SetInventory Bugfix","Tier+Tactical | Ammo","$Pref::Server::TT::SetInvBugfix","bool","Weapon_Package_Tier1",1,0,1);
   // attempts to clear unused TT_toolAmmo variables, fixing some cases where RemoveItem event allows ammo of deleted weapons to carry over
   RTB_registerPref("RemoveItem Bugfix","Tier+Tactical | Ammo","$Pref::Server::TT::RemoveItemBugfix","bool","Weapon_Package_Tier1",0,0,1);

   RTB_registerPref("Players Drop Ammo","Tier+Tactical | Ammo","$Pref::Server::TT::PlayerAmmoDrop","bool","Weapon_Package_Tier1",1,0,1);
   // bots originally couldn't drop ammo
   RTB_registerPref("Bots Drop Ammo","Tier+Tactical | Ammo","$Pref::Server::TT::BotAmmoDrop","bool","Weapon_Package_Tier1",0,0,1);

   // bots originally couldn't pick up ammo
   RTB_registerPref("Bots Pick Up Ammo","Tier+Tactical | Ammo","$Pref::Server::TT::BotAmmoPickUp","bool","Weapon_Package_Tier1",0,0,1);

   // if disabled, then you won't consume an ammo pickup if you can't pick up at least 1 bullet from it (originally always picked up ammo)
   RTB_registerPref("Pick Up Ammo When Full","Tier+Tactical | Ammo","$Pref::Server::TT::FullAmmoPickUp","bool","Weapon_Package_Tier1",0,0,1);
   // stops ammo pickups from showing up in item list (in case you don't want to use them), requires server restart
   RTB_registerPref("Disable Ammo Pickups","Tier+Tactical | Ammo","$Pref::Server::TT::DisableAmmoItems","bool","Weapon_Package_Tier1",0,1,1);

   // enable screen shaking when firing (originally did)
   RTB_registerPref("Enable Recoil","Tier+Tactical | Miscellaneous","$Pref::Server::TT::Recoil","bool","Weapon_Package_Tier1",1,0,1);
   // stop blockheads from pumping shotguns when dead (originally didn't)
   RTB_registerPref("Stop Anims on Death","Tier+Tactical | Miscellaneous","$Pref::Server::TT::DeathStopAnims","bool","Weapon_Package_Tier1",1,0,1);
   // mainly for akimbo weapons, can stop left handed guns from firing after death (originally didn't)
   RTB_registerPref("Firing on Death Bugfix","Tier+Tactical | Miscellaneous","$Pref::Server::TT::DeathStopFiring","bool","Weapon_Package_Tier1",1,0,1);
   // if you have multiple copies of an item in your inventory, this lets you use them like separate weapons
   // otherwise they are all tied to the same image, so if one reloads it tries to finish the reload even if you switch to a full ammo item
   RTB_registerPref("Remount Duplicate Items","Tier+Tactical | Miscellaneous","$Pref::Server::TT::RemountDups","bool","Weapon_Package_Tier1",0,0,1);
   // if you were ever annoyed by SMGs stopping you dead in your tracks, this is the pref for you
   // this also affects weapons like LMGs and battle rifles
   // this can't stop any custom weapons if they don't use TT_dampenVelocity
   RTB_registerPref("Disable Bullet Slowdown","Tier+Tactical | Miscellaneous","$Pref::Server::TT::DisableBulletSlow","bool","Weapon_Package_Tier1",0,0,1);
   // this makes it much more convenient to enable and disable easter eggs. requires server restart
   RTB_registerPref("???","Tier+Tactical | Miscellaneous","$Pref::Server::TT::EasterEgg","bool","Weapon_Package_Tier1",0,1,1);
   // you absolute madman. disables most tier 1 datablocks (but not most prefs) in case you wanted the ammo system but none of the guns. requires server restart
   // do not think that you can use any of the other tiers without tier 1 this way. missing datablocks will fuck everything up.
   // only use if you have an expansion pack that uses absolutely nothing from tier 1 besides the ammo system, ammo drops, and prefs
   RTB_registerPref("Disable Tier 1 Weapons","Tier+Tactical | Miscellaneous","$Pref::Server::TT::DisableTier1","bool","Weapon_Package_Tier1",0,1,1);

   // where weapons like grenade launcher and RPG can do rapid fire in ammo modes 2 and 3
   // if not, they'll reload like normal after every shot (originally did)
   RTB_registerPref("Always Reload (Explosive)","Tier+Tactical | Single Shot Weapons","$Pref::Server::TT::AlwaysReloadEx","bool","Weapon_Package_Tier1",1,0,1);
   // same as above but for crossbow and single shotgun (originally didn't)
   RTB_registerPref("Always Reload (Other)","Tier+Tactical | Single Shot Weapons","$Pref::Server::TT::AlwaysReloadNonEx","bool","Weapon_Package_Tier1",1,0,1);

   // this forms the basis of tier+tactical's new ammo mod. just sayin'.
   if(!$Pref::Server::TT::DisableTier1)
   {
   	RTB_registerPref("Starting Pistol Ammo","Tier+Tactical | Starting Ammo","$Pref::Server::TT::Start9MM","int 0 280","Weapon_Package_Tier1",35*4,0,1);
   	RTB_registerPref("Starting Rifle Ammo","Tier+Tactical | Starting Ammo","$Pref::Server::TT::Start556","int 0 180","Weapon_Package_Tier1",15*6,0,1);
   	RTB_registerPref("Starting Shotgun Ammo","Tier+Tactical | Starting Ammo","$Pref::Server::TT::Startshotgun","int 0 48","Weapon_Package_Tier1",6*4,0,1);
   	RTB_registerPref("Starting Sniper Ammo","Tier+Tactical | Starting Ammo","$Pref::Server::TT::Start270","int 0 32","Weapon_Package_Tier1",4*4,0,1);
   	RTB_registerPref("Starting Magnum Ammo","Tier+Tactical | Starting Ammo","$Pref::Server::TT::Start880","int 0 36","Weapon_Package_Tier1",6*3,0,1);
   	RTB_registerPref("Starting Grenade Ammo","Tier+Tactical | Starting Ammo","$Pref::Server::TT::Startbomb","int 0 12","Weapon_Package_Tier1",1*3,0,1);
   	RTB_registerPref("Starting Rocket Ammo","Tier+Tactical | Starting Ammo","$Pref::Server::TT::Startrocket","int 0 12","Weapon_Package_Tier1",1*2,0,1);
   	RTB_registerPref("Starting Big Rifle Ammo","Tier+Tactical | Starting Ammo","$Pref::Server::TT::Start708","int 0 144","Weapon_Package_Tier1",24*3,0,1);
   	RTB_registerPref("Starting Bolt Ammo","Tier+Tactical | Starting Ammo","$Pref::Server::TT::Startbolt","int 0 24","Weapon_Package_Tier1",1*12,0,1);

   	RTB_registerPref("Max Pistol Ammo","Tier+Tactical | Maximum Ammo","$Pref::Server::TT::Max9MM","int 1 560","Weapon_Package_Tier1",280,0,1);
   	RTB_registerPref("Max Rifle Ammo","Tier+Tactical | Maximum Ammo","$Pref::Server::TT::Max556","int 1 360","Weapon_Package_Tier1",180,0,1);
   	RTB_registerPref("Max Shotgun Ammo","Tier+Tactical | Maximum Ammo","$Pref::Server::TT::Maxshotgun","int 1 96","Weapon_Package_Tier1",48,0,1);
   	RTB_registerPref("Max Sniper Ammo","Tier+Tactical | Maximum Ammo","$Pref::Server::TT::Max270","int 1 64","Weapon_Package_Tier1",32,0,1);
   	RTB_registerPref("Max Magnum Ammo","Tier+Tactical | Maximum Ammo","$Pref::Server::TT::Max880","int 1 72","Weapon_Package_Tier1",36,0,1);
   	RTB_registerPref("Max Grenade Ammo","Tier+Tactical | Maximum Ammo","$Pref::Server::TT::Maxbomb","int 1 24","Weapon_Package_Tier1",12,0,1);
   	RTB_registerPref("Max Rocket Ammo","Tier+Tactical | Maximum Ammo","$Pref::Server::TT::Maxrocket","int 1 24","Weapon_Package_Tier1",12,0,1);
   	RTB_registerPref("Max Big Rifle Ammo","Tier+Tactical | Maximum Ammo","$Pref::Server::TT::Max708","int 1 288","Weapon_Package_Tier1",144,0,1);
   	RTB_registerPref("Max Bolt Ammo","Tier+Tactical | Maximum Ammo","$Pref::Server::TT::Maxbolt","int 1 48","Weapon_Package_Tier1",24,0,1);
   }
}
else
{
   // SO THIS IS THE POWER OF SUBLIME TEXT...
   // KILL ME
   TT_defaultIfUnset("Ammo", 0);
   TT_defaultIfUnset("DisplayAmmo", 1);
   TT_defaultIfUnset("DisplayTime", 4);
   TT_defaultIfUnset("BotInfAmmo", 0);
   TT_defaultIfUnset("SetInvBugfix", 1);
   TT_defaultIfUnset("RemoveItemBugfix", 0);
   TT_defaultIfUnset("PlayerAmmoDrop", 1);
   TT_defaultIfUnset("BotAmmoDrop", 0);
   TT_defaultIfUnset("BotAmmoPickUp", 0);
   TT_defaultIfUnset("FullAmmoPickUp", 0);
   TT_defaultIfUnset("DisableAmmoItems", 0);
   TT_defaultIfUnset("Recoil", 1);
   TT_defaultIfUnset("DeathStopAnims", 1);
   TT_defaultIfUnset("DeathStopFiring", 1);
   TT_defaultIfUnset("RemountDups", 0);
   TT_defaultIfUnset("DisableBulletSlow", 0);
   TT_defaultIfUnset("DisableTier1", 0);
   TT_defaultIfUnset("AlwaysReloadNonEx", 1);
   TT_defaultIfUnset("AlwaysReloadEx", 1);
   TT_defaultIfUnset("Start9MM", 35*4); // 140
   TT_defaultIfUnset("Start556", 15*6); // 90
   TT_defaultIfUnset("Startshotgun", 6*4); // 24
   TT_defaultIfUnset("Start270", 4*4); // 16
   TT_defaultIfUnset("Start880", 6*3); // 18
   TT_defaultIfUnset("Startbomb", 1*3); // 3
   TT_defaultIfUnset("Startrocket", 1*2); // 2
   TT_defaultIfUnset("Start708", 24*3); // 72
   TT_defaultIfUnset("Startbolt", 1*12); // 12
   TT_defaultIfUnset("Max9MM", 280);
   TT_defaultIfUnset("Max556", 180);
   TT_defaultIfUnset("Maxshotgun", 48);
   TT_defaultIfUnset("Max270", 32);
   TT_defaultIfUnset("Max880", 36);
   TT_defaultIfUnset("Maxbomb", 12);
   TT_defaultIfUnset("Maxrocket", 12);
   TT_defaultIfUnset("Max708", 144);
   TT_defaultIfUnset("Maxbolt", 24);
}

exec("./Support_TT_Ammo.cs");
exec("./Support_TT_Weapons.cs");
exec("./Support_TT_Raycasting.cs");

exec("./Item_Ammo.cs");
exec("./Ammo_Unified.cs");

TT_initAmmo();
TT_registerAmmoSet("weps", "All Weapons", false);
TT_registerAmmoSet("nades", "All Grenades", false);

if($Pref::Server::TT::DisableTier1)
{
   warn("WARNING: Weapon_Package_Tier1 - datablocks disabled, set $Pref::Server::TT::DisableTier1 to 0 to re-enable them");
   return; // apparently we can return without a function
}

//////////////////////////////////////////////////////////////////////////////////

if(isFile("Add-Ons/Sound_Blockland/server.cs"))
{
   ForceRequiredAddOn("Sound_Blockland");
}
else
{
   datablock AudioProfile(Block_MoveBrick_Sound)
   {
      filename = "base/data/sound/clickMove.wav";
      description = AudioClosest3d;
      preload = false;
   };
   datablock AudioProfile(Block_PlantBrick_Sound)
   {
      filename = "base/data/sound/clickPlant.wav";
      description = AudioClosest3d;
      preload = false;
   };
   datablock AudioProfile(Block_ChangeBrick_Sound)
   {
      filename = "base/data/sound/clickChange.wav";
      description = AudioClosest3d;
      preload = false;
   };
}

//////////////////////////////////////////////////////////////////////////////////

datablock AudioProfile(ReloadClick1Sound)
{
   filename    = "./reload_1.wav";
   description = AudioClose3d;
   preload = true;
};
// datablock AudioProfile(ReloadClick2Sound)
// {
//    filename    = "./reload_2.wav";
//    description = AudioClose3d;
//    preload = true;
// };
// datablock AudioProfile(ReloadClick3Sound)
// {
//    filename    = "./reload_3.wav";
//    description = AudioClose3d;
//    preload = true;
// };
// datablock AudioProfile(ReloadClick4Sound)
// {
//    filename    = "./reload_4.wav";
//    description = AudioClose3d;
//    preload = true;
// };
datablock AudioProfile(ReloadClick5Sound)
{
   filename    = "./reload_5.wav";
   description = AudioClose3d;
   preload = true;
};
datablock AudioProfile(ReloadClick6Sound)
{
   filename    = "./reload_6.wav";
   description = AudioClose3d;
   preload = true;
};
// datablock AudioProfile(ReloadClick7Sound)
// {
//    filename    = "./reload_7.wav";
//    description = AudioClose3d;
//    preload = true;
// };
datablock AudioProfile(ReloadClick8Sound)
{
   filename    = "./reload_8.wav";
   description = AudioClose3d;
   preload = true;
};
datablock AudioProfile(MagazineOutSound)
{
   filename    = "./magazine_out.wav";
   description = AudioClose3d;
   preload = true;
};

//////////////////////////////////////////////////////////////////////////////////

// note: do not use local variables in server.cs after entering a function (in this case activating a package)

//we need the gun add-on for this, so force it to load
if(ForceRequiredAddOn("Weapon_Gun") == $Error::AddOn_NotFound)
{
   //we don't have the gun, so we're screwed
   error("ERROR: Weapon_Package_Tier1 - required add-on Weapon_Gun not found");
}
else
{
   exec("./Weapon_Submachinegun.cs");
   exec("./Weapon_Pump Shotgun.cs");
   exec("./Weapon_Pistol.cs");
   exec("./Weapon_Akimbo Pistol.cs");
   exec("./Weapon_Sport Rifle.cs");
}

//////////////////////////////////////////////////////////////////////////////////

datablock ExplosionData(TTLittleRecoilExplosion)
{
   explosionShape = "";

   lifeTimeMS = 150;

   faceViewer     = true;
   explosionScale = "1 1 1";

   shakeCamera = true;
  camShakeFreq = "1 1 1";
  camShakeAmp = "0.1 0.3 0.2";
   camShakeDuration = 0.5;
   camShakeRadius = 10.0;
};

datablock ProjectileData(TTLittleRecoilProjectile)
{
	lifetime						= 10;
	fadeDelay						= 10;
	explodeondeath						= true;
	explosion						= TTLittleRecoilExplosion;

};

datablock ExplosionData(TTRecoilExplosion)
{
   explosionShape = "";

   lifeTimeMS = 150;

   faceViewer     = true;
   explosionScale = "1 1 1";

   shakeCamera = true;
  camShakeFreq = "2 2 2";
  camShakeAmp = "0.3 0.5 0.4";
   camShakeDuration = 0.5;
   camShakeRadius = 10.0;
};

datablock ProjectileData(TTRecoilProjectile)
{
	lifetime						= 10;
	fadeDelay						= 10;
	explodeondeath						= true;
	explosion						= TTRecoilExplosion;
};

datablock ExplosionData(TTBigRecoilExplosion)
{
   explosionShape = "";

   lifeTimeMS = 150;

   faceViewer     = true;
   explosionScale = "1 1 1";

   shakeCamera = true;
  camShakeFreq = "3 3 3";
  camShakeAmp = "0.6 0.8 0.7";
   camShakeDuration = 0.5;
   camShakeRadius = 10.0;
};

datablock ProjectileData(TTBigRecoilProjectile)
{
	lifetime						= 10;
	fadeDelay						= 10;
	explodeondeath						= true;
	explosion						= TTBigRecoilExplosion;

};

// datablock ExplosionData(TTHugeRecoilExplosion)
// {
//    explosionShape = "";

//    lifeTimeMS = 150;

//    faceViewer     = true;
//    explosionScale = "1 1 1";

//    shakeCamera = true;
//   camShakeFreq = "5 5 5";
//   camShakeAmp = "1.1 1.3 1.2";
//    camShakeDuration = 0.5;
//    camShakeRadius = 10.0;
// };

// datablock ProjectileData(TTHugeRecoilProjectile)
// {
// 	lifetime						= 10;
// 	fadeDelay						= 10;
// 	explodeondeath						= true;
// 	explosion						= TTHugeRecoilExplosion;

// };

TT_registerAmmoSet("tt", "Tier+Tactical", true);
TT_registerAmmoSet("tt_pile", "Ammo Pile", false);
// ammo types
// 9MM: smg, pistol rounds
// 556: various rifle rounds
// shotgun: combat shotgun, shotgun rounds
// 270: mil. sniper rounds
// 708: battle rifle rounds
// 880: magnum rounds
// bomb: grenade rounds
// rocket: rocket rounds
// bolt: crossbow rounds
TT_registerAmmoType("9MM", "9mm", "9mm", true, "tt", "weps", "tt_pile");
TT_registerAmmoType("556", "5.56", "5.56 Little Rifle", true, "tt", "weps", "tt_pile");
TT_registerAmmoType("shotgun", "Buckshot", "12-gauge shotgun", true, "tt", "weps", "tt_pile");
TT_registerAmmoType("270", ".270", ".270 Huge Rifle", true, "tt", "weps", "tt_pile");
TT_registerAmmoType("880", ".880M", ".880 Magnum", true, "tt", "weps", "tt_pile");
TT_registerAmmoType("bomb", "Grenade", "Explosive Rounds", true, "tt", "weps", "tt_pile");
TT_registerAmmoType("rocket", "Rocket", "Rocket-Propelled Grenades", true, "tt", "weps", "tt_pile");
TT_registerAmmoType("708", "7.08", "7.08 Heavy Rifle", true, "tt", "weps", "tt_pile");
TT_registerAmmoType("bolt", "Bolt", "Standard Steel Bolt", true, "tt", "weps", "tt_pile");
