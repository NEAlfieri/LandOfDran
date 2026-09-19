// support stuff for the stunt plane vehicle
exec("./biplane_TireSpring.cs");
exec("./biplane_Explosion.cs");
exec("./biplane_FinalExplosion.cs");

//we need the jeep add-on for this, so force it to load
%error = ForceRequiredAddOn("Vehicle_Jeep");

if(%error == $Error::AddOn_Disabled)
{
   //A bit of a hack:
   //  we just forced the jeep to load, but the user had it disabled
   //  so lets make it so they can't select it
   JeepVehicle.uiName = "";
}

if(%error == $Error::AddOn_NotFound)
{
   //we don't have the jeep, so we're screwed
   error("ERROR: Vehicle_Biplane - required add-on Vehicle_Jeep not found");
}
else
{
   exec("./Vehicle_Biplane.cs"); 
}