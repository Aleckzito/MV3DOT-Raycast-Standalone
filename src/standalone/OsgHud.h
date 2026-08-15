#ifndef RC_OSG_HUD_H
#define RC_OSG_HUD_H

#include <osg/Camera>
#include <osg/GraphicsContext>
#include <osg/Node>
#include <osg/Vec4>
#include <osg/ref_ptr>
#include <osgText/Text>

#include <string>

namespace rc {
namespace standalone {

// 36 / 42 / 45 / 46 / 49. Overlay 2D: HP + SP + kills + LVL + time.
class OsgHud {
public:
    OsgHud();

    bool create(int width, int height, osg::GraphicsContext* gc);
    osg::Node* getNode();
    void update(int playerHp, int playerMaxHp, float stamina,
                bool hasTarget, int targetHp, int targetMaxHp, int killCount,
                float survivalTime, int level, int exp, int expToNext,
                const std::string& compass, const std::string& camLabel,
                const osg::Vec4& camColor, int energyCells, const std::string& alertBanner);

private:
    osg::ref_ptr<osg::Camera> m_camera;
    osg::ref_ptr<osgText::Text> m_playerHpText;
    osg::ref_ptr<osgText::Text> m_staminaText;
    osg::ref_ptr<osgText::Text> m_targetHpText;
    osg::ref_ptr<osgText::Text> m_killText;
    osg::ref_ptr<osgText::Text> m_levelText;
    osg::ref_ptr<osgText::Text> m_timeText;
    osg::ref_ptr<osgText::Text> m_compassText;
    osg::ref_ptr<osgText::Text> m_camText;
    osg::ref_ptr<osgText::Text> m_energyText;
    osg::ref_ptr<osgText::Text> m_alertText;
    osg::ref_ptr<osgText::Text> m_hotbarText;
};

} // namespace standalone
} // namespace rc

#endif
