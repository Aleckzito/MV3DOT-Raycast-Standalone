#include "StandaloneEngine.h"

#include "DataRoot.h"
#include "LocalChunkMesher.h"
#include "MiniVoxelGrid.h"
#include "StandaloneWorldIO.h"
#include "TerrainGenerator.h"
#include "VoxelMaterials.h"

#include <SDL.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

// Genera la arena y sale. Sin SDL ni ventana: es una herramienta, no una partida.
// El motor cargara el JSON despues, por la ruta de siempre.
int generateArena(int argc, char** argv)
{
    rc::standalone::ArenaSpec spec;
    std::string worldRelative = "data/worlds/arena_120.json";

    if (argc > 2 && argv[2] != nullptr) {
        worldRelative = argv[2];
    }
    if (argc > 3 && argv[3] != nullptr) {
        spec.seed = static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10));
    }

    rc::standalone::MiniVoxelGrid grid;
    rc::standalone::ArenaLayout layout;
    rc::standalone::generateArena(spec, grid, layout);

    const std::string worldPath = rc::standalone::dataPath(worldRelative);
    if (!rc::standalone::saveWorld(&grid, worldPath)) {
        std::cerr << "[arena] no se pudo escribir " << worldPath << "\n";
        return 1;
    }

    std::string metaRelative = worldRelative;
    const size_t dot = metaRelative.rfind('.');
    if (dot != std::string::npos) {
        metaRelative = metaRelative.substr(0, dot);
    }
    metaRelative += ".meta.json";

    if (!rc::standalone::saveArenaMeta(layout, worldRelative,
                                       rc::standalone::dataPath(metaRelative))) {
        return 1;
    }

    std::cout << "[arena] semilla=" << spec.seed
              << " " << spec.sizeX << "x" << spec.sizeZ << " mini-voxels"
              << " (" << (spec.sizeX / 3) << "x" << (spec.sizeZ / 3) << " tiles)\n";
    return 0;
}

