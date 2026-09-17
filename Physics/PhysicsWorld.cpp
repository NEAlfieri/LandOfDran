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

  world->setInternalTickCallback(&PhysicsWorld::substepStarting, this, true);
  world->setInternalTickCallback(&PhysicsWorld::substepFinished, this, false);

  planeShape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);
  planeState = new btDefaultMotionState();
  btRigidBody::btRigidBodyConstructionInfo planeCon(0, planeState, planeShape);
  groundPlane = new btRigidBody(planeCon);
  groundPlane->setFriction(1.0);
  groundPlane->setUserIndex(RigidBodyUserIndex::groundPlane);
  world->addRigidBody(groundPlane);
}

void PhysicsWorld::substepStarting(btDynamicsWorld* world, btScalar timeStep)
{
  PhysicsWorld* self = (PhysicsWorld*)world->getWorldUserInfo();
  if (self && self->beforeSubstep)
    self->beforeSubstep(timeStep);
}

void PhysicsWorld::substepFinished(btDynamicsWorld* world, btScalar timeStep)
{
  PhysicsWorld* self = (PhysicsWorld*)world->getWorldUserInfo();
  if (self && self->afterSubstep)
    self->afterSubstep(timeStep);
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

std::vector<btRigidBody*> PhysicsWorld::getTouching(const btRigidBody* body) const
{
  std::vector<btRigidBody*> touching;

  int numManifolds = dispatcher->getNumManifolds();
  for (int i = 0; i < numManifolds; i++)
  {
    btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);

    const btCollisionObject* other = nullptr;
    if (manifold->getBody0() == body)
      other = manifold->getBody1();
    else if (manifold->getBody1() == body)
      other = manifold->getBody0();
    else
      continue;

    //A manifold can exist for objects that are merely close (within Bullet's collision margin) without actually touching
    bool actuallyTouching = false;
    for (int c = 0; c < manifold->getNumContacts(); c++)
    {
      if (manifold->getContactPoint(c).getDistance() < 0.0f)
      {
        actuallyTouching = true;
        break;
      }
    }

    if (actuallyTouching)
      touching.push_back((btRigidBody*)other);
  }

  return touching;
}

btRigidBody* PhysicsWorld::getFirstContact(const btRigidBody* body, btScalar within, btVector3& point,
	const std::function<bool(const btRigidBody*)>& skip) const
{
  int numManifolds = dispatcher->getNumManifolds();
  for (int i = 0; i < numManifolds; i++)
  {
    btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);

    bool bodyIsFirst = manifold->getBody0() == body;
    if (!bodyIsFirst && manifold->getBody1() != body)
      continue;

    const btCollisionObject* other = bodyIsFirst ? manifold->getBody1() : manifold->getBody0();
    if (!other->hasContactResponse())
      continue;

    if (skip && skip((const btRigidBody*)other))
      continue;

    for (int c = 0; c < manifold->getNumContacts(); c++)
    {
      const btManifoldPoint& contact = manifold->getContactPoint(c);
      if (contact.getDistance() < within)
      {
        point = bodyIsFirst ? contact.getPositionWorldOnB() : contact.getPositionWorldOnA();
        return (btRigidBody*)other;
      }
    }
  }

  return nullptr;
}

btRigidBody* PhysicsWorld::boxSweepTest(const btVector3& halfExtents, const btTransform& from, const btTransform& to, btRigidBody* ignore)
{
    return boxSweep(halfExtents, from, to, ignore).body;
}

SweepResult PhysicsWorld::boxSweep(const btVector3& halfExtents, const btTransform& from, const btTransform& to, btRigidBody* ignore,
    const std::function<bool(const btCollisionObject*)>& skip, int group, int mask)
{
    btClosestNotMeConvexResultCallback callback(ignore, from.getOrigin(), to.getOrigin(), world->getPairCache(), world->getDispatcher());
    //The default mask leaves out purely visual debris, like undone bricks, which shouldn't block movement checks
    callback.m_collisionFilterGroup = group;
    callback.m_collisionFilterMask = mask;
    callback.skip = skip;
    btBoxShape test(halfExtents);
    world->convexSweepTest(&test, from, to, callback);

    SweepResult result;
    if (callback.hasHit())
    {
        result.body = (btRigidBody*)callback.m_hitCollisionObject;
        result.point = callback.m_hitPointWorld;
        result.fraction = callback.m_closestHitFraction;
        result.normal = callback.m_hitNormalWorld;
    }
    return result;
}

