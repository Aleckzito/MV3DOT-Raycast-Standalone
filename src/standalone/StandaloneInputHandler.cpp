#include "StandaloneInputHandler.h"

#include "DummyActor.h"
#include "LocalChunkMesher.h"
#include "MiniVoxelGrid.h"
#include "PhantomCursor.h"
#include "StandaloneEngine.h"
#include "StandaloneWorldIO.h"

#include "DataRoot.h"

#include <iostream>

namespace rc {
namespace standalone {

StandaloneInputHandler::StandaloneInputHandler()
    : m_grid(nullptr)
    , m_mesher(nullptr)
    , m_cursor(nullptr)
    , m_dummy(nullptr)
    , m_engine(nullptr)
    , m_buildMaterial(1)
    , m_worldPath("data/worlds/standalone_sandbox.json")
    , m_keyUp(false)
    , m_keyDown(false)
    , m_keyLeft(false)
    , m_keyRight(false)
    , m_keyW(false)
    , m_keyA(false)
    , m_keyS(false)
    , m_keyD(false)
    , m_keySpace(false)
    , m_key1(false)
    , m_shiftHeld(false)
    , m_jumpLatched(false)
    , m_invertMove(false)
    , m_arrowOrbit(false)
    , m_lastMouseX(0.0f)
    , m_lastMouseY(0.0f)
    , m_hasMouse(false)
{
}

void StandaloneInputHandler::bind(MiniVoxelGrid* grid, LocalChunkMesher* mesher, PhantomCursor* cursor,
                                 DummyActor* dummy, StandaloneEngine* engine)
{
    m_grid = grid;
    m_mesher = mesher;
    m_cursor = cursor;
    m_dummy = dummy;
    m_engine = engine;
}

void StandaloneInputHandler::getMoveAxes(float& forward, float& strafe, float& turn) const
{
    forward = 0.0f;
    strafe = 0.0f;
    turn = 0.0f;
    if (m_keyW || (!m_arrowOrbit && m_keyUp)) {
        forward += 1.0f;
    }
    if (m_keyS || (!m_arrowOrbit && m_keyDown)) {
        forward -= 1.0f;
    }
    if (m_keyD) {
        strafe += 1.0f;
    }
    if (m_keyA) {
        strafe -= 1.0f;
    }
    if (!m_arrowOrbit) {
        if (m_keyLeft) {
            turn += 1.0f;
        }
        if (m_keyRight) {
            turn -= 1.0f;
        }
    }
    if (m_invertMove) {
        forward = -forward;
        strafe = -strafe;
        turn = -turn;
    }
}

void StandaloneInputHandler::getOrbitAxes(float& yaw, float& pitch) const
{
    yaw = 0.0f;
    pitch = 0.0f;
    if (!m_arrowOrbit) {
        return;
    }
    if (m_keyLeft) {
        yaw += 1.0f;
    }
    if (m_keyRight) {
        yaw -= 1.0f;
    }
    if (m_keyUp) {
        pitch += 1.0f;
    }
    if (m_keyDown) {
        pitch -= 1.0f;
    }
}

bool StandaloneInputHandler::isJumpPressed()
{
    const bool pressed = m_jumpLatched;
    m_jumpLatched = false;
    return pressed;
}

void StandaloneInputHandler::setKeyState(int key, bool down)
{
    if (key == osgGA::GUIEventAdapter::KEY_Up) {
        m_keyUp = down;
    } else if (key == osgGA::GUIEventAdapter::KEY_Down) {
        m_keyDown = down;
    } else if (key == osgGA::GUIEventAdapter::KEY_Left) {
        m_keyLeft = down;
    } else if (key == osgGA::GUIEventAdapter::KEY_Right) {
        m_keyRight = down;
    } else if (key == osgGA::GUIEventAdapter::KEY_W || key == 'w' || key == 'W') {
        m_keyW = down;
    } else if (key == osgGA::GUIEventAdapter::KEY_A || key == 'a' || key == 'A') {
        m_keyA = down;
    } else if (key == osgGA::GUIEventAdapter::KEY_S || key == 's' || key == 'S') {
        m_keyS = down;
    } else if (key == osgGA::GUIEventAdapter::KEY_D || key == 'd' || key == 'D') {
        m_keyD = down;
    } else if (key == osgGA::GUIEventAdapter::KEY_Shift_L ||
               key == osgGA::GUIEventAdapter::KEY_Shift_R) {
        m_shiftHeld = down;
    } else if (key == '1' || key == osgGA::GUIEventAdapter::KEY_KP_1) {
        if (down && !m_key1 && !m_shiftHeld) {
            m_jumpLatched = true;
        }
        m_key1 = down;
    } else if (key == osgGA::GUIEventAdapter::KEY_Space) {
        m_keySpace = down;
    }
}

bool StandaloneInputHandler::handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter&)
{
    if (ea.getEventType() == osgGA::GUIEventAdapter::KEYUP) {
        setKeyState(ea.getKey(), false);
        return false;
    }

    if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN) {
        const int key = ea.getKey();
        setKeyState(key, true);

        if (key == osgGA::GUIEventAdapter::KEY_F2) {
            if (m_engine != nullptr) {
                m_engine->setEditorMode(!m_engine->isEditorMode());
            }
            return true;
        }
        if (key == osgGA::GUIEventAdapter::KEY_Tab || key == '\t') {
            // 108. TAB = lock-on hostil. SHIFT+TAB = cubos/rocas. Nunca camara ni editor.
            const bool shift = m_shiftHeld ||
                ((ea.getModKeyMask() & osgGA::GUIEventAdapter::MODKEY_SHIFT) != 0);
            if (m_engine != nullptr) {
                if (shift) {
                    m_engine->cycleBoxTarget();
                } else {
                    m_engine->cycleEnemyTarget();
                }
            }
            return true;
        }
        {
            const int raw = ea.getUnmodifiedKey();
            const bool shift = m_shiftHeld ||
                ((ea.getModKeyMask() & osgGA::GUIEventAdapter::MODKEY_SHIFT) != 0);
            const bool is1 = (key == '1' || key == osgGA::GUIEventAdapter::KEY_KP_1 ||
                              raw == '1' || raw == osgGA::GUIEventAdapter::KEY_KP_1);
            const bool is2 = (key == '2' || key == osgGA::GUIEventAdapter::KEY_KP_2 ||
                              raw == '2' || raw == osgGA::GUIEventAdapter::KEY_KP_2);
            const bool is3 = (key == '3' || key == osgGA::GUIEventAdapter::KEY_KP_3 ||
                              raw == '3' || raw == osgGA::GUIEventAdapter::KEY_KP_3);
            const bool is4 = (key == '4' || key == osgGA::GUIEventAdapter::KEY_KP_4 ||
                              raw == '4' || raw == osgGA::GUIEventAdapter::KEY_KP_4);
            const bool is5 = (key == '5' || key == osgGA::GUIEventAdapter::KEY_KP_5 ||
                              raw == '5' || raw == osgGA::GUIEventAdapter::KEY_KP_5);
            if (is1 || is2 || is3 || is4 || is5) {
                if (m_engine != nullptr && !m_engine->isEditorMode()) {
                    if (shift) {
                        if (is1) {
                            m_engine->placeBox();
                        } else if (is2) {
                            if (!m_engine->tryPushBoulder()) {
                                if (m_engine->hasBoxSelection()) {
                                    m_engine->throwSelectedBlock();
                                } else {
                                    m_engine->tryPushBox();
                                }
                            }
                        } else if (is3) {
                            m_engine->tryCollapse();
                        } else if (is4) {
                            m_engine->hasteBurst();
                        }
                    } else if (is2) {
                        m_engine->fireProjectile();
                    } else if (is3) {
                        m_engine->performMeleeAttack();
                    } else if (is4) {
                        m_engine->triggerBomb3x3();
                    } else if (is5) {
                        m_engine->fireHunterLaser();
                    }
                }
                return true;
            }
        }
        if (key == osgGA::GUIEventAdapter::KEY_F5) {
            saveSandbox();
            return true;
        }
        if (key == osgGA::GUIEventAdapter::KEY_F9) {
            // 11.5 Recarga en caliente del mapa JSON. Sin recompilar.
            loadSandbox();
            return true;
        }
        if (key == osgGA::GUIEventAdapter::KEY_C || key == 'c' || key == 'C') {
            if (m_engine != nullptr && !m_engine->isEditorMode()) {
                m_engine->cycleCamera();
            }
            return true;
        }
        return false;
    }

