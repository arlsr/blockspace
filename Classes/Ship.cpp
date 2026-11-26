#include "Ship.h"

/*
So far this is acting as the player's ship. It should be a base class for any ship or at least rename this one PlayerShip.

*/


Ship::Ship(cocos2d::Label *lblCoords, cocos2d::Node *farStars, cocos2d::Node *midStars, cocos2d::Node *nearStars, cocos2d::Node *planet)
{
	rotatingLeft = rotatingRight = propellingForward = false;
	worldX = worldY = 0;
	speed = 0;
	velX = velY = 0;

	this->lblCoords = lblCoords;
	this->starLayers[0] = farStars;
	this->starLayers[1] = midStars;
	this->starLayers[2] = nearStars;
	this->planet = planet;
	this->initWithFile("Ship.png");
	this->setAnchorPoint(cocos2d::Vec2(0.5f, 0.5f));
	retroFlame = cocos2d::DrawNode::create();
	// Ring centred on the hull's drawn centre, which sits up and right of the node origin.
	retroFlame->setPosition(cocos2d::Vec2(4.42f, 4.13f));
	this->addChild(retroFlame, -1);
	this->scheduleUpdate();

	this->lblCoords->setString("init");

	auto listener = cocos2d::EventListenerKeyboard::create();
	listener->onKeyPressed = CC_CALLBACK_2(Ship::onKeyPressed, this);
	listener->onKeyReleased = CC_CALLBACK_2(Ship::onKeyReleased, this);

	_eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);
}


Ship::~Ship()
{
}

// Implementation of the keyboard event callback function prototype
void Ship::onKeyPressed(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event)
{
	if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_LEFT_ARROW)
		this->rotatingLeft = true;
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_RIGHT_ARROW)
		this->rotatingRight = true;
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_UP_ARROW)
		this->propellingForward = true;
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_DOWN_ARROW)
		this->retroBraking = true;
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_SPACE) {
		// Rocket launch: replace all momentum and drift with a huge speed straight out of the nose.
		const float rotationRadians = this->getRotation() * (M_PI / 180);
		const float launchSpeed = 100.0f; // px/s, about 670 km/s at the game's scale
		const float nx = sin(rotationRadians), ny = cos(rotationRadians);
		// Sideways momentum is cancelled. Speed already along the nose is kept, and the boost adds to it.
		const float along = velX * nx + velY * ny;
		// The boost lifts the nose speed to 100 px/s but never adds past it. Faster ships keep their speed.
		const float newAlong = along > launchSpeed ? along : launchSpeed;
		velX = nx * newAlong;
		velY = ny * newAlong;
		launchClock = 0.0f;
	}
}

void Ship::onKeyReleased(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event)
{
	if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_LEFT_ARROW)
		this->rotatingLeft = false;
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_RIGHT_ARROW)
		this->rotatingRight = false;
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_UP_ARROW)
		this->propellingForward = false;
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_DOWN_ARROW)
		this->retroBraking = false;
}

// Acceleration grows as the view zooms out (1/zoom), so distant travel is quick. Zooming in keeps the normal rate.
static float accelMultiplier(const cocos2d::Node* world)
{
	float zoom = world ? world->getScaleX() : 1.0f;
	float mul = 1.0f / zoom;
	if (mul < 1.0f) mul = 1.0f;
	if (mul > 10000.0f) mul = 10000.0f;
	return mul;
}

void Ship::stepForward(float delta)
{
	float rotationRadians = this->getRotation() * (M_PI / 180);

	// Thrust accelerates the ship along its heading. The velocity is kept when thrust stops.
	// Ignition gives ten times the normal thrust, fading back to 10 px/s² over roughly a second.
	const float thrust = accelMultiplier(this->getParent()) * 10.0f * (1.0f + 9.0f * expf(-boostTime / 0.4f));
	// Two rings per ignition, spawned at 0 s and 0.5 s, at the ship's world position.
	if (shockSpawned < 2 && boostTime >= shockSpawned * 0.5f) {
		// Thruster sits behind the hull centre (about 6 px back along the hull's own axis), so offset from the ship's origin and rotate with the ship.
		const float tlx = 0.0f, tly = -6.0f;
		const float tailX = tlx * cos(rotationRadians) + tly * sin(rotationRadians);
		const float tailY = -tlx * sin(rotationRadians) + tly * cos(rotationRadians);
		shockX[shockNext] = worldX + tailX;
		shockY[shockNext] = worldY + tailY;
		shockBorn[shockNext] = shockClock;
		shockNext = (shockNext + 1) % 6;
		shockSpawned++;
	}
	boostTime += delta;
	// Sin and cos are swapped because cocos2d uses a different corner for 0,0 than other libraries.
	velX += sin(rotationRadians) * thrust * delta;
	velY += cos(rotationRadians) * thrust * delta;
}


