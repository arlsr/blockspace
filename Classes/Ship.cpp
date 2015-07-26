#include "Ship.h"



Ship::Ship(cocos2d::Label *lblCoords)
{
	this->lblCoords = lblCoords;
	this->initWithFile("Ship.png");
	this->scheduleUpdate();

	this->lblCoords->setString("init");
}


Ship::~Ship()
{
}

void Ship::update(float delta) {
	

	this->setPositionX(this->getPositionX() + 1);

	std::ostringstream os;
	os << "x: " << this->getPositionX() << ", y:" << this->getPositionY();
	this->lblCoords->setString(os.str());
}