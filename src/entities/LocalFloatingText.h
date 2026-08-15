#ifndef RC_LOCAL_FLOATING_TEXT_H
#define RC_LOCAL_FLOATING_TEXT_H

#include <osg/Geode>
#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/ref_ptr>
#include <osgText/Text>

#include <string>

namespace rc {
namespace standalone {

// 62. Popup 3D de combate. Billboard + fade. Cero red.
class LocalFloatingText {
public:
    LocalFloatingText();

    osg::Node* getNode();
    void syncVisual();
    void activate(const osg::Vec3& spawnPos, const osg::Vec3& velocity,
                  const std::string& msg, const osg::Vec4& color, float scale, float life);
    void deactivate();
    void applyAlpha(float alpha);

    osg::Vec3 pos;
    osg::Vec3 vel;
    float ttl;
    float maxTtl;
    bool isActive;
    osg::ref_ptr<osgText::Text> textNode;
    osg::ref_ptr<osg::Geode> geodeNode;

private:
    void buildVisual();

    osg::Vec4 m_baseColor;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
};

} // namespace standalone
} // namespace rc

#endif
