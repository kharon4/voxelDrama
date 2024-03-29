#include "collisionReaction.h"

//#include<iostream>//debugging

inline vec3d calculatePtVel(kineticProperties kp, vec3d pt) {
	return kp.vel + (kp.angularVel * (pt - kp.COM));
}

#define MAX(a,b) ((a>b)?a:b)
#define MIN(a,b) ((a<b)?a:b)

physicalMaterial getCollphysicalMat(physicalMaterial p1 , physicalMaterial p2) {
	physicalMaterial rVal;
	combinationTechnique c;
	bool firstChosen = true;
	if (p1.materialIndex > p2.materialIndex) c = p1.CT;
	else if (p1.materialIndex < p2.materialIndex) { 
		firstChosen = false;
		c = p2.CT; 
	}
	else {
		if (p1.CT == p2.CT) { 
			if (p1.CT == combinationTechnique::otherVal)return defaultPMat;
			c = p1.CT; 
		}
		else if (p1.CT == combinationTechnique::otherVal) {
			return p2;
		}
		else if (p2.CT == combinationTechnique::otherVal) {
			return p1;
		}
		else if (p1.CT == combinationTechnique::thisVal) {
			return p1;
		}
		else if (p2.CT == combinationTechnique::thisVal) {
			return p2;
		}
		else {
			c = combinationTechnique::average;
		}
	}

	switch (c)
	{
	case combinationTechnique::average:
		rVal.coeffLFriction = (p1.coeffLFriction + p2.coeffLFriction) / 2;
		rVal.coeffRFriction = (p1.coeffRFriction + p2.coeffRFriction) / 2;
		rVal.coeffRestitution = (p1.coeffRestitution + p2.coeffRestitution) / 2;
		break;
	case combinationTechnique::greaterVal:
		rVal.coeffLFriction = MAX(p1.coeffLFriction , p2.coeffLFriction);
		rVal.coeffRFriction = MAX(p1.coeffRFriction , p2.coeffRFriction);
		rVal.coeffRestitution = MAX(p1.coeffRestitution , p2.coeffRestitution);
		break;
	case combinationTechnique::lowerVal:
		rVal.coeffLFriction = MIN(p1.coeffLFriction, p2.coeffLFriction);
		rVal.coeffRFriction = MIN(p1.coeffRFriction, p2.coeffRFriction);
		rVal.coeffRestitution = MIN(p1.coeffRestitution, p2.coeffRestitution);
		break;
	case combinationTechnique::thisVal:
		if (firstChosen)return p1;
		else return p2;
		break;
	case combinationTechnique::otherVal:
		if (firstChosen)return p2;
		else return p1;
		break;
	default:
		break;
	}
	return rVal;
}


inline vec3d getNormal(collider* c1, collider* c2, vec3d pt) {//wrt c2
	return vec3d::normalizeRaw_s((c2->getNormal(pt) - c1->getNormal(pt)) / 2);
}

inline vec3d getColParallel(vec3d normal, vec3d v1, vec3d v2) {//wrt c2 , here v is the vel of the points of coll
	return vec3d::normalizeRaw_s((v1-v2) - (vec3d::dot((v1 - v2), normal) * normal)) ;
}

void calculateNewVel(collider* c1, collider* c2, dynamicProperties* d1, dynamicProperties* d2, vec3d collPt, bool STATIC) {
	
	//aquire starting info
	vec3d colNormal = getNormal(c1, c2, collPt);
	//std::cout << "normal = " << colNormal.x << " , " << colNormal.y << " , " << colNormal.z << std::endl;
	vec3d initVels[2];
	initVels[0] = calculatePtVel(d1->kP, collPt);
	initVels[1] = calculatePtVel(d2->kP, collPt);
	vec3d colParallel = getColParallel(colNormal, initVels[0], initVels[1]);
	//std::cout << "parallel = " << colParallel.x << " , " << colParallel.y << " , " << colParallel.z << std::endl;
	vec3d r[2] = { collPt - d1->kP.COM , collPt - d2->kP.COM };

	//coll parameters
	physicalMaterial p = getCollphysicalMat(d1->pMat, d2->pMat);
	double k0 = 0;
	if(!STATIC) k0 = (1 / d1->mass) + vec3d::dot(vec3d::cross((vec3d::cross(r[0], colNormal) / d1->TOI), r[0]), colNormal);
	double k1 = (1 / d2->mass) + vec3d::dot(vec3d::cross((vec3d::cross(r[1], colNormal) / d2->TOI), r[1]), colNormal);
	k1 = -k1;//adjusting for -ve force dir
	//std::cout << "k1 = " << k0 << std::endl;
	//std::cout << "k2 = " << k1 << std::endl;
	double nImpulseMag = vec3d::dot((initVels[1] - initVels[0]), colNormal)*(-p.coeffRestitution-1)/ (k1 - k0);
	//std::cout << "nImpulse = " << nImpulseMag << std::endl;
	double pImpusleMag = abs(nImpulseMag * p.coeffLFriction);
	if (STATIC) {
		if (pImpusleMag > abs(vec3d::dot((initVels[1] - initVels[0]), colParallel))* (d1->mass))
			pImpusleMag = abs(vec3d::dot((initVels[1] - initVels[0]), colParallel)) * (d1->mass);
	}
	//change for static
	else if (pImpusleMag > abs(vec3d::dot((initVels[1] - initVels[0]), colParallel)) * (d1->mass*d2->mass/(d1->mass+d2->mass)))
		pImpusleMag = abs(vec3d::dot((initVels[1] - initVels[0]), colParallel)) * (d1->mass * d2->mass / (d1->mass + d2->mass));
	
	//implement normal Angular Impulse
	double nAngularImpulseMag = nImpulseMag * p.coeffRFriction;
	//
	double wDiff = abs(vec3d::dot(d1->kP.angularVel- d2->kP.angularVel, colNormal));
	if (vec3d::dot(nAngularImpulseMag * (colNormal / d1->TOI + colNormal / d2->TOI), colNormal) >= wDiff) {
		d2->kP.angularVel += (vec3d::dot(d1->kP.angularVel- d2->kP.angularVel, colNormal)) *colNormal;
		nAngularImpulseMag = 0;
	}

	if (vec3d::dot(d1->kP.angularVel - d2->kP.angularVel, colNormal) > 0)nAngularImpulseMag *= -1;
	vec3d netImpulse = nImpulseMag * colNormal - pImpusleMag * colParallel;//force on body 1
	if (!STATIC) {
		d1->kP.vel += netImpulse / d1->mass;
		d1->kP.angularVel += vec3d::cross(r[0], netImpulse) / d1->TOI;
		d1->kP.angularVel += nAngularImpulseMag*colNormal / d1->TOI;
	}
	d2->kP.vel -= netImpulse / d2->mass;
	d2->kP.angularVel += vec3d::cross(r[1], -netImpulse) / d2->TOI;
	d2->kP.angularVel -= nAngularImpulseMag * colNormal / d2->TOI;
}