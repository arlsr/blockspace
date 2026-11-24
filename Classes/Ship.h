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
	float boostTime = 0.0f;
	// Shock rings are stored in world coordinates when they are created and drawn on the planet.
	float shockX[6] = {};
	float shockY[6] = {};
	float shockBorn[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
	int shockSpawned = 0;
	int shockNext = 0;
	float shockClock = 0.0f;
	cocos2d::DrawNode *shockFx = nullptr;
	cocos2d::DrawNode *sunFx = nullptr;
	// Earth's position and velocity in simulation coordinates. Earth orbits the fixed Sun.
	float earthX = 0.0f;
	float earthY = -1000.0f;
	float earthVX = 0.0f;
	float earthVY = 4.4413f;
	bool rotatingLeft;
	bool rotatingRight;
	bool propellingForward;
	bool retroBraking = false;
	bool retroFiring = false;
	cocos2d::DrawNode *retroFlame = nullptr;
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
	void brakeRetro(float delta);
	void updateRetroFlame();
	void update(float delta) override;
	// Ship state for the fleet, relative to Earth's centre: position (px), velocity (px/s), and the unit nose direction.
	cocos2d::Vec2 relativePos() const { return cocos2d::Vec2(worldX - earthX, worldY - earthY); }
	cocos2d::Vec2 relativeVel() const { return cocos2d::Vec2(velX - earthVX, velY - earthVY); }
	cocos2d::Vec2 noseDirection() const { const float r = getRotation() * (float)M_PI / 180.0f; return cocos2d::Vec2(sinf(r), cosf(r)); }
};
