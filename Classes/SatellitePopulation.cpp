#include "Satellite.h"
#include "Ship.h"
#include <cstdlib>

namespace {

const float PI = (float)M_PI;

// Converts kilometres to pixels at 1 px = 6,706 m.
float kmToPixels(float km) { return km * 1000.0f / 6706.0f; }

// Random number from 0 to 1.
float randomUnit() { return (rand() % 10000) / 10000.0f; }

// Random angle from 0 to 2 pi.
float randomAngle() { return randomUnit() * 2.0f * PI; }

// Orbit radius from Earth's centre for an altitude in km: Earth's radius (950 px) plus altitude.
float radiusAt(float altitudeKm) { return Ship::PLANET_RADIUS + kmToPixels(altitudeKm); }

// Broadband shell (Starlink-like): circular, 550 km, 53 degrees.
Satellite* createShellSatellite()
{
	return Satellite::create(radiusAt(550.0f), 0.0f, 53.0f * PI / 180.0f, randomAngle(), randomAngle());
}

// Other low Earth orbits: circular, 400 to 1,200 km, 40 to 98 degrees.
Satellite* createLowOrbitSatellite()
{
	float altitude = 400.0f + randomUnit() * 800.0f;
	float incl = (40.0f + randomUnit() * 58.0f) * PI / 180.0f;
	return Satellite::create(radiusAt(altitude), 0.0f, incl, randomAngle(), randomAngle());
}

// Geostationary: circular, 35,786 km, equatorial.
Satellite* createGeostationarySatellite()
{
	return Satellite::create(radiusAt(35786.0f), 0.0f, 0.0f, randomAngle(), randomAngle());
}

// Medium Earth orbit (GPS-like): circular, 20,200 km, 55 degrees.
Satellite* createMediumOrbitSatellite()
{
	return Satellite::create(radiusAt(20200.0f), 0.0f, 55.0f * PI / 180.0f, randomAngle(), randomAngle());
}

// Highly elliptical (Molniya-like): perigee 500 km, apogee 39,700 km, 63.4 degrees, argument of perigee 270 degrees.
Satellite* createEllipticalSatellite()
{
	float rp = radiusAt(500.0f);
	float ra = radiusAt(39700.0f);
	float a = (rp + ra) / 2.0f;
	float e = (ra - rp) / (ra + rp);
	return Satellite::create(a, e, 63.4f * PI / 180.0f, 270.0f * PI / 180.0f, randomAngle());
}

}  // namespace

void Satellite::populate(cocos2d::Node* planet, int count)
{
	// Shares are approximate proportions of the real active satellite population.
	for (int n = 0; n < count; ++n) {
		float roll = randomUnit();
		Satellite* sat;
		if (roll < 0.62f) sat = createShellSatellite();
		else if (roll < 0.92f) sat = createLowOrbitSatellite();
		else if (roll < 0.98f) sat = createGeostationarySatellite();
		else if (roll < 0.99f) sat = createMediumOrbitSatellite();
		else sat = createEllipticalSatellite();
		// Only orbits that stay clear of Earth in this flat view are shown.
		if (sat->clearsPlanet()) planet->addChild(sat);
	}
}