//Closest hit that skips two particular bodies
struct IgnoringClosestRayCallback : public btCollisionWorld::ClosestRayResultCallback
{
    const btCollisionObject* ignoreA;
    const btCollisionObject* ignoreB;

    IgnoringClosestRayCallback(const btVector3& from, const btVector3& to, const btCollisionObject* a, const btCollisionObject* b)
        : ClosestRayResultCallback(from, to), ignoreA(a), ignoreB(b)
    {
    }

    bool needsCollision(btBroadphaseProxy* proxy) const override
    {
        const btCollisionObject* object = (const btCollisionObject*)proxy->m_clientObject;
        if (object == ignoreA || object == ignoreB)
            return false;
        return ClosestRayResultCallback::needsCollision(proxy);
    }
};

btScalar PhysicsWorld::rayHitFraction(const btVector3& start, const btVector3& end, const btRigidBody* ignoreA, const btRigidBody* ignoreB) const
{
    IgnoringClosestRayCallback callback(start, end, ignoreA, ignoreB);
    callback.m_collisionFilterMask = btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::DebrisFilter;
    world->rayTest(start, end, callback);
    return callback.hasHit() ? callback.m_closestHitFraction : 1;
}

//Every hit that isn't one of two particular bodies
struct IgnoringAllHitsRayCallback : public btCollisionWorld::AllHitsRayResultCallback
{
    const btCollisionObject* ignoreA;
    const btCollisionObject* ignoreB;

    IgnoringAllHitsRayCallback(const btVector3& from, const btVector3& to, const btCollisionObject* a, const btCollisionObject* b)
        : AllHitsRayResultCallback(from, to), ignoreA(a), ignoreB(b)
    {
        m_collisionFilterMask = btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::DebrisFilter;
    }

    bool needsCollision(btBroadphaseProxy* proxy) const override
    {
        const btCollisionObject* object = (const btCollisionObject*)proxy->m_clientObject;
        if (object == ignoreA || object == ignoreB)
            return false;
        return AllHitsRayResultCallback::needsCollision(proxy);
    }
};

btScalar PhysicsWorld::solidThickness(const btVector3& start, const btVector3& end, const btRigidBody* ignoreA, const btRigidBody* ignoreB) const
{
    btScalar length = (end - start).length();
    if (length <= 0)
        return 0;

    IgnoringAllHitsRayCallback forward(start, end, ignoreA, ignoreB);
    world->rayTest(start, end, forward);
    if (forward.m_collisionObjects.size() == 0)
        return 0;

    //Casting back from the end finds where the forward ray leaves each body
    IgnoringAllHitsRayCallback backward(end, start, ignoreA, ignoreB);
    world->rayTest(end, start, backward);

    //A ray that starts inside something reports it right at its start, so there's no real entry or exit to measure
    const btScalar insideFraction = 0.0001f;

    btScalar total = 0;
    for (int a = 0; a < forward.m_collisionObjects.size(); a++)
    {
        btScalar entry = forward.m_hitFractions[a];
        if (entry < insideFraction)
            continue;

        for (int b = 0; b < backward.m_collisionObjects.size(); b++)
        {
            if (backward.m_collisionObjects[b] != forward.m_collisionObjects[a])
                continue;

            btScalar exit = 1 - backward.m_hitFractions[b];
            if (backward.m_hitFractions[b] >= insideFraction && exit > entry)
                total += (exit - entry) * length;
            break;
        }
    }

    return total;
}

btRigidBody *PhysicsWorld::doRaycast(const btVector3 &start,const btVector3 &end,btRigidBody *ignore,btVector3 &hitPos,btVector3 &hitNormal, const btRigidBody* ignoreAlso) const
{
  btCollisionWorld::AllHitsRayResultCallback ground(start,end);
  //Purely visual debris, like undone bricks, shouldn't block clicks or the camera
  ground.m_collisionFilterMask = btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::DebrisFilter;
  world->rayTest(start,end,ground);

  if (ground.m_collisionObjects.size() < 1)
      return nullptr;

  int closestIdx = -1;
  float closestDist = 0;

  for(int a = 0; a<ground.m_collisionObjects.size(); a++)
  {
      if(ground.m_collisionObjects[a] == ignore || (ignoreAlso && ground.m_collisionObjects[a] == ignoreAlso))
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