// Comprueba la invalidacion por chunks con coordenadas exactas: dentro, en
// frontera y en esquina no se pueden provocar disparando a ojo. Sin ventana:
// el mesher construye nodos OSG, que no necesitan contexto GL para existir.
int selfTestChunks()
{
    using namespace rc::standalone;

    MiniVoxelGrid grid;
    LocalChunkMesher mesher;
    mesher.setGrid(&grid);

    // Bloque solido de 48x4x48: cubre varios chunks de 16.
    for (int x = 0; x < 48; ++x) {
        for (int z = 0; z < 48; ++z) {
            for (int y = 0; y < 4; ++y) {
                grid.setVoxel(x, y, z, MAT_STONE);
            }
        }
    }
    mesher.rebuildMesh();
    const size_t chunksTotal = mesher.chunkCount();
    std::cout << "[selftest] build inicial: chunks=" << chunksTotal
              << " rebuilt=" << mesher.lastRebuiltChunks()
              << " caras=" << mesher.solidFaceCount() << "\n";

    int failures = 0;
    struct Case {
        const char* name;
        int vx, vy, vz;
        size_t expected;
    };
    // lx/lz = 8 esta en el interior; 0 toca la frontera anterior en ese eje.
    const Case cases[3] = {
        { "interior       ", 24, 1, 24, 1 },
        { "frontera X     ", 32, 1, 24, 2 },
        { "esquina X/Z    ", 32, 1, 32, 3 }
    };
    for (int i = 0; i < 3; ++i) {
        const Case& c = cases[i];
        grid.setVoxel(c.vx, c.vy, c.vz, 0);  // destruir
        mesher.rebuildMesh();
        const size_t got = mesher.lastRebuiltChunks();
        const bool ok = (got == c.expected);
        if (!ok) {
            failures += 1;
        }
        std::cout << "[selftest] " << c.name
                  << " (" << c.vx << "," << c.vy << "," << c.vz << ")"
                  << " rebuilt=" << got << " esperado=" << c.expected
                  << (ok ? "  OK" : "  FALLO") << "\n";
    }

    // Bomba: varios setVoxel y UNA sola reconstruccion.
    const size_t facesBefore = mesher.solidFaceCount();
    int removed = 0;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            grid.setVoxel(20 + dx, 3, 20 + dz, 0);
            removed += 1;
        }
    }
    const size_t dirtyVoxels = grid.dirtyVoxels().size();
    mesher.rebuildMesh();
    std::cout << "[selftest] bomba 3x3: setVoxel=" << removed
              << " dirty=" << dirtyVoxels
              << " rebuilt=" << mesher.lastRebuiltChunks()
              << " (una sola llamada)\n";
    if (dirtyVoxels != static_cast<size_t>(removed)) {
        std::cout << "[selftest] FALLO: los cambios no se acumularon\n";
        failures += 1;
    }

    // Al quitar voxels de la capa superior se exponen caras laterales de los
    // vecinos: el total de caras opacas debe subir, no bajar.
    const size_t facesAfter = mesher.solidFaceCount();
    const bool exposed = facesAfter > facesBefore;
    std::cout << "[selftest] caras expuestas: antes=" << facesBefore
              << " despues=" << facesAfter
              << (exposed ? "  OK" : "  FALLO") << "\n";
    if (!exposed) {
        failures += 1;
    }

    // --- Casos de regresion ---

    // A. Borrar un voxel visible debe eliminar su slot, no dejarlo apuntando a
    //    vertices que ahora pertenecen a otro voxel.
    grid.setVoxel(10, 3, 10, MAT_STONE);
    mesher.rebuildMesh();
    const bool slotAntes = mesher.hasSlot(10, 3, 10);
    grid.setVoxel(10, 3, 10, 0);
    mesher.rebuildMesh();
    const bool slotDespues = mesher.hasSlot(10, 3, 10);
    const bool okSlot = slotAntes && !slotDespues;
    std::cout << "[selftest] slot tras borrado: antes=" << slotAntes
              << " despues=" << slotDespues << (okSlot ? "  OK" : "  FALLO") << "\n";
    if (!okSlot) failures += 1;

    // B. Un chunk remallado con X-Ray activo debe dejar el voxel colapsado; si
    //    no, se dibuja opaco y translucido a la vez.
    mesher.setXRay(12, 3, 12, true);
    const bool hiddenAntes = mesher.slotHidden(12, 3, 12);
    // Mismo chunk y cambio REAL: repetir el material no ensucia el grid, y el
    // caso quedaria sin ejercer nada.
    grid.setVoxel(14, 3, 14, MAT_BRICK);
    mesher.rebuildMesh();
    const bool hiddenDespues = mesher.slotHidden(12, 3, 12);
    const bool sigueXray = mesher.isXRay(12, 3, 12);
    const bool okXray = hiddenAntes && hiddenDespues && sigueXray;
    std::cout << "[selftest] X-Ray tras rebuild: oculto_antes=" << hiddenAntes
              << " oculto_despues=" << hiddenDespues
              << " en_xray=" << sigueXray << (okXray ? "  OK" : "  FALLO") << "\n";
    if (!okXray) failures += 1;

    // C. Coordenadas en cero y negativas: el mapeo no puede usar >> ni <<.
    grid.setVoxel(0, 0, 0, MAT_STONE);
    grid.setVoxel(-1, 0, 0, MAT_STONE);
    grid.setVoxel(-17, 0, -17, MAT_STONE);
    mesher.rebuildMesh();
    const bool okNeg = mesher.hasSlot(0, 0, 0) && mesher.hasSlot(-1, 0, 0) &&
                       mesher.hasSlot(-17, 0, -17);
    std::cout << "[selftest] coords negativas: (0,0,0)=" << mesher.hasSlot(0, 0, 0)
              << " (-1,0,0)=" << mesher.hasSlot(-1, 0, 0)
              << " (-17,0,-17)=" << mesher.hasSlot(-17, 0, -17)
              << (okNeg ? "  OK" : "  FALLO") << "\n";
    if (!okNeg) failures += 1;

    // B2. Borrar un voxel que esta en X-Ray: no basta con quitar su slot, hay
    //     que retirar tambien su Geode translucido o queda un cubo fantasma.
    grid.setVoxel(20, 3, 12, MAT_STONE);
    mesher.rebuildMesh();
    mesher.setXRay(20, 3, 12, true);
    grid.setVoxel(20, 3, 12, 0);
    mesher.rebuildMesh();
    const bool okBorrado = !mesher.hasSlot(20, 3, 12) && !mesher.isXRay(20, 3, 12);
    std::cout << "[selftest] X-Ray + borrado: slot=" << mesher.hasSlot(20, 3, 12)
              << " xray=" << mesher.isXRay(20, 3, 12)
              << (okBorrado ? "  OK" : "  FALLO") << "\n";
    if (!okBorrado) failures += 1;

    // B3. Convertir a liquido un voxel en X-Ray: los liquidos no llevan slot,
    //     y el Geode conservaria el color solido anterior.
    grid.setVoxel(22, 3, 12, MAT_STONE);
    mesher.rebuildMesh();
    mesher.setXRay(22, 3, 12, true);
    grid.setVoxel(22, 3, 12, MAT_WATER);
    mesher.rebuildMesh();
    const bool okAgua = !mesher.hasSlot(22, 3, 12) && !mesher.isXRay(22, 3, 12);
    std::cout << "[selftest] X-Ray -> agua: slot=" << mesher.hasSlot(22, 3, 12)
              << " xray=" << mesher.isXRay(22, 3, 12)
              << (okAgua ? "  OK" : "  FALLO") << "\n";
    if (!okAgua) failures += 1;

    // B4. Cambiar el material de un voxel en X-Ray: debe seguir oculto y la
    //     visual translucida tiene que reflejar el material nuevo.
    grid.setVoxel(26, 3, 12, MAT_STONE);
    mesher.rebuildMesh();
    mesher.setXRay(26, 3, 12, true);
    grid.setVoxel(26, 3, 12, MAT_BRICK);
    mesher.rebuildMesh();
    const bool okMat = mesher.slotHidden(26, 3, 12) && mesher.isXRay(26, 3, 12) &&
                       mesher.xrayMaterial(26, 3, 12) == MAT_BRICK;
    std::cout << "[selftest] X-Ray cambia material: oculto=" << mesher.slotHidden(26, 3, 12)
              << " xray=" << mesher.isXRay(26, 3, 12)
              << " material=" << mesher.xrayMaterial(26, 3, 12)
              << " (esperado " << static_cast<int>(MAT_BRICK) << ")"
              << (okMat ? "  OK" : "  FALLO") << "\n";
    if (!okMat) failures += 1;

    // D. Vaciar un chunk debe retirarlo: si no, se acumulan Geodes vacios.
    const size_t chunksConAislado = mesher.chunkCount();
    grid.setVoxel(-17, 0, -17, 0);
    mesher.rebuildMesh();
    const size_t chunksTrasVaciar = mesher.chunkCount();
    const bool okVacio = chunksTrasVaciar < chunksConAislado;
    std::cout << "[selftest] chunk vaciado: antes=" << chunksConAislado
              << " despues=" << chunksTrasVaciar
              << (okVacio ? "  OK" : "  FALLO") << "\n";
    if (!okVacio) failures += 1;

    std::cout << "[selftest] " << (failures == 0 ? "TODO OK" : "FALLOS") << "\n";
    return failures == 0 ? 0 : 1;
}

