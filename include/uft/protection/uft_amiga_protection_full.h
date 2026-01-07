/**
 * @file uft_amiga_protection_full.h
 * @brief Complete Amiga Protection Registry - 194 Handlers
 * @version 2.0.0
 * @date 2026-01-06
 *
 * Complete extraction of all Amiga copy protection formats.
 */

#ifndef UFT_AMIGA_PROTECTION_FULL_H
#define UFT_AMIGA_PROTECTION_FULL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Protection Type Enumeration (194 types)
 *============================================================================*/

typedef enum {
    /* Standard */
    UFT_PROT_AMIGADOS = 0,
    UFT_PROT_AMIGADOS_EXTENDED,
    
    /* Major Protection Systems (10-29) */
    UFT_PROT_COPYLOCK = 10,
    UFT_PROT_COPYLOCK_OLD,
    UFT_PROT_SPEEDLOCK,
    UFT_PROT_LONGTRACK,
    UFT_PROT_RNC_DUALFORMAT,
    UFT_PROT_RNC_TRIFORMAT,
    UFT_PROT_RNC_GAP,
    UFT_PROT_RNC_PROTECT_PROCESS,
    UFT_PROT_SOFTLOCK_DUALFORMAT,
    UFT_PROT_XTROLL_DUALFORMAT,
    
    /* Publisher: Psygnosis (30-39) */
    UFT_PROT_PSYGNOSIS_A = 30,
    UFT_PROT_PSYGNOSIS_B,
    UFT_PROT_PSYGNOSIS_C,
    UFT_PROT_SHADOW_BEAST,
    UFT_PROT_SHADOW_BEAST_II,
    UFT_PROT_BARBARIAN_II,
    UFT_PROT_BAAL,
    UFT_PROT_OBLITERATOR,
    
    /* Publisher: Factor 5 (40-49) */
    UFT_PROT_FACTOR5 = 40,
    UFT_PROT_TURRICAN,
    UFT_PROT_TURRICAN_II,
    UFT_PROT_BC_KID,
    UFT_PROT_DENARIS,
    
    /* Publisher: Rainbow Arts (50-59) */
    UFT_PROT_RAINBOW_ARTS = 50,
    UFT_PROT_GRAND_MONSTER_SLAM,
    UFT_PROT_ROCK_N_ROLL,
    UFT_PROT_X_OUT,
    UFT_PROT_KATAKIS,
    
    /* Publisher: Thalion (60-69) */
    UFT_PROT_THALION = 60,
    UFT_PROT_LIONHEART,
    UFT_PROT_AMBERMOON,
    UFT_PROT_TREX_WARRIOR,
    UFT_PROT_CHAMBERS_OF_SHAOLIN,
    
    /* Publisher: Team17 (70-79) */
    UFT_PROT_ALIENBREED = 70,
    UFT_PROT_SUPERFROG,
    UFT_PROT_PROJECT_X,
    UFT_PROT_BODY_BLOWS,
    UFT_PROT_WORMS,
    
    /* Publisher: Gremlin (80-89) */
    UFT_PROT_GREMLIN = 80,
    UFT_PROT_DISPOSABLE_HERO,
    UFT_PROT_ZOOL,
    UFT_PROT_PREMIER_MANAGER,
    UFT_PROT_LOTUS_TURBO,
    
    /* Publisher: Firebird/MicroProse (90-99) */
    UFT_PROT_FIREBIRD = 90,
    UFT_PROT_ELITE,
    UFT_PROT_CARRIER_COMMAND,
    UFT_PROT_MIDWINTER,
    UFT_PROT_GUNSHIP,
    
    /* Publisher: Cinemaware (100-109) */
    UFT_PROT_CINEMAWARE = 100,
    UFT_PROT_DEFENDER_OF_CROWN,
    UFT_PROT_IT_CAME_FROM_DESERT,
    UFT_PROT_WINGS,
    UFT_PROT_TV_SPORTS,
    
    /* Publisher: Ocean (110-119) */
    UFT_PROT_OCEAN = 110,
    UFT_PROT_ROBOCOP,
    UFT_PROT_BATMAN_MOVIE,
    UFT_PROT_SHADOW_WARRIORS,
    UFT_PROT_CHASE_HQ,
    
    /* Publisher: US Gold (120-129) */
    UFT_PROT_US_GOLD = 120,
    UFT_PROT_OUTRUN,
    UFT_PROT_GAUNTLET,
    UFT_PROT_STRIDER,
    UFT_PROT_FORGOTTEN_WORLDS,
    
    /* Publisher: Sensible Software (130-139) */
    UFT_PROT_SENSIBLE = 130,
    UFT_PROT_SENSIBLE_SOCCER,
    UFT_PROT_CANNON_FODDER,
    UFT_PROT_MEGA_LO_MANIA,
    UFT_PROT_WIZKID,
    
    /* Publisher: DMA Design (140-149) */
    UFT_PROT_DMA_DESIGN = 140,
    UFT_PROT_LEMMINGS,
    UFT_PROT_LEMMINGS_2,
    UFT_PROT_WALKER,
    UFT_PROT_HIRED_GUNS,
    
    /* Publisher: Bitmap Brothers (150-159) */
    UFT_PROT_BITMAP_BROS = 150,
    UFT_PROT_XENON,
    UFT_PROT_XENON_2,
    UFT_PROT_SPEEDBALL,
    UFT_PROT_SPEEDBALL_2,
    UFT_PROT_GODS,
    UFT_PROT_MAGIC_POCKETS,
    UFT_PROT_CHAOS_ENGINE,
    
    /* Publisher: Core Design (160-169) */
    UFT_PROT_CORE_DESIGN = 160,
    UFT_PROT_RICK_DANGEROUS,
    UFT_PROT_RICK_DANGEROUS_2,
    UFT_PROT_THUNDERHAWK,
    UFT_PROT_HEIMDALL,
    
    /* Publisher: Ubi Soft (170-179) */
    UFT_PROT_UBI_SOFT = 170,
    UFT_PROT_ZOMBI,
    UFT_PROT_BAT,
    UFT_PROT_UNREAL,
    UFT_PROT_IRON_LORD,
    
    /* Publisher: Infogrames (180-189) */
    UFT_PROT_INFOGRAMES = 180,
    UFT_PROT_NORTH_SOUTH,
    UFT_PROT_HOSTAGES,
    UFT_PROT_DRAKKHEN,
    UFT_PROT_ALPHA_WAVES,
    
    /* Publisher: Konami (190-199) */
    UFT_PROT_KONAMI = 190,
    UFT_PROT_SALAMANDER,
    UFT_PROT_NEMESIS_GRADIUS,
    UFT_PROT_AJAX,
    UFT_PROT_BLADES_OF_STEEL,
    
    /* Publisher: Electronic Arts (200-209) */
    UFT_PROT_EA = 200,
    UFT_PROT_POPULOUS,
    UFT_PROT_POWERMONGER,
    UFT_PROT_DESERT_STRIKE,
    UFT_PROT_ROAD_RASH,
    
    /* Publisher: Millennium (210-219) */
    UFT_PROT_MILLENNIUM = 210,
    UFT_PROT_JAMES_POND,
    UFT_PROT_JAMES_POND_2,
    UFT_PROT_JAMES_POND_3,
    UFT_PROT_ROBIN_HOOD,
    
    /* Game-specific protections (220-319) */
    UFT_PROT_1000CC = 220,
    UFT_PROT_ACID,
    UFT_PROT_ADLS,
    UFT_PROT_AK_AVALON,
    UFT_PROT_ALADDINS_MAGIC_LAMP,
    UFT_PROT_ALBEDO,
    UFT_PROT_ALDERAN,
    UFT_PROT_ALIEN_LEGION,
    UFT_PROT_ALTERNATIVE,
    UFT_PROT_AMEGAS_HIGH_SCORES,
    UFT_PROT_ANTCLIFFE,
    UFT_PROT_ARC_DEVELOPMENT,
    UFT_PROT_ARMOURGEDDON,
    UFT_PROT_AUNT_ARCTIC,
    UFT_PROT_AWESOME_DEMO,
    UFT_PROT_BATTLE_SQUADRON,
    UFT_PROT_BLUE_BYTE,
    UFT_PROT_BOMBUZAL,
    UFT_PROT_BRIDES_OF_DRACULA,
    UFT_PROT_CARDIAXX,
    UFT_PROT_CHW,
    UFT_PROT_COBRA,
    UFT_PROT_COSMO_RANGER,
    UFT_PROT_CRACKDOWN,
    UFT_PROT_CREEPSOFT,
    UFT_PROT_CYBERBLAST,
    UFT_PROT_CYBERDOS,
    UFT_PROT_CYBERWORLD,
    UFT_PROT_DEEP_CORE,
    UFT_PROT_DEFLEKTOR,
    UFT_PROT_DEJA_VU_II,
    UFT_PROT_DELIVERANCE,
    UFT_PROT_DETECTOR,
    UFT_PROT_DISCOSCOPIE,
    UFT_PROT_DISCOVERY,
    UFT_PROT_DISKSPARE,
    UFT_PROT_DOMINATION,
    UFT_PROT_DUGGER,
    UFT_PROT_DUNGEON_MASTER,
    UFT_PROT_DYTER_07,
    UFT_PROT_EGO,
    UFT_PROT_ELFMANIA,
    UFT_PROT_EPIC,
    UFT_PROT_EXECUTIONER,
    UFT_PROT_EYE_OF_HORUS,
    UFT_PROT_FANTASTIC_VOYAGE,
    UFT_PROT_FANTASY_GAMES,
    UFT_PROT_FEARS,
    UFT_PROT_FED_FREE_TRADERS,
    UFT_PROT_FLIMBOS_QUEST,
    UFT_PROT_FULL_CONTACT,
    UFT_PROT_FUN_FACTORY,
    UFT_PROT_FUZZBALL,
    UFT_PROT_GARRISON,
    UFT_PROT_GHOST_BATTLE,
    UFT_PROT_GLADIATORS,
    UFT_PROT_GLOBULUS,
    UFT_PROT_GROLET,
    UFT_PROT_GUNSHOOT,
    UFT_PROT_HAMMERFIST,
    UFT_PROT_HELLFIRE_ATTACK,
    UFT_PROT_HELLWIG,
    UFT_PROT_HERNDON,
    UFT_PROT_ILYAD,
    UFT_PROT_INTACT,
    UFT_PROT_JEFF_SPANGENBERG,
    UFT_PROT_JUDGE_DREDD,
    UFT_PROT_KELLOGGS_LAND,
    UFT_PROT_KICKOFF2,
    UFT_PROT_KILLING_GAME_SHOW,
    UFT_PROT_LANKHOR,
    UFT_PROT_LIN_WU_CHALLENGE,
    UFT_PROT_LOGOTRON,
    UFT_PROT_MAGIC_MARBLE,
    UFT_PROT_MANIC_MINER,
    UFT_PROT_MEGA_HOCKEY,
    UFT_PROT_MENTASM,
    UFT_PROT_MICKEY_MOUSE,
    UFT_PROT_MICRODEAL,
    UFT_PROT_MOOCHIES,
    UFT_PROT_NIGHT_HUNTER,
    UFT_PROT_NIGHTDAWN,
    UFT_PROT_NINE_LIVES,
    UFT_PROT_NO_SECOND_PRIZE,
    UFT_PROT_NOVAGEN,
    UFT_PROT_OKAY,
    UFT_PROT_PDOS,
    UFT_PROT_PDOS_OLD,
    UFT_PROT_PERSIAN_GULF,
    UFT_PROT_PHANTOM_FIGHTER,
    UFT_PROT_PICK_N_PILE,
    UFT_PROT_PIERRE_ADANE,
    UFT_PROT_PIETER_OPDAM,
    UFT_PROT_PINBALL_DREAMS,
    UFT_PROT_PINBALL_FANTASIES,
    UFT_PROT_PINBALL_PRELUDE,
    UFT_PROT_PLAN9,
    UFT_PROT_PODPIERDZIELACZ,
    UFT_PROT_POWER_DRIFT,
    UFT_PROT_PRIME_MOVER,
    UFT_PROT_PRISON,
    UFT_PROT_PROJEKT_IKARUS,
    UFT_PROT_PROMIC,
    UFT_PROT_PUFFYS_SAGA,
    UFT_PROT_RAINBIRD,
    UFT_PROT_RALLYE_MASTER,
    UFT_PROT_RATT_DOS,
    UFT_PROT_RATTLEHEADS,
    UFT_PROT_READYSOFT,
    UFT_PROT_RHINO,
    UFT_PROT_ROME_AD,
    UFT_PROT_RTYPE,
    UFT_PROT_RUFFIAN,
    UFT_PROT_SALES_CURVE,
    UFT_PROT_SAVAGE,
    UFT_PROT_SEXTETT,
    UFT_PROT_SHUFFLE,
    UFT_PROT_SILKWORM,
    UFT_PROT_SINK_OR_SWIM,
    UFT_PROT_SKAERMTROLDEN_HUGO,
    UFT_PROT_SLIDERS,
    UFT_PROT_SLIDING_SKILL,
    UFT_PROT_SMARTDOS,
    UFT_PROT_SOFT_TOUCH,
    UFT_PROT_SPECIAL_FX,
    UFT_PROT_SS_MFM,
    UFT_PROT_STAR_TRASH,
    UFT_PROT_STARDUST,
    UFT_PROT_STARRAY,
    UFT_PROT_STEIGENBERGER,
    UFT_PROT_STREET_GANG,
    UFT_PROT_STREET_HOCKEY,
    UFT_PROT_SUMMER_GAMES,
    UFT_PROT_SUPAPLEX,
    UFT_PROT_SUPER_HANG_ON,
    UFT_PROT_TAEKWONDO_MASTER,
    UFT_PROT_TANK_BUSTER,
    UFT_PROT_TECH,
    UFT_PROT_TELEXPRESS,
    UFT_PROT_THE_PLAGUE,
    UFT_PROT_TLK_DOS,
    UFT_PROT_TOLTEKA,
    UFT_PROT_TOPOSOFT,
    UFT_PROT_TRACKER,
    UFT_PROT_TURN_IT,
    UFT_PROT_TYPHOON_THOMPSON,
    UFT_PROT_UNIVERSAL_WARRIOR,
    UFT_PROT_USS_JOHN_YOUNG,
    UFT_PROT_VADE_RETRO,
    UFT_PROT_VAMPIRES_EMPIRE,
    UFT_PROT_VISIONARY_DESIGN,
    UFT_PROT_WAYNE_GRETZKY,
    UFT_PROT_WJS_DESIGN,
    UFT_PROT_WORLD_BOXING,
    UFT_PROT_XORRON_2001,
    UFT_PROT_YO_JOE,
    UFT_PROT_ZGZ,
    UFT_PROT_ZYCONIX,
    UFT_PROT_ZZKJ,
    
    /* Sentinel */
    UFT_PROT_COUNT
} uft_amiga_prot_type_t;

