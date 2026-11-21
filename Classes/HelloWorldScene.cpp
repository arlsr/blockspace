#include "HelloWorldScene.h"
#include <sstream>

USING_NS_CC;

Scene* HelloWorld::createScene()
{
    // 'scene' is an autorelease object
    auto scene = Scene::create();
    
    // 'layer' is an autorelease object
    auto layer = HelloWorld::create();

    // add layer as a child to scene
    scene->addChild(layer);

    // return the scene
    return scene;
}

// on "init" you need to initialize your instance
bool HelloWorld::init()
{
    ///////////////////////////////
    // 1. super init first
    if ( !Layer::init() )
    {
        return false;
    }
    
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    /////////////////////////////
    // 2. add a menu item with "X" image, which is clicked to quit the program
    //    you may modify it.

    // add a "close" icon to exit the progress. it's an autorelease object
    auto closeItem = MenuItemImage::create(
                                           "CloseNormal.png",
                                           "CloseSelected.png",
                                           CC_CALLBACK_1(HelloWorld::menuCloseCallback, this));
    
	closeItem->setPosition(Vec2(origin.x + visibleSize.width - closeItem->getContentSize().width/2 ,
                                origin.y + closeItem->getContentSize().height/2));

    // create menu, it's an autorelease object
    auto menu = Menu::create(closeItem, NULL);
    menu->setPosition(Vec2::ZERO);
    this->addChild(menu, 1);
    
    auto label = Label::createWithTTF("Blockspace", "fonts/Marker Felt.ttf", 24);
    
    // position the label on the center of the screen
    label->setPosition(Vec2(origin.x + visibleSize.width/2,
                            origin.y + visibleSize.height - label->getContentSize().height));

	label->setColor(cocos2d::Color3B(190, 230, 240));
    // add the label as a child to this layer
    this->addChild(label, 1);

	// Captain's console: a dark panel with cyan trim along the bottom centre of the screen.
	const float consoleW = 340.0f, consoleH = 56.0f;
	const float cx = origin.x + visibleSize.width / 2, baseY = origin.y + 8.0f;
	auto console = DrawNode::create();
	Vec2 p1(cx - consoleW / 2, baseY), p2(cx + consoleW / 2, baseY + consoleH);
	for (int i = 0; i < 14; ++i) {
		float t = i / 14.0f;
		float y0 = p1.y + consoleH * t;
		console->drawSolidRect(Vec2(p1.x, y0), Vec2(p2.x, y0 + consoleH / 14.0f), Color4F(0.12f + 0.20f * t, 0.14f + 0.21f * t, 0.17f + 0.22f * t, 1.0f));
	}
	console->drawRect(p1, p2, Color4F(0.03f, 0.04f, 0.05f, 1.0f));
	console->drawLine(Vec2(p1.x + 1, p2.y - 1), Vec2(p2.x - 1, p2.y - 1), Color4F(0.75f, 0.8f, 0.85f, 0.8f));
	console->drawLine(Vec2(p1.x + 1, p1.y + 1), Vec2(p2.x - 1, p1.y + 1), Color4F(0.02f, 0.02f, 0.03f, 1.0f));
	Vec2 screws[4] = { Vec2(p1.x + 5, p1.y + 5), Vec2(p2.x - 5, p1.y + 5), Vec2(p1.x + 5, p2.y - 5), Vec2(p2.x - 5, p2.y - 5) };
	for (const Vec2 &s : screws) {
		console->drawSolidCircle(s, 2.5f, 0, 10, Color4F(0.45f, 0.47f, 0.5f, 1.0f));
		console->drawLine(Vec2(s.x - 2, s.y - 1), Vec2(s.x + 2, s.y + 1), Color4F(0.1f, 0.1f, 0.12f, 1.0f));
	}
	
	this->addChild(console, 1);

	const float boxW = 96.0f, boxH = 30.0f, boxY = baseY + 12.0f;
	const char *names[3] = { "X", "Y", "SPEED" };
	Label *readouts[3];
	for (int i = 0; i < 3; ++i) {
		float bx = p1.x + 14.0f + i * (boxW + 12.0f);
		Vec2 b1(bx, boxY), b2(bx + boxW, boxY + boxH);
		console->drawSolidRect(b1, b2, Color4F(0.01f, 0.02f, 0.03f, 1.0f));
		console->drawRect(b1, b2, Color4F(0.45f, 0.47f, 0.50f, 1.0f));
		console->drawRect(Vec2(b1.x + 2, b1.y + 2), Vec2(b2.x - 2, b2.y - 2), Color4F(0.3f, 0.9f, 1.0f, 0.35f));
		Vec2 corners[4] = { Vec2(b1.x + 4, b1.y + 4), Vec2(b2.x - 4, b1.y + 4), Vec2(b1.x + 4, b2.y - 4), Vec2(b2.x - 4, b2.y - 4) };
		for (const Vec2 &c : corners)
			console->drawSolidCircle(c, 1.5f, 0, 8, Color4F(0.6f, 0.62f, 0.65f, 1.0f));
		auto name = Label::createWithTTF(names[i], "fonts/ShareTechMono-Regular.ttf", 8);
		name->setColor(cocos2d::Color3B(230, 170, 90));
		name->setPosition(bx + boxW / 2, boxY + boxH + 7.0f);
		this->addChild(name, 2);
		readouts[i] = Label::createWithTTF("0", "fonts/ShareTechMono-Regular.ttf", 14);
		readouts[i]->setColor(cocos2d::Color3B(120, 240, 255));
		readouts[i]->setPosition(bx + boxW / 2, boxY + boxH / 2);
		this->addChild(readouts[i], 2);
	}
	this->lblCoords = readouts[0];
	auto serial = Label::createWithTTF("S/N BS-7731-C  MK IV", "fonts/ShareTechMono-Regular.ttf", 7);
	serial->setAnchorPoint(Vec2(1.0f, 0.0f));
	serial->setColor(cocos2d::Color3B(150, 160, 170));
	serial->setPosition(p2.x - 14.0f, p1.y + 2.0f);
	this->addChild(serial, 2);
	// Readouts were positioned above.
	// Readouts were added above.

	// Three star layers at different depths. Each scrolls at a different fraction of the ship's movement.
	auto farStars = DrawNode::create();
	auto midStars = DrawNode::create();
	auto nearStars = DrawNode::create();
	DrawNode* layers[3] = { farStars, midStars, nearStars };
	const int layerStars[3] = { 120, 80, 40 };
	const float layerRadius[3] = { 0.4f, 0.7f, 1.1f };
	const int tile = Ship::STAR_TILE_SIZE;
	for (int layer = 0; layer < 3; ++layer) {
		for (int i = 0; i < layerStars[layer]; ++i) {
			float x = (float)(rand() % tile);
			float y = (float)(rand() % tile);
			float radius = layerRadius[layer] * (0.5f + (rand() % 100) / 100.0f);
			float brightness = 0.5f + (rand() % 50) / 100.0f;
			for (int tx = 0; tx < 3; ++tx) {
				for (int ty = 0; ty < 3; ++ty) {
					layers[layer]->drawDot(Vec2(x + tx * tile, y + ty * tile), radius, Color4F(1.0f, 1.0f, 1.0f, brightness));
				}
			}
		}
		this->addChild(layers[layer], layer - 4);
	}

	// Earth scaled to a 100-mile ship (1 px = 6,706 m): radius 950 px, drawn in world coordinates.
	auto planet = DrawNode::create();
	planet->drawSolidCircle(Vec2::ZERO, Ship::PLANET_RADIUS + 20, 0, 180, Color4F(0.5f, 0.8f, 1.0f, 0.25f));
	planet->drawSolidCircle(Vec2::ZERO, Ship::PLANET_RADIUS, 0, 360, Color4F(0.1f, 0.35f, 0.7f, 1.0f));
	// Real country outlines, projected onto the visible hemisphere by /tmp/make_earth.py.
	// Coordinates are in units of the planet radius.
	std::string mapData = FileUtils::getInstance()->getStringFromFile("earth_map.txt");
	std::istringstream mapStream(mapData);
	std::string kind;
	const float R = (float)Ship::PLANET_RADIUS;
	while (mapStream >> kind) {
		if (kind == "T") {
			float r, g, b, x1, y1, x2, y2, x3, y3;
			mapStream >> r >> g >> b >> x1 >> y1 >> x2 >> y2 >> x3 >> y3;
			planet->drawTriangle(Vec2(x1 * R, y1 * R), Vec2(x2 * R, y2 * R), Vec2(x3 * R, y3 * R), Color4F(r, g, b, 1.0f));
		}
		
	}
	
	
	
	
	this->addChild(planet, -1);

	auto ship = new Ship(this->lblCoords, farStars, midStars, nearStars, planet);
    ship->setReadouts(readouts[0], readouts[1], readouts[2]);

    // position the sprite on the center of the screen
    ship->setPosition(Vec2(visibleSize.width/2 + origin.x, visibleSize.height/2 + origin.y));

    // add the sprite as a child to this layer
    this->addChild(ship, 0);
    
    return true;
}


void HelloWorld::menuCloseCallback(Ref* pSender)
{
    Director::getInstance()->end();

#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    exit(0);
#endif
}