// Verifica la cinematica de intercepcion con numeros conocidos. La punteria no
// se puede comprobar disparando de forma reproducible, pero la matematica si.
int selfTestAim()
{
    using namespace rc::standalone;

    int failures = 0;
    const float bullet = 30.0f;  // gunSpeed tipico

    // Cada caso mide el ERROR DE INTERCEPCION, no solo que el punto se moviera:
    //   t_bala   = |lead - muzzle| / v_bala
    //   objetivo = target + v * t_bala
    //   error    = |lead - objetivo|
    // Si la solucion es correcta, bala y objetivo llegan al mismo sitio a la
    // vez y el error es ~0. Comprobar solo el desplazamiento dejaba pasar una
    // formula equivocada, que es justo lo que ocurria.
    struct Case {
        const char* name;
        osg::Vec3 vel;
        float speed;
        bool solvable;      // existe intercepcion real
        float expectedLead; // adelanto exacto esperado, -1 si no aplica
    };
    const osg::Vec3 muzzle(0, 0, 0);
    const osg::Vec3 target(30, 0, 0);

    const Case cases[8] = {
        { "quieto           ", osg::Vec3(0, 0, 0),    bullet, false, 0.0f },
        // Cierre 20 m/s sobre 30 m => t=1.5 => adelanto 15.
        { "se aleja 10      ", osg::Vec3(10, 0, 0),   bullet, true,  15.0f },
        // Acercandose: la bala y el objetivo se aproximan, t es corto.
        { "se acerca 25     ", osg::Vec3(-25, 0, 0),  bullet, true,  -1.0f },
        // Mas rapido que la bala pero DE FRENTE: sigue siendo resoluble.
        { "se acerca 40     ", osg::Vec3(-40, 0, 0),  bullet, true,  -1.0f },
        { "cruza en Z 10    ", osg::Vec3(0, 0, 10),   bullet, true,  -1.0f },
        // Cruce cerca del limite de la bala: la cuadratica da t=3.9 s, pero el
        // proyectil solo vive 2 s. Solucion matematica fuera de alcance real.
        { "cruza en Z 29    ", osg::Vec3(0, 0, 29),   bullet, false, 0.0f },
        { "sube en Y 8      ", osg::Vec3(0, 8, 0),    bullet, true,  -1.0f },
        // Huye mas rapido que la bala: sin raiz positiva.
        { "huye 40 (sin raiz)", osg::Vec3(40, 0, 0),  bullet, false, 0.0f }
    };

    for (int i = 0; i < 8; ++i) {
        const Case& c = cases[i];
        const osg::Vec3 lead = computeLeadPoint(muzzle, target, c.vel, c.speed);
        const float tBullet = (lead - muzzle).length() / c.speed;
        const osg::Vec3 targetAtT = target + c.vel * tBullet;
        const float error = (lead - targetAtT).length();
        const float shift = (lead - target).length();

        bool ok = false;
        if (c.solvable) {
            // Con solucion: el error debe ser practicamente cero.
            ok = error < 0.01f;
            if (c.expectedLead >= 0.0f) {
                ok = ok && std::fabs(shift - c.expectedLead) < 0.01f;
            }
        } else {
            // Sin solucion: se apunta a la posicion actual, sin adelanto.
            ok = shift < 0.01f;
        }
        if (!ok) failures += 1;

        std::cout << "[selftest-aim] " << c.name
                  << " adelanto=" << shift
                  << " error=" << (c.solvable ? error : 0.0f);
        if (c.expectedLead >= 0.0f) {
            std::cout << " (exacto " << c.expectedLead << ")";
        }
        if (!c.solvable) {
            std::cout << " sin-solucion";
        }
        std::cout << (ok ? "  OK" : "  FALLO") << "\n";
    }

    // El TTL debe ser el que decide, no un umbral inventado: el mismo cruce a
    // 29 m/s si se acepta cuando el proyectil vive lo suficiente.
    {
        const osg::Vec3 fast(0, 0, 29);
        const osg::Vec3 conTtl = computeLeadPoint(muzzle, target, fast, bullet,
                                                  GUN_PROJECTILE_TTL);
        const osg::Vec3 sinTtl = computeLeadPoint(muzzle, target, fast, bullet, 10.0f);
        const float shiftCorto = (conTtl - target).length();
        const float shiftLargo = (sinTtl - target).length();
        const bool ok = shiftCorto < 0.01f && shiftLargo > 100.0f;
        std::cout << "[selftest-aim] TTL decide      "
                  << " ttl=" << GUN_PROJECTILE_TTL << "s -> adelanto=" << shiftCorto
                  << " | ttl=10s -> adelanto=" << shiftLargo
                  << (ok ? "  OK" : "  FALLO") << "\n";
        if (!ok) failures += 1;
    }

    // Degenerado a ~= 0 con b != 0: el objetivo viene de frente justo a la
    // velocidad de la bala. La cuadratica se anula y queda la forma lineal;
    // se cruzan a mitad de camino, t = 0.5 s y adelanto 15 m.
    {
        const osg::Vec3 head_on(-bullet, 0, 0);
        const osg::Vec3 lead = computeLeadPoint(muzzle, target, head_on, bullet, 100.0f);
        const float tB = (lead - muzzle).length() / bullet;
        const float error = (lead - (target + head_on * tB)).length();
        const float shift = (lead - target).length();
        const bool ok = error < 0.05f && std::fabs(shift - 15.0f) < 0.05f;
        std::cout << "[selftest-aim] a~=0 lineal     adelanto=" << shift
                  << " error=" << error << " (exacto 15)"
                  << (ok ? "  OK" : "  FALLO") << "\n";
        if (!ok) failures += 1;
    }

    // Degenerado a ~= 0 con b == 0: cruce perpendicular a la velocidad de la
    // bala. No existe intercepcion: |D + v t| = s t se reduce a |D|^2 = 0.
    {
        const osg::Vec3 perp(0, 0, bullet);
        const osg::Vec3 lead = computeLeadPoint(muzzle, target, perp, bullet, 100.0f);
        const bool ok = (lead - target).length() < 0.01f;
        std::cout << "[selftest-aim] a~=0 b=0        adelanto="
                  << (lead - target).length() << " sin-solucion"
                  << (ok ? "  OK" : "  FALLO") << "\n";
        if (!ok) failures += 1;
    }

    // Discriminante exactamente cero CON a != 0, es decir raiz doble de verdad.
    // D=(30,0,0), v=(-10,30,0), bala=30 => a=100, b=-600, c=900, disc=0, t=3.
    // Hace falta un TTL largo o el limite taparia la raiz.
    {
        const osg::Vec3 v(-10, 30, 0);
        const osg::Vec3 lead = computeLeadPoint(muzzle, target, v, bullet, 100.0f);
        const osg::Vec3 expected(0, 90, 0);
        const float tB = (lead - muzzle).length() / bullet;
        const float error = (lead - (target + v * tB)).length();
        const bool ok = (lead - expected).length() < 0.05f && error < 0.05f;
        std::cout << "[selftest-aim] disc=0 raiz doble punto=(" << lead.x() << ","
                  << lead.y() << "," << lead.z() << ") esperado=(0,90,0) error=" << error
                  << (ok ? "  OK" : "  FALLO") << "\n";
        if (!ok) failures += 1;
    }

    // TTL <= 0 es una bala que no vuela: no debe interpretarse como sin limite.
    {
        const osg::Vec3 v(0, 0, 10);
        const osg::Vec3 leadZero = computeLeadPoint(muzzle, target, v, bullet, 0.0f);
        const osg::Vec3 leadNeg = computeLeadPoint(muzzle, target, v, bullet, -1.0f);
        const bool ok = (leadZero - target).length() < 0.01f &&
                        (leadNeg - target).length() < 0.01f;
        std::cout << "[selftest-aim] ttl<=0 no vuela adelanto_0=" << (leadZero - target).length()
                  << " adelanto_neg=" << (leadNeg - target).length()
                  << (ok ? "  OK" : "  FALLO") << "\n";
        if (!ok) failures += 1;
    }

    // Limite exacto: t == maxFlightTime no debe aceptarse. Cruce a 10 m/s da
    // t = 1.0607 s, asi que un TTL de justo ese valor debe rechazarlo.
    {
        const osg::Vec3 v(0, 0, 10);
        const osg::Vec3 leadFull = computeLeadPoint(muzzle, target, v, bullet, 100.0f);
        const float tExact = (leadFull - muzzle).length() / bullet;
        const osg::Vec3 leadAt = computeLeadPoint(muzzle, target, v, bullet, tExact);
        const osg::Vec3 leadOver = computeLeadPoint(muzzle, target, v, bullet, tExact * 1.01f);
        const bool ok = (leadAt - target).length() < 0.01f &&
                        (leadOver - target).length() > 0.01f;
        std::cout << "[selftest-aim] t == ttl        rechazado=" << ((leadAt - target).length() < 0.01f)
                  << " t < ttl aceptado=" << ((leadOver - target).length() > 0.01f)
                  << (ok ? "  OK" : "  FALLO") << "\n";
        if (!ok) failures += 1;
    }

    // Bala con velocidad invalida: no debe dividir por cero ni mover el punto.
    const osg::Vec3 leadBad = computeLeadPoint(muzzle, target, osg::Vec3(0, 0, 10), 0.0f);
    const bool okBad = (leadBad - target).length() < 0.01f;
    std::cout << "[selftest-aim] bala v=0           "
              << " adelanto=" << (leadBad - target).length()
              << (okBad ? "  OK" : "  FALLO") << "\n";
    if (!okBad) failures += 1;

    // Comparativa: cuanto se erraba antes de adelantar.
    const osg::Vec3 crossVel(0, 0, 10);
    const float tNaive = (target - muzzle).length() / bullet;
    const float naiveMiss = (crossVel * tNaive).length();
    std::cout << "[selftest-aim] sin adelantar, un objetivo a 10 m/s se escapa "
              << naiveMiss << " m\n";

    std::cout << "[selftest-aim] " << (failures == 0 ? "TODO OK" : "FALLOS") << "\n";
    return failures == 0 ? 0 : 1;
}

} // namespace