/*============================================================================
 * Sync Pattern Definitions
 *============================================================================*/

#define UFT_SYNC_AMIGA_STD      0x4489      /* Standard Amiga sync */
#define UFT_SYNC_AMIGA_ALT      0x448A      /* Alternative sync */
#define UFT_SYNC_AMIGA_DOUBLE   0x448A448A  /* Double sync (AmigaDOS) */
#define UFT_SYNC_EGO            0x8951      /* Ego/Dyter07 protection */
#define UFT_SYNC_QUARTZ         0x8944      /* Quartz protection */
#define UFT_SYNC_FACTOR5        0x4124      /* Factor 5 protection */
#define UFT_SYNC_MAGIC          0x5242      /* Magic protection */
#define UFT_SYNC_SPECIAL        0x4454      /* Special protection */
#define UFT_SYNC_TRACKER        0x92429242  /* Tracker protection */
#define UFT_SYNC_SEGA           0xAAAA4891  /* Sega conversions */
#define UFT_SYNC_CORE           0xA245      /* Core Design */

/*============================================================================
 * Protection Entry Structure
 *============================================================================*/

/**
 * @brief Protection entry for full registry
 */
typedef struct {
    uft_amiga_prot_type_t type;     /**< Protection type ID */
    const char* name;               /**< Protection name */
    const char* publisher;          /**< Publisher (NULL if game-specific) */
    uint32_t sync;                  /**< Primary sync pattern */
    uint8_t  key_track;             /**< Key track (0 = any) */
    uint16_t flags;                 /**< Protection flags */
} uft_prot_entry_t;

