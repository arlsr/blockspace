#include "Satellite.h"
#include "Ship.h"
#include <algorithm>

Satellite::Satellite(float a, float e, float inclination, float argPeri, float M0)
{
	semiMajor = a;
	eccentricity = e;
	cosInclination = cos(inclination);
	argPeriapsis = argPeri;
	meanAnomaly = M0;
	// Kepler's third law: mean motion = sqrt(GM / a^3), where GM = surface gravity * R^2 (same constants as the ship's gravity).
	const float surfaceGravity = 0.001463f;
	float gm = surfaceGravity * Ship::PLANET_RADIUS * Ship::PLANET_RADIUS;
	meanMotion = sqrt(gm / (a * a * a));
}

Satellite* Satellite::create(float a, float e, float inclination, float argPeri, float M0)
{
	auto sat = new (std::nothrow) Satellite(a, e, inclination, argPeri, M0);
	if (sat && sat->initWithFile("Satellite.png")) {
		sat->autorelease();
		sat->scheduleUpdate();
		return sat;
	}
	CC_SAFE_DELETE(sat);
	return nullptr;
}

void Satellite::update(float delta)
{
	const float twoPi = 2.0f * (float)M_PI;
	meanAnomaly = fmod(meanAnomaly + meanMotion * delta, twoPi);
	// Solve Kepler's equation M = E - e sin E for the eccentric anomaly E with Newton's method.
	float E = meanAnomaly;
	for (int i = 0; i < 6; ++i) {
		E -= (E - eccentricity * sin(E) - meanAnomaly) / (1.0f - eccentricity * cos(E));
	}
	// Position in the orbit's own plane, with the focus (Earth's centre) at the origin.
	float x = semiMajor * (cos(E) - eccentricity);
	float y = semiMajor * sqrt(1.0f - eccentricity * eccentricity) * sin(E);
	// Rotate by the argument of periapsis, then flatten by the inclination to get the cross-section view.
	float xr = x * cos(argPeriapsis) - y * sin(argPeriapsis);
	float yr = (x * sin(argPeriapsis) + y * cos(argPeriapsis)) * cosInclination;
	this->setPosition(xr, yr);
}

bool Satellite::clearsPlanet() const
{
	// Sample the whole orbit and find the closest the projected path gets to Earth's centre.
	const int samples = 720;
	const float twoPi = 2.0f * (float)M_PI;
	float minDist = 1e9f;
	for (int i = 0; i < samples; ++i) {
		float E = twoPi * i / samples;
		float x = semiMajor * (cos(E) - eccentricity);
		float y = semiMajor * sqrt(1.0f - eccentricity * eccentricity) * sin(E);
		float xr = x * cos(argPeriapsis) - y * sin(argPeriapsis);
		float yr = (x * sin(argPeriapsis) + y * cos(argPeriapsis)) * cosInclination;
		minDist = std::min(minDist, sqrt(xr * xr + yr * yr));
	}
	// Keep a clearance of about half a sprite width (8 px) so the satellite sprite does not touch the planet.
	return minDist > Ship::PLANET_RADIUS + 8.0f;
}
