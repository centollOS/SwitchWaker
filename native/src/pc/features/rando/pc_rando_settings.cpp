// The randomizer's settings and its options-menu tab (docs/RANDOMIZER_PLAN.md step R4.1;
// pc_rando_settings.h, namespace pc): the COS_RANDO_* settings registered with pc_settings.h, and
// the rows of the "Randomizer" tab, which pc_menu.cpp draws and drives (RowKind::Rando). The menu's
// own code (keys, drawing, the language) stays in pc_menu.cpp; this file only answers with text.
//
// Compiled into cos_rando_core too (the features/rando glob), but no unit test may call into it:
// it needs pc_settings.cpp. The pure part is pc_rando_presets.cpp.
#include "pc_rando_settings.h"

#include "pc/pc_settings.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <unistd.h>

namespace pc {

namespace {

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

// D16: USA only until R7.6.
#if defined(VERSION) && VERSION == 3
constexpr bool kRegionOk = false;
#else
constexpr bool kRegionOk = true;
#endif

const char* L(bool es, const char* s, const char* e) {
    return es ? s : e;
}

// ---- labels ---------------------------------------------------------------------------------------

struct OptText {
    int id;
    const char* labelEs;
    const char* labelEn;
    const char* helpEs;
    const char* helpEn;
};

// The rows' text, from wwrando's option descriptions (options/wwrando_options.py, MIT).
const OptText kText[] = {
    {pcr::OPT_progression_dungeons, "Categoría: mazmorras", "Category: dungeons",
     "Las mazmorras pueden tener objetos de progreso.", "Dungeons can contain progress items."},
    {pcr::OPT_progression_tingle_chests, "Categoría: cofres de Tingle", "Category: Tingle chests",
     "Los cofres de Tingle ocultos en las mazmorras (aparecen con una bomba: 2 en la Cueva del Dragón, 1 en "
     "cada una de las demás) pueden tener objetos de progreso.",
     "The Tingle Chests hidden in dungeons (bomb them to make them appear: 2 in DRC, 1 each in FW, TotG, "
     "ET and WT) can contain progress items."},
    {pcr::OPT_progression_dungeon_secrets, "Categoría: secretos de mazmorra", "Category: dungeon secrets",
     "Los 11 objetos secretos de las mazmorras (2-3 en cada una) pueden ser de progreso.",
     "The 11 secret items within the dungeons (2-3 each) can be progress items."},
    {pcr::OPT_progression_puzzle_secret_caves, "Categoría: cuevas de acertijo", "Category: puzzle secret caves",
     "Las cuevas secretas de acertijos pueden tener objetos de progreso.",
     "Puzzle-focused secret caves can contain progress items."},
    {pcr::OPT_progression_combat_secret_caves, "Categoría: cuevas de combate", "Category: combat secret caves",
     "Las cuevas secretas de combate (salvo el Laberinto Salvaje) pueden tener objetos de progreso.",
     "Combat-focused secret caves (besides Savage Labyrinth) can contain progress items."},
    {pcr::OPT_progression_savage_labyrinth, "Categoría: Laberinto Salvaje", "Category: Savage Labyrinth",
     "El Laberinto Salvaje puede tener objetos de progreso.", "The Savage Labyrinth can contain progress items."},
    {pcr::OPT_progression_great_fairies, "Categoría: Grandes Hadas", "Category: Great Fairies",
     "Los regalos de las Grandes Hadas pueden ser objetos de progreso.",
     "The items given by Great Fairies can be progress items."},
    {pcr::OPT_progression_short_sidequests, "Categoría: misiones cortas", "Category: short sidequests",
     "Las misiones secundarias rápidas pueden dar objetos de progreso.",
     "Sidequests that can be completed quickly can reward progress items."},
    {pcr::OPT_progression_long_sidequests, "Categoría: misiones largas", "Category: long sidequests",
     "Las misiones secundarias largas (el ayudante de Lenzo, los árboles secos, el comercio goron) pueden "
     "dar objetos de progreso.",
     "Long sidequests (e.g. Lenzo's assistant, withered trees, goron trading) can reward progress items."},
    {pcr::OPT_progression_spoils_trading, "Categoría: canje de botín", "Category: spoils trading",
     "Lo que dan los personajes a cambio de botín puede ser de progreso.",
     "The items you get by trading in spoils to NPCs can be progress items."},
    {pcr::OPT_progression_minigames, "Categoría: minijuegos", "Category: minigames",
     "La mayoría de los minijuegos (subastas, clasificar cartas, tiro al barril, concurso de hombres "
     "pájaro) pueden dar objetos de progreso.",
     "Most minigames can reward progress items (auctions, mail sorting, barrel shooting, bird-man contest)."},
    {pcr::OPT_progression_battlesquid, "Categoría: Hundir la flota", "Category: Battlesquid",
     "El minijuego de los barcos de Taura puede dar objetos de progreso.",
     "The Windfall battleship minigame can reward progress items."},
    {pcr::OPT_progression_free_gifts, "Categoría: regalos", "Category: free gifts",
     "Los regalos de los personajes (Tott, la Compañía de Rescate, Tingle encerrado) pueden ser de progreso.",
     "Gifts freely given by NPCs can be progress items (Tott, Salvage Corp, imprisoned Tingle)."},
    {pcr::OPT_progression_mail, "Categoría: correo", "Category: mail",
     "Las cartas pueden contener objetos de progreso.", "Mail can contain progress items."},
    {pcr::OPT_progression_platforms_rafts, "Categoría: plataformas y balsas", "Category: platforms and rafts",
     "Las plataformas de vigía y las balsas pueden tener objetos de progreso.",
     "Lookout platforms and rafts can contain progress items."},
    {pcr::OPT_progression_submarines, "Categoría: submarinos", "Category: submarines",
     "Los submarinos pueden tener objetos de progreso.", "Submarines can contain progress items."},
    {pcr::OPT_progression_eye_reef_chests, "Categoría: arrecifes del ojo", "Category: eye reef chests",
     "Los cofres que aparecen al limpiar los arrecifes del ojo pueden tener objetos de progreso.",
     "The chests that appear after clearing out the eye reefs can contain progress items."},
    {pcr::OPT_progression_big_octos_gunboats, "Categoría: Octorroks gigantes y cañoneras",
     "Category: Big Octos and gunboats",
     "Lo que sueltan los Octorroks gigantes y las cañoneras puede ser de progreso.",
     "The items dropped by Big Octos and Gunboats can be progress items."},
    {pcr::OPT_progression_triforce_charts, "Categoría: cartas de la Trifuerza", "Category: Triforce Charts",
     "Los cofres hundidos de las cartas de la Trifuerza pueden tener objetos de progreso.",
     "The sunken treasure chests marked on Triforce Charts can contain progress items."},
    {pcr::OPT_progression_treasure_charts, "Categoría: cartas del tesoro", "Category: Treasure Charts",
     "Los cofres hundidos de las cartas del tesoro pueden tener objetos de progreso.",
     "The sunken treasure chests marked on Treasure Charts can contain progress items."},
    {pcr::OPT_progression_expensive_purchases, "Categoría: compras caras", "Category: expensive purchases",
     "Lo que cuesta muchas rupias (la tienda de la Isla Aguja, las subastas, la carta de Tingle, el "
     "comercio) puede ser de progreso.",
     "Items that cost a lot of rupees can be progress items (Rock Spire shop, auctions, Tingle's letter, "
     "trading quest)."},
    {pcr::OPT_progression_island_puzzles, "Categoría: acertijos de islas", "Category: island puzzles",
     "Los acertijos de las islas (p. ej. cofres en sitios raros) pueden tener objetos de progreso.",
     "Various island puzzles can contain progress items (e.g. chests hidden in unusual places)."},
    {pcr::OPT_progression_misc, "Categoría: varios", "Category: miscellaneous",
     "Lugares que no encajan en otra categoría (cofres al aire libre, el santuario del viento, Ciclos...).",
     "Miscellaneous locations that don't fit into any other category (outdoors chests, wind shrine, "
     "Cyclos, etc)."},
    {pcr::OPT_keylunacy, "Llaves en cualquier lugar", "Key-lunacy",
     "Las llaves, mapas y brújulas de las mazmorras pueden aparecer en cualquier parte, no solo en su "
     "mazmorra.",
     "Dungeon keys (as well as maps and compasses) can appear anywhere in the game, not just in their "
     "dungeon."},
    {pcr::OPT_sword_mode, "Espada", "Sword mode",
     "Empezar con la Espada del Héroe, que la espada esté entre los objetos aleatorios, o un juego sin "
     "espadas.",
     "Start with the Hero's Sword, randomize the Hero's Sword, or no swords in the entire game."},
    {pcr::OPT_required_bosses, "Jefes obligatorios", "Required bosses",
     "No se puede terminar el juego sin vencer a unos jefes elegidos al azar; nada en las mazmorras de los "
     "demás jefes será necesario.",
     "You cannot beat the game until certain randomly-chosen bosses are defeated. Nothing in dungeons for "
     "other bosses will ever be required."},
    {pcr::OPT_num_required_bosses, "Número de jefes obligatorios", "Number of required bosses",
     "Cuántos jefes elegidos al azar hay que vencer en el modo de jefes obligatorios.",
     "The number of randomly-chosen bosses that are required in Required Bosses mode."},
    {pcr::OPT_chest_type_matches_contents, "Cofre según contenido", "Chest type matches contents",
     "El cofre muestra lo que tiene: metálico un objeto de progreso, de madera uno que no lo es, verde una "
     "llave quizá necesaria.",
     "A metal chest has a progress item, a wooden chest a non-progress item or a consumable, and a green "
     "chest a potentially required dungeon key."},
    {pcr::OPT_trap_chests, "Cofres trampa", "Trap chests",
     "Algunos cofres del juego son trampas y no dan objeto.",
     "Several trapped chests across the game do not give you items."},
    {pcr::OPT_hero_mode, "Modo héroe", "Hero mode",
     "Recibes cuatro veces más daño y no aparecen corazones.",
     "You take four times more damage than normal and heart refills will not drop."},
    {pcr::OPT_logic_obscurity, "Trucos rebuscados", "Obscure tricks",
     "Formas poco evidentes de conseguir objetos que la lógica puede exigir.",
     "Obscure ways of obtaining items, which may involve thinking outside the box, that the logic may "
     "require."},
    {pcr::OPT_logic_precision, "Trucos precisos", "Precise tricks",
     "Formas de conseguir objetos que piden puntería o tiempos exactos y que la lógica puede exigir.",
     "Ways of obtaining items with difficult inputs, such as accurate aiming or perfect timing, that the "
     "logic may require."},
    {pcr::OPT_randomize_dungeon_entrances, "Entradas de mazmorras", "Dungeon entrances",
     "Mezcla a qué mazmorra lleva cada entrada de mazmorra.",
     "Shuffles around which dungeon entrances take you into which dungeons."},
    {pcr::OPT_randomize_secret_cave_entrances, "Entradas de cuevas secretas", "Secret cave entrances",
     "Mezcla a qué cueva secreta lleva cada entrada.",
     "Shuffles around which secret cave entrances take you into which secret caves."},
    {pcr::OPT_randomize_miniboss_entrances, "Puertas de minijefes", "Miniboss entrances",
     "Las puertas de los minijefes también se mezclan.",
     "Dungeon miniboss doors act as entrances to be randomized."},
    {pcr::OPT_randomize_boss_entrances, "Puertas de jefes", "Boss entrances",
     "Las puertas de los jefes también se mezclan.", "Dungeon boss doors act as entrances to be randomized."},
    {pcr::OPT_randomize_secret_cave_inner_entrances, "Entradas interiores de cuevas",
     "Inner secret cave entrances",
     "El pozo de la cueva de la Isla del Anillo de Hielo y la salida trasera de la de las Islas Meseta "
     "también se mezclan.",
     "The pit in Ice Ring Isle's secret cave and the rear exit of Cliff Plateau Isles' secret cave act as "
     "entrances to be randomized."},
    {pcr::OPT_randomize_fairy_fountain_entrances, "Entradas de fuentes de hadas", "Fairy fountain entrances",
     "Los pozos que bajan a las fuentes de las hadas también se mezclan.",
     "The pits that lead down into Fairy Fountains act as entrances to be randomized."},
    {pcr::OPT_mix_entrances, "Mezcla de entradas", "Mix entrances",
     "Si las mazmorras se mezclan solo entre ellas o con las cuevas y las fuentes.",
     "Whether dungeons stay separate from the other randomized entrances, or all types can lead into each "
     "other."},
    // R7.4d: experimental, as upstream (hidden=True, unbeatable=True; D18); the menu allows it
    // (ensureValidCombination's allowUnbeatable below).
    {pcr::OPT_randomize_enemies, "Enemigos al azar (experimental, puede ser imposible)",
     "Enemy randomizer (experimental, can be unbeatable)",
     "Cambia de sitio los enemigos que no son jefes. Experimental: una partida puede quedar sin solución.",
     "Randomizes the placement of non-boss enemies. Experimental: a seed can be unbeatable."},
    {pcr::OPT_randomize_enemy_palettes, "Colores de enemigos al azar", "Randomize enemy palettes",
     "Da colores aleatorios a todos los enemigos.", "Gives all the enemies in the game random colors."},
    {pcr::OPT_randomize_starting_island, "Isla inicial al azar", "Randomize starting island",
     "La partida empieza en una isla elegida al azar.", "Randomizes which island you start the game on."},
    {pcr::OPT_randomize_charts, "Cartas al azar", "Randomize charts",
     "Cambia qué sector muestra cada carta de la Trifuerza y del tesoro.",
     "Randomizes which sector is drawn on each Triforce/Treasure Chart."},
    {pcr::OPT_hoho_hints, "Pistas: Viejo Hoho", "Hints: Old Man Ho Ho",
     "Pistas del Viejo Hoho (en 10 islas).", "Places hints on Old Man Ho Ho (at 10 different islands)."},
    {pcr::OPT_fishmen_hints, "Pistas: hombres pez", "Hints: fishmen",
     "Pistas de los hombres pez (uno por isla; hay que darles cebo).",
     "Places hints on the fishmen (one at each island; feed them All-Purpose Bait first)."},
    {pcr::OPT_korl_hints, "Pistas: Mascarón Rojo", "Hints: King of Red Lions",
     "Pistas del Mascarón Rojo al hablar con él.", "Places hints on the King of Red Lions."},
    {pcr::OPT_num_item_hints, "Pistas de objeto", "Item hints",
     "Cuántas pistas dicen en qué zona está un objeto de progreso.",
     "How many hints tell you which area contains a particular progress item."},
    {pcr::OPT_num_location_hints, "Pistas de lugar", "Location hints",
     "Cuántas pistas dicen qué objeto hay en un lugar concreto.",
     "How many hints tell you what item is at a specific location."},
    {pcr::OPT_num_barren_hints, "Pistas de zona vacía", "Barren hints",
     "Cuántas pistas dicen que una zona no tiene nada necesario.",
     "How many hints tell you that an area does not contain any required items."},
    {pcr::OPT_num_path_hints, "Pistas de camino", "Path hints",
     "Cuántas pistas dicen que una zona tiene algo necesario para llegar a una meta.",
     "How many hints tell you that an area contains an item required to reach a particular goal."},
    {pcr::OPT_cryptic_hints, "Pistas crípticas", "Cryptic hints",
     "Las pistas no nombran lugares ni objetos directamente.",
     "Hints are phrased cryptically instead of naming locations and items directly."},
    {pcr::OPT_prioritize_remote_hints, "Priorizar lugares remotos", "Prioritize remote hints",
     "Los lugares apartados y largos de completar tienen prioridad en las pistas de lugar.",
     "Out-of-the-way, time-consuming locations take precedence over normal location hints."},
    {pcr::OPT_hint_importance, "Importancia en las pistas", "Hint importance",
     "Las pistas de objeto y de lugar dicen si el objeto es necesario, quizá necesario o no necesario.",
     "Item and location hints also tell whether the item is required, possibly required, or not required."},
    {pcr::OPT_swift_sail, "Vela rápida", "Swift sail",
     "Se navega al doble de velocidad y el viento siempre sopla a favor con la vela puesta.",
     "Sailing speed is doubled and the wind is always at your back while the sail is out."},
    {pcr::OPT_instant_text_boxes, "Texto instantáneo", "Instant text boxes",
     "Los textos aparecen de golpe.", "Text appears instantly."},
    {pcr::OPT_reveal_full_sea_chart, "Carta náutica completa", "Reveal full sea chart",
     "La carta náutica empieza dibujada entera.", "Start the game with the sea chart fully drawn out."},
    {pcr::OPT_add_shortcut_warps_between_dungeons, "Atajos entre mazmorras", "Shortcut warps between dungeons",
     "Nuevas vasijas de viaje que unen las mazmorras (Cueva del Dragón, Bosque Prohibido, Torre de los "
     "Dioses; y Fortaleza Maldita, Templo de la Tierra, Templo del Viento).",
     "New warp pots connecting the dungeons directly (DRC, FW, TotG, and separately FF, ET, WT)."},
    {pcr::OPT_skip_rematch_bosses, "Sin revancha de jefes", "Skip rematch bosses",
     "Quita la puerta de la Torre de Ganon que solo se abre tras vencer de nuevo a Gohma, Kalle Demos, "
     "Jalhalla y Molgera.",
     "Removes the door in Ganon's Tower that only unlocks after the rematches against Gohma, Kalle Demos, "
     "Jalhalla and Molgera."},
    {pcr::OPT_invert_sea_compass_x_axis, "Brújula del mar invertida", "Invert sea compass X axis",
     "Invierte el este y el oeste de la brújula que se ve en el mar.",
     "Inverts the east-west direction of the compass shown at sea."},
    {pcr::OPT_remove_music, "Sin música", "Remove music", "Silencia toda la música del juego.",
     "Mutes all ingame music."},
    {pcr::OPT_switch_targeting_mode, "Fijar con L: alternar", "Switch targeting mode",
     "Las partidas nuevas empiezan con la fijación con L en modo alternar, no mantener.",
     "New save files start with L-targeting set to 'Switch' instead of 'Hold'."},
    {pcr::OPT_num_starting_triforce_shards, "Fragmentos de Trifuerza iniciales", "Starting Triforce Shards",
     "Con cuántos fragmentos de la Trifuerza empiezas.", "The number of Triforce Shards you start with."},
    {pcr::OPT_starting_pohs, "Piezas de corazón iniciales", "Starting heart pieces",
     "Cuántas piezas de corazón extra tienes al empezar.", "Extra pieces of heart that you start with."},
    {pcr::OPT_starting_hcs, "Contenedores de corazón iniciales", "Starting heart containers",
     "Cuántos contenedores de corazón tienes al empezar.", "Heart containers that you start with."},
    {pcr::OPT_num_extra_starting_items, "Objetos de progreso extra", "Extra starting items",
     "Cuántos objetos de progreso al azar tienes además al empezar.",
     "Extra random progression items that you start with."},
    {pcr::OPT_do_not_generate_spoiler_log, "Sin registro de soluciones", "No spoiler log",
     "No se escribe el registro con dónde está cada objeto; también cambia dónde se colocan. Estas semillas "
     "no coinciden con las de wwrando oficial.",
     "No log listing where every item is; this also changes where items are placed. Such seeds do not "
     "match official wwrando seeds."},
};

const OptText* textOf(int id) {
    for (const OptText& t : kText) {
        if (t.id == id) {
            return &t;
        }
    }
    return nullptr;
}

struct EnumLabel {
    int id;
    const char* es[4];
    const char* en[4];
};
const EnumLabel kEnumLabels[] = {
    {pcr::OPT_sword_mode,
     {"Empezar con espada", "Sin espada al empezar", "Sin espadas"},
     {"Start with Hero's Sword", "No starting sword", "Swordless"}},
    {pcr::OPT_mix_entrances,
     {"Mazmorras aparte", "Todo mezclado"},
     {"Separate dungeons", "Mix dungeons, caves and fountains"}},
    {pcr::OPT_logic_obscurity,
     {"Ninguno", "Normal", "Difícil", "Muy difícil"},
     {"None", "Normal", "Hard", "Very hard"}},
    {pcr::OPT_logic_precision,
     {"Ninguno", "Normal", "Difícil", "Muy difícil"},
     {"None", "Normal", "Hard", "Very hard"}},
};

const char* enumLabel(int id, int index, bool es) {
    for (const EnumLabel& e : kEnumLabels) {
        if (e.id == id && index >= 0 && index < 4 && e.es[index] != nullptr) {
            return es ? e.es[index] : e.en[index];
        }
    }
    return "?";
}

// ---- registration ---------------------------------------------------------------------------------

constexpr int kGearOrder = 9000;

const PcSettingChoice kBoolChoices[] = {{"0", "Desactivado", "Off"}, {"1", "Activado", "On"}};

bool sRegistered = false;
std::deque<std::string> sStrings;                    // keys, defaults and int labels (stable)
std::deque<std::vector<PcSettingChoice>> sChoices;   // per-option choices (stable)
std::vector<RandoMenuAction> sActions;
bool sGearOpen = false;
bool sScripted = false;
// The settings as Options, read once per randoMenuRows call (the menu builds its rows a few times a
// frame) and after each change, instead of once per row.
bool sHaveCache = false;
pcr::Options sRaw;
pcr::Options sValid;

const char* keep(const std::string& s) {
    sStrings.push_back(s);
    return sStrings.back().c_str();
}

std::string getter(const char* key, void*) {
    return pc_settings_get(key);
}

void refresh() {
    sRaw = randoSettingsOptions(false);
    sValid = sRaw;
    pcr::ensureValidCombination(sValid, true); // R7.4d: allowUnbeatable (the enemy randomizer row)
    sHaveCache = true;
}

const pcr::Options& raw() {
    if (!sHaveCache) {
        refresh();
    }
    return sRaw;
}

const pcr::Options& valid() {
    if (!sHaveCache) {
        refresh();
    }
    return sValid;
}

} // namespace

void randoSettingsRegister() {
    if (sRegistered) {
        return;
    }
    sRegistered = true;
    const pcr::Options defaults;
    int count = 0;
    for (int id = 0; id < pcr::OPT_COUNT; id++) {
        if (!pcr::settingExposed(id)) {
            continue;
        }
        const pcr::OptionInfo& info = pcr::kOptionInfo[id];
        const OptText* t = textOf(id);
        PcSettingDesc d{};
        d.key = keep(pcr::settingKey(id));
        d.label = t != nullptr ? t->labelEs : info.name;
        d.labelEn = t != nullptr ? t->labelEn : info.name;
        d.help = t != nullptr ? t->helpEs : "";
        d.helpEn = t != nullptr ? t->helpEn : "";
        d.tab = PC_SETTING_TAB_RANDO;
        d.flags = 0;
        d.defaultValue = keep(pcr::settingValue(defaults, id));
        d.order = 10 + id * 10;
        if (info.kind == pcr::OptKind::Bool) {
            d.choices = kBoolChoices;
            d.choiceCount = 2;
        } else if (info.kind == pcr::OptKind::Enum) {
            int n = 0;
            const char* const* tokens = pcr::enumTokens(id, &n);
            sChoices.emplace_back();
            for (int i = 0; i < n; i++) {
                sChoices.back().push_back({tokens[i], enumLabel(id, i, true), enumLabel(id, i, false)});
            }
            d.choices = sChoices.back().data();
            d.choiceCount = n;
        } else {
            sChoices.emplace_back();
            for (int v = info.minimum; v <= info.maximum; v++) {
                const char* s = keep(std::to_string(v));
                sChoices.back().push_back({s, s, s});
            }
            d.choices = sChoices.back().data();
            d.choiceCount = (int)sChoices.back().size();
        }
        pc_settings_register(&d);
        count++;
    }
    PcSettingDesc g{};
    g.key = pcr::kStartingGearKey;
    g.label = "Equipo inicial";
    g.labelEn = "Starting gear";
    g.help = "Los objetos con los que empieza la partida; los demás se reparten al azar.";
    g.helpEn = "The items in your inventory at the start of a new game; the rest are randomized.";
    g.tab = PC_SETTING_TAB_RANDO;
    g.defaultValue = keep(pcr::gearStored(defaults));
    g.order = kGearOrder;
    pc_settings_register(&g);
    logf("[cos] rando: %d option settings and %s registered (tab Randomizer)%s\n", count, pcr::kStartingGearKey,
         kRegionOk ? "" : "; this region has no randomizer yet (D16)");
}

pcr::Options randoSettingsOptions(bool validated) {
    pcr::Options o = pcr::optionsFromSettings(getter, nullptr);
    if (validated) {
        pcr::ensureValidCombination(o, true); // R7.4d: allowUnbeatable (the enemy randomizer row)
    }
    return o;
}

int randoSettingsApply(const pcr::Options& o) {
    int locked = 0;
    for (int id = 0; id < pcr::OPT_COUNT; id++) {
        if (!pcr::settingExposed(id)) {
            continue;
        }
        const std::string key = pcr::settingKey(id);
        const std::string v = pcr::settingValue(o, id);
        if (v == pc_settings_get(key.c_str())) {
            continue;
        }
        if (pc_settings_locked(key.c_str())) {
            locked++;
            continue;
        }
        pc_settings_set(key.c_str(), v.c_str());
    }
    const std::string gear = pcr::gearStored(o);
    if (gear != pc_settings_get(pcr::kStartingGearKey)) {
        if (pc_settings_locked(pcr::kStartingGearKey)) {
            locked++;
        } else {
            pc_settings_set(pcr::kStartingGearKey, gear.c_str());
        }
    }
    return locked;
}

// ---- the tab's rows -------------------------------------------------------------------------------

void randoMenuAddAction(const RandoMenuAction& action) {
    sActions.push_back(action);
    std::stable_sort(sActions.begin(), sActions.end(),
                     [](const RandoMenuAction& a, const RandoMenuAction& b) { return a.order < b.order; });
}

void randoMenuSetScripted(bool on) {
    sScripted = on;
}

bool randoMenuScripted() {
    return sScripted;
}

namespace {
std::vector<RandoRow> sRows; // the last randoMenuRows
} // namespace

RandoRow randoMenuRowAt(int index) {
    if (sRows.empty()) {
        randoMenuRows();
    }
    return index >= 0 && index < (int)sRows.size() ? sRows[index] : RandoRow{RandoRowKind::Notice};
}

std::vector<RandoRow> randoMenuRows() {
    refresh();
    std::vector<RandoRow> rows;
    if (!kRegionOk) {
        rows.push_back({RandoRowKind::Notice});
    }
    // R8.2: page 0's actions and page 1's early ones (order < kRandoOrderLate) come first.
    for (int a = 0; a < (int)sActions.size(); a++) {
        if (sActions[a].order < kRandoOrderLate && (sActions[a].shown == nullptr || sActions[a].shown())) {
            rows.push_back({RandoRowKind::Action, a});
        }
    }
    rows.push_back({RandoRowKind::Preset});
    for (int id = 0; id < pcr::OPT_COUNT; id++) {
        if (id == pcr::OPT_starting_gear) {
            rows.push_back({RandoRowKind::GearHeader});
            if (sGearOpen) {
                for (int g = 0; g < pcr::gearItemCount(); g++) {
                    rows.push_back({RandoRowKind::Gear, g});
                }
            }
        }
        if (pcr::settingExposed(id)) {
            rows.push_back({RandoRowKind::Option, id});
        }
    }
    for (int a = 0; a < (int)sActions.size(); a++) {
        if (sActions[a].order >= kRandoOrderLate && (sActions[a].shown == nullptr || sActions[a].shown())) {
            rows.push_back({RandoRowKind::Action, a});
        }
    }
    sRows = rows;
    return rows;
}

int randoRowPage(const RandoRow& r) {
    if (r.kind == RandoRowKind::Notice) {
        return -1;
    }
    if (r.kind == RandoRowKind::Action && r.id >= 0 && r.id < (int)sActions.size() && sActions[r.id].order < 0) {
        return kRandoPageGame;
    }
    return kRandoPageNext;
}

std::vector<int> randoMenuPageRows(int page) {
    const std::vector<RandoRow> rows = randoMenuRows();
    std::vector<int> out;
    for (int i = 0; i < (int)rows.size(); i++) {
        const int p = randoRowPage(rows[i]);
        if (p < 0 || p == page) {
            out.push_back(i);
        }
    }
    return out;
}

std::string randoRowKey(const RandoRow& r) {
    return r.kind == RandoRowKind::Option ? pcr::settingKey(r.id) : std::string();
}

std::string randoRowLabel(const RandoRow& r, bool es) {
    switch (r.kind) {
    case RandoRowKind::Notice:
        return L(es, "Randomizer no disponible", "Randomizer not available");
    case RandoRowKind::Preset:
        return L(es, "Preajuste", "Preset");
    case RandoRowKind::Option: {
        const OptText* t = textOf(r.id);
        return t != nullptr ? L(es, t->labelEs, t->labelEn) : pcr::kOptionInfo[r.id].name;
    }
    case RandoRowKind::GearHeader:
        return L(es, "Equipo inicial", "Starting gear");
    case RandoRowKind::Gear:
        return std::string("   ") + pcr::gearItem(r.id).name; // wwrando's item names
    case RandoRowKind::Action:
        return r.id < (int)sActions.size() ? L(es, sActions[r.id].labelEs, sActions[r.id].labelEn) : "";
    }
    return "";
}

std::string randoRowValue(const RandoRow& r, bool es) {
    const pcr::Options& v = valid(); // what a seed would get (greyed rows too)
    switch (r.kind) {
    case RandoRowKind::Notice:
        return L(es, "solo disco USA", "USA disc only");
    case RandoRowKind::Preset: {
        return pcr::presetName(pcr::presetMatch(raw()), es);
    }
    case RandoRowKind::Option: {
        const pcr::OptionInfo& info = pcr::kOptionInfo[r.id];
        const int x = pcr::optionGetInt(v, r.id);
        if (info.kind == pcr::OptKind::Bool) {
            return x ? L(es, "Activado", "On") : L(es, "Desactivado", "Off");
        }
        if (info.kind == pcr::OptKind::Enum) {
            return enumLabel(r.id, x, es);
        }
        return std::to_string(x);
    }
    case RandoRowKind::GearHeader: {
        char text[96];
        snprintf(text, sizeof text, es ? "%d objetos (A: %s)" : "%d items (A: %s)", (int)v.starting_gear.size(),
                 sGearOpen ? L(es, "ocultar", "hide") : L(es, "mostrar", "show"));
        return text;
    }
    case RandoRowKind::Gear: {
        const pcr::GearItem g = pcr::gearItem(r.id);
        const int n = pcr::gearCount(v, g.name);
        if (g.max == 1) {
            return n ? L(es, "Sí", "Yes") : L(es, "No", "No");
        }
        return std::to_string(n) + " / " + std::to_string(g.max);
    }
    case RandoRowKind::Action:
        if (r.id < (int)sActions.size() && sActions[r.id].value != nullptr) {
            return sActions[r.id].value(es);
        }
        return L(es, "A: elegir", "A: choose");
    }
    return "";
}

std::string randoRowHelp(const RandoRow& r, bool es) {
    switch (r.kind) {
    case RandoRowKind::Notice:
        return L(es, "El randomizer solo funciona con el disco USA (GZLE01) por ahora.",
                 "The randomizer only works with the USA disc (GZLE01) for now.");
    case RandoRowKind::Preset:
        return L(es,
                 "Pone todas las opciones de esta pestaña: los valores de wwrando, todas las categorías (todas "
                 "las ubicaciones), o solo mazmorras y regalos (partida corta). Después se puede cambiar cada una.",
                 "Sets every option of this tab: wwrando's defaults, every category (all checks), or only "
                 "dungeons and free gifts (a short game). Each option can be changed afterwards.");
    case RandoRowKind::Option: {
        const OptText* t = textOf(r.id);
        return t != nullptr ? L(es, t->helpEs, t->helpEn) : "";
    }
    case RandoRowKind::GearHeader:
        return L(es,
                 "Los objetos con los que empieza la partida; el resto se reparte al azar. A muestra u oculta "
                 "la lista.",
                 "The items in your inventory at the start of a new game; the rest are randomized. A shows or "
                 "hides the list.");
    case RandoRowKind::Gear:
        return L(es, "Empezar la partida con este objeto (los progresivos: cuántos niveles).",
                 "Start the game with this item (progressive items: how many levels).");
    case RandoRowKind::Action:
        return r.id < (int)sActions.size() ? L(es, sActions[r.id].helpEs, sActions[r.id].helpEn) : "";
    }
    return "";
}

std::string randoRowDisabled(const RandoRow& r, bool es) {
    if (r.kind == RandoRowKind::Notice) {
        return "";
    }
    if (!kRegionOk) {
        return L(es, "solo con el disco USA", "USA disc only");
    }
    const pcr::Options& v = valid();
    switch (r.kind) {
    case RandoRowKind::Option: {
        if (pc_settings_locked(pcr::settingKey(r.id).c_str())) {
            return L(es, "fijado por variable de entorno", "set by an environment variable");
        }
        if (pcr::optionEnabled(v, r.id)) {
            return "";
        }
        switch (r.id) {
        case pcr::OPT_required_bosses:
            return L(es, "necesita la categoría mazmorras", "needs the dungeons category");
        case pcr::OPT_num_required_bosses:
            return L(es, "necesita jefes obligatorios", "needs required bosses");
        case pcr::OPT_prioritize_remote_hints:
            return L(es, "necesita pistas de lugar", "needs location hints");
        case pcr::OPT_mix_entrances:
            return L(es, "necesita mezclar entradas de mazmorra y de cueva o fuente",
                     "needs both dungeon and cave or fountain entrances randomized");
        default:
            return L(es, "no disponible", "not available");
        }
    }
    case RandoRowKind::Gear:
    case RandoRowKind::GearHeader:
        if (pc_settings_locked(pcr::kStartingGearKey)) {
            return L(es, "fijado por variable de entorno", "set by an environment variable");
        }
        if (r.kind == RandoRowKind::Gear && !pcr::gearEnabled(v, pcr::gearItem(r.id).name)) {
            return L(es, "no con este modo de espada", "not with this sword mode");
        }
        return "";
    default:
        return "";
    }
}

bool randoRowCycles(const RandoRow& r) {
    return r.kind == RandoRowKind::Preset || r.kind == RandoRowKind::Option || r.kind == RandoRowKind::Gear;
}

std::string randoRowActivate(const RandoRow& r, int dir, bool es) {
    if (r.kind == RandoRowKind::Notice) {
        return "";
    }
    refresh(); // the settings now, not as the menu last read them
    const std::string why = randoRowDisabled(r, es);
    if (!why.empty()) {
        return randoRowLabel(r, es) + ": " + why;
    }
    const int step = dir < 0 ? -1 : 1;
    sHaveCache = false; // every change below writes settings
    switch (r.kind) {
    case RandoRowKind::Preset: {
        const int cur = pcr::presetMatch(randoSettingsOptions(false));
        const int next = cur < 0 ? (step > 0 ? 0 : pcr::kPresetCount - 1)
                                 : (cur + step + pcr::kPresetCount) % pcr::kPresetCount;
        const int locked = randoSettingsApply(pcr::presetOptions(next));
        logf("[cos] rando menu: preset %s applied (%d keys set by the environment kept)\n",
             pcr::presetName(next, false), locked);
        return std::string(L(es, "Preajuste: ", "Preset: ")) + pcr::presetName(next, es);
    }
    case RandoRowKind::Option: {
        const pcr::OptionInfo& info = pcr::kOptionInfo[r.id];
        pcr::Options o = randoSettingsOptions(false);
        int x = pcr::optionGetInt(o, r.id);
        if (info.kind == pcr::OptKind::Bool) {
            x = !x;
        } else {
            int lo = 0, hi = info.numChoices - 1;
            if (info.kind == pcr::OptKind::Int) {
                lo = info.minimum;
                hi = info.maximum;
            }
            x = x + step > hi ? lo : x + step < lo ? hi : x + step;
        }
        pcr::optionSetInt(o, r.id, x);
        const std::string key = pcr::settingKey(r.id);
        const std::string value = pcr::settingValue(o, r.id);
        logf("[cos] rando menu: %s -> %s\n", key.c_str(), value.c_str());
        pc_settings_set(key.c_str(), value.c_str());
        return "";
    }
    case RandoRowKind::GearHeader:
        if (dir == 0) {
            sGearOpen = !sGearOpen;
        }
        return "";
    case RandoRowKind::Gear: {
        pcr::Options o = randoSettingsOptions(false);
        const pcr::GearItem g = pcr::gearItem(r.id);
        const int n = pcr::gearCount(o, g.name);
        pcr::gearSetCount(o, g.name, n + step > g.max ? 0 : n + step < 0 ? g.max : n + step);
        const std::string gear = pcr::gearStored(o);
        logf("[cos] rando menu: %s %s -> %d\n", pcr::kStartingGearKey, g.name, pcr::gearCount(o, g.name));
        pc_settings_set(pcr::kStartingGearKey, gear.c_str());
        return "";
    }
    case RandoRowKind::Action:
        if (dir == 0 && r.id < (int)sActions.size() && sActions[r.id].activate != nullptr) {
            return sActions[r.id].activate(es);
        }
        return "";
    default:
        return "";
    }
}

} // namespace pc
