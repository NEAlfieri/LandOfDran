datablock WheeledVehicleTire(stuntplaneTire)
{
   // Tires act as springs and generate lateral and longitudinal
   // forces to move the vehicle. These distortion/spring forces
   // are what convert wheel angular velocity into forces that
   // act on the rigid body.
   shapeFile = "./stuntplanetire.dts";
	
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

datablock WheeledVehicleSpring(stuntplaneSpring)
{
   // Wheel suspension properties
   length = 0.2;			 // Suspension travel
   force = 3000; //3000;		 // Spring force
   damping = 800; //600;		 // Spring damping
   antiSwayForce = 6; //3;		 // Lateral anti-sway force
};