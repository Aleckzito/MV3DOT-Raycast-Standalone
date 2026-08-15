#ifndef RC_STANDALONE_INPUT_HANDLER_H
#define RC_STANDALONE_INPUT_HANDLER_H

#include <osgGA/GUIEventHandler>

#include <cstdint>
#include <string>

namespace rc {
namespace standalone {

class MiniVoxelGrid;
class LocalChunkMesher;
class PhantomCursor;
class DummyActor;
class StandaloneEngine;

// 81 / 82 / 83 / 84. Hotbar 1-4 + SHIFT capa. WASD/flechas. Sin letras E/F/Q/R.
class StandaloneInputHandler : public osgGA::GUIEventHandler {
public:
    StandaloneInputHandler();

    void bind(MiniVoxelGrid* grid, LocalChunkMesher* mesher, PhantomCursor* cursor, DummyActor* dummy,
              StandaloneEngine* engine);

    bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa) override;

    // 17.2 / 50. forward: W/UP=+1 S/DOWN=-1. strafe: D=+1 A=-1. turn: LEFT=+1 RIGHT=-1 (desinvertido).
    void getMoveAxes(float& forward, float& strafe, float& turn) const;
    void getOrbitAxes(float& yaw, float& pitch) const;
    void setInvertMove(bool invert) { m_invertMove = invert; }
    void setArrowOrbit(bool orbit) { m_arrowOrbit = orbit; }
    bool isJumpPressed();
    bool isSpaceHeld() const { return m_keySpace; }

private:
    void setKeyState(int key, bool down);
    void buildAtCursor();
    void destroyAtCursor();
    void saveSandbox();
    void loadSandbox();
    std::string worldPath() const;
    void tossDummy();

    MiniVoxelGrid* m_grid;
    LocalChunkMesher* m_mesher;
    PhantomCursor* m_cursor;
    DummyActor* m_dummy;
    StandaloneEngine* m_engine;
    uint16_t m_buildMaterial;
    std::string m_worldPath;

    bool m_keyUp;
    bool m_keyDown;
    bool m_keyLeft;
    bool m_keyRight;
    bool m_keyW;
    bool m_keyA;
    bool m_keyS;
    bool m_keyD;
    bool m_keySpace;
    bool m_key1;
    bool m_shiftHeld;
    bool m_jumpLatched;
    bool m_invertMove;
    bool m_arrowOrbit;
    float m_lastMouseX;
    float m_lastMouseY;
    bool m_hasMouse;
};

} // namespace standalone
} // namespace rc

#endif
