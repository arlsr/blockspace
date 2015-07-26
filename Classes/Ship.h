#pragma once
#include "cocos2d.h"

class Ship :
	public cocos2d::Sprite
{
private:
	cocos2d::Label *lblCoords;
	bool rotatingLeft;
	bool rotatingRight;
	bool propellingForward;
	int worldX;
	int worldY;
public:
	Ship(cocos2d::Label *lblCoords);
	~Ship();
	void onKeyPressed(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event * event);
	void onKeyReleased(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event * event);
	void stepForward(float delta);
	void update(float delta) override;
};

