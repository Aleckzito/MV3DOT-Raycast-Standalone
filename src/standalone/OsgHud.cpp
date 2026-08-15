#include "OsgHud.h"

#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/GL>
#include <osg/Geode>
#include <osg/Matrix>
#include <osg/StateSet>
#include <osgText/Font>

#include <cstdio>
#include <string>

namespace rc {
namespace standalone {

namespace {

osgText::Font* loadHudFont()
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

OsgHud::OsgHud()
{
}

bool OsgHud::create(int width, int height, osg::GraphicsContext* gc)
{
    if (width < 1 || height < 1) {
        return false;
    }

    m_camera = new osg::Camera;
    if (gc != nullptr) {
        m_camera->setGraphicsContext(gc);
    }
    m_camera->setViewport(0, 0, width, height);
    m_camera->setClearMask(GL_DEPTH_BUFFER_BIT);
    m_camera->setRenderOrder(osg::Camera::POST_RENDER, 100);
    m_camera->setAllowEventFocus(false);
    m_camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    m_camera->setProjectionMatrix(osg::Matrix::ortho2D(0.0, width, 0.0, height));
    m_camera->setViewMatrix(osg::Matrix::identity());
    m_camera->setComputeNearFarMode(osg::Camera::DO_NOT_COMPUTE_NEAR_FAR);

    osg::StateSet* state = m_camera->getOrCreateStateSet();
    state->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    state->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    state->setMode(GL_BLEND, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    state->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    state->setAttributeAndModes(
        new osg::BlendFunc(osg::BlendFunc::SRC_ALPHA, osg::BlendFunc::ONE_MINUS_SRC_ALPHA),
        osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    state->setAttributeAndModes(new osg::Depth(osg::Depth::ALWAYS, 0.0, 1.0, false),
                                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

    osgText::Font* font = loadHudFont();

    m_playerHpText = new osgText::Text;
    if (font != nullptr) {
        m_playerHpText->setFont(font);
    }
    m_playerHpText->setCharacterSize(22.0f);
    m_playerHpText->setColor(osg::Vec4(0.25f, 0.95f, 0.35f, 1.0f));
    m_playerHpText->setAlignment(osgText::Text::LEFT_BOTTOM);
    m_playerHpText->setAxisAlignment(osgText::Text::SCREEN);
    m_playerHpText->setPosition(osg::Vec3(18.0f, 42.0f, 0.0f));
    m_playerHpText->setDataVariance(osg::Object::DYNAMIC);
    m_playerHpText->setText("HP: 100/100");

    m_staminaText = new osgText::Text;
    if (font != nullptr) {
        m_staminaText->setFont(font);
    }
    m_staminaText->setCharacterSize(18.0f);
    m_staminaText->setColor(osg::Vec4(1.0f, 1.0f, 0.0f, 1.0f));
    m_staminaText->setAlignment(osgText::Text::LEFT_BOTTOM);
    m_staminaText->setAxisAlignment(osgText::Text::SCREEN);
    m_staminaText->setPosition(osg::Vec3(18.0f, 18.0f, 0.0f));
    m_staminaText->setDataVariance(osg::Object::DYNAMIC);
    m_staminaText->setText("SP: 02:00:00");

    m_killText = new osgText::Text;
    if (font != nullptr) {
        m_killText->setFont(font);
    }
    m_killText->setCharacterSize(18.0f);
    m_killText->setColor(osg::Vec4(0.20f, 0.95f, 1.0f, 1.0f));
    m_killText->setAlignment(osgText::Text::LEFT_BOTTOM);
    m_killText->setAxisAlignment(osgText::Text::SCREEN);
    m_killText->setPosition(osg::Vec3(18.0f, 92.0f, 0.0f));
    m_killText->setDataVariance(osg::Object::DYNAMIC);
    m_killText->setText("Kills: 0");

    m_levelText = new osgText::Text;
    if (font != nullptr) {
        m_levelText->setFont(font);
    }
    m_levelText->setCharacterSize(18.0f);
    m_levelText->setColor(osg::Vec4(1.0f, 0.55f, 0.15f, 1.0f));
    m_levelText->setAlignment(osgText::Text::LEFT_BOTTOM);
    m_levelText->setAxisAlignment(osgText::Text::SCREEN);
    m_levelText->setPosition(osg::Vec3(18.0f, 68.0f, 0.0f));
    m_levelText->setDataVariance(osg::Object::DYNAMIC);
    m_levelText->setText("LVL: 1 (EXP: 0/100)");

    m_timeText = new osgText::Text;
    if (font != nullptr) {
        m_timeText->setFont(font);
    }
    m_timeText->setCharacterSize(20.0f);
    m_timeText->setColor(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f));
    m_timeText->setAlignment(osgText::Text::RIGHT_TOP);
    m_timeText->setAxisAlignment(osgText::Text::SCREEN);
    m_timeText->setPosition(osg::Vec3(static_cast<float>(width) - 18.0f - 276.0f,
                                      static_cast<float>(height) - 16.0f,
                                      0.0f));
    m_timeText->setDataVariance(osg::Object::DYNAMIC);
    m_timeText->setText("Time: 00:00:00");

    m_targetHpText = new osgText::Text;
    if (font != nullptr) {
        m_targetHpText->setFont(font);
    }
    m_targetHpText->setCharacterSize(20.0f);
    m_targetHpText->setColor(osg::Vec4(0.95f, 0.25f, 0.90f, 1.0f));
    m_targetHpText->setAlignment(osgText::Text::CENTER_TOP);
    m_targetHpText->setAxisAlignment(osgText::Text::SCREEN);
    m_targetHpText->setPosition(osg::Vec3(static_cast<float>(width) * 0.5f,
                                          static_cast<float>(height) - 64.0f,
                                          0.0f));
    m_targetHpText->setDataVariance(osg::Object::DYNAMIC);
    m_targetHpText->setText("");

    m_compassText = new osgText::Text;
    if (font != nullptr) {
        m_compassText->setFont(font);
    }
    m_compassText->setCharacterSize(22.0f);
    m_compassText->setColor(osg::Vec4(1.0f, 1.0f, 0.0f, 1.0f));
    m_compassText->setAlignment(osgText::Text::CENTER_TOP);
    m_compassText->setAxisAlignment(osgText::Text::SCREEN);
    m_compassText->setPosition(osg::Vec3(static_cast<float>(width) * 0.5f,
                                         static_cast<float>(height) - 16.0f,
                                         0.0f));
    m_compassText->setDataVariance(osg::Object::DYNAMIC);
    m_compassText->setText("Brujula: [N]");

    m_camText = new osgText::Text;
    if (font != nullptr) {
        m_camText->setFont(font);
    }
    m_camText->setCharacterSize(18.0f);
    m_camText->setColor(osg::Vec4(1.00f, 0.22f, 0.18f, 1.0f));
    m_camText->setAlignment(osgText::Text::CENTER_TOP);
    m_camText->setAxisAlignment(osgText::Text::SCREEN);
    m_camText->setPosition(osg::Vec3(static_cast<float>(width) * 0.5f,
                                     static_cast<float>(height) - 42.0f,
                                     0.0f));
    m_camText->setDataVariance(osg::Object::DYNAMIC);
    m_camText->setText("Cam: Brazo");

    m_energyText = new osgText::Text;
    if (font != nullptr) {
        m_energyText->setFont(font);
    }
    m_energyText->setCharacterSize(16.0f);
    m_energyText->setColor(osg::Vec4(0.25f, 0.95f, 1.00f, 1.0f));
    m_energyText->setAlignment(osgText::Text::RIGHT_BOTTOM);
    m_energyText->setAxisAlignment(osgText::Text::SCREEN);
    m_energyText->setPosition(osg::Vec3(static_cast<float>(width) - 18.0f, 18.0f, 0.0f));
    m_energyText->setDataVariance(osg::Object::DYNAMIC);
    m_energyText->setText("ENERGY: [        ] (x0)");

    m_alertText = new osgText::Text;
    if (font != nullptr) {
        m_alertText->setFont(font);
    }
    m_alertText->setCharacterSize(20.0f);
    m_alertText->setColor(osg::Vec4(1.00f, 0.88f, 0.12f, 1.0f));
    m_alertText->setAlignment(osgText::Text::CENTER_TOP);
    m_alertText->setAxisAlignment(osgText::Text::SCREEN);
    m_alertText->setPosition(osg::Vec3(static_cast<float>(width) * 0.5f,
                                       static_cast<float>(height) - 88.0f,
                                       0.0f));
    m_alertText->setDataVariance(osg::Object::DYNAMIC);
    m_alertText->setText("");

    m_hotbarText = new osgText::Text;
    if (font != nullptr) {
        m_hotbarText->setFont(font);
    }
    m_hotbarText->setCharacterSize(14.0f);
    m_hotbarText->setColor(osg::Vec4(0.95f, 0.95f, 0.70f, 1.0f));
    m_hotbarText->setAlignment(osgText::Text::CENTER_BOTTOM);
    m_hotbarText->setAxisAlignment(osgText::Text::SCREEN);
    m_hotbarText->setPosition(osg::Vec3(static_cast<float>(width) * 0.5f, 6.0f, 0.0f));
    m_hotbarText->setDataVariance(osg::Object::DYNAMIC);
    m_hotbarText->setText("[1] Jump | [2] Gun (Auto) | [3] Melee | [4] Bomb | [5] Laser");

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(m_playerHpText.get());
    geode->addDrawable(m_staminaText.get());
    geode->addDrawable(m_killText.get());
    geode->addDrawable(m_levelText.get());
    geode->addDrawable(m_timeText.get());
    geode->addDrawable(m_targetHpText.get());
    geode->addDrawable(m_compassText.get());
    geode->addDrawable(m_camText.get());
    geode->addDrawable(m_energyText.get());
    geode->addDrawable(m_alertText.get());
    geode->addDrawable(m_hotbarText.get());
    m_camera->addChild(geode.get());
    return true;
}

osg::Node* OsgHud::getNode()
{
    return m_camera.get();
}

void OsgHud::update(int playerHp, int playerMaxHp, float stamina,
                    bool hasTarget, int targetHp, int targetMaxHp, int killCount,
                    float survivalTime, int level, int exp, int expToNext,
                    const std::string& compass, const std::string& camLabel,
                    const osg::Vec4& camColor, int energyCells, const std::string& alertBanner)
{
    if (m_playerHpText.valid()) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "HP: %d/%d", playerHp, playerMaxHp);
        m_playerHpText->setText(buf);
    }
    if (m_staminaText.valid()) {
        int totalSecs = static_cast<int>(stamina);
        if (totalSecs < 0) {
            totalSecs = 0;
        }
        const int h = totalSecs / 3600;
        const int m = (totalSecs % 3600) / 60;
        const int s = totalSecs % 60;
        char buf[48];
        std::snprintf(buf, sizeof(buf), "SP: %02d:%02d:%02d", h, m, s);
        m_staminaText->setText(buf);
    }
    if (m_killText.valid()) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "Kills: %d", killCount);
        m_killText->setText(buf);
    }
    if (m_levelText.valid()) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "LVL: %d (EXP: %d/%d)", level, exp, expToNext);
        m_levelText->setText(buf);
    }
    if (m_timeText.valid()) {
        int totalSecs = static_cast<int>(survivalTime);
        if (totalSecs < 0) {
            totalSecs = 0;
        }
        const int h = totalSecs / 3600;
        const int m = (totalSecs % 3600) / 60;
        const int s = totalSecs % 60;
        char buf[48];
        std::snprintf(buf, sizeof(buf), "Time: %02d:%02d:%02d", h, m, s);
        m_timeText->setText(buf);
    }
    if (m_compassText.valid()) {
        m_compassText->setText(std::string("Brujula: [") + compass + "]");
    }
    if (m_camText.valid()) {
        m_camText->setText(std::string("Cam: ") + camLabel);
        m_camText->setColor(camColor);
    }
    if (m_energyText.valid()) {
        int cells = energyCells;
        if (cells < 0) {
            cells = 0;
        }
        int filled = cells;
        if (filled > 8) {
            filled = 8;
        }
        char bar[9];
        for (int i = 0; i < 8; ++i) {
            bar[i] = (i < filled) ? '|' : ' ';
        }
        bar[8] = '\0';
        char buf[48];
        std::snprintf(buf, sizeof(buf), "ENERGY: [%s] (x%d)", bar, cells);
        m_energyText->setText(buf);
    }
    if (m_alertText.valid()) {
        m_alertText->setText(alertBanner);
    }
    if (!m_targetHpText.valid()) {
        return;
    }
    if (!hasTarget) {
        m_targetHpText->setText("");
        return;
    }
    char buf[48];
    std::snprintf(buf, sizeof(buf), "Target HP: %d/%d", targetHp, targetMaxHp);
    m_targetHpText->setText(buf);
}

} // namespace standalone
} // namespace rc