    if (ea.getEventType() == osgGA::GUIEventAdapter::MOVE ||
        ea.getEventType() == osgGA::GUIEventAdapter::DRAG) {
        const float mx = ea.getX();
        const float my = ea.getY();
        const bool orbit = (m_engine != nullptr && !m_engine->isEditorMode() &&
                            m_engine->isOrbitCamera());
        if (orbit && m_hasMouse) {
            m_engine->applyOrbitMouse(mx - m_lastMouseX, my - m_lastMouseY);
        }
        m_lastMouseX = mx;
        m_lastMouseY = my;
        m_hasMouse = true;
        return orbit;
    }

    if (ea.getEventType() != osgGA::GUIEventAdapter::PUSH) {
        return false;
    }
    if (m_grid == nullptr || m_mesher == nullptr || m_cursor == nullptr) {
        return false;
    }

    const unsigned int button = ea.getButton();
    if (button == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON) {
        if (m_engine != nullptr && !m_engine->isEditorMode()) {
            m_engine->fireProjectile();
            return true;
        }
        buildAtCursor();
        return true;
    }
    if (button == osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON || button == 3u) {
        destroyAtCursor();
        return true;
    }
    return false;
}

void StandaloneInputHandler::buildAtCursor()
{
    if (!m_cursor->hasPose()) {
        return;
    }
    const SnappedPosition pose = m_cursor->currentPose();
    if (pose.vy < 0) {
        return;
    }
    if (m_grid->getVoxel(pose.vx, pose.vy, pose.vz).isActive) {
        return;
    }

    m_grid->setVoxel(pose.vx, pose.vy, pose.vz, m_buildMaterial);
    m_mesher->rebuildMesh();
    m_cursor->resetDda();
    std::cout << "[build] voxel (" << pose.vx << ", " << pose.vy << ", " << pose.vz << ")\n";
}

