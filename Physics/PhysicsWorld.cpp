#include "PhysicsWorld.h"


//Returns how many fixedTimeStep substeps were actually simulated (0 to maxSubSteps)
int PhysicsWorld::step(float deltaT)
{
  //Clamp so a hitch/debugger pause can't hand Bullet more time than maxSubSteps can cover in one call - see the comment on maxSubSteps
  float seconds = std::clamp(deltaT / 1000.0f, 0.0f, fixedTimeStep * maxSubSteps);
  return world->stepSimulation(seconds, maxSubSteps, fixedTimeStep);
}

PhysicsWorld::PhysicsWorld()
{
  collisionConfig = new btDefaultCollisionConfiguration();
  dispatcher = new btCollisionDispatcher(collisionConfig);
  broadphase = new btDbvtBroadphase();
  solver = new btSequentialImpulseConstraintSolver;
  btGImpactCollisionAlgorithm::registerAlgorithm(dispatcher);
  world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfig);

  //Added only after it was apparently needed for radiusImpulse, might have preformance impact?
  pairCallback = new btGhostPairCallback();
  world->getBroadphase()->getOverlappingPairCache()->setInternalGhostPairCallback(pairCallback);
  btVector3 gravity = btVector3(0, -70, 0);
  world->setGravity(gravity);

  //Very important to call this, forgetting to do so will massivly increase the performance impact of having static objects like bricks 
  world->setForceUpdateAllAabbs(false);

  planeShape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);
  planeState = new btDefaultMotionState();
  btRigidBody::btRigidBodyConstructionInfo planeCon(0, planeState, planeShape);
  groundPlane = new btRigidBody(planeCon);
  groundPlane->setFriction(1.0);
  groundPlane->setUserIndex(RigidBodyUserIndex::groundPlane);
  world->addRigidBody(groundPlane);
}

PhysicsWorld::~PhysicsWorld()
{
  world->removeRigidBody(groundPlane);//?
  delete groundPlane;
  delete planeShape;
  delete planeState;

  delete world;
  delete solver;
  delete broadphase;
  delete dispatcher;
  delete collisionConfig;
  delete pairCallback;
}

btRigidBody* PhysicsWorld::boxSweepTest(const btVector3& halfExtents, const btTransform& from, const btTransform& to, btRigidBody* ignore)
{
    btClosestNotMeConvexResultCallback callback(ignore, from.getOrigin(), to.getOrigin(), world->getPairCache(), world->getDispatcher());
    btBoxShape test(halfExtents);
    world->convexSweepTest(&test, from, to, callback);
    return (btRigidBody*)callback.m_hitCollisionObject;
}

btRigidBody *PhysicsWorld::doRaycast(const btVector3 &start,const btVector3 &end,btRigidBody *ignore,btVector3 &hitPos,btVector3 &hitNormal) const
{
  btCollisionWorld::AllHitsRayResultCallback ground(start,end);
  world->rayTest(start,end,ground);

  if (ground.m_collisionObjects.size() < 1)
      return nullptr;

  int closestIdx = -1;
  float closestDist = 0;

  for(int a = 0; a<ground.m_collisionObjects.size(); a++)
  {
      if(ground.m_collisionObjects[a] == ignore)
          continue;

      float dist = (ground.m_hitPointWorld[a]-start).length();
      if(dist < closestDist || closestIdx == -1)
      {
          hitPos = ground.m_hitPointWorld[a];
          closestIdx = a;
          closestDist = dist;
          hitNormal = ground.m_hitNormalWorld[a];
      }
  }
  
  if(closestIdx != -1)
      return (btRigidBody*)ground.m_collisionObjects[closestIdx];
  else
      return nullptr;
}

btRigidBody *PhysicsWorld::doRaycast(const btVector3 &start,const btVector3 &end,btRigidBody *ignore) const
{
  btVector3 pos,norm;
  return doRaycast(start,end,ignore,pos,norm);
}