/* Protection flags */
#define UFT_PROT_FL_LONGTRACK   0x0001  /* Uses long tracks */
#define UFT_PROT_FL_TIMING      0x0002  /* Timing-based protection */
#define UFT_PROT_FL_WEAKBITS    0x0004  /* Uses weak bits */
#define UFT_PROT_FL_MULTIREV    0x0008  /* Requires multiple revolutions */
#define UFT_PROT_FL_DUALFORMAT  0x0010  /* Dual format disk */
#define UFT_PROT_FL_LFSR        0x0020  /* Uses LFSR verification */
#define UFT_PROT_FL_CUSTOM_MFM  0x0040  /* Custom MFM encoding */
#define UFT_PROT_FL_EXTRA_SECT  0x0080  /* Extra sectors */
#define UFT_PROT_FL_CRC_HACK    0x0100  /* CRC manipulation */
#define UFT_PROT_FL_INDEX       0x0200  /* Index-based protection */

/*============================================================================
 * Detection Result
 *============================================================================*/

/**
 * @brief Detection result with confidence
 */
typedef struct {
    uft_amiga_prot_type_t type;     /**< Detected protection */
    uint8_t  confidence;            /**< Confidence 0-100 */
    uint32_t sync_found;            /**< Sync pattern found */
    uint8_t  track;                 /**< Detection track */
    uint16_t flags;                 /**< Detected flags */
    char     name[64];              /**< Protection name */
    char     publisher[32];         /**< Publisher name */
} uft_prot_detect_result_t;

/*============================================================================
 * Track Signature for Detection
 *============================================================================*/

/**
 * @brief Track signature for protection detection
 */
typedef struct {
    uint8_t  track_num;             /**< Track number */
    uint8_t  side;                  /**< Side (0 or 1) */
    uint32_t track_length;          /**< Track length in bits */
    uint16_t sector_count;          /**< Number of sectors found */
    uint32_t sync_words[16];        /**< Sync words found */
    uint8_t  sync_count;            /**< Number of sync words */
    bool     has_weak_bits;         /**< Weak bits detected */
    bool     has_timing_variation;  /**< Timing variation detected */
    uint32_t min_cell_time;         /**< Min cell time in ns */
    uint32_t max_cell_time;         /**< Max cell time in ns */
} uft_track_signature_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Get protection registry
 * @param count Output: number of entries
 * @return Pointer to registry array
 */
const uft_prot_entry_t* uft_prot_get_registry(size_t* count);

/**
 * @brief Get protection entry by type
 * @param type Protection type
 * @return Entry or NULL if not found
 */
const uft_prot_entry_t* uft_prot_get_entry(uft_amiga_prot_type_t type);

/**
 * @brief Detect protection from track signatures
 * @param tracks Array of track signatures
 * @param num_tracks Number of tracks
 * @param results Output array for results
 * @param max_results Maximum results to return
 * @return Number of protections detected
 */
int uft_prot_detect(const uft_track_signature_t* tracks,
                    size_t num_tracks,
                    uft_prot_detect_result_t* results,
                    size_t max_results);

/**
 * @brief Detect protection from single track
 * @param track Track signature
 * @param result Output result
 * @return true if protection detected
 */
bool uft_prot_detect_track(const uft_track_signature_t* track,
                           uft_prot_detect_result_t* result);

/**
 * @brief Get protection name
 * @param type Protection type
 * @return Name string
 */
const char* uft_prot_name(uft_amiga_prot_type_t type);

/**
 * @brief Check if protection uses long tracks
 * @param type Protection type
 * @return true if uses long tracks
 */
bool uft_prot_is_longtrack(uft_amiga_prot_type_t type);

/**
 * @brief Check if protection requires timing analysis
 * @param type Protection type
 * @return true if timing-based
 */
bool uft_prot_needs_timing(uft_amiga_prot_type_t type);

/**
 * @brief Check if protection has weak bits
 * @param type Protection type
 * @return true if uses weak bits
 */
bool uft_prot_has_weak_bits(uft_amiga_prot_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_PROTECTION_FULL_H */