void Ship::brakeRetro(float delta)
{
	// Retro braking removes a share of the speed each second: fast at high speed, gentler below 30 px/s.
	const float thrust = 10.0f;
	float sp = sqrt(velX * velX + velY * velY);
	// Below 2 px/s the ship stops at once rather than decaying.
	if (sp < 2.0f) {
		velX = velY = 0;
		retroFiring = true;
		return;
	}
	// Never apply more change than the current speed, so the ship stops at zero and does not reverse.
	// Above 30 px/s braking is a share of speed; below 30 px/s it is a constant 20 px/s per second.
	float dv = sp >= 30.0f ? 0.8f * sp * delta : 20.0f * accelMultiplier(this->getParent()) * delta;
	if (dv > sp) {
		dv = sp;
	}
	velX -= velX / sp * dv;
	velY -= velY / sp * dv;
	if (dv >= sp) {
		velX = velY = 0;
	}
	retroFiring = true;
}

void Ship::updateRetroFlame()
{
	retroFlame->clear();

	// White-hot launch jet: three stacked tapered flames from the nozzle, fading and flickering over about 1.2 s.
	if (launchClock >= 0.0f) {
		launchClock += cocos2d::Director::getInstance()->getDeltaTime();
		const float t = launchClock;
		const float fade = fmaxf(0.0f, 1.0f - t / 0.5f);
		if (fade <= 0.0f) {
			launchClock = -1.0f;
		} else {
			// Nozzle sits 6 px behind the hull centre, which is retroFlame's origin.
			const float nx = 0.0f, ny = -6.0f;
			const float flicker = 0.9f + 0.1f * sinf(t * 60.0f) + 0.05f * sinf(t * 127.0f);
			struct Layer { float w; float len; cocos2d::Color4F col; };
			const Layer layers[3] = {
				{3.5f, 60.0f, cocos2d::Color4F(0.55f, 0.75f, 1.0f, 0.45f * fade)},
				{2.4f, 48.0f, cocos2d::Color4F(0.85f, 0.93f, 1.0f, 0.8f * fade)},
				{1.2f, 34.0f, cocos2d::Color4F(1.0f, 1.0f, 1.0f, fade)},
			};
			for (const Layer &L : layers) {
				const float len = L.len * flicker * (0.6f + 0.4f * fade);
				const cocos2d::Vec2 poly[3] = {
					cocos2d::Vec2(nx - L.w, ny),
					cocos2d::Vec2(nx + L.w, ny),
					cocos2d::Vec2(nx, ny - len),
				};
				retroFlame->drawSolidPoly(poly, 3, L.col);
			}
		}
	}
	// Shock rings are fixed in world space. They are drawn on a child of the planet, in planet-local coordinates.
	if (!shockFx) {
		shockFx = cocos2d::DrawNode::create();
		planet->addChild(shockFx, 1);
	}
	shockFx->clear();
	const int disc = 16;
	const float tau = (float)M_PI * 2.0f;
	for (int n = 0; n < 6; ++n) {
		float age = shockClock - shockBorn[n];
		if (age < 0.0f || age > 1.2f) {
			continue;
		}
		float p = age / 1.2f;
		float r = 3.0f + p * 12.0f;
		float w = 0.8f;
		cocos2d::Vec2 c(shockX[n] - earthX, shockY[n] - earthY);
		cocos2d::Color4F ringCol(1.0f, 0.6f, 0.25f, 0.7f * (1.0f - p));
		for (int i = 0; i < disc; ++i) {
			float a0 = (float)i / disc * tau, a1 = (float)(i + 1) / disc * tau;
			cocos2d::Vec2 d0(cos(a0), sin(a0)), d1(cos(a1), sin(a1));
			shockFx->drawTriangle(c + d0 * (r - w), c + d0 * (r + w), c + d1 * (r + w), ringCol);
			shockFx->drawTriangle(c + d0 * (r - w), c + d1 * (r + w), c + d1 * (r - w), ringCol);
		}
	}
	bool firing = retroFiring;
	retroFiring = false;
	if (!firing) {
		return;
	}
	// The ring expands outward from the hull and fades, then repeats.
	const int segments = 72;
	const float baseRadius = 20.0f;
	const float maxGrow = 14.0f;
	const float period = 40.0f;
	float p = fmod(cocos2d::Director::getInstance()->getTotalFrames() / period, 1.0f);
	float r = baseRadius + p * maxGrow;
	cocos2d::Color4F col(0.45f, 0.55f, 0.65f, 0.6f * (1.0f - p));
	// Arc centred on the direction of travel, in the ship's own frame. Rotation is clockwise, so add it back to the world angle.
	float travel = atan2f(velY, velX) + getRotation() * (float)M_PI / 180.0f;
	if (velX == 0 && velY == 0) {
		travel = (float)M_PI / 2.0f;
	}
	const float arcHalf = 1.05f; // 60 degrees either side, so the arc spans 120 degrees.
	cocos2d::Vec2 prevOuter, prevInner;
	for (int i = 0; i <= segments; ++i) {
		float t = travel - arcHalf + (float)i / segments * 2.0f * arcHalf;
		cocos2d::Vec2 dir(cos(t), sin(t));
		cocos2d::Vec2 outer = dir * (r + 1.5f);
		cocos2d::Vec2 inner = dir * (r - 1.5f);
		if (i > 0) {
			retroFlame->drawTriangle(prevOuter, prevInner, outer, col);
			retroFlame->drawTriangle(prevInner, inner, outer, col);
		}
		prevOuter = outer;
		prevInner = inner;
	}
}

