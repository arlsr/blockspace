#include "AlienFleet.h"
#include "Ship.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
const float kTrackScreens = 2.0f;          // sensor range for tracking raiders, in screens
const float kMissileLaunchSpeed = 900.0f;  // muzzle speed relative to the ship, px/s
const float kMissileCoastScreens = 0.3f;   // distance flown before the thrusters light
const float kMissileThrust = 700.0f;       // homing acceleration, px/s^2
const float kMissileMaxSpeed = 1600.0f;    // speed cap, px/s
const float kMissileLife = 5.0f;          // seconds before it burns out
const float kMissileHitRadius = 9.0f;      // px
const float kBlastLife = 1.6f;
const float kAlienBurnTime = 3.0f;        // seconds a raider burns before it falls
const float kShipBurnTime = 8.0f;         // seconds the player's ship burns             // seconds
}
#include "base/CCDirector.h"
#include "2d/CCLabel.h"
#include "Ship.h"
#include "platform/CCFileUtils.h"
#include "audio/include/AudioEngine.h"
#include <new>
#include <sstream>
#include <cmath>

USING_NS_CC;

namespace {
// Bolts that miss their target stop after this many screen heights of flight (2.0 * _screenH).
constexpr float kMissileBoostTime = 0.25f; // seconds of high launch speed before the missile drops to cruise speed
constexpr float kMissileCruiseSpeed = 50.0f; // speed a missile settles to after the boost
constexpr float kMissileDecelSpeed = 200.0f; // speed a missile is at when the boost ends
constexpr float kMissileDecelTime = 4.0f; // seconds to slow from the boost-end speed down to the cruise speed

// Missile speed profile: 900 px/s down to 200 px/s over the boost, then gradually down to 50 px/s.
static float missileSpeedAt(float age)
{
    if (age < kMissileBoostTime) {
        return kMissileDecelSpeed + (kMissileLaunchSpeed - kMissileDecelSpeed) * (1.0f - age / kMissileBoostTime);
    }
    const float t = std::min(1.0f, (age - kMissileBoostTime) / kMissileDecelTime);
    return kMissileDecelSpeed + (kMissileCruiseSpeed - kMissileDecelSpeed) * t;
}
constexpr float kLaunchSkipSeconds = 1.0f; // silent lead-in of the launch woosh, skipped so it sounds at once
constexpr float kBoltMaxScreens = 1.0f;       // ship-aimed bolts that miss
constexpr float kLandBoltMaxScreens = 2.0f;   // land-aimed bolts, twice the ship-bolt range
constexpr size_t kMaxAliens = 20;             // no new raiders spawn once this many are alive
}

