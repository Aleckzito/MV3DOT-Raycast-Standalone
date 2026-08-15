#ifndef RC_LOCAL_PROJECTILE_H
#define RC_LOCAL_PROJECTILE_H

#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/Vec3>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

// 123. Intercepcion de primer orden para proyectiles balisticos.
//
// Vive fuera del motor a proposito: asi la cinematica se puede verificar con
// numeros conocidos, sin ventana ni estado de juego.
//
//   t = |D| / v_bala        y luego   P_lead = P_obj + v_obj * t
//
// Se hace una segunda iteracion porque al adelantar el punto cambia la
// distancia, y con una basta para quitar casi todo el error en cruces.
// Si el objetivo se acerca a la velocidad de la bala la aproximacion deja de
// valer, y entonces se devuelve la posicion actual en vez de un disparate.
osg::Vec3 computeLeadPoint(const osg::Vec3& muzzle, const osg::Vec3& targetPos,
                           const osg::Vec3& targetVel, float bulletSpeed);

// 21 / 92. Proyectil local: aliado naranja o flecha hostil verde.
class LocalProjectile {
public:
    LocalProjectile();
    LocalProjectile(const osg::Vec3& pos, const osg::Vec3& vel, float ttl);
    LocalProjectile(const osg::Vec3& pos, const osg::Vec3& vel, float ttl, bool hostile);

    osg::Node* getNode();
    void syncVisual();
    bool alive() const { return m_alive; }
    void kill() { m_alive = false; }
    bool isHostile() const { return m_hostile; }

    osg::Vec3 m_pos;
    osg::Vec3 m_vel;
    float m_ttl;

private:
    void buildVisual();

    bool m_alive;
    bool m_hostile;
    osg::ref_ptr<osg::PositionAttitudeTransform> m_pat;
};

} // namespace standalone
} // namespace rc

#endif
