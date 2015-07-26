#include "Ship.h"



Ship::Ship(cocos2d::Label *lblCoords)
{
	rotatingLeft = rotatingRight = propellingForward = false;
	worldX = worldY = 0;

	this->lblCoords = lblCoords;
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

void Ship::stepForward(float delta) {

	cocos2d::Vec2 direction, velocity;
	float rotation = this->getRotation();
	float speed = 100.0;
	float rotationRadians = rotation * (M_PI / 180);

	direction.x = cos(rotationRadians);
	direction.y = sin(rotationRadians);
	if (direction.length() > 0) {
		direction.normalize();
	}
	//Then scale it by the current speed to get the velocity
	velocity.x = direction.x * speed;
	velocity.y = direction.y * speed;

	//So now you know your velocity based on your rotation.You can the update your position with that information.

	//Update the position based on our current speed
	//This is basic s = vt physics
	worldX += velocity.x * delta;
	worldY += velocity.y * delta;
}

void Ship::update(float delta) {
	
	std::ostringstream os;
	os << "x: " << worldX << ", y:" << worldY;
	this->lblCoords->setString(os.str());

	if (rotatingLeft) {
		this->setRotation(this->getRotation() - (60 * delta));
	}
	if (rotatingRight) {
		this->setRotation(this->getRotation() + (60 * delta));
	}
	// HACK: This has to be the wrong way to change a sprite.
	if (propellingForward) {
		this->initWithFile("Ship_fwd.png");

		stepForward(delta);
	}
	else {
		this->initWithFile("Ship.png");
	}
}