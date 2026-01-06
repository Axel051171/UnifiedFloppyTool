/**
 * @file uft_amiga_protection_full.c
 * @brief Complete Amiga Protection Registry Implementation - 194 Handlers
 * @version 2.0.0
 * @date 2026-01-06
 *
 * Source: disk-utilities by Keir Fraser (Public Domain)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "uft/protection/uft_amiga_protection_full.h"

/*============================================================================
 * Complete Protection Registry - 194 Entries
 *============================================================================*/

static const uft_prot_entry_t protection_registry[] = {
    /* Standard AmigaDOS */
    {UFT_PROT_AMIGADOS, "AmigaDOS", NULL, UFT_SYNC_AMIGA_DOUBLE, 0, 0},
    {UFT_PROT_AMIGADOS_EXTENDED, "AmigaDOS Extended", NULL, UFT_SYNC_AMIGA_DOUBLE, 0, 0},
    
    /* Major Protection Systems */
    {UFT_PROT_COPYLOCK, "CopyLock", "Rob Northen", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_TIMING | UFT_PROT_FL_LFSR},
    {UFT_PROT_COPYLOCK_OLD, "CopyLock (Old)", "Rob Northen", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_TIMING | UFT_PROT_FL_LFSR},
    {UFT_PROT_SPEEDLOCK, "SpeedLock", "Speedlock Associates", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_TIMING},
    {UFT_PROT_LONGTRACK, "Long Track", NULL, UFT_SYNC_AMIGA_STD, 0, UFT_PROT_FL_LONGTRACK},
    {UFT_PROT_RNC_DUALFORMAT, "RNC Dualformat", "Rob Northen", UFT_SYNC_AMIGA_STD, 0, UFT_PROT_FL_DUALFORMAT},
    {UFT_PROT_RNC_TRIFORMAT, "RNC Triformat", "Rob Northen", UFT_SYNC_AMIGA_STD, 0, UFT_PROT_FL_DUALFORMAT},
    {UFT_PROT_RNC_GAP, "RNC Gap", "Rob Northen", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_RNC_PROTECT_PROCESS, "RNC Protect Process", "Rob Northen", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SOFTLOCK_DUALFORMAT, "Softlock Dualformat", NULL, UFT_SYNC_AMIGA_STD, 0, UFT_PROT_FL_DUALFORMAT},
    {UFT_PROT_XTROLL_DUALFORMAT, "X-Troll Dualformat", NULL, UFT_SYNC_AMIGA_STD, 0, UFT_PROT_FL_DUALFORMAT},
    
    /* Psygnosis */
    {UFT_PROT_PSYGNOSIS_A, "Psygnosis Type A", "Psygnosis", UFT_SYNC_AMIGA_STD, 79, 0},
    {UFT_PROT_PSYGNOSIS_B, "Psygnosis Type B", "Psygnosis", 0x8914, 79, UFT_PROT_FL_TIMING},
    {UFT_PROT_PSYGNOSIS_C, "Psygnosis Type C", "Psygnosis", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_LONGTRACK},
    {UFT_PROT_SHADOW_BEAST, "Shadow of the Beast", "Psygnosis", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_TIMING},
    {UFT_PROT_SHADOW_BEAST_II, "Shadow of the Beast II", "Psygnosis", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_TIMING},
    {UFT_PROT_BARBARIAN_II, "Barbarian II", "Psygnosis", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_BAAL, "Baal", "Psygnosis", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_OBLITERATOR, "Obliterator", "Psygnosis", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Factor 5 */
    {UFT_PROT_FACTOR5, "Factor 5", "Factor 5", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_TIMING},
    {UFT_PROT_TURRICAN, "Turrican", "Factor 5", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_TIMING},
    {UFT_PROT_TURRICAN_II, "Turrican II", "Factor 5", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_TIMING},
    {UFT_PROT_BC_KID, "BC Kid", "Factor 5", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DENARIS, "Denaris", "Factor 5", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Rainbow Arts */
    {UFT_PROT_RAINBOW_ARTS, "Rainbow Arts", "Rainbow Arts", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_GRAND_MONSTER_SLAM, "Grand Monster Slam", "Rainbow Arts", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ROCK_N_ROLL, "Rock 'n Roll", "Rainbow Arts", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_X_OUT, "X-Out", "Rainbow Arts", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_KATAKIS, "Katakis", "Rainbow Arts", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Thalion */
    {UFT_PROT_THALION, "Thalion", "Thalion", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_WEAKBITS},
    {UFT_PROT_LIONHEART, "Lionheart", "Thalion", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_WEAKBITS},
    {UFT_PROT_AMBERMOON, "Ambermoon", "Thalion", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_TREX_WARRIOR, "T-Rex Warrior", "Thalion", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_CHAMBERS_OF_SHAOLIN, "Chambers of Shaolin", "Thalion", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Team17 */
    {UFT_PROT_ALIENBREED, "Alien Breed", "Team17", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SUPERFROG, "Superfrog", "Team17", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PROJECT_X, "Project-X", "Team17", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_BODY_BLOWS, "Body Blows", "Team17", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_WORMS, "Worms", "Team17", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Gremlin */
    {UFT_PROT_GREMLIN, "Gremlin", "Gremlin", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_LONGTRACK},
    {UFT_PROT_DISPOSABLE_HERO, "Disposable Hero", "Gremlin", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_LONGTRACK},
    {UFT_PROT_ZOOL, "Zool", "Gremlin", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PREMIER_MANAGER, "Premier Manager", "Gremlin", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_LOTUS_TURBO, "Lotus Turbo Challenge", "Gremlin", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Firebird/MicroProse */
    {UFT_PROT_FIREBIRD, "Firebird", "Firebird", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ELITE, "Elite", "Firebird", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_TIMING},
    {UFT_PROT_CARRIER_COMMAND, "Carrier Command", "MicroProse", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_MIDWINTER, "Midwinter", "MicroProse", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_GUNSHIP, "Gunship", "MicroProse", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Cinemaware */
    {UFT_PROT_CINEMAWARE, "Cinemaware", "Cinemaware", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DEFENDER_OF_CROWN, "Defender of the Crown", "Cinemaware", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_IT_CAME_FROM_DESERT, "It Came From the Desert", "Cinemaware", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_WINGS, "Wings", "Cinemaware", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_TV_SPORTS, "TV Sports", "Cinemaware", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Ocean */
    {UFT_PROT_OCEAN, "Ocean", "Ocean", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ROBOCOP, "RoboCop", "Ocean", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_BATMAN_MOVIE, "Batman - The Movie", "Ocean", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SHADOW_WARRIORS, "Shadow Warriors", "Ocean", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_CHASE_HQ, "Chase HQ", "Ocean", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* US Gold */
    {UFT_PROT_US_GOLD, "US Gold", "US Gold", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_OUTRUN, "Out Run", "US Gold", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_GAUNTLET, "Gauntlet", "US Gold", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_STRIDER, "Strider", "US Gold", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_FORGOTTEN_WORLDS, "Forgotten Worlds", "US Gold", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Sensible */
    {UFT_PROT_SENSIBLE, "Sensible Software", "Sensible", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SENSIBLE_SOCCER, "Sensible Soccer", "Sensible", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_CANNON_FODDER, "Cannon Fodder", "Sensible", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_MEGA_LO_MANIA, "Mega Lo Mania", "Sensible", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_WIZKID, "Wizkid", "Sensible", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* DMA Design */
    {UFT_PROT_DMA_DESIGN, "DMA Design", "DMA Design", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_LEMMINGS, "Lemmings", "DMA Design", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_LEMMINGS_2, "Lemmings 2", "DMA Design", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_WALKER, "Walker", "DMA Design", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_HIRED_GUNS, "Hired Guns", "DMA Design", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Bitmap Brothers */
    {UFT_PROT_BITMAP_BROS, "Bitmap Brothers", "Bitmap Bros", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_XENON, "Xenon", "Bitmap Bros", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_XENON_2, "Xenon 2: Megablast", "Bitmap Bros", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SPEEDBALL, "Speedball", "Bitmap Bros", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SPEEDBALL_2, "Speedball 2", "Bitmap Bros", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_GODS, "Gods", "Bitmap Bros", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_MAGIC_POCKETS, "Magic Pockets", "Bitmap Bros", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_CHAOS_ENGINE, "The Chaos Engine", "Bitmap Bros", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Core Design */
    {UFT_PROT_CORE_DESIGN, "Core Design", "Core Design", UFT_SYNC_CORE, 0, 0},
    {UFT_PROT_RICK_DANGEROUS, "Rick Dangerous", "Core Design", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_RICK_DANGEROUS_2, "Rick Dangerous 2", "Core Design", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_THUNDERHAWK, "Thunderhawk", "Core Design", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_HEIMDALL, "Heimdall", "Core Design", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Ubi Soft */
    {UFT_PROT_UBI_SOFT, "Ubi Soft", "Ubi Soft", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ZOMBI, "Zombi", "Ubi Soft", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_BAT, "B.A.T.", "Ubi Soft", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_UNREAL, "Unreal", "Ubi Soft", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_IRON_LORD, "Iron Lord", "Ubi Soft", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Infogrames */
    {UFT_PROT_INFOGRAMES, "Infogrames", "Infogrames", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_NORTH_SOUTH, "North & South", "Infogrames", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_HOSTAGES, "Hostages", "Infogrames", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DRAKKHEN, "Drakkhen", "Infogrames", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ALPHA_WAVES, "Alpha Waves", "Infogrames", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Konami */
    {UFT_PROT_KONAMI, "Konami", "Konami", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SALAMANDER, "Salamander", "Konami", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_NEMESIS_GRADIUS, "Nemesis/Gradius", "Konami", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_AJAX, "Ajax", "Konami", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_BLADES_OF_STEEL, "Blades of Steel", "Konami", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Electronic Arts */
    {UFT_PROT_EA, "Electronic Arts", "EA", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_POPULOUS, "Populous", "EA", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_POWERMONGER, "Powermonger", "EA", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DESERT_STRIKE, "Desert Strike", "EA", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ROAD_RASH, "Road Rash", "EA", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Millennium */
    {UFT_PROT_MILLENNIUM, "Millennium", "Millennium", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_JAMES_POND, "James Pond", "Millennium", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_JAMES_POND_2, "James Pond 2: Robocod", "Millennium", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_JAMES_POND_3, "James Pond 3", "Millennium", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ROBIN_HOOD, "Robin Hood", "Millennium", UFT_SYNC_AMIGA_STD, 0, 0},
    
    /* Game-specific protections (alphabetical) */
    {UFT_PROT_1000CC, "1000cc", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ACID, "Acid", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ADLS, "ADLS", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_AK_AVALON, "AK Avalon", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ALADDINS_MAGIC_LAMP, "Aladdin's Magic Lamp", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ALBEDO, "Albedo", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ALDERAN, "Alderan", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ALIEN_LEGION, "Alien Legion", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ALTERNATIVE, "Alternative", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_AMEGAS_HIGH_SCORES, "Amegas High Scores", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ANTCLIFFE, "Antcliffe", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ARC_DEVELOPMENT, "Arc Development", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ARMOURGEDDON, "Armourgeddon", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_AUNT_ARCTIC, "Aunt Arctic Adventure", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_AWESOME_DEMO, "Awesome Demo", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_BATTLE_SQUADRON, "Battle Squadron", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_BLUE_BYTE, "Blue Byte", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_BOMBUZAL, "Bombuzal", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_BRIDES_OF_DRACULA, "Brides of Dracula", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_CARDIAXX, "Cardiaxx", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_CHW, "CHW", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_COBRA, "Cobra", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_COSMO_RANGER, "Cosmo Ranger", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_CRACKDOWN, "Crackdown", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_CREEPSOFT, "Creepsoft", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_CYBERBLAST, "Cyberblast", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_CYBERDOS, "CyberDOS", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_CYBERWORLD, "Cyberworld", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DEEP_CORE, "Deep Core", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DEFLEKTOR, "Deflektor", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DEJA_VU_II, "Deja Vu II", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DELIVERANCE, "Deliverance", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DETECTOR, "Detector", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DISCOSCOPIE, "Discoscopie", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DISCOVERY, "Discovery", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DISKSPARE, "DiskSpare", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DOMINATION, "Domination", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DUGGER, "Dugger", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_DUNGEON_MASTER, "Dungeon Master", "FTL Games", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_TIMING},
    {UFT_PROT_DYTER_07, "Dyter-07", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_EGO, "Ego", NULL, UFT_SYNC_EGO, 0, 0},
    {UFT_PROT_ELFMANIA, "Elfmania", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_EPIC, "Epic", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_EXECUTIONER, "Executioner", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_EYE_OF_HORUS, "Eye of Horus", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_FANTASTIC_VOYAGE, "Fantastic Voyage", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_FANTASY_GAMES, "Fantasy Games", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_FEARS, "Fears", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_FED_FREE_TRADERS, "Federation of Free Traders", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_FLIMBOS_QUEST, "Flimbo's Quest", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_FULL_CONTACT, "Full Contact", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_FUN_FACTORY, "Fun Factory", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_FUZZBALL, "Fuzzball", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_GARRISON, "Garrison", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_GHOST_BATTLE, "Ghost Battle", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_GLADIATORS, "Gladiators", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_GLOBULUS, "Globulus", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_GROLET, "Grolet", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_GUNSHOOT, "Gunshoot", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_HAMMERFIST, "Hammerfist", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_HELLFIRE_ATTACK, "Hellfire Attack", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_HELLWIG, "Hellwig", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_HERNDON, "Herndon", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ILYAD, "Ilyad", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_INTACT, "Intact", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_JEFF_SPANGENBERG, "Jeff Spangenberg", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_JUDGE_DREDD, "Judge Dredd", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_KELLOGGS_LAND, "Kellogg's Land", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_KICKOFF2, "Kick Off 2", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_KILLING_GAME_SHOW, "Killing Game Show", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_LANKHOR, "Lankhor", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_LIN_WU_CHALLENGE, "Lin Wu's Challenge", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_LOGOTRON, "Logotron", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_MAGIC_MARBLE, "Magic Marble", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_MANIC_MINER, "Manic Miner", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_MEGA_HOCKEY, "Mega Hockey", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_MENTASM, "Mentasm", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_MICKEY_MOUSE, "Mickey Mouse", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_MICRODEAL, "Microdeal", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_MOOCHIES, "Moochies", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_NIGHT_HUNTER, "Night Hunter", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_NIGHTDAWN, "Nightdawn", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_NINE_LIVES, "Nine Lives", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_NO_SECOND_PRIZE, "No Second Prize", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_NOVAGEN, "Novagen", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_OKAY, "Okay Protection", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PDOS, "PDOS", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PDOS_OLD, "PDOS (Old)", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PERSIAN_GULF, "Persian Gulf Inferno", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PHANTOM_FIGHTER, "Phantom Fighter", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PICK_N_PILE, "Pick 'n Pile", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PIERRE_ADANE, "Pierre Adane", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PIETER_OPDAM, "Pieter Opdam", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PINBALL_DREAMS, "Pinball Dreams", "21st Century", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PINBALL_FANTASIES, "Pinball Fantasies", "21st Century", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PINBALL_PRELUDE, "Pinball Prelude", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PLAN9, "Plan 9", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PODPIERDZIELACZ, "Podpierdzielacz", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_POWER_DRIFT, "Power Drift", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PRIME_MOVER, "Prime Mover", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PRISON, "Prison", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PROJEKT_IKARUS, "Projekt Ikarus", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PROMIC, "Promic", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_PUFFYS_SAGA, "Puffy's Saga", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_RAINBIRD, "Rainbird", "Rainbird", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_RALLYE_MASTER, "Rallye Master", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_RATT_DOS, "RATT DOS", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_RATTLEHEADS, "Rattleheads Disk Protector", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_READYSOFT, "ReadySoft", "ReadySoft", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_RHINO, "Rhino", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ROME_AD, "Rome AD", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_RTYPE, "R-Type", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_RUFFIAN, "Ruffian", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SALES_CURVE, "Sales Curve", "Sales Curve", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SAVAGE, "Savage", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SEXTETT, "Sextett", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SHUFFLE, "Shuffle", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SILKWORM, "Silkworm", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SINK_OR_SWIM, "Sink or Swim", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SKAERMTROLDEN_HUGO, "Skaermtrolden Hugo", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SLIDERS, "Sliders", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SLIDING_SKILL, "Sliding Skill", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SMARTDOS, "SmartDOS", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SOFT_TOUCH, "Soft Touch", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SPECIAL_FX, "Special FX", "Special FX", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SS_MFM, "SS MFM", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_STAR_TRASH, "Star Trash", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_STARDUST, "Stardust", "Bloodhouse", UFT_SYNC_AMIGA_STD, 79, UFT_PROT_FL_WEAKBITS},
    {UFT_PROT_STARRAY, "Starray", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_STEIGENBERGER, "Steigenberger Hotel Manager", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_STREET_GANG, "Street Gang", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_STREET_HOCKEY, "Street Hockey", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SUMMER_GAMES, "Summer Games", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SUPAPLEX, "Supaplex", "Digital Integration", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_SUPER_HANG_ON, "Super Hang-On", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_TAEKWONDO_MASTER, "Taekwondo Master", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_TANK_BUSTER, "Tank Buster", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_TECH, "Tech", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_TELEXPRESS, "Telexpress", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_THE_PLAGUE, "The Plague", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_TLK_DOS, "TLK DOS", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_TOLTEKA, "Tolteka", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_TOPOSOFT, "Toposoft", "Toposoft", UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_TRACKER, "Tracker", NULL, UFT_SYNC_TRACKER, 0, 0},
    {UFT_PROT_TURN_IT, "Turn It", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_TYPHOON_THOMPSON, "Typhoon Thompson", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_UNIVERSAL_WARRIOR, "Universal Warrior", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_USS_JOHN_YOUNG, "USS John Young", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_VADE_RETRO, "Vade Retro Alienas", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_VAMPIRES_EMPIRE, "Vampire's Empire", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_VISIONARY_DESIGN, "Visionary Design", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_WAYNE_GRETZKY, "Wayne Gretzky Hockey", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_WJS_DESIGN, "WJS Design", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_WORLD_BOXING, "World Championship Boxing", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_XORRON_2001, "Xorron 2001", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_YO_JOE, "Yo! Joe!", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ZGZ, "ZGZ", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ZYCONIX, "Zyconix", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
    {UFT_PROT_ZZKJ, "ZZKJ", NULL, UFT_SYNC_AMIGA_STD, 0, 0},
};

static const size_t registry_count = sizeof(protection_registry) / sizeof(protection_registry[0]);

/*============================================================================
 * API Implementation
 *============================================================================*/

const uft_prot_entry_t* uft_prot_get_registry(size_t* count)
{
    if (count) *count = registry_count;
    return protection_registry;
}

const uft_prot_entry_t* uft_prot_get_entry(uft_amiga_prot_type_t type)
{
    for (size_t i = 0; i < registry_count; i++) {
        if (protection_registry[i].type == type) {
            return &protection_registry[i];
        }
    }
    return NULL;
}

/**
 * @brief Calculate match score for detection
 */
static int calc_match_score(const uft_track_signature_t* track,
                            const uft_prot_entry_t* entry)
{
    int score = 0;
    
    /* Sync pattern match */
    for (int i = 0; i < track->sync_count && i < 16; i++) {
        if (track->sync_words[i] == entry->sync ||
            (track->sync_words[i] & 0xFFFF) == (entry->sync & 0xFFFF)) {
            score += 30;
            break;
        }
    }
    
    /* Key track match */
    if (entry->key_track > 0 && track->track_num == entry->key_track) {
        score += 25;
    }
    
    /* Longtrack flag */
    if ((entry->flags & UFT_PROT_FL_LONGTRACK) && track->track_length > 105000) {
        score += 20;
    }
    
    /* Timing flag */
    if ((entry->flags & UFT_PROT_FL_TIMING) && track->has_timing_variation) {
        score += 15;
    }
    
    /* Weak bits flag */
    if ((entry->flags & UFT_PROT_FL_WEAKBITS) && track->has_weak_bits) {
        score += 15;
    }
    
    return score;
}

int uft_prot_detect(const uft_track_signature_t* tracks,
                    size_t num_tracks,
                    uft_prot_detect_result_t* results,
                    size_t max_results)
{
    if (!tracks || !results || num_tracks == 0 || max_results == 0) {
        return 0;
    }
    
    /* Score all protections */
    typedef struct {
        int score;
        const uft_prot_entry_t* entry;
        const uft_track_signature_t* track;
    } match_t;
    
    match_t* matches = calloc(registry_count, sizeof(match_t));
    if (!matches) return 0;
    
    for (size_t i = 0; i < registry_count; i++) {
        const uft_prot_entry_t* entry = &protection_registry[i];
        int best_score = 0;
        const uft_track_signature_t* best_track = NULL;
        
        for (size_t t = 0; t < num_tracks; t++) {
            int score = calc_match_score(&tracks[t], entry);
            if (score > best_score) {
                best_score = score;
                best_track = &tracks[t];
            }
        }
        
        matches[i].score = best_score;
        matches[i].entry = entry;
        matches[i].track = best_track;
    }
    
    /* Sort by score */
    for (size_t i = 0; i < registry_count - 1; i++) {
        for (size_t j = 0; j < registry_count - i - 1; j++) {
            if (matches[j].score < matches[j + 1].score) {
                match_t tmp = matches[j];
                matches[j] = matches[j + 1];
                matches[j + 1] = tmp;
            }
        }
    }
    
    /* Return results */
    int count = 0;
    for (size_t i = 0; i < registry_count && count < (int)max_results; i++) {
        if (matches[i].score < 30) break;
        
        results[count].type = matches[i].entry->type;
        results[count].confidence = (matches[i].score > 100) ? 100 : matches[i].score;
        results[count].sync_found = matches[i].entry->sync;
        results[count].track = matches[i].track ? matches[i].track->track_num : 0;
        results[count].flags = matches[i].entry->flags;
        
        strncpy(results[count].name, matches[i].entry->name, 63);
        results[count].name[63] = '\0';
        
        if (matches[i].entry->publisher) {
            strncpy(results[count].publisher, matches[i].entry->publisher, 31);
            results[count].publisher[31] = '\0';
        } else {
            results[count].publisher[0] = '\0';
        }
        
        count++;
    }
    
    free(matches);
    return count;
}

bool uft_prot_detect_track(const uft_track_signature_t* track,
                           uft_prot_detect_result_t* result)
{
    if (!track || !result) return false;
    return uft_prot_detect(track, 1, result, 1) > 0;
}

const char* uft_prot_name(uft_amiga_prot_type_t type)
{
    const uft_prot_entry_t* entry = uft_prot_get_entry(type);
    return entry ? entry->name : "Unknown";
}

bool uft_prot_is_longtrack(uft_amiga_prot_type_t type)
{
    const uft_prot_entry_t* entry = uft_prot_get_entry(type);
    return entry && (entry->flags & UFT_PROT_FL_LONGTRACK);
}

bool uft_prot_needs_timing(uft_amiga_prot_type_t type)
{
    const uft_prot_entry_t* entry = uft_prot_get_entry(type);
    return entry && (entry->flags & UFT_PROT_FL_TIMING);
}

bool uft_prot_has_weak_bits(uft_amiga_prot_type_t type)
{
    const uft_prot_entry_t* entry = uft_prot_get_entry(type);
    return entry && (entry->flags & UFT_PROT_FL_WEAKBITS);
}
