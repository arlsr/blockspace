#pragma once

#include <new>
#include <vector>
#include "2d/CCDrawNode.h"

// DrawNode that takes a colour at each vertex, so colours blend across the planet.
class EarthNode : public cocos2d::DrawNode
{
public:
    static EarthNode* create()
    {
        auto ret = new (std::nothrow) EarthNode();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    // Appends triangles (three vertices each) to the draw buffer in one update.
    void addColouredTriangles(const std::vector<cocos2d::V2F_C4B_T2F>& verts)
    {
        const int n = (int)verts.size();
        if (n == 0) {
            return;
        }
        ensureCapacity(n);
        for (int i = 0; i < n; i++) {
            _buffer[_bufferCount + i] = verts[i];
        }
        _customCommand.updateVertexBuffer(_buffer + _bufferCount, _bufferCount * sizeof(cocos2d::V2F_C4B_T2F), n * sizeof(cocos2d::V2F_C4B_T2F));
        _bufferCount += n;
        _dirty = true;
        _customCommand.setVertexDrawInfo(0, _bufferCount);
    }
};
