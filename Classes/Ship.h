#pragma once
#include "cocos2d.h"

class Ship :
	public cocos2d::Sprite
{
private:
	cocos2d::Label *lblCoords;
public:
	Ship(cocos2d::Label *lblCoords);
	~Ship();
	void update(float delta) override;
};

