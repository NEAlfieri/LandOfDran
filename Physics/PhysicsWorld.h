#pragma once

#include "../LandOfDran.h"

//#define BT_USE_DOUBLE_PRECISION Preferred, but will cause linking errors with currently libraries
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include "SweepTest.h"

//glm::vec3 to btVector3
inline btVector3 g2b3(const glm::vec3 &in)
{
	return btVector3(in.x, in.y, in.z);
}

//btVector3 to glm::vec3
inline glm::vec3 b2g3(const btVector3 &in)
{
	return glm::vec3(in.x(), in.y(), in.z());
}

/*
	Passed to btRigidBody through setUserIndex
	You can getUserIndex to figure out what type of pointer the btRigidBody's getUserData is meant to be
	This is to physics code as SimObjectType is to net code
	REMINDER: btRigidBody user data if not nullptr is always a pointer *to* a smart pointer *to* the underlying SimObject
*/
enum RigidBodyUserIndex
{
	groundPlane = 10,		//The single infinite ground plane at the bottom of the world created on start-up
	dynamicBody = 20,
	staticBody = 30
};

/*
	Holds a btDynamicsWorld but also a few extra things like the ground plane we'll always have
*/
class PhysicsWorld
{
	private:

	btDefaultCollisionConfiguration* collisionConfig = nullptr;
	btCollisionDispatcher* dispatcher = nullptr;
	btBroadphaseInterface* broadphase = nullptr;
	btSequentialImpulseConstraintSolver* solver = nullptr;
	btDiscreteDynamicsWorld* world = nullptr;
	btCollisionShape* planeShape = nullptr;
	btDefaultMotionState* planeState = nullptr;
	btRigidBody* groundPlane = nullptr;
	btGhostPairCallback* pairCallback = nullptr;

	public:

	//Bullet internally simulates in increments of this size (seconds), regardless of how much real time a step() call covers
	//Keeping this fixed (rather than handing Bullet the raw frame delta) is what makes the simulation deterministic between server and client
	static constexpr float fixedTimeStep = 1.0f / 60.0f;

	//Upper bound on how many fixedTimeStep increments a single step() call can run
	//Without this, a hitch/debugger pause producing a huge deltaT would make Bullet take one enormous, unstable step (tunneling risk)
	//Instead the simulation just falls behind real time slightly in that case, which is far safer
	static constexpr int maxSubSteps = 8;

	//Performs a sweep test using the body from its current transform to the supplied, and returns the closest non-from body contacted if any
	btRigidBody* boxSweepTest(const btVector3& halfExtents, const btTransform& from, const btTransform& to, btRigidBody* ignore);

	//Bodies with an actual (penetrating) contact against the given body as of the most recent step() - cheap, since Bullet
	//already computes these each step, just reads the dispatcher's cached manifolds rather than testing anything itself
	std::vector<btRigidBody*> getTouching(const btRigidBody* body) const;

	btRigidBody *doRaycast(const btVector3 &start,const btVector3 &end,btRigidBody *ignore,btVector3 &hitPos,btVector3 &hitNormal) const;
	btRigidBody *doRaycast(const btVector3 &start,const btVector3 &end,btRigidBody *ignore) const;

	void addBody(btRigidBody* body)
	{
		world->addRigidBody(body);
	}

	void removeBody(btRigidBody* body)
	{
		world->removeRigidBody(body);
	}

	//I think it returns how many substeps were used or something? 
	int step(float deltaT);

	PhysicsWorld();

	~PhysicsWorld();
};

