#pragma once

#include <random>
#include <vector>
#include "2d/CCNode.h"
#include "2d/CCDrawNode.h"
#include "cocos2d.h"

// Flying-saucer raiders. They hunt the player when close, and otherwise bombard random unscorched land on Earth.
// All positions are in planet-local pixels (Earth centre at 0,0), so the fleet follows Earth's motion.
class AlienFleet : public cocos2d::Node
{
public:
    static AlienFleet* create(cocos2d::Node* planet, cocos2d::Node* ship, float screenHeight);
    bool initFleet(cocos2d::Node* planet, cocos2d::Node* ship, float screenHeight);
    void update(float delta) override;
    // Keys: R locks the raider nearest dead ahead, then cycles the longest-unlocked; E steps to the next tracked raider; F fires a missile.
    void onKey(cocos2d::EventKeyboard::KeyCode key);

private:
    struct Alien {
        cocos2d::Vec2 pos;
        cocos2d::Vec2 vel;
        float fireTimer;
        float phase;
        bool combat;
        float orbitRadius = 0.0f; // patrol distance from Earth's centre in px; 0 means not yet set
        int id = 0;               // unique per raider, so missiles can keep a fixed lock
        float burn = 0.0f;        // seconds left on fire; 0 when not burning
        float lastLocked = -1.0e9f; // game time of the last lock, so R can cycle the longest-ago target first
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

    struct Missile {
        cocos2d::Vec2 pos;
        cocos2d::Vec2 vel;
        int targetId;   // the raider locked when the missile was fired; it never changes
        float flown;    // distance covered so far, px
        float age;      // seconds since launch
        bool seeker;    // true when fired without a lock: it heat-seeks the nearest raider
    };
    struct Spark {
        cocos2d::Vec2 pos;
        cocos2d::Vec2 vel;
        float life;
    };
    struct Blast {
        cocos2d::Vec2 pos;
        float age;
        float maxR;
        bool fresh; // true only on the frame it appears; only things present then are affected
    };

    cocos2d::Vec2 shipLocal() const;
    void shipHit();
    void shipDestroyed();
    int indexOfAlien(int id) const;
    void updateTargeting();
    void fireMissile();
    void updateMissiles(float delta);
    void explodeAt(const cocos2d::Vec2& p, float size);
    void drawEffects();
    void updateBurning(float delta);
    bool pickLandTarget(const cocos2d::Vec2& from, cocos2d::Vec2& out, float maxDist);
    void fireAt(const cocos2d::Vec2& from, const cocos2d::Vec2& to, bool aimedAtShip);
    void addScorch(const cocos2d::Vec2& p);
    void impactAt(const cocos2d::Vec2& p);
    void updateAlien(Alien& a, float delta, const cocos2d::Vec2& ship);
    void drawFleet();
    void markCountryAt(const cocos2d::Vec2& p);
    void startDoom();
    void updateDoom(float delta);
    void setLock(int id);
    void cycleRaiderLock();
    int frontmostTracked() const;

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
    std::vector<Missile> _missiles;
    std::vector<Spark> _sparks;
    std::vector<Blast> _blasts;
    std::vector<int> _tracked;   // ids of raiders in sensor range, nearest first
    int _lockId = 0;             // raider currently locked, 0 for none
    int _nextId = 1;
    float _shipBurn = 0.0f;      // seconds the player's ship stays on fire
    int _shieldHits = 0;         // hits taken since the shield last came up
    float _shieldDown = 0.0f;    // seconds until the shield returns, 0 while it is up
    bool _shipDead = false;      // the ship has exploded and the game is ending
    float _deathT = 0.0f;        // seconds since the ship exploded
    float _clock = 0.0f;         // game time in seconds, used to order locks
    std::vector<Chunk> _chunks;
    std::vector<Fireball> _fireballs;
    float _spawnTimer = 5.0f;              // seconds until the next raider appears
    cocos2d::Vec2 _spawnBase;              // planet-local start point where raiders appear
    std::vector<cocos2d::Vec2> _land; // three vertices per land triangle, in planet-local pixels
    std::mt19937 _rng;
};
