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
}

void Ship::onKeyReleased(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event* event)
{
	if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_LEFT_ARROW)
		this->rotatingLeft = false;
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_RIGHT_ARROW)
		this->rotatingRight = false;
	else if (keyCode == cocos2d::EventKeyboard::KeyCode::KEY_UP_ARROW)
		this->propellingForward = false;
}

void Ship::stepForward(float delta)
{
	float rotationRadians = this->getRotation() * (M_PI / 180);

	// Thrust accelerates the ship along its heading. The velocity is kept when thrust stops.
	const float thrust = 10.0f;
	// Sin and cos are swapped because cocos2d uses a different corner for 0,0 than other libraries.
	velX += sin(rotationRadians) * thrust * delta;
	velY += cos(rotationRadians) * thrust * delta;
}


void Ship::update(float delta)
{
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
	}

	// Gravity from the planet follows the inverse-square law. Surface gravity is 9.81 m/s^2, which is 0.001463 px/s^2 at this scale (1 px = 6,706 m).
	const float surfaceGravity = 0.001463f;
	float dx = (float)PLANET_CENTER_X - worldX;
	float dy = (float)PLANET_CENTER_Y - worldY;
	float dist = sqrt(dx * dx + dy * dy);
	float gravity = surfaceGravity * (float)PLANET_RADIUS * PLANET_RADIUS / (dist * dist);
	velX += gravity * dx / dist * delta;
	velY += gravity * dy / dist * delta;

	// Momentum: the ship keeps moving at its stored velocity when thrust is off.
	worldX += velX * delta;
	worldY += velY * delta;
	// The planet is fixed in world space, so its drawn position moves opposite to the ship.
	cocos2d::Vec2 shipHome = this->getPosition();
	this->planet->setPosition(shipHome.x + PLANET_CENTER_X - worldX, shipHome.y + PLANET_CENTER_Y - worldY);
	float tile = (float)STAR_TILE_SIZE;
	// Each layer scrolls at a fraction of the ship's movement, so nearer stars appear to move faster.
	const float parallax[3] = { 0.25f, 0.5f, 1.0f };
	for (int i = 0; i < 3; ++i) {
		float wrapX = fmod(fmod(worldX * parallax[i], tile) + tile, tile);
		float wrapY = fmod(fmod(worldY * parallax[i], tile) + tile, tile);
		this->starLayers[i]->setPosition(-wrapX, -wrapY);
	}

	speed = sqrt(velX * velX + velY * velY);
	// Readouts are updated individually below.
	if (lblX) lblX->setString(std::to_string((int)worldX));
	if (lblY) lblY->setString(std::to_string((int)worldY));
	if (lblSpeed) lblSpeed->setString(std::to_string((int)speed));
	(void)lblCoords;
}
