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

    struct Case {
        const char* name;
        osg::Vec3 muzzle;
        osg::Vec3 target;
        osg::Vec3 vel;
        float speed;
        bool expectLead;  // se espera que el punto se desplace
    };
    const Case cases[6] = {
        // Objetivo quieto: el punto no se toca.
        { "quieto        ", osg::Vec3(0,0,0), osg::Vec3(30,0,0), osg::Vec3(0,0,0), bullet, false },
        // Cruzando en Z a 10 m/s a 30 m: la bala tarda 1 s, adelanto ~10 m.
        { "cruza en Z    ", osg::Vec3(0,0,0), osg::Vec3(30,0,0), osg::Vec3(0,0,10), bullet, true },
        // Subiendo: el adelanto debe tener componente Y, que es lo que hacia
        // que los disparos pasaran por debajo de los voladores.
        { "sube en Y     ", osg::Vec3(0,0,0), osg::Vec3(30,0,0), osg::Vec3(0,8,0), bullet, true },
        // Alejandose en linea: el punto se aleja mas.
        { "se aleja      ", osg::Vec3(0,0,0), osg::Vec3(30,0,0), osg::Vec3(10,0,0), bullet, true },
        // Mas rapido que la bala: la aproximacion no vale, se deja igual.
        { "supera la bala", osg::Vec3(0,0,0), osg::Vec3(30,0,0), osg::Vec3(0,0,40), bullet, false },
        // Velocidad de bala invalida: no se toca.
        { "bala v=0      ", osg::Vec3(0,0,0), osg::Vec3(30,0,0), osg::Vec3(0,0,10), 0.0f, false }
    };

    for (int i = 0; i < 6; ++i) {
        const Case& c = cases[i];
        const osg::Vec3 lead = computeLeadPoint(c.muzzle, c.target, c.vel, c.speed);
        const float shift = (lead - c.target).length();
        const bool moved = shift > 0.01f;
        const bool ok = (moved == c.expectLead);
        if (!ok) failures += 1;
        std::cout << "[selftest-aim] " << c.name
                  << " adelanto=" << shift
                  << " esperado=" << (c.expectLead ? "si" : "no")
                  << (ok ? "  OK" : "  FALLO") << "\n";
    }

    // El caso que importa: con el adelanto, el proyectil y el objetivo llegan
    // al mismo punto a la vez. Sin el, el objetivo ya no esta ahi.
    const osg::Vec3 muzzle(0, 0, 0);
    const osg::Vec3 target(30, 0, 0);
    const osg::Vec3 vel(0, 0, 10);
    const osg::Vec3 lead = computeLeadPoint(muzzle, target, vel, bullet);
    const float tBullet = (lead - muzzle).length() / bullet;
    const osg::Vec3 targetAtT = target + vel * tBullet;
    const float missDistance = (lead - targetAtT).length();

    const osg::Vec3 naiveDir = target - muzzle;
    const float tNaive = naiveDir.length() / bullet;
    const float naiveMiss = (target - (target + vel * tNaive)).length();

    const bool okIntercept = missDistance < 0.35f && missDistance < naiveMiss;
    std::cout << "[selftest-aim] intercepcion: error_con_lead=" << missDistance
              << " error_sin_lead=" << naiveMiss
              << (okIntercept ? "  OK" : "  FALLO") << "\n";
    if (!okIntercept) failures += 1;

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
