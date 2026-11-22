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

// The Moon: circular, 384,400 km, 5.14 degrees to the ecliptic. It is drawn as a disc of 1,737 km radius with near-side maria, craters and limb darkening, with the sprite hidden.
Satellite* createMoon()
{
	Satellite* moon = Satellite::create(kmToPixels(384400.0f), 0.0f, 5.14f * PI / 180.0f, 0.0f, 0.0f);
	if (!moon) {
		return nullptr;
	}
	moon->setOpacity(0);
	auto disc = cocos2d::DrawNode::create();
	const float R = kmToPixels(1737.4f);
	// Positions below are fractions of the radius, with x to the right and y up.
	auto P = [R](float x, float y) { return cocos2d::Vec2(x * R, y * R); };
	const int seg = 180;
	// Highlands: light grey base, darker towards the limb so the disc reads as a sphere.
	disc->drawSolidCircle(cocos2d::Vec2::ZERO, R, 0.0f, seg, cocos2d::Color4F(0.50f, 0.49f, 0.47f, 1.0f));
	disc->drawSolidCircle(cocos2d::Vec2::ZERO, R * 0.97f, 0.0f, seg, cocos2d::Color4F(0.60f, 0.59f, 0.57f, 1.0f));
	disc->drawSolidCircle(cocos2d::Vec2::ZERO, R * 0.85f, 0.0f, seg, cocos2d::Color4F(0.70f, 0.69f, 0.67f, 1.0f));
	// Maria: dark basalt plains on the near side.
	const cocos2d::Color4F mare(0.36f, 0.36f, 0.36f, 1.0f);
	disc->drawSolidCircle(P(-0.50f, 0.20f), R * 0.38f, 0.0f, seg, mare); // Oceanus Procellarum
	disc->drawSolidCircle(P(-0.38f, 0.55f), R * 0.20f, 0.0f, seg, mare); // Mare Imbrium
	disc->drawSolidCircle(P(0.12f, 0.45f), R * 0.14f, 0.0f, seg, mare); // Mare Serenitatis
	disc->drawSolidCircle(P(0.30f, 0.15f), R * 0.16f, 0.0f, seg, mare); // Mare Tranquillitatis
	disc->drawSolidCircle(P(0.60f, 0.40f), R * 0.10f, 0.0f, seg, mare); // Mare Crisium
	disc->drawSolidCircle(P(0.55f, -0.10f), R * 0.14f, 0.0f, seg, mare); // Mare Fecunditatis
	disc->drawSolidCircle(P(0.30f, -0.25f), R * 0.08f, 0.0f, seg, mare); // Mare Nectaris
	disc->drawSolidCircle(P(0.00f, -0.45f), R * 0.16f, 0.0f, seg, mare); // Mare Nubium
	disc->drawSolidCircle(P(-0.45f, -0.25f), R * 0.10f, 0.0f, seg, mare); // Mare Humorum
	disc->drawSolidCircle(P(-0.10f, 0.85f), R * 0.08f, 0.0f, seg, mare); // Mare Frigoris, near the top limb
	// Craters: a bright central peak on a dark floor, and Tycho as a plain crater.
	disc->drawSolidCircle(P(-0.18f, 0.12f), R * 0.05f, 0.0f, 60, cocos2d::Color4F(0.30f, 0.30f, 0.30f, 1.0f)); // Copernicus floor
	disc->drawSolidCircle(P(-0.18f, 0.12f), R * 0.015f, 0.0f, 24, cocos2d::Color4F(0.85f, 0.84f, 0.82f, 1.0f));
	const cocos2d::Vec2 tycho = P(0.02f, -0.72f);
	disc->drawSolidCircle(tycho, R * 0.04f, 0.0f, 60, cocos2d::Color4F(0.45f, 0.45f, 0.44f, 1.0f));
	moon->addChild(disc);
	return moon;
}

}  // namespace

void Satellite::populate(cocos2d::Node* planet, int count)
{
	// The Moon is always added, on its own orbit.
	if (Satellite* moon = createMoon()) {
		planet->addChild(moon);
	}
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