AlienFleet* AlienFleet::create(Node* planet, Node* ship, float screenHeight)
{
    auto ret = new (std::nothrow) AlienFleet();
    if (ret && ret->initFleet(planet, ship, screenHeight)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool AlienFleet::initFleet(Node* planet, Node* ship, float screenHeight)
{
    if (!Node::init()) {
        return false;
    }
    _planet = planet;
    _ship = ship;
    _screenH = screenHeight;
    _rng.seed(4321);
    // Load the sounds now so the first press plays at once instead of decoding the file.
    AudioEngine::preload("sfx/freesound/missile_launch_woosh.mp3");
    AudioEngine::preload("sfx/kenney/alien_pew.ogg");
    AudioEngine::preload("sfx/kenney/missile_explosion.ogg");

    _scorch = DrawNode::create();
    addChild(_scorch, 0);
    _fx = DrawNode::create();
    addChild(_fx, 1);

    // Land triangles come from the same outline file the planet is drawn from.
    std::string mapData = FileUtils::getInstance()->getStringFromFile("earth_map.txt");
    std::istringstream mapStream(mapData);
    std::string kind;
    const float R = (float)Ship::PLANET_RADIUS;
    while (mapStream >> kind) {
        if (kind == "T") {
            float c[9], x[3], y[3];
            for (float &v : c) {
                mapStream >> v;
            }
            for (int i = 0; i < 3; i++) {
                mapStream >> x[i] >> y[i];
            }
            // Zero-area triangles (all vertices equal or collinear) would make the point-in-triangle test report land everywhere.
            const float area2 = (x[1] - x[0]) * R * (y[2] - y[0]) * R - (y[1] - y[0]) * R * (x[2] - x[0]) * R;
            if (fabsf(area2) < 1e-3f) {
                continue;
            }
            // Each country is drawn in one colour, so the colour identifies the country.
            const int key = (int)lroundf(c[0] * 1000.0f) * 1000000 + (int)lroundf(c[1] * 1000.0f) * 1000 + (int)lroundf(c[2] * 1000.0f);
            size_t country = 0;
            while (country < _countryKeys.size() && _countryKeys[country] != key) {
                country++;
            }
            if (country == _countryKeys.size()) {
                _countryKeys.push_back(key);
            }
            _landCountry.push_back((int)country);
            for (int i = 0; i < 3; i++) {
                _land.push_back(Vec2(x[i] * R, y[i] * R));
            }
        }
    }

    // Raiders appear at the player's start point, nine screens out, and dive toward it.
    _countryHit.assign(_countryKeys.size(), false);
    _spawnBase = shipLocal();
    for (int i = 0; i < 3; i++) {
        Alien a;
        Vec2 offset((i - 1) * 0.4f * _screenH, (9.0f + 1.5f * i) * _screenH);
        a.pos = shipLocal() + offset;
        a.vel = Vec2(0.0f, -150.0f);
        a.fireTimer = 1.5f + 0.5f * i;
        a.phase = i * 2.1f;
        a.combat = false;
        a.id = _nextId++;
        _aliens.push_back(a);
    }
    scheduleUpdate();
    return true;
}

// Ship position relative to Earth's centre. Both nodes sit in the world node, so the difference is the planet-local offset.
Vec2 AlienFleet::shipLocal() const
{
    return _ship->getPosition() - _planet->getPosition();
}

// Picks a random land point on the attacker's side of Earth that has no scorch mark nearby.
bool AlienFleet::pickLandTarget(const Vec2& from, Vec2& out, float maxDist)
{
    const unsigned triCount = (unsigned)_land.size() / 3;
    if (triCount == 0) {
        return false;
    }
    const Vec2 facing = from.getNormalized();
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    for (int attempt = 0; attempt < 200; attempt++) {
        const size_t t = (size_t)(_rng() % triCount) * 3;
        // Prefer countries not yet hit: a country that already has a scorch is kept only one time in ten.
        if (_countryHit[(size_t)_landCountry[t / 3]] && unit(_rng) > 0.1f) {
            continue;
        }
        float u = unit(_rng), v = unit(_rng);
        if (u + v > 1.0f) {
            u = 1.0f - u;
            v = 1.0f - v;
        }
        const Vec2 a = _land[t], b = _land[t + 1], c = _land[t + 2];
        const Vec2 p = a + (b - a) * u + (c - a) * v;
        // Only the face toward the attacker can be reached by a straight shot.
        if (p.distance(from) > maxDist) {
            continue;
        }
        if (from.length() > 1.0f && p.getNormalized().dot(facing) < 0.25f) {
            continue;
        }
        bool scorched = false;
        for (const Vec2 &s : _scorches) {
            if (p.distance(s) < 30.0f) {
                scorched = true;
                break;
            }
        }
        // Targets of land-aimed bolts still in flight count as scorched, so two aliens never pick the same spot.
        for (const Bolt &b : _bolts) {
            if (!b.aimedAtShip && p.distance(b.target) < 30.0f) {
                scorched = true;
                break;
            }
        }
        if (scorched) {
            continue;
        }
        out = p;
        return true;
    }
    return false;
}

// Fires a laser bolt in a straight line at 520 px/s. It ignores Earth until it hits the ship, reaches its target or reaches max range.
// Ship-aimed bolts use aimedAtShip=true. Land-aimed bolts fly to 'to', which is stored as their target.
void AlienFleet::fireAt(const Vec2& from, const Vec2& to, bool aimedAtShip)
{
    Bolt b;
    b.pos = from;
    b.vel = (to - from).getNormalized() * 520.0f;
    b.target = to;
    b.travelled = 0.0f;
    b.aimedAtShip = aimedAtShip;
    _bolts.push_back(b);
    // Only bolts fired within one screen of the ship are audible.
    if (from.distance(shipLocal()) < _screenH) {
        AudioEngine::play2d("sfx/kenney/alien_pew.ogg", false, 0.5f);
    }
}

// A charred spot: a dark core with a faint orange rim. It stays on Earth permanently.
void AlienFleet::addScorch(const Vec2& p)
{
    _scorches.push_back(p);
    // Stacked rings: the outer edge is almost transparent and the glow strengthens toward the core.
    _scorch->drawSolidCircle(p, 8.0f, 0.0f, 16, Color4F(1.0f, 0.2f, 0.05f, 0.04f));
    _scorch->drawSolidCircle(p, 7.5f, 0.0f, 16, Color4F(1.0f, 0.2f, 0.05f, 0.08f));
    _scorch->drawSolidCircle(p, 7.0f, 0.0f, 16, Color4F(1.0f, 0.2f, 0.05f, 0.12f));
    _scorch->drawSolidCircle(p, 6.5f, 0.0f, 16, Color4F(1.0f, 0.2f, 0.05f, 0.16f));
    _scorch->drawSolidCircle(p, 6.0f, 0.0f, 16, Color4F(1.0f, 0.2f, 0.05f, 0.2f));
    _scorch->drawSolidCircle(p, 5.5f, 0.0f, 16, Color4F(0.06f, 0.05f, 0.04f, 0.9f));
    _scorch->drawSolidCircle(p, 3.0f, 0.0f, 16, Color4F(0.12f, 0.08f, 0.05f, 0.9f));
}

// True when a point lies inside one of the land triangles.
static bool overLand(const std::vector<Vec2> &land, const Vec2 &p)
{
    auto cross = [](const Vec2 &o, const Vec2 &u, const Vec2 &v) {
        return (u.x - o.x) * (v.y - o.y) - (u.y - o.y) * (v.x - o.x);
    };
    for (size_t t = 0; t + 2 < land.size(); t += 3) {
        const Vec2 &a = land[t], &b = land[t + 1], &c = land[t + 2];
        const float d1 = cross(a, b, p), d2 = cross(b, c, p), d3 = cross(c, a, p);
        const bool hasNeg = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
        const bool hasPos = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
        if (!(hasNeg && hasPos)) {
            return true;
        }
    }
    return false;
}

// Records the country under a land scorch. Each country counts once.
void AlienFleet::markCountryAt(const Vec2 &p)
{
    auto cross = [](const Vec2 &o, const Vec2 &u, const Vec2 &v) {
        return (u.x - o.x) * (v.y - o.y) - (u.y - o.y) * (v.x - o.x);
    };
    for (size_t t = 0; t + 2 < _land.size(); t += 3) {
        const Vec2 &a = _land[t], &b = _land[t + 1], &c = _land[t + 2];
        const float d1 = cross(a, b, p), d2 = cross(b, c, p), d3 = cross(c, a, p);
        const bool hasNeg = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
        const bool hasPos = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
        if (!(hasNeg && hasPos)) {
            const size_t country = (size_t)_landCountry[t / 3];
            if (!_countryHit[country]) {
                _countryHit[country] = true;
                _countriesHit++;
            }
            return;
        }
    }
}

// Every country scorched: the planet begins to break apart. Debris and fireballs live in the world, not under the planet, so they survive its hiding.
void AlienFleet::startDoom()
{
    _doomStarted = true;
    _doomT = 0.0f;
    _doomFx = DrawNode::create();
    _planet->getParent()->addChild(_doomFx, 5);
}

// The only place marks are made. Bolts ending in space leave nothing, land is scorched, and open ocean throws up steam.
void AlienFleet::impactAt(const Vec2 &p)
{
    if (p.length() > (float)Ship::PLANET_RADIUS) {
        return;
    }
    if (overLand(_land, p)) {
        addScorch(p);
        markCountryAt(p);
        // Doom starts once one country or fewer is still untouched.
        if (!_doomStarted && !_countryKeys.empty() && _countriesHit + 1 >= _countryKeys.size()) {
            startDoom();
        }
    } else {
        _steam.push_back({p, 2.0f});
    }
}

// Earth's break-up in four stages: it caves in, flashes white, shatters into debris with fireballs, then shows game over.
void AlienFleet::updateDoom(float delta)
{
    const float R = (float)Ship::PLANET_RADIUS;
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    _doomT += delta;
    _doomFx->setPosition(_planet->getPosition());
    _doomFx->clear();

    if (_doomT < 1.5f) {
        // Crumble: the planet caves in on itself.
        _planet->setScale(1.0f - 0.12f * _doomT / 1.5f);
    } else if (_doomT < 2.1f) {
        // Flash: a blinding white light blooms from the centre.
        _planet->setVisible(false);
        const float k = (_doomT - 1.5f) / 0.6f;
        _doomFx->drawSolidCircle(Vec2::ZERO, R * (0.9f + 0.8f * k), 0.0f, 48, Color4F(1.0f, 1.0f, 0.95f, 1.0f - 0.3f * k));
    } else {
        _planet->setVisible(false);
        if (!_doomBroke) {
            _doomBroke = true;
            for (int i = 0; i < 60; i++) {
                Chunk c;
                const float ang = unit(_rng) * 6.2832f;
                const float dist = R * 0.7f * unit(_rng);
                c.pos = Vec2(cosf(ang), sinf(ang)) * dist;
                c.vel = Vec2(cosf(ang), sinf(ang)) * (150.0f + 400.0f * unit(_rng));
                c.rot = 0.0f;
                c.spin = (unit(_rng) - 0.5f) * 6.0f;
                const float size = R * (0.08f + 0.12f * unit(_rng));
                for (int k = 0; k < 7; k++) {
                    const float a2 = k * 6.2832f / 7.0f;
                    const float r = size * (0.6f + 0.4f * unit(_rng));
                    c.shape.push_back(Vec2(cosf(a2), sinf(a2)) * r);
                }
                _chunks.push_back(c);
            }
            for (int i = 0; i < 40; i++) {
                Fireball f;
                const float ang = unit(_rng) * 6.2832f;
                f.pos = Vec2(cosf(ang), sinf(ang)) * (R * 0.8f * unit(_rng));
                f.age = -2.0f * unit(_rng);
                f.life = 1.8f + unit(_rng);
                f.maxR = R * (0.1f + 0.15f * unit(_rng));
                _fireballs.push_back(f);
            }
        }
        // Debris drifts outward and tumbles.
        for (Chunk &c : _chunks) {
            c.pos += c.vel * delta;
            c.rot += c.spin * delta;
            std::vector<Vec2> pts;
            for (const Vec2 &v : c.shape) {
                pts.push_back(v.rotateByAngle(Vec2::ZERO, c.rot) + c.pos);
            }
            _doomFx->drawSolidPoly(pts.data(), (unsigned)pts.size(), Color4F(0.45f, 0.35f, 0.25f, 1.0f));
        }
        // Fireballs swell, shift from yellow to red, and fade.
        for (Fireball &f : _fireballs) {
            f.age += delta;
            if (f.age > 0.0f && f.age < f.life) {
                const float k = f.age / f.life;
                const float r = f.maxR * (0.3f + 0.7f * k);
                _doomFx->drawSolidCircle(f.pos, r, 0.0f, 24, Color4F(1.0f, 0.9f - 0.7f * k, 0.3f - 0.25f * k, 1.0f - k));
            }
        }
    }

    if (_doomT >= 8.0f && !_gameOver) {
        _gameOver = true;
        Director::getInstance()->pause();
        auto label = Label::createWithSystemFont("GAME OVER", "Arial", 64);
        label->setPosition(Director::getInstance()->getVisibleOrigin() + Director::getInstance()->getVisibleSize() * 0.5f);
        Director::getInstance()->getRunningScene()->addChild(label, 1000);
    }
}

void AlienFleet::update(float delta)
{
    if (_gameOver) {
        return;
    }
    if (_doomStarted) {
        updateDoom(delta);
        return;
    }
    const Vec2 ship = shipLocal();

    // A new raider appears every 5 s above the player's start point, nine screens out.
    _spawnTimer -= delta;
    if (_spawnTimer <= 0.0f && _aliens.size() < kMaxAliens) {
        _spawnTimer += 5.0f;
        std::uniform_real_distribution<float> side(-0.4f, 0.4f);
        std::uniform_real_distribution<float> phase(0.0f, 6.28f);
        Alien a;
        a.pos = _spawnBase + Vec2(side(_rng) * _screenH, 9.0f * _screenH);
        a.vel = Vec2(0.0f, -150.0f);
        a.fireTimer = 1.5f;
        a.phase = phase(_rng);
        a.combat = false;
        a.id = _nextId++;
        _aliens.push_back(a);
    }

    for (Alien &a : _aliens) {
        updateAlien(a, delta, ship);
    }

    // Bolts fly straight over Earth. Only the player's hull, a land-aimed target, or max range ends them.
    for (size_t i = 0; i < _bolts.size();) {
        Bolt &b = _bolts[i];
        const Vec2 prev = b.pos;
        b.travelled += b.vel.length() * delta;
        b.pos += b.vel * delta;
        const float maxRange = (b.aimedAtShip ? kBoltMaxScreens : kLandBoltMaxScreens) * _screenH;
        bool gone = false;
        if (b.pos.distance(ship) < 6.0f) {
            gone = true; // hit the player's ship, no mark
        } else if (!b.aimedAtShip && (b.pos.distance(b.target) < 4.0f || (b.target - prev).dot(b.target - b.pos) <= 0.0f)) {
            impactAt(b.target);
            gone = true;
        } else if (b.travelled >= maxRange) {
            impactAt(b.pos);
            gone = true;
        }
        if (gone) {
            _bolts[i] = _bolts.back();
            _bolts.pop_back();
        } else {
            ++i;
        }
    }
    for (size_t i = 0; i < _steam.size();) {
        _steam[i].life -= delta;
        if (_steam[i].life <= 0.0f) {
            _steam[i] = _steam.back();
            _steam.pop_back();
        } else {
            ++i;
        }
    }
    _clock += delta;
    updateMissiles(delta);
    updateBurning(delta);
    updateTargeting();
    drawFleet();

    // Steam is white, spreads as it ages and fades to nothing over 2 s. Drawn after drawFleet, which clears _fx.
    for (const Puff &s : _steam) {
        const float age = 2.0f - s.life;
        _fx->drawSolidCircle(s.pos, 2.0f + 3.0f * age, 0.0f, 16, Color4F(1.0f, 1.0f, 1.0f, 0.8f * s.life / 2.0f));
    }
    drawEffects();
}

void AlienFleet::updateAlien(Alien &a, float delta, const Vec2 &ship)
{
    const float R = (float)Ship::PLANET_RADIUS;
    const float dShip = a.pos.distance(ship);
    a.combat = dShip < _screenH;
    std::uniform_real_distribution<float> jitter(0.0f, 1.0f);
    Vec2 vel;
    if (a.combat) {
        // Close to a standoff distance of 0.6 screens, then circle the ship while firing at it.
        const float standoff = 0.6f * _screenH;
        const Vec2 toShip = (ship - a.pos).getNormalized();
        const Vec2 tangent(-toShip.y, toShip.x);
        vel = toShip * ((dShip - standoff) * 0.8f) + tangent * 90.0f;
        if (vel.length() > 200.0f) {
            vel = vel.getNormalized() * 200.0f;
        }
        a.fireTimer -= delta;
        if (a.fireTimer <= 0.0f) {
            fireAt(a.pos, ship, true);
            a.fireTimer = 0.8f + 0.6f * jitter(_rng);
        }
    } else {
        // Patrol: circle Earth at 1.3 radii and bombard unscorched land from there.
        if (a.orbitRadius <= 0.0f) {
            a.orbitRadius = 1.3f * R;
        }
        const Vec2 n = a.pos.getNormalized();
        const Vec2 tangent(-n.y, n.x);
        vel = tangent * 40.0f - n * ((a.pos.length() - a.orbitRadius) * 0.5f);
        a.fireTimer -= delta;
        if (a.fireTimer <= 0.0f) {
            Vec2 target;
            if (pickLandTarget(a.pos, target, kLandBoltMaxScreens * _screenH)) {
                // Unscorched land is in range: fire, and climb back toward the cruising orbit of 1.3 radii.
                fireAt(a.pos, target, false);
                const float higher = a.orbitRadius + 0.05f * _screenH;
                a.orbitRadius = higher < 1.3f * R ? higher : 1.3f * R;
                a.fireTimer = 2.5f + 2.0f * jitter(_rng);
            } else {
                // No unscorched land in range: sink the orbit gradually toward Earth's centre, and check again soon.
                const float lower = a.orbitRadius - 0.05f * _screenH;
                a.orbitRadius = lower > 0.0f ? lower : 0.0f;
                a.fireTimer = 0.5f;
            }
        }
    }
    a.vel = vel;
    a.pos += vel * delta;
}

// Saucers are drawn about 11 px wide, the same as the player's hull at default zoom.
int AlienFleet::indexOfAlien(int id) const
{
    for (size_t i = 0; i < _aliens.size(); i++) {
        if (_aliens[i].id == id) {
            return (int)i;
        }
    }
    return -1;
}

// Every raider inside the sensor range is tracked, nearest first. A lock that leaves range is dropped.
void AlienFleet::updateTargeting()
{
    const Vec2 ship = shipLocal();
    const float range = kTrackScreens * _screenH;
    std::vector<std::pair<float, int>> inRange;
    for (const Alien &a : _aliens) {
        const float d = a.pos.distance(ship);
        if (d <= range) {
            inRange.push_back({d, a.id});
        }
    }
    std::sort(inRange.begin(), inRange.end());
    _tracked.clear();
    for (const auto &p : inRange) {
        _tracked.push_back(p.second);
    }
    if (_lockId != 0 && std::find(_tracked.begin(), _tracked.end(), _lockId) == _tracked.end()) {
        _lockId = 0;
    }
}

// The tracked raider closest to dead ahead of the ship, found by the angle between the nose and the bearing to it.
int AlienFleet::frontmostTracked() const
{
    const Vec2 ship = shipLocal();
    const Vec2 nose = static_cast<const Ship *>(_ship)->noseDirection();
    int best = -1;
    float bestCos = -2.0f;
    for (int id : _tracked) {
        const int idx = indexOfAlien(id);
        if (idx < 0) {
            continue;
        }
        const float c = (_aliens[idx].pos - ship).getNormalized().dot(nose);
        if (c > bestCos) {
            bestCos = c;
            best = id;
        }
    }
    return best;
}

// Locking stamps the raider with the game time, so cycling can pick the one locked longest ago.
void AlienFleet::setLock(int id)
{
    _lockId = id;
    const int idx = indexOfAlien(id);
    if (idx >= 0) {
        _aliens[idx].lastLocked = _clock;
    }
}

// Moves to the tracked raider that was locked longest ago. Never-locked raiders go first, nearest first among ties.
void AlienFleet::cycleRaiderLock()
{
    int best = -1;
    float oldest = 1.0e18f;
    for (int id : _tracked) {
        const int idx = indexOfAlien(id);
        if (idx < 0) {
            continue;
        }
        if (_aliens[idx].lastLocked < oldest) {
            oldest = _aliens[idx].lastLocked;
            best = id;
        }
    }
    if (best < 0) {
        _lockId = 0;
    } else {
        setLock(best);
    }
}

void AlienFleet::onKey(EventKeyboard::KeyCode key)
{
    if (_doomStarted || _gameOver) {
        return;
    }
    if (key == EventKeyboard::KeyCode::KEY_R) {
        if (_tracked.empty()) {
            _lockId = 0;
            return;
        }
        // First press locks the raider nearest dead ahead. Presses while locked cycle through the rest.
        if (_lockId != 0 && indexOfAlien(_lockId) >= 0) {
            cycleRaiderLock();
        } else {
            const int front = frontmostTracked();
            if (front < 0) {
                _lockId = 0;
            } else {
                setLock(front);
            }
        }
    } else if (key == EventKeyboard::KeyCode::KEY_E) {
        if (_tracked.empty()) {
            _lockId = 0;
            return;
        }
        int idx = -1;
        for (size_t i = 0; i < _tracked.size(); i++) {
            if (_tracked[i] == _lockId) {
                idx = (int)i;
            }
        }
        const int n = (int)_tracked.size();
        idx = (idx + 1) % n;
        setLock(_tracked[idx]);
    } else if (key == EventKeyboard::KeyCode::KEY_F) {
        fireMissile();
    }
}

// The missile leaves the nose at muzzle speed on top of the ship's own velocity. Its target is fixed at launch.
void AlienFleet::fireMissile()
{
    const Ship *s = static_cast<const Ship *>(_ship);
    const Vec2 nose = s->noseDirection();
    Missile m;
    m.pos = shipLocal() + nose * 12.0f;
    m.vel = s->relativeVel() + nose * kMissileLaunchSpeed;
    // With a lock the missile keeps that raider for life. Without one it heat-seeks the nearest raider.
    m.seeker = (_lockId == 0 || indexOfAlien(_lockId) < 0);
    m.targetId = m.seeker ? 0 : _lockId;
    m.flown = 0.0f;
    m.age = 0.0f;
    _missiles.push_back(m);
    const int sid = AudioEngine::play2d("sfx/freesound/missile_launch_woosh.mp3", false, 0.8f);
    if (sid >= 0) {
        AudioEngine::setCurrentTime(sid, kLaunchSkipSeconds);
    }
}

void AlienFleet::explodeAt(const Vec2 &p, float size)
{
    // Every explosion makes the rumble, louder for bigger blasts.
    AudioEngine::play2d("sfx/kenney/missile_explosion.ogg", false, std::min(1.0f, 0.5f + 0.5f * size));
    _blasts.push_back({p, 0.0f, 75.0f * size, true});
    std::uniform_real_distribution<float> angle(0.0f, 6.2832f);
    std::uniform_real_distribution<float> speed(60.0f, 380.0f * size + 60.0f);
    std::uniform_real_distribution<float> life(0.5f, 1.4f);
    for (int i = 0; i < 48; i++) {
        const float a = angle(_rng);
        const Vec2 v = Vec2(cosf(a), sinf(a)) * speed(_rng);
        _sparks.push_back({p, v, life(_rng)});
    }
}

void AlienFleet::updateMissiles(float delta)
{
    const float coast = kMissileCoastScreens * _screenH;
    for (size_t i = 0; i < _missiles.size();) {
        Missile &m = _missiles[i];
        m.age += delta;
        m.flown += m.vel.length() * delta;
        // The boost phase bleeds speed from the launch value down to 50 px/s, so the missile slows before its thrusters take over.
        if (m.vel.lengthSquared() > 0.0f) {
            const float target = missileSpeedAt(m.age);
            m.vel = m.vel.getNormalized() * target;
        }
        // Coasts out of the ship first, then the thrusters light and steer toward the fixed target with a lead.
        int idx = -1;
        if (m.seeker) {
            // Heat seeking: whichever raider is nearest to the missile right now.
            float best = 1e30f;
            for (size_t j = 0; j < _aliens.size(); j++) {
                const float d = _aliens[j].pos.distance(m.pos);
                if (d < best) {
                    best = d;
                    idx = (int)j;
                }
            }
        } else {
            idx = indexOfAlien(m.targetId);
        }
        // The thrusters stay off during the boost, so the missile is at cruise speed when they light.
        // The thrusters burn only until the slowdown ends; after that the missile heads straight.
        if (m.flown >= coast && m.age >= kMissileBoostTime && m.age < kMissileBoostTime + kMissileDecelTime && idx >= 0) {
            const Alien &t = _aliens[idx];
            const float d = t.pos.distance(m.pos);
            const Vec2 aim = t.pos + t.vel * std::min(d / kMissileMaxSpeed, 3.0f);
            m.vel += (aim - m.pos).getNormalized() * (kMissileThrust * delta);
        }
        // Speed is capped at the launch speed during the boost, then held at the cruise speed.
        const float cap = m.age < kMissileBoostTime ? kMissileMaxSpeed : missileSpeedAt(m.age);
        const float sp = m.vel.length();
        if (sp > cap) {
            m.vel *= cap / sp;
        }
        m.pos += m.vel * delta;

        // A missile caught in another blast's shock ring detonates too.
        bool shocked = false;
        for (const Blast &b : _blasts) {
            // Only things inside the fireball when it appears are affected, not ones that drive in later.
            if (!b.fresh) {
                continue;
            }
            const float reach = b.maxR * 0.9f;
            if (m.pos.distance(b.pos) <= reach) {
                shocked = true;
                break;
            }
        }

        // Collisions: any raider, the planet's surface, or burn-out.
        int hit = -1;
        for (size_t j = 0; j < _aliens.size(); j++) {
            if (_aliens[j].pos.distance(m.pos) < kMissileHitRadius) {
                hit = (int)j;
                break;
            }
        }
        bool gone = false;
        if (hit >= 0) {
            explodeAt(m.pos, 1.0f);
            _aliens.erase(_aliens.begin() + hit);
            gone = true;
        } else if (shocked) {
            explodeAt(m.pos, 0.8f);
            gone = true;
        } else if (m.age > kMissileLife) {
            // Burn-out: steam over sea, a scorch over land, nothing in space.
            explodeAt(m.pos, 0.6f);
            impactAt(m.pos);
            gone = true;
        }
        if (gone) {
            _missiles[i] = _missiles.back();
            _missiles.pop_back();
        } else {
            ++i;
        }
    }

    // The fireball sets fire to anything it reaches: raiders and the player's ship.
    const Vec2 shipPos = shipLocal();
    for (const Blast &b : _blasts) {
        if (!b.fresh) {
            continue;
        }
        const float reach = b.maxR * 0.9f;
        for (Alien &a : _aliens) {
            if (a.burn <= 0.0f && a.pos.distance(b.pos) <= reach) {
                a.burn = kAlienBurnTime;
            }
        }
        if (shipPos.distance(b.pos) <= reach) {
            _shipBurn = kShipBurnTime;
        }
    }
    for (size_t i = 0; i < _blasts.size();) {
        _blasts[i].age += delta;
        _blasts[i].fresh = false;
        if (_blasts[i].age >= kBlastLife) {
            _blasts[i] = _blasts.back();
            _blasts.pop_back();
        } else {
            ++i;
        }
    }
    for (size_t i = 0; i < _sparks.size();) {
        Spark &s = _sparks[i];
        s.life -= delta;
        s.pos += s.vel * delta;
        s.vel *= std::max(0.0f, 1.0f - 1.2f * delta);
        if (s.life <= 0.0f) {
            _sparks[i] = _sparks.back();
            _sparks.pop_back();
        } else {
            ++i;
        }
    }
}

static void drawRing(DrawNode *d, const Vec2 &c, float r, const Color4F &col)
{
    const int n = 40;
    for (int i = 0; i < n; i++) {
        const float a0 = 6.2832f * i / n;
        const float a1 = 6.2832f * (i + 1) / n;
        d->drawSegment(c + Vec2(cosf(a0), sinf(a0)) * r, c + Vec2(cosf(a1), sinf(a1)) * r, 1.5f, col);
    }
}

// Flames: six flickering tongues around a burning body. Each one flickers at its own rate and the ring slowly turns.
static void drawFlames(DrawNode *d, const Vec2 &c, float clock, float size)
{
    for (int i = 0; i < 6; i++) {
        const float a = 6.2832f * i / 6.0f + clock * 3.0f;
        const Vec2 dir(cosf(a), sinf(a));
        const float len = size * (0.6f + 0.4f * sinf(clock * 17.0f + i * 2.3f));
        d->drawSegment(c + dir * size * 0.4f, c + dir * (size * 0.4f + len), 1.5f, Color4F(1.0f, 0.5f, 0.1f, 0.85f));
    }
}

// A burning raider counts down its burn and then explodes. The player's ship burns on its own timer.
void AlienFleet::updateBurning(float delta)
{
    if (_shipBurn > 0.0f) {
        _shipBurn = std::max(0.0f, _shipBurn - delta);
    }
    for (size_t i = 0; i < _aliens.size();) {
        Alien &a = _aliens[i];
        if (a.burn > 0.0f) {
            a.burn -= delta;
            if (a.burn <= 0.0f) {
                explodeAt(a.pos, 0.5f);
                _aliens.erase(_aliens.begin() + i);
                continue;
            }
        }
        ++i;
    }
}

// Tracking brackets on every raider in range (amber), with the locked one in red. Missiles, blasts and sparks are drawn here too.
void AlienFleet::drawEffects()
{
    for (int id : _tracked) {
        const int idx = indexOfAlien(id);
        if (idx < 0) {
            continue;
        }
        const Vec2 c = _aliens[idx].pos;
        const bool locked = (id == _lockId);
        const Color4F col = locked ? Color4F(1.0f, 0.2f, 0.2f, 1.0f) : Color4F(1.0f, 0.75f, 0.2f, 0.9f);
        const float h = 12.0f, L = 5.0f;
        const float xs[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
        const float ys[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
        for (int k = 0; k < 4; k++) {
            const Vec2 corner = c + Vec2(xs[k] * h, ys[k] * h);
            _fx->drawSegment(corner, corner - Vec2(xs[k] * L, 0.0f), 1.0f, col);
            _fx->drawSegment(corner, corner - Vec2(0.0f, ys[k] * L), 1.0f, col);
        }
    }

    // Burning raiders and the burning ship trail flames until their burn ends.
    for (const Alien &a : _aliens) {
        if (a.burn > 0.0f) {
            drawFlames(_fx, a.pos, _clock, 6.0f);
        }
    }
    if (_shipBurn > 0.0f) {
        drawFlames(_fx, shipLocal(), _clock, 10.0f);
    }

    for (const Missile &m : _missiles) {
        const Vec2 dir = m.vel.getNormalized();
        const bool lit = m.age >= kMissileBoostTime;
        const float tail = lit ? 7.0f : 3.0f;
        const Color4F flame = lit ? Color4F(1.0f, 0.6f, 0.2f, 0.9f) : Color4F(0.8f, 0.8f, 0.8f, 0.6f);
        // Once the slowdown ends the thrusters are cut, so there is no flame at all.
        if (m.age < kMissileBoostTime + kMissileDecelTime) {
            _fx->drawSegment(m.pos - dir * tail, m.pos, 1.0f, flame);
        }
        _fx->drawDot(m.pos, 2.0f, Color4F(1.0f, 1.0f, 0.9f, 1.0f));
    }

    for (const Blast &b : _blasts) {
        const float t = b.age / kBlastLife;
        // Flash: a white core that dies within a quarter of a second.
        if (b.age < 0.25f) {
            _fx->drawSolidCircle(b.pos, b.maxR * 0.5f, 0.0f, 24, Color4F(1.0f, 1.0f, 0.95f, 1.0f - b.age / 0.25f));
        }
        // Fireball: yellow-white cooling to dark red as it grows and fades.
        const float fr = b.maxR * (0.3f + 0.6f * std::min(1.0f, b.age / 0.4f));
        _fx->drawSolidCircle(b.pos, fr, 0.0f, 24, Color4F(1.0f, 0.55f - 0.4f * t, 0.1f, 0.85f * (1.0f - t)));
        // Two shock waves, the second following the first by 0.12 s, racing well past the fireball.
        for (int w = 0; w < 2; w++) {
            const float a = b.age - 0.12f * w;
            if (a <= 0.0f) {
                continue;
            }
            const float r = b.maxR * 3.0f * std::min(1.0f, a / 0.9f);
            const float alpha = std::max(0.0f, 1.0f - a / kBlastLife);
            const Color4F ring = (w == 0) ? Color4F(0.85f, 0.95f, 1.0f, alpha) : Color4F(1.0f, 0.7f, 0.4f, 0.8f * alpha);
            drawRing(_fx, b.pos, r, ring);
        }
    }

    for (const Spark &s : _sparks) {
        const float fade = std::min(1.0f, s.life);
        _fx->drawSegment(s.pos - s.vel * 0.05f, s.pos, 1.2f, Color4F(1.0f, 0.8f * fade, 0.3f * fade, fade));
    }
}

void AlienFleet::drawFleet()
{
    _fx->clear();
    const int n = 16;
    for (const Alien &a : _aliens) {
        const Vec2 c = a.pos;
        const Color4F hull = a.combat ? Color4F(0.75f, 0.3f, 0.3f, 1.0f) : Color4F(0.55f, 0.6f, 0.65f, 1.0f);
        Vec2 disc[n], dome[n];
        for (int i = 0; i < n; i++) {
            const float ang = (float)i / n * 2.0f * (float)M_PI;
            disc[i] = c + Vec2(cosf(ang) * 5.5f, sinf(ang) * 2.2f);
            dome[i] = c + Vec2(cosf(ang) * 2.6f, 1.0f + sinf(ang) * 1.6f);
        }
        _fx->drawSolidPoly(disc, n, hull);
        _fx->drawSolidPoly(dome, n, Color4F(0.5f, 0.95f, 0.6f, 0.85f));
        for (int k = -1; k <= 1; k++) {
            _fx->drawDot(c + Vec2(k * 3.5f, -1.2f), 0.7f, Color4F(1.0f, 0.9f, 0.3f, 1.0f));
        }
    }
    // Laser bolts are short red-orange streaks trailing behind their direction of travel.
    for (const Bolt &b : _bolts) {
        _fx->drawSegment(b.pos - b.vel.getNormalized() * 10.0f, b.pos, 0.8f, Color4F(1.0f, 0.3f, 0.2f, 1.0f));
    }
}
