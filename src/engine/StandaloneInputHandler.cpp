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

namespace {

// 124. Frames de desplazamiento lateral por muesca de rueda. La rueda no se
// mantiene pulsada como A/D, y la fisica recalcula la velocidad cada frame
// desde los ejes, asi que un empujon suelto no sobreviviria: se sostiene el
// eje unos frames. A 60 fps, 10 frames son ~0.17 s de strafe.
const int kScrollStrafeFrames = 10;
// 124.2 Movimiento del puntero tratado como las flechas del teclado. Cada
// evento renueva el contador, asi que mientras la bola siga rodando el eje se
// mantiene, y se apaga solo al parar. El umbral evita que el temblor de la mano
// dispare movimiento.
const int kPointerFrames = 8;
const float kPointerDeadZone = 1.5f;
// Botones laterales. OSG no los declara: en Windows llegan como 4 y 5 si el
// backend los reenvia, y si no, simplemente no se disparan.
const unsigned int kMouseButtonBack = 4u;
const unsigned int kMouseButtonForward = 5u;

} // namespace

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
    , m_rightControlDown(false)
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
    // 124. Rueda del raton: mismo eje que A/D, sostenido unos frames por muesca.
    if (m_scrollStrafeFrames > 0) {
        strafe += m_scrollStrafeDir;
        m_scrollStrafeFrames -= 1;
    }
    // 124.2 Puntero del raton: mismos ejes que las flechas del teclado.
    if (m_pointerForwardFrames > 0) {
        forward += m_pointerForwardDir;
        m_pointerForwardFrames -= 1;
    }
    if (!m_arrowOrbit && m_pointerTurnFrames > 0) {
        turn += m_pointerTurnDir;
        m_pointerTurnFrames -= 1;
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

bool StandaloneInputHandler::handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
{
    if (ea.getEventType() == osgGA::GUIEventAdapter::KEYUP) {
        if (ea.getKey() == osgGA::GUIEventAdapter::KEY_Control_R) {
            m_rightControlDown = false;
        }
        setKeyState(ea.getKey(), false);
        return false;
    }

    if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN) {
        const int key = ea.getKey();
        if (m_engine != nullptr && !m_engine->hasInputFocus()) {
            clearKeys();
            return true;
        }
        setKeyState(key, true);

        // 125. Ctrl derecho libera, como la tecla Host de VirtualBox. Se
        // ignoran repeticiones automaticas hasta que llegue KEYUP.
        if (key == osgGA::GUIEventAdapter::KEY_Control_R) {
            if (!m_rightControlDown && m_engine != nullptr && m_engine->isMouseCaptured()) {
                m_engine->setMouseCaptured(false);
            }
            m_rightControlDown = true;
            return true;
        }
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
        // 125. Con el raton capturado el delta se mide contra el centro y el
        // puntero vuelve ahi: asi la bola rueda sin llegar nunca a un borde.
        const bool captured = (m_engine != nullptr && m_engine->isMouseCaptured());
        float centerX = 0.0f;
        float centerY = 0.0f;
        if (captured) {
            // getX/getY ya vienen en coordenadas locales del area cliente.
            // Sumar getWindowX/getWindowY (posicion en el escritorio) dejaba
            // un delta constante y hacia girar al personaje indefinidamente.
            centerX = 0.5f * static_cast<float>(ea.getWindowWidth());
            centerY = 0.5f * static_cast<float>(ea.getWindowHeight());
            // El propio recentrado genera otro MOVE; si ya esta en el centro,
            // es ese eco y no una entrada del usuario.
            if (std::fabs(mx - centerX) < 0.5f && std::fabs(my - centerY) < 0.5f) {
                m_lastMouseX = mx;
                m_lastMouseY = my;
                return true;
            }
            m_lastMouseX = centerX;
            m_lastMouseY = centerY;
        }

        if (orbit && m_hasMouse) {
            m_engine->applyOrbitMouse(mx - m_lastMouseX, my - m_lastMouseY);
        } else if (m_hasMouse) {
            // 124.2 El puntero hace de flechas: arriba/abajo avanza y retrocede,
            // izquierda/derecha gira. En diagonal se aplican los dos ejes.
            const float dx = mx - m_lastMouseX;
            const float dy = my - m_lastMouseY;
            if (dy > kPointerDeadZone) {
                m_pointerForwardDir = 1.0f;
                m_pointerForwardFrames = kPointerFrames;
            } else if (dy < -kPointerDeadZone) {
                m_pointerForwardDir = -1.0f;
                m_pointerForwardFrames = kPointerFrames;
            }
            if (dx > kPointerDeadZone) {
                m_pointerTurnDir = -1.0f;
                m_pointerTurnFrames = kPointerFrames;
            } else if (dx < -kPointerDeadZone) {
                m_pointerTurnDir = 1.0f;
                m_pointerTurnFrames = kPointerFrames;
            }
        }
        if (captured) {
            // SetCursorPos del sistema, no requestWarpPointer: es el mismo
            // camino por el que se confina y se oculta el cursor.
            m_engine->centerPointer();
            m_hasMouse = true;
            return true;
        }
        m_lastMouseX = mx;
        m_lastMouseY = my;
        m_hasMouse = true;
        return orbit;
    }

    // 124. Mando completo con raton: el teclado inalambrico puede fallar y el
    // raton tiene que bastar para jugar y para probar.
    if (ea.getEventType() == osgGA::GUIEventAdapter::SCROLL) {
        if (m_dummy == nullptr) {
            return false;
        }
        const osgGA::GUIEventAdapter::ScrollingMotion motion = ea.getScrollingMotion();
        float dir = 0.0f;
        if (motion == osgGA::GUIEventAdapter::SCROLL_UP ||
            motion == osgGA::GUIEventAdapter::SCROLL_RIGHT) {
            dir = 1.0f;
        } else if (motion == osgGA::GUIEventAdapter::SCROLL_DOWN ||
                   motion == osgGA::GUIEventAdapter::SCROLL_LEFT) {
            dir = -1.0f;
        } else {
            // En Windows la rueda suele llegar como SCROLL_2D con delta, no
            // como SCROLL_UP/DOWN.
            const float dy = ea.getScrollingDeltaY();
            const float dx = ea.getScrollingDeltaX();
            const float delta = (dy != 0.0f) ? dy : dx;
            dir = (delta > 0.0f) ? 1.0f : ((delta < 0.0f) ? -1.0f : 0.0f);
        }
        if (dir == 0.0f) {
            return false;
        }
        // Desplazamiento lateral, no giro: la camara se queda como esta.
        m_scrollStrafeDir = dir;
        m_scrollStrafeFrames = kScrollStrafeFrames;
        return true;
    }

    if (ea.getEventType() != osgGA::GUIEventAdapter::PUSH) {
        return false;
    }

    // Igual que una VM: el primer clic dentro del juego toma el puntero y se
    // consume para no disparar una accion accidental. Ctrl derecho lo libera.
    if (m_engine != nullptr && !m_engine->isEditorMode() &&
        !m_engine->isMouseCaptured()) {
        m_engine->setMouseCaptured(true);
        return m_engine->isMouseCaptured();
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
        // Boton derecho = TAB. Construir y destruir se van al boton back.
        if (m_engine != nullptr && !m_engine->isEditorMode()) {
            m_engine->cycleEnemyTarget();
            return true;
        }
        destroyAtCursor();
        return true;
    }
    // Boton back: construye si la celda apuntada esta vacia, destruye si no.
    if (button == kMouseButtonBack) {
        toggleAtCursor();
        return true;
    }
    // Boton forward (kMouseButtonForward) queda libre a proposito.
    return false;
}

void StandaloneInputHandler::clearKeys()
{
    m_keyUp = false;
    m_keyDown = false;
    m_keyLeft = false;
    m_keyRight = false;
    m_keyW = false;
    m_keyA = false;
    m_keyS = false;
    m_keyD = false;
    m_keySpace = false;
    m_key1 = false;
    m_shiftHeld = false;
    m_jumpLatched = false;
    m_rightControlDown = false;
    m_scrollStrafeFrames = 0;
    m_pointerForwardFrames = 0;
    m_pointerTurnFrames = 0;
}

void StandaloneInputHandler::toggleAtCursor()
{
    if (m_grid == nullptr || m_cursor == nullptr || !m_cursor->hasPose()) {
        return;
    }
    // Un solo boton para las dos acciones: lo que decide es si hay bloque.
    const SnappedPosition pose = m_cursor->currentPose();
    if (pose.vy < 0) {
        return;
    }
    if (m_grid->getVoxel(pose.vx, pose.vy, pose.vz).isActive) {
        destroyAtCursor();
    } else {
        buildAtCursor();
    }
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
