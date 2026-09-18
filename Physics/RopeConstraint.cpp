#include "RopeConstraint.h"

//Not one of Bullet's own, which only matters to its serializer and debug drawer, neither of which we use
static constexpr int ropeConstraintType = MAX_CONSTRAINT_TYPE + 1;

RopeConstraint::RopeConstraint(btRigidBody& bodyA, btRigidBody& bodyB, const btVector3& _pivotA, const btVector3& _pivotB, btScalar _maxLength)
	: btTypedConstraint((btTypedConstraintType)ropeConstraintType, bodyA, bodyB), pivotA(_pivotA), pivotB(_pivotB), maxLength(_maxLength)
{
}

btScalar RopeConstraint::measure(btVector3& pullA, btVector3& pullB) const
{
	btVector3 endA = m_rbA.getCenterOfMassTransform() * pivotA;
	btVector3 endB = m_rbB.getCenterOfMassTransform() * pivotB;

	//What each end is pulled toward: the nearest bend, or the other end when there are none
	btVector3 towardA = bends.empty() ? endB : bends.front();
	btVector3 towardB = bends.empty() ? endA : bends.back();

	btScalar length = 0;
	for (size_t a = 1; a < bends.size(); a++)
		length += bends[a].distance(bends[a - 1]);

	pullA = towardA - endA;
	btScalar lengthA = pullA.length();
	if (lengthA > SIMD_EPSILON)
		pullA /= lengthA;
	else
		pullA.setZero();
	length += lengthA;

	if (bends.empty())
	{
		pullB = -pullA;
		return length;
	}

	pullB = towardB - endB;
	btScalar lengthB = pullB.length();
	if (lengthB > SIMD_EPSILON)
		pullB /= lengthB;
	else
		pullB.setZero();

	return length + lengthB;
}

void RopeConstraint::getInfo1(btConstraintInfo1* info)
{
	//Always the one row: while the rope is slack it asks for nothing the bodies aren't already doing, see getInfo2
	info->m_numConstraintRows = 1;
	info->nub = 5;
}

void RopeConstraint::getInfo2(btConstraintInfo2* info)
{
	btVector3 pullA, pullB;
	btScalar length = measure(pullA, pullB);

	btVector3 armA = m_rbA.getCenterOfMassTransform().getBasis() * pivotA;
	btVector3 armB = m_rbB.getCenterOfMassTransform().getBasis() * pivotB;
	btVector3 turnA = armA.cross(pullA);
	btVector3 turnB = armB.cross(pullB);

	//The row's velocity is how fast the path is getting shorter: each end's speed along its own pull
	for (int a = 0; a < 3; a++)
	{
		info->m_J1linearAxis[a] = pullA[a];
		info->m_J1angularAxis[a] = turnA[a];
		info->m_J2linearAxis[a] = pullB[a];
		info->m_J2angularAxis[a] = turnB[a];
	}

	/*
		Past its length, it has to shorten by a share of the excess this step, like any other joint's error
		Short of it, it may lengthen by no more than the slack that's left, which is what stops something moving
		fast from running well past the end of the rope in the step it goes taut, and then being yanked back
	*/
	btScalar excess = length - maxLength;
	info->m_constraintError[0] = excess > 0 ? excess * info->fps * info->erp : excess * info->fps;
	info->cfm[0] = 0;

	//A rope pulls, it never pushes
	info->m_lowerLimit[0] = 0;
	info->m_upperLimit[0] = SIMD_INFINITY;
}