void StandaloneInputHandler::destroyAtCursor()
{
    int vx = 0;
    int vy = 0;
    int vz = 0;
    bool haveTarget = false;

    if (m_cursor->hasHitSolid() && m_cursor->hitVy() >= 0) {
        vx = m_cursor->hitVx();
        vy = m_cursor->hitVy();
        vz = m_cursor->hitVz();
        haveTarget = true;
    } else if (m_cursor->hasPose()) {
        const SnappedPosition pose = m_cursor->currentPose();
        vx = pose.vx;
        vy = pose.vy;
        vz = pose.vz;
        haveTarget = true;
    }
    if (!haveTarget) {
        return;
    }
    if (!m_grid->getVoxel(vx, vy, vz).isActive) {
        return;
    }

    m_grid->setVoxel(vx, vy, vz, 0);
    m_mesher->rebuildMesh();
    m_cursor->resetDda();
    std::cout << "[destroy] voxel (" << vx << ", " << vy << ", " << vz << ")\n";
}

std::string StandaloneInputHandler::worldPath() const
{
    // 11.5 F5/F9 escriben y leen el mismo archivo que cargo el motor,
    // no una ruta relativa al CWD.
    if (m_engine != nullptr && !m_engine->worldJsonPath().empty()) {
        return m_engine->worldJsonPath();
    }
    return dataPath(m_worldPath);
}

void StandaloneInputHandler::saveSandbox()
{
    if (m_grid == nullptr) {
        return;
    }
    saveWorld(m_grid, worldPath());
}

void StandaloneInputHandler::loadSandbox()
{
    if (m_engine != nullptr) {
        m_engine->reloadWorldJson();
        return;
    }
    if (m_grid == nullptr || m_mesher == nullptr) {
        return;
    }
    if (!loadWorld(m_grid, worldPath())) {
        return;
    }
    m_mesher->rebuildMesh();
    if (m_cursor != nullptr) {
        m_cursor->resetDda();
    }
}

void StandaloneInputHandler::tossDummy()
{
    if (m_dummy == nullptr) {
        return;
    }
    m_dummy->teleport(4.0f, 5.0f, 4.0f);
    std::cout << "[dummy] toss y=5 center\n";
}

} // namespace standalone
} // namespace rc
