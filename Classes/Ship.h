#pragma once
#include "cocos2d.h"

class Ship :
	public cocos2d::Sprite
{
private:
	cocos2d::Label *lblCoords;
	cocos2d::Node *starLayers[3];
	cocos2d::Node *planet;
	cocos2d::Label *lblX = nullptr, *lblY = nullptr, *lblSpeed = nullptr;
	cocos2d::Texture2D *texShip = nullptr, *texShipFwd = nullptr;
	bool thrustShown = false;
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
	static const int PLANET_RADIUS = 950;
	static const int PLANET_CENTER_X = 0;
	static const int PLANET_CENTER_Y = -1000;
	Ship(cocos2d::Label *lblCoords, cocos2d::Node *farStars, cocos2d::Node *midStars, cocos2d::Node *nearStars, cocos2d::Node *planet);
	void setReadouts(cocos2d::Label *x, cocos2d::Label *y, cocos2d::Label *s) { lblX = x; lblY = y; lblSpeed = s; }
	~Ship();
	void onKeyPressed(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event * event);
	void onKeyReleased(cocos2d::EventKeyboard::KeyCode keyCode, cocos2d::Event * event);
	void stepForward(float delta);
	void update(float delta) override;
};