// 1.3 Punto de entrada local. SDL2 + OSG. Cero sockets.
int main(int argc, char** argv)
{
    // Modo herramienta: --gen-arena [ruta relativa] [semilla]
    if (argc > 1 && argv[1] != nullptr && std::string(argv[1]) == "--gen-arena") {
        return generateArena(argc, argv);
    }
    if (argc > 1 && argv[1] != nullptr && std::string(argv[1]) == "--selftest-chunks") {
        return selfTestChunks();
    }
    if (argc > 1 && argv[1] != nullptr && std::string(argv[1]) == "--selftest-aim") {
        return selfTestAim();
    }

    // 11. argv[1] opcional = mapa de voxeles. Sin argumento manda el meta del pack.
    std::string worldJsonPath;
    if (argc > 1 && argv[1] != nullptr) {
        worldJsonPath = argv[1];
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        std::cerr << "[standalone] SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    rc::standalone::StandaloneEngine engine;
    if (!engine.initialize(worldJsonPath)) {
        std::cerr << "[standalone] initialize failed\n";
        SDL_Quit();
        return 1;
    }

    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();

    while (!engine.shouldQuit()) {
        // 2.3 Poll SDL solo para QUIT. Input de camara/editor no cableado.
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                engine.requestQuit();
            }
        }

        const auto now = Clock::now();
        const float deltaTime = std::chrono::duration<float>(now - last).count();
        last = now;

        engine.update(deltaTime);
        engine.render();
    }

    engine.shutdown();
    SDL_Quit();
    return 0;
}
