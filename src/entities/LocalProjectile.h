#ifndef RC_LOCAL_PROJECTILE_H
#define RC_LOCAL_PROJECTILE_H

#include <osg/Node>
#include <osg/PositionAttitudeTransform>
#include <osg/Vec3>
#include <osg/ref_ptr>

namespace rc {
namespace standalone {

// Vida del proyectil del gun, en segundos. Una intercepcion que exija mas
// tiempo de vuelo que esto es un disparo imposible: la bala se destruye antes
// de llegar. Se define aqui para que la balistica y el disparo no puedan
// desincronizarse.
const float GUN_PROJECTILE_TTL = 2.00f;

// 123. Punto de intercepcion para proyectiles balisticos.
//
// Vive fuera del motor a proposito: asi la cinematica se puede verificar con
// numeros conocidos, sin ventana ni estado de juego.
//
// Resuelve la forma cerrada de |D + v t| = s t y toma la primera raiz positiva.
// Devuelve la posicion actual del objetivo cuando no hay intercepcion util:
// objetivo quieto, velocidad de bala invalida, sin raiz positiva (huye mas
// rapido de lo que la bala lo alcanza), o t por encima de maxFlightTime, que es
// una solucion matematica fuera del alcance real del arma.
osg::Vec3 computeLeadPoint(const osg::Vec3& muzzle, const osg::Vec3& targetPos,
                           const osg::Vec3& targetVel, float bulletSpeed,
                           float maxFlightTime = GUN_PROJECTILE_TTL);

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
