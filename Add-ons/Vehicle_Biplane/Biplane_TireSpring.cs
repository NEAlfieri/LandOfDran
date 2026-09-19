datablock WheeledVehicleSpring(BiplaneSpring)
{
   // Wheel suspension properties
   length = 0.1;			 // Suspension travel
   force = 3000; //3000;		 // Spring force
   damping = 400; //600;		 // Spring damping
   antiSwayForce = 3; //3;		 // Lateral anti-sway force
};

datablock WheeledVehicleSpring(BiplaneBackSpring)
{
   // Wheel suspension properties
   length = 0.1;			 // Suspension travel
   force = 3000; //3000;		 // Spring force
   damping = 400; //600;		 // Spring damping
   antiSwayForce = 3; //3;		 // Lateral anti-sway force
};

datablock WheeledVehicleSpring(BiplaneFauxSpring)
{
	length = 0.1;
	force = 1000;
	damping = 100;
	antiSwayForce = 3;
};

///////////////////////////////////////////////////////////////

datablock WheeledVehicleTire(BiplaneTire)
{
   // Tires act as springs and generate lateral and longitudinal
   // forces to move the vehicle. These distortion/spring forces
   // are what convert wheel angular velocity into forces that
   // act on the rigid body.
   shapeFile = "./biplanefrontwheel.dts";
	
	mass = 10;
    radius = 1;
    staticFriction = 5;
   kineticFriction = 5;
   restitution = 0.5;	

   // Spring that generates lateral tire forces
   lateralForce = 18000;
   lateralDamping = 4000;
   lateralRelaxation = 0.01;

   // Spring that generates longitudinal tire forces
   longitudinalForce = 14000;
   longitudinalDamping = 2000;
   longitudinalRelaxation = 0.01;
};

datablock WheeledVehicleTire(BiplaneBackTire)
{
   // Tires act as springs and generate lateral and longitudinal
   // forces to move the vehicle. These distortion/spring forces
   // are what convert wheel angular velocity into forces that
   // act on the rigid body.
   shapeFile = "./biplanebackwheel.dts";
	
	mass = 10;
    radius = 1;
    staticFriction = 5;
   kineticFriction = 5;
   restitution = 0.5;	

   // Spring that generates lateral tire forces
   lateralForce = 18000;
   lateralDamping = 4000;
   lateralRelaxation = 0.01;

   // Spring that generates longitudinal tire forces
   longitudinalForce = 14000;
   longitudinalDamping = 2000;
   longitudinalRelaxation = 0.01;
};

datablock WheeledVehicleTire(BiplaneFauxTire)
{
	shapeFile = "base/data/shapes/empty.dts";
	mass = 0;
	radius = 0;
	staticFriction = 5;
	kineticFriction = 5;
	restitution = 0;	
	lateralForce = 18000;
	lateralDamping = 4000;
	lateralRelaxation = 0.01;
	longitudinalForce = 14000;
	longitudinalDamping = 2000;
	longitudinalRelaxation = 0.01;
};

package hideBiplaneTires
{
	function BiplaneVehicle::Damage(%this,%obj,%col,%pos,%damage,%type)
	{
		Parent::Damage(%this,%obj,%col,%pos,%damage,%type);

		if(%obj.getDamageState() $= "Disabled")
		{
			%obj.setWheelTire(0,biplaneFauxTire);
			%obj.setWheelSpring(0,biplaneFauxSpring);
			%obj.setWheelTire(1,biplaneFauxTire);
			%obj.setWheelSpring(1,biplaneFauxSpring);
			%obj.setWheelTire(2,biplaneFauxTire);
			%obj.setWheelSpring(2,biplaneFauxSpring);
		}
	}
};

activatePackage(hideBiplaneTires);