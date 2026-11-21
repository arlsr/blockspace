#include "Ship.h"

/*
So far this is acting as the player's ship. It should be a base class for any ship or at least rename this one PlayerShip.

*/


Ship::Ship(cocos2d::Label *lblCoords, cocos2d::Node *farStars, cocos2d::Node *midStars, cocos2d::Node *nearStars)
{
	rotatingLeft = rotatingRight = propellingForward = false;
	worldX = worldY = 0;
	speed = 0;
	velX = velY = 0;

	this->lblCoords = lblCoords;
	this->starLayers[0] = farStars;
	this->starLayers[1] = midStars;
	this->starLayers[2] = nearStars;
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
	// HACK: This has to be the wrong way to change a sprite.
	if (propellingForward) {
		this->initWithFile("Ship_fwd.png");
		stepForward(delta);
	}
	else {
		this->initWithFile("Ship.png");
	}

	// Momentum: the ship keeps moving at its stored velocity when thrust is off.
	worldX += velX * delta;
	worldY += velY * delta;
	float tile = (float)STAR_TILE_SIZE;
	// Each layer scrolls at a fraction of the ship's movement, so nearer stars appear to move faster.
	const float parallax[3] = { 0.25f, 0.5f, 1.0f };
	for (int i = 0; i < 3; ++i) {
		float wrapX = fmod(fmod(worldX * parallax[i], tile) + tile, tile);
		float wrapY = fmod(fmod(worldY * parallax[i], tile) + tile, tile);
		this->starLayers[i]->setPosition(-wrapX, -wrapY);
	}

	speed = sqrt(velX * velX + velY * velY);
	std::ostringstream os;
	os << "x: " << (int)worldX << ", y: " << (int)worldY << ", speed: " << (int)speed;
	this->lblCoords->setString(os.str());
}
