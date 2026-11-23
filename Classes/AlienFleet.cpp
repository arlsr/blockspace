#include "AlienFleet.h"
#include "base/CCDirector.h"
#include "2d/CCLabel.h"
#include "Ship.h"
#include "platform/CCFileUtils.h"
#include <new>
#include <sstream>
#include <cmath>

USING_NS_CC;

namespace {
// Bolts that miss their target stop after this many screen heights of flight (2.0 * _screenH).
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
    drawFleet();

    // Steam is white, spreads as it ages and fades to nothing over 2 s. Drawn after drawFleet, which clears _fx.
    for (const Puff &s : _steam) {
        const float age = 2.0f - s.life;
        _fx->drawSolidCircle(s.pos, 2.0f + 3.0f * age, 0.0f, 16, Color4F(1.0f, 1.0f, 1.0f, 0.8f * s.life / 2.0f));
    }
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
