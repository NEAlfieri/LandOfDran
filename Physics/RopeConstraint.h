#pragma once

#include <btBulletDynamicsCommon.h>
#include <vector>

/*
	What a rope does to the two things it's tied between: nothing while it hangs slack, and once it's pulled
	out to its full length it won't let them get any further apart. One solver row that can only ever pull

	The length is measured along a path, from the point on body A, through any bends, to the point on body B.
	Bends are fixed spots in the world the rope runs over, like the edge of a ledge or a pulley, and each end
	is pulled toward the first bend on its side rather than straight at the other end

	An end tied to nothing that moves (a spot in the world, a brick) uses btTypedConstraint::getFixedBody()
	with the spot itself as the pivot, since that body sits at the origin unturned

	Exists in the server's world and in each client's, so whoever simulates a body feels the rope on it
	directly, see SimObjects/Rope.h
*/
class RopeConstraint : public btTypedConstraint
{
	//Where the rope is tied on each body, in that body's own space
	btVector3 pivotA;
	btVector3 pivotB;

	//World space, in order from A's end to B's
	std::vector<btVector3> bends;

	btScalar maxLength;

	public:

	//Set by PhysicsWorld, which takes a rope out of the world along with either of its bodies
	bool inWorld = false;

	RopeConstraint(btRigidBody& bodyA, btRigidBody& bodyB, const btVector3& _pivotA, const btVector3& _pivotB, btScalar _maxLength);

	void setMaxLength(btScalar length) { maxLength = length; }
	btScalar getMaxLength() const { return maxLength; }

	void setPivotA(const btVector3& pivot) { pivotA = pivot; }
	void setPivotB(const btVector3& pivot) { pivotB = pivot; }

	void setBends(const std::vector<btVector3>& _bends) { bends = _bends; }

	//Whether either end is tied to this body
	bool involves(const btRigidBody* body) const { return &m_rbA == body || &m_rbB == body; }

	/*
		How long the path is right now, with the way each end would have to move to shorten it
		An end sitting right on top of what it's pulled toward has no such direction, and gets zero
	*/
	btScalar measure(btVector3& pullA, btVector3& pullB) const;

	virtual void getInfo1(btConstraintInfo1* info) override;
	virtual void getInfo2(btConstraintInfo2* info) override;

	//Nothing about it is tunable through these
	virtual void setParam(int num, btScalar value, int axis = -1) override {}
	virtual btScalar getParam(int num, int axis = -1) const override { return 0; }
};