void Ship::update(float delta)
{
	shockClock += delta;
	if (rotatingLeft) {
		this->setRotation(this->getRotation() - (100 * delta));
	}
	if (rotatingRight) {
		this->setRotation(this->getRotation() + (100 * delta));
	}
	// Textures are cached once and swapped only when the thrust state changes.
	if (!texShip) {
		texShip = cocos2d::Director::getInstance()->getTextureCache()->addImage("Ship.png");
		texShipFwd = cocos2d::Director::getInstance()->getTextureCache()->addImage("Ship_fwd.png");
	}
	if (propellingForward != thrustShown) {
		thrustShown = propellingForward;
		this->setTexture(thrustShown ? texShipFwd : texShip);
		this->setTextureRect(cocos2d::Rect(cocos2d::Vec2::ZERO, this->getTexture()->getContentSize()));
	}
	if (propellingForward) {
		stepForward(delta);
	} else {
		boostTime = 0.0f;
		shockSpawned = 0;
	}
	if (retroBraking) {
		brakeRetro(delta);
	}

	// Gravity from the planet follows the inverse-square law. Surface gravity is 9.81 m/s^2, which is 0.001463 px/s^2 at this scale (1 px = 6,706 m).
	const float surfaceGravity = 0.001463f;
	float dx = earthX - worldX;
	float dy = earthY - worldY;
	float dist = sqrt(dx * dx + dy * dy);
	float gravity = surfaceGravity * (float)PLANET_RADIUS * PLANET_RADIUS / (dist * dist);
	velX += gravity * dx / dist * delta;
	velY += gravity * dy / dist * delta;

	// Momentum: the ship keeps moving at its stored velocity when thrust is off.
	worldX += velX * delta;
	worldY += velY * delta;
	// The planet is fixed in world space, so its drawn position moves opposite to the ship.
	cocos2d::Vec2 shipHome = this->getPosition();
	// Earth orbits the fixed Sun under inverse-square gravity. The Sun is 1 AU (about 22,308,380 px) to the left, and Earth moves at about 4.44 px/s (about 30 km/s).
	const float sunX = -22308380.0f;
	const float sunY = -1000.0f;
	const float gmSun = 4.4003e8f;
	float sdx = sunX - earthX;
	float sdy = sunY - earthY;
	float sdist = sqrt(sdx * sdx + sdy * sdy);
	float sunPull = gmSun / (sdist * sdist);
	earthVX += sunPull * sdx / sdist * delta;
	earthVY += sunPull * sdy / sdist * delta;
	earthX += earthVX * delta;
	earthY += earthVY * delta;
	this->planet->setPosition(shipHome.x + earthX - worldX, shipHome.y + earthY - worldY);
	// The Sun is fixed in world space, drawn once on the world node. Its radius is 696,000 km (about 103,787 px).
	if (!sunFx) {
		const float sunRadius = 103787.0f;
		const cocos2d::Vec2 sunCentre(0.0f, 0.0f);
		sunFx = cocos2d::DrawNode::create();
		// Faint outer halo first, then the disc on top.
		sunFx->drawSolidCircle(sunCentre, sunRadius * 1.25f, 0.0f, 720, cocos2d::Color4F(1.0f, 0.85f, 0.4f, 0.05f));
		sunFx->drawSolidCircle(sunCentre, sunRadius * 1.08f, 0.0f, 720, cocos2d::Color4F(1.0f, 0.9f, 0.5f, 0.12f));
		sunFx->drawSolidCircle(sunCentre, sunRadius, 0.0f, 720, cocos2d::Color4F(1.0f, 0.93f, 0.6f, 1.0f));
		this->planet->getParent()->addChild(sunFx, -1);
	}
	sunFx->setPosition(shipHome.x + sunX - worldX, shipHome.y + sunY - worldY);
	float tile = (float)STAR_TILE_SIZE;
	// Each layer scrolls at a fraction of the ship's movement, so nearer stars appear to move faster.
	const float parallax[3] = { 0.25f, 0.5f, 1.0f };
	for (int i = 0; i < 3; ++i) {
		float wrapX = fmod(fmod(worldX * parallax[i], tile) + tile, tile);
		float wrapY = fmod(fmod(worldY * parallax[i], tile) + tile, tile);
		this->starLayers[i]->setPosition(-wrapX, -wrapY);
	}

	speed = sqrt(velX * velX + velY * velY);
	updateRetroFlame();
	// Readouts are updated individually below.
	if (lblX) lblX->setString(std::to_string((int)worldX));
	if (lblY) lblY->setString(std::to_string((int)worldY));
	if (lblSpeed) lblSpeed->setString(std::to_string((int)speed));
	(void)lblCoords;
}
