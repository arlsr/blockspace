#pragma once

#include <random>
#include <vector>
#include "2d/CCNode.h"
#include "2d/CCDrawNode.h"

// Flying-saucer raiders. They hunt the player when close, and otherwise bombard random unscorched land on Earth.
// All positions are in planet-local pixels (Earth centre at 0,0), so the fleet follows Earth's motion.
class AlienFleet : public cocos2d::Node
{
public:
    static AlienFleet* create(cocos2d::Node* planet, cocos2d::Node* ship, float screenHeight);
    bool initFleet(cocos2d::Node* planet, cocos2d::Node* ship, float screenHeight);
    void update(float delta) override;

private:
    struct Alien {
        cocos2d::Vec2 pos;
        cocos2d::Vec2 vel;
        float fireTimer;
        float phase;
        bool combat;
        float orbitRadius = 0.0f; // patrol distance from Earth's centre in px; 0 means not yet set
    };
    struct Bolt {
        cocos2d::Vec2 pos;
        cocos2d::Vec2 vel;
        cocos2d::Vec2 target;   // chosen land point for land-aimed bolts
        float travelled;        // pixels flown so far
        bool aimedAtShip;
    };
    struct Puff {
        cocos2d::Vec2 pos;
        float life;
    };
    struct Chunk {
        cocos2d::Vec2 pos;
        cocos2d::Vec2 vel;
        float rot;
        float spin;
        std::vector<cocos2d::Vec2> shape; // jagged outline in local px
    };
    struct Fireball {
        cocos2d::Vec2 pos;
        float age;   // seconds since it started; negative while waiting
        float life;
        float maxR;
    };

    cocos2d::Vec2 shipLocal() const;
    bool pickLandTarget(const cocos2d::Vec2& from, cocos2d::Vec2& out, float maxDist);
    void fireAt(const cocos2d::Vec2& from, const cocos2d::Vec2& to, bool aimedAtShip);
    void addScorch(const cocos2d::Vec2& p);
    void impactAt(const cocos2d::Vec2& p);
    void updateAlien(Alien& a, float delta, const cocos2d::Vec2& ship);
    void drawFleet();
    void markCountryAt(const cocos2d::Vec2& p);
    void startDoom();
    void updateDoom(float delta);

    cocos2d::Node* _planet = nullptr;
    cocos2d::Node* _ship = nullptr;
    cocos2d::DrawNode* _fx = nullptr;
    cocos2d::DrawNode* _scorch = nullptr;
    float _screenH = 800.0f;
    std::vector<Alien> _aliens;
    std::vector<Bolt> _bolts;
    std::vector<cocos2d::Vec2> _scorches;
    std::vector<Puff> _steam;
    std::vector<int> _countryKeys;   // one colour key per country
    std::vector<int> _landCountry;   // country index for each land triangle
    std::vector<bool> _countryHit;   // true once a country has a scorch
    size_t _countriesHit = 0;
    bool _doomStarted = false;
    bool _doomBroke = false;
    bool _gameOver = false;
    float _doomT = 0.0f;
    cocos2d::DrawNode* _doomFx = nullptr;
    std::vector<Chunk> _chunks;
    std::vector<Fireball> _fireballs;
    float _spawnTimer = 5.0f;              // seconds until the next raider appears
    cocos2d::Vec2 _spawnBase;              // planet-local start point where raiders appear
    std::vector<cocos2d::Vec2> _land; // three vertices per land triangle, in planet-local pixels
    std::mt19937 _rng;
};
