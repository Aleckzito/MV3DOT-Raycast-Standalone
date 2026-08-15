#include "LocalFloatingText.h"

#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/GL>
#include <osg/StateSet>
#include <osgText/Font>

namespace rc {
namespace standalone {

namespace {

osgText::Font* loadFloatFont()
{
    static osg::ref_ptr<osgText::Font> font;
    if (font.valid()) {
        return font.get();
    }
    const char* candidates[] = {
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/lucon.ttf",
    };
    for (int i = 0; i < 3; ++i) {
        font = osgText::readFontFile(candidates[i]);
        if (font.valid()) {
            return font.get();
        }
    }
    return nullptr;
}

} // namespace

LocalFloatingText::LocalFloatingText()
    : pos(0.0f, 0.0f, 0.0f)
    , vel(0.0f, 0.8f, 0.0f)
    , ttl(0.0f)
    , maxTtl(0.8f)
    , isActive(false)
    , m_baseColor(1.0f, 1.0f, 1.0f, 1.0f)
{
    buildVisual();
}

void LocalFloatingText::buildVisual()
{
    textNode = new osgText::Text;
    osgText::Font* font = loadFloatFont();
    if (font != nullptr) {
        textNode->setFont(font);
    }
    textNode->setText("");
    textNode->setCharacterSize(0.12f);
    textNode->setCharacterSizeMode(osgText::Text::OBJECT_COORDS);
    textNode->setAxisAlignment(osgText::Text::SCREEN);
    textNode->setAlignment(osgText::Text::CENTER_CENTER);
    textNode->setColor(m_baseColor);
    textNode->setBackdropType(osgText::Text::NONE);

    geodeNode = new osg::Geode;
    geodeNode->addDrawable(textNode.get());

    osg::StateSet* state = geodeNode->getOrCreateStateSet();
    state->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    state->setMode(GL_BLEND, osg::StateAttribute::ON);
    state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    state->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON);
    // 65.3 No escribe depth: el popup no ocluye el mundo.
    state->setAttributeAndModes(
        new osg::Depth(osg::Depth::ALWAYS, 0.0, 1.0, false),
        osg::StateAttribute::ON);

    m_pat = new osg::PositionAttitudeTransform;
    m_pat->addChild(geodeNode.get());
    syncVisual();
}

osg::Node* LocalFloatingText::getNode()
{
    return m_pat.get();
}

void LocalFloatingText::syncVisual()
{
    if (m_pat.valid()) {
        m_pat->setPosition(pos);
        m_pat->setNodeMask(isActive ? 0xffffffff : 0);
    }
}

void LocalFloatingText::applyAlpha(float alpha)
{
    if (!textNode.valid()) {
        return;
    }
    if (alpha < 0.0f) {
        alpha = 0.0f;
    } else if (alpha > 1.0f) {
        alpha = 1.0f;
    }
    osg::Vec4 c = m_baseColor;
    c.a() = m_baseColor.a() * alpha;
    textNode->setColor(c);
}

void LocalFloatingText::activate(const osg::Vec3& spawnPos, const osg::Vec3& velocity,
                                 const std::string& msg, const osg::Vec4& color, float scale, float life)
{
    pos = spawnPos;
    vel = velocity;
    ttl = life;
    maxTtl = life;
    isActive = true;
    m_baseColor = color;
    if (textNode.valid()) {
        textNode->setText(msg);
        textNode->setCharacterSize(scale);
        textNode->setColor(color);
    }
    applyAlpha(1.0f);
    syncVisual();
}

void LocalFloatingText::deactivate()
{
    isActive = false;
    ttl = 0.0f;
    if (textNode.valid()) {
        textNode->setText("");
    }
    syncVisual();
}

} // namespace standalone
} // namespace rc
