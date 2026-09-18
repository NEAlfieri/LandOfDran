#pragma once

#include "../LandOfDran.h"
#include <functional>

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
	REMINDER: btRigidBody user data if not nullptr is a pointer *to* a smart pointer *to* the underlying SimObject,
	except for brickBody, where it's a plain Brick*
*/
enum RigidBodyUserIndex
{
	groundPlane = 10,		//The single infinite ground plane at the bottom of the world created on start-up
	dynamicBody = 20,
	staticBody = 30,
	brickBody = 40,
	vehicleBody = 50		//A Vehicle's bricks, see SimObjects/Vehicle.h
};

//Our own btBroadphaseProxy::CollisionFilterGroups bit, past the ones Bullet names: projectiles don't collide with one another
static constexpr int ProjectileFilter = 64;

//What a box sweep hit, body is nullptr if nothing was
struct SweepResult
{
	btRigidBody* body = nullptr;

	//Where on the body it was touched
	btVector3 point = btVector3(0, 0, 0);

	//0 at the start of the sweep, 1 at the end
	btScalar fraction = 1;

	//Surface normal of whatever was hit, pointing back toward the sweep
	btVector3 normal = btVector3(0, 1, 0);
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

	//Bullet's internal tick callbacks, which hand each substep to beforeSubstep and afterSubstep
	static void substepStarting(btDynamicsWorld* world, btScalar timeStep);
	static void substepFinished(btDynamicsWorld* world, btScalar timeStep);

	public:

	/*
		Called after every fixedTimeStep substep a step() runs, still inside the step. For anything that
		has to be seen the moment it happens rather than once the whole step is over: a projectile's
		sweep stops it on a wall in one substep and the bounce carries it away in the next, so a check
		after the step alone misses the touch whenever a frame covers more than one substep. Don't add
		or remove bodies from in here
	*/
	std::function<void(btScalar timeStep)> afterSubstep;

	//Called before every substep, still inside the step, with how long it's about to cover: for sweeping
	//projectiles along what they're about to do. Bodies can be moved from in here, not added or removed
	std::function<void(btScalar timeStep)> beforeSubstep;

	//Bullet internally simulates in increments of this size (seconds), regardless of how much real time a step() call covers
	//Keeping this fixed (rather than handing Bullet the raw frame delta) is what makes the simulation deterministic between server and client
	static constexpr float fixedTimeStep = 1.0f / 60.0f;

	//Upper bound on how many fixedTimeStep increments a single step() call can run
	//Without this, a hitch/debugger pause producing a huge deltaT would make Bullet take one enormous, unstable step (tunneling risk)
	//Instead the simulation just falls behind real time slightly in that case, which is far safer
	static constexpr int maxSubSteps = 8;

	//Performs a sweep test using the body from its current transform to the supplied, and returns the closest non-from body contacted if any
	btRigidBody* boxSweepTest(const btVector3& halfExtents, const btTransform& from, const btTransform& to, btRigidBody* ignore);

	//Same sweep as boxSweepTest, but with how far along the sweep the hit was and its surface normal
	//Ignores non-colliding bodies and anything the box is moving away from. skip, if given, says which other bodies to pass
	//through too, and group and mask are the sweep's own collision filter, see addBody
	SweepResult boxSweep(const btVector3& halfExtents, const btTransform& from, const btTransform& to, btRigidBody* ignore,
		const std::function<bool(const btCollisionObject*)>& skip = nullptr,
		int group = btBroadphaseProxy::DefaultFilter, int mask = btBroadphaseProxy::AllFilter ^ btBroadphaseProxy::DebrisFilter);

	//Bodies with an actual (penetrating) contact against the given body as of the most recent step() - cheap, since Bullet
	//already computes these each step, just reads the dispatcher's cached manifolds rather than testing anything itself
	std::vector<btRigidBody*> getTouching(const btRigidBody* body) const;

	//The first body that collides and was within `within` of the given body as of the most recent step(), with where on that body
	//and the normal of its surface there pointing out of it, or nullptr
	//Reads the same cached manifolds as getTouching
	//skip, if given, says which other bodies don't count as something to touch, like one projectile to another
	btRigidBody* getFirstContact(const btRigidBody* body, btScalar within, btVector3& point, btVector3& normal,
		const std::function<bool(const btRigidBody*)>& skip = nullptr) const;

	//ignoreAlso skips a second body, like the vehicle a player is driving
	btRigidBody *doRaycast(const btVector3 &start,const btVector3 &end,btRigidBody *ignore,btVector3 &hitPos,btVector3 &hitNormal, const btRigidBody* ignoreAlso = nullptr) const;
	btRigidBody *doRaycast(const btVector3 &start,const btVector3 &end,btRigidBody *ignore) const;

	//For things that do their own work each physics step, like a vehicle's wheels
	void addAction(btActionInterface* action)
	{
		world->addAction(action);
	}

	void removeAction(btActionInterface* action)
	{
		world->removeAction(action);
	}

	btDynamicsWorld* getDynamicsWorld() const
	{
		return world;
	}

	//How far from start to end (0-1) the nearest hit is, skipping up to two bodies and debris, 1 if nothing's in the way
	//Cheaper than doRaycast, which collects every hit along the ray
	btScalar rayHitFraction(const btVector3& start, const btVector3& end, const btRigidBody* ignoreA, const btRigidBody* ignoreB) const;

	//Total length of solid the segment from start to end passes through, skipping up to two bodies and debris
	//Only bodies it enters and leaves again count, so something start or end is inside of (like the brick a sound comes from) doesn't
	btScalar solidThickness(const btVector3& start, const btVector3& end, const btRigidBody* ignoreA, const btRigidBody* ignoreB) const;

	void addBody(btRigidBody* body)
	{
		world->addRigidBody(body);
	}

	//group and mask are btBroadphaseProxy::CollisionFilterGroups, bodies collide when each one's group is in the other's mask
	void addBody(btRigidBody* body, int group, int mask)
	{
		world->addRigidBody(body, group, mask);
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

