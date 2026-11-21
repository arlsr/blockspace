#pragma once
#include "cocos2d.h"

class Ship :
	public cocos2d::Sprite
{
private:
	cocos2d::Label *lblCoords;
	cocos2d::Node *starLayers[3];
	bool rotatingLeft;
	bool rotatingRight;
	bool propellingForward;
	float speed;
	float velX;
	float velY;
	float worldX;
	float worldY;
public:
	static const int STAR_TILE_SIZE = 1024;
	Ship(cocos2d::Label *lblCoords, cocos2d::Node *farStars, cocos2d::Node *midStars, cocos2d::Node *nearStars);
	~Ship();
	void onKeyPressed(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event * event);
	void onKeyReleased(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event * event);
	void stepForward(float delta);
	void update(float delta) override;
};
