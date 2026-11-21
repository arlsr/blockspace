#pragma once
#include "cocos2d.h"

// A satellite on a Keplerian orbit, shown on the flat cross-section of Earth.
class Satellite :
	public cocos2d::Sprite
{
private:
	float semiMajor;
	float eccentricity;
	float cosInclination;
	float argPeriapsis;
	float meanAnomaly;
	float meanMotion;
	Satellite(float a, float e, float inclination, float argPeri, float M0);
public:
	static Satellite* create(float a, float e, float inclination, float argPeri, float M0);
	void update(float delta) override;
	// True if the projected orbit never comes within the planet's disk, so it does not overlap Earth in this view.
	bool clearsPlanet() const;
	// Adds a mix of orbit types to the planet node, in roughly real proportions.
	static void populate(cocos2d::Node* planet, int count);
};
