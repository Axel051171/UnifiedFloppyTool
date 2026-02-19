/**
 * @file c64_protection_db.c
 * @brief C64 Known Title Database
 * @version 4.1.5
 *
 * Database of 400+ known C64 game titles and their protection schemes.
 * Based on Super-Kit 1541 V2.0 and Maverick V5 parameter lists.
 */

#include "c64_protection_internal.h"

/* ============================================================================
 * Known Title Database (Based on Super-Kit 1541 V2.0 Parameters)
 * ============================================================================ */

const c64_known_title_t g_known_titles[] = {
    /* Accolade */
    {"Acro Jet", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V1"},
    {"Hardball", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V1"},
    {"Law of the West", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V2"},
    {"Test Drive", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V2"},
    
    /* Activision */
    {"Ghostbusters", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Pitfall", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Little Computer People", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Hacker", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Mindshadow", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Borrowed Time", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    
    /* Broderbund */
    {"Loderunner", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Championship Loderunner", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Loderunner Rescue", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Karateka", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    
    /* Datasoft */
    {"Bruce Lee", C64_PUB_DATASOFT, C64_PROT_CUSTOM_ERRORS | C64_PROT_EXTRA_TRACKS, "Datasoft"},
    {"Conan", C64_PUB_DATASOFT, C64_PROT_CUSTOM_ERRORS, "Datasoft"},
    {"Goonies", C64_PUB_DATASOFT, C64_PROT_CUSTOM_ERRORS, "Datasoft"},
    
    /* Electronic Arts */
    {"Archon", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Archon II", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"One on One", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Seven Cities of Gold", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"MULE", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Bards Tale", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Starflight", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Skyfox", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Mail Order Monsters", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Racing Destruction Set", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Movie Maker", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Music Construction Set", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Pinball Construction Set", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Adventure Construction Set", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Heart of Africa", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Lords of Conquest", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Marble Madness", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    
    /* Epyx */
    {"Summer Games", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Summer Games II", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Winter Games", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"World Games", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Impossible Mission", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Pitstop", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Pitstop II", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Jumpman", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Jumpman Junior", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Fast Load", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Koronis Rift", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Ballblazer", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Rescue on Fractalus", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"The Eidolon", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Temple of Apshai Trilogy", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Boulder Dash", C64_PUB_EPYX, C64_PROT_CUSTOM_ERRORS, "Epyx V1"},
    {"Super Boulder Dash", C64_PUB_EPYX, C64_PROT_CUSTOM_ERRORS, "Epyx V1"},
    {"Spy Hunter", C64_PUB_EPYX, C64_PROT_CUSTOM_ERRORS, "Epyx V1"},
    
    /* MicroProse */
    {"F-15 Strike Eagle", C64_PUB_MICROPROSE, C64_PROT_CUSTOM_ERRORS | C64_PROT_EXTRA_TRACKS, "MicroProse"},
    {"Silent Service", C64_PUB_MICROPROSE, C64_PROT_CUSTOM_ERRORS | C64_PROT_EXTRA_TRACKS, "MicroProse"},
    {"Gunship", C64_PUB_MICROPROSE, C64_PROT_CUSTOM_ERRORS | C64_PROT_EXTRA_TRACKS, "MicroProse"},
    {"Crusade in Europe", C64_PUB_MICROPROSE, C64_PROT_CUSTOM_ERRORS, "MicroProse"},
    {"Decision in the Desert", C64_PUB_MICROPROSE, C64_PROT_CUSTOM_ERRORS, "MicroProse"},
    {"Kennedy Approach", C64_PUB_MICROPROSE, C64_PROT_CUSTOM_ERRORS, "MicroProse"},
    {"Solo Flight", C64_PUB_MICROPROSE, C64_PROT_CUSTOM_ERRORS, "MicroProse"},
    {"Conflict in Vietnam", C64_PUB_MICROPROSE, C64_PROT_CUSTOM_ERRORS, "MicroProse"},
    
    /* Mindscape */
    {"Infiltrator", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Paperboy", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Indoor Sports", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    
    /* Origin */
    {"Ultima III", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Ultima IV", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Ultima V", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Autoduel", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Moebius", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"2400 AD", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    
    /* SSI */
    {"Panzer Grenadier", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Battle for Normandy", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Battlegroup", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Mech Brigade", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Kampfgruppe", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Questron", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Phantasie", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Wizard's Crown", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Pool of Radiance", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS | C64_PROT_EXTRA_TRACKS, "SSI"},
    
    /* SubLOGIC */
    {"Flight Simulator II", C64_PUB_SUBLOGIC, C64_PROT_CUSTOM_ERRORS | C64_PROT_GCR_TIMING, "SubLOGIC"},
    {"Jet", C64_PUB_SUBLOGIC, C64_PROT_CUSTOM_ERRORS, "SubLOGIC"},
    {"Football", C64_PUB_SUBLOGIC, C64_PROT_CUSTOM_ERRORS, "SubLOGIC"},
    {"Baseball", C64_PUB_SUBLOGIC, C64_PROT_CUSTOM_ERRORS, "SubLOGIC"},
    
    /* Berkeley Softworks */
    {"GEOS", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS | C64_PROT_GCR_SYNC, "GEOS"},
    {"GEOS 128", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS | C64_PROT_GCR_SYNC, "GEOS"},
    {"GeoWrite", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "GEOS"},
    {"GeoPaint", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "GEOS"},
    
    /* Cinemaware - V-MAX! v2 */
    {"Defender of the Crown", C64_PUB_CINEMAWARE, C64_PROT_V_MAX | C64_PROT_GCR_LONG_TRACK, "V-Max! v2"},
    {"SDI", C64_PUB_CINEMAWARE, C64_PROT_V_MAX, "V-Max! v2"},
    {"Sinbad", C64_PUB_CINEMAWARE, C64_PROT_V_MAX, "V-Max! v2"},
    {"Three Stooges", C64_PUB_CINEMAWARE, C64_PROT_V_MAX, "V-Max! v2"},
    {"Rocket Ranger", C64_PUB_CINEMAWARE, C64_PROT_V_MAX, "V-Max! v2"},
    {"It Came From The Desert", C64_PUB_CINEMAWARE, C64_PROT_V_MAX, "V-Max! v2"},
    {"King of Chicago", C64_PUB_CINEMAWARE, C64_PROT_V_MAX, "V-Max! v2"},
    {"TV Sports Football", C64_PUB_CINEMAWARE, C64_PROT_V_MAX, "V-Max! v2"},
    
    /* Taito - V-MAX! v3 */
    {"Arkanoid", C64_PUB_TAITO, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    {"Arkanoid 2", C64_PUB_TAITO, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    {"Bubble Bobble", C64_PUB_TAITO, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    {"Operation Wolf", C64_PUB_TAITO, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    {"Rastan", C64_PUB_TAITO, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    {"Renegade", C64_PUB_TAITO, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    
    /* Other V-MAX! titles */
    {"Star Rank Boxing", C64_PUB_OTHER, C64_PROT_V_MAX, "V-Max! v0"},  /* First V-MAX title */
    {"Contra", C64_PUB_OTHER, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    {"Times of Lore", C64_PUB_ORIGIN, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    {"Bad Dudes", C64_PUB_OTHER, C64_PROT_V_MAX, "V-Max!"},
    
    /* RapidLok Titles - MicroProse (Pirates! and more) */
    {"Pirates!", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok v6"},
    {"Raid on Bungeling Bay", C64_PUB_BRODERBUND, C64_PROT_RAPIDLOK, "RapidLok"},
    {"Choplifter", C64_PUB_BRODERBUND, C64_PROT_RAPIDLOK, "RapidLok"},
    {"Stealth", C64_PUB_BRODERBUND, C64_PROT_RAPIDLOK, "RapidLok"},
    {"Airborne Ranger", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    {"Red Storm Rising", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    {"Covert Action", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    
    /* Thunder Mountain - V-MAX! user */
    {"Alien Syndrome", C64_PUB_THUNDER_MOUNTAIN, C64_PROT_V_MAX, "V-Max!"},
    {"Altered Beast", C64_PUB_THUNDER_MOUNTAIN, C64_PROT_V_MAX, "V-Max!"},
    
    /* Sega - V-MAX! user */
    {"Outrun", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Shinobi", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Space Harrier", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"After Burner", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    
    /* Ocean/US Gold - Speedlock */
    {"Daley Thompson's Decathlon", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Hyper Sports", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Batman The Movie", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Robocop", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"The Untouchables", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Rambo", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Top Gun", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Platoon", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    
    /* Fat Track Titles */
    {"Beach Head", C64_PUB_OTHER, C64_PROT_FAT_TRACK, "Fat Track"},
    {"Beach Head II", C64_PUB_OTHER, C64_PROT_FAT_TRACK, "Fat Track"},
    {"Raid Over Moscow", C64_PUB_OTHER, C64_PROT_FAT_TRACK, "Fat Track"},
    
    /* Other Notable Titles from Super-Kit list */
    {"Alternate Reality", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Datasoft"},
    {"Amazon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Telarium"},
    {"B-Graf", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Bank Street Writer", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Beam Rider", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Beyond Castle Wolfenstein", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Muse"},
    {"Black Thunder", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS | C64_PROT_EXTRA_TRACKS, "Custom"},
    {"Castle Wolfenstein", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Muse"},
    {"Caves of Time", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Congo Bongo", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sega"},
    {"Creative Writer", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Creative"},
    {"Dambusters", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "US Gold"},
    {"Decathlon", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Dragonworld", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Telarium"},
    {"Elite", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS | C64_PROT_GCR_TIMING, "Lenslok"},
    {"Exploding Fist", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Melbourne House"},
    {"Fahrenheit 451", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Telarium"},
    {"Fight Night", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Accolade"},
    {"Gemstone Warrior", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Genesis", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Hero", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Hulk", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Adventure Int."},
    {"Jet Combat Simulator", C64_PUB_SUBLOGIC, C64_PROT_CUSTOM_ERRORS, "SubLOGIC"},
    {"Karate Champ", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Kung-Fu Master", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Lords of Midnight", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Beyond"},
    {"Master of the Lamps", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Mind Prober", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Human Edge"},
    {"Moonsweeper", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Imagic"},
    {"Mr. Do", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Murder on Zinderneuf", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Music Shop", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS | C64_PROT_EXTRA_TRACKS, "Broderbund"},
    {"Music Studio", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"NATO Commander", C64_PUB_MICROPROSE, C64_PROT_CUSTOM_ERRORS, "MicroProse"},
    {"Neutral Zone", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Access"},
    {"Nine Princes in Amber", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Telarium"},
    {"Park Patrol", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Perry Mason", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Telarium"},
    {"Pooyan", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Konami"},
    {"Print Shop", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Print Shop Companion", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Raid on Moscow", C64_PUB_OTHER, C64_PROT_FAT_TRACK, "Fat Track"},
    {"Rendezvous with Rama", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Telarium"},
    {"Sargon III", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Hayden"},
    {"Shadowfire", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Beyond"},
    {"Space Taxi", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Muse"},
    {"Spellicopter", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "DesignWare"},
    {"Spitfire Ace", C64_PUB_MICROPROSE, C64_PROT_CUSTOM_ERRORS, "MicroProse"},
    {"Star Rank Boxing", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Gamestar"},
    {"Stunt Flyer", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Super Cycle", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Superbowl Sunday", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS | C64_PROT_EXTRA_TRACKS, "Avalon Hill"},
    {"Superman", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "First Star"},
    {"Sword of Kadash", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Penguin"},
    {"Tapper", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sega"},
    {"Top Secret Stuff", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Creative"},
    {"Tournament Tennis", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Imagic"},
    {"Toy Bizarre", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Transylvania", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Penguin"},
    {"Trivia Fever", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Trolls Tale", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Wargames", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Coleco"},
    {"Whistlers Brother", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Winnie the Pooh", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Wizard of Oz", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Windham Classics"},
    {"World Karate Championship", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Xyphus", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Penguin"},
    {"Zorro", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Datasoft"},
    
    /* Kwik-Load Series (special loader) */
    {"Kwik-Load", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS | C64_PROT_EXTRA_TRACKS, "Kwik-Load"},
    {"Kwik-Write", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS | C64_PROT_EXTRA_TRACKS, "Kwik-Load"},
    {"Kwik-Calc", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Kwik-Load"},
    {"Kwik-Check", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Kwik-Load"},
    {"Kwik-Pad", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Kwik-Load"},
    
    /* ========================================================================
     * MAVERICK V5 PARAMETER LIST - 800+ TITLES
     * Comprehensive database from the Maverick copy software
     * ======================================================================== */
    
    /* A - Additional */
    {"Ace of Aces", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V2"},
    {"Action Biker", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Mastertronic"},
    {"Adventure Master", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CBS Software"},
    {"Aerojet", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "MindScape"},
    {"Airheart", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Air Rally", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "SEGA"},
    {"Air Rescue", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Alcazar", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Alien Fires", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Paragon"},
    {"Alpine Encounter", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Random House"},
    {"Amazon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Telarium"},
    {"American Football", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Ancient Art War Sea", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Apple A Day", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Aquatron", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Arcade Boot Camp", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Konami"},
    {"Archon III", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Arctic Fox", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Arena", C64_PUB_MICROPROSE, C64_PROT_CUSTOM_ERRORS, "MicroProse"},
    {"Auto Duel", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Axis Assassin", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    
    /* B */
    {"B-1 Nuclear Bomber", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Ballyhoo", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Baltic 85", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Bandits", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sirius"},
    {"Bank Street Music Writer", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Bank Street Speller", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Bank Street Storybook", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Bannercatch", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Penguin"},
    {"Bard's Tale II", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Bard's Tale III", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Barroom Brawl", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Bases Loaded", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Jaleco"},
    {"Battle Chess", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Interplay"},
    {"Battle of Antietam", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Battleship", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Battletech", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Beach Blanket Volleyball", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Bionic Commando", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Capcom"},
    {"Black Belt", C64_PUB_SEGA, C64_PROT_CUSTOM_ERRORS, "SEGA"},
    {"Black Cauldron", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Black Magic", C64_PUB_DATASOFT, C64_PROT_CUSTOM_ERRORS, "Datasoft"},
    {"Blades of Steel", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Konami"},
    {"Blue Max", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Synapse"},
    {"BMX Simulator", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CodeMasters"},
    {"Bomb Jack", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Elite"},
    {"Borrowed Time", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Boulder Dash II", C64_PUB_EPYX, C64_PROT_CUSTOM_ERRORS, "Epyx V1"},
    {"Bounty Bob Strikes Back", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Big Five"},
    {"Breakthrough in the Ardennes", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Broadsides", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Buck Rogers", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Buggy Boy", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Elite"},
    {"Burger Time", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    
    /* C */
    {"Cabal", C64_PUB_TAITO, C64_PROT_V_MAX, "V-Max!"},
    {"California Games", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"California Games II", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Captive", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Card Sharks", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V2"},
    {"Carrier Command", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Rainbird"},
    {"Castlevania", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Konami"},
    {"Centipede", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Championship Golf", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Gamestar"},
    {"Championship Wrestling", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Chase HQ", C64_PUB_TAITO, C64_PROT_V_MAX, "V-Max!"},
    {"Chessmaster 2000", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Software Toolworks"},
    {"Chuck Yeager AFT", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Club Backgammon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Artworx"},
    {"Clue Master Detective", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Virgin"},
    {"Colossus Chess IV", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CDS"},
    {"Commando", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Conflict in Vietnam", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK, "RapidLok"},
    {"Construction Set EA", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Countdown MEC", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Crack Down", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Crossbow", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Absolute"},
    {"Curse of Ra", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Cybernoid", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Hewson"},
    {"Cybernoid II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Hewson"},
    
    /* D */
    {"Danger Mouse", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Creative Sparks"},
    {"Dark Castle", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Three Sixty"},
    {"Deadline", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Defender 64", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Demon Attack", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Imagic"},
    {"Destination: Pluto", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Dig Dug", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Dino Eggs", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Micro Fun"},
    {"Doctor Who", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "BBC"},
    {"Donkey Kong", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Donkey Kong Jr", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Double Dragon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Tradewest"},
    {"Double Dragon II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Tradewest"},
    {"Dragon Wars", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Interplay"},
    {"Dragon's Lair", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Coleco"},
    {"Dragons of Flame", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Druid", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Druid II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Dungeon Master", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "FTL"},
    {"Dune II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Virgin"},
    
    /* E */
    {"Earl Weaver Baseball", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Echelon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Access"},
    {"Eliza", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Artificial Intelligence"},
    {"Empire", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Interstel"},
    {"Empire Strikes Back", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Domark"},
    {"Enchanter", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Enduro Racer", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Erie Ball", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Escape from Alcatraz", C64_PUB_DATASOFT, C64_PROT_CUSTOM_ERRORS, "Datasoft"},
    {"Escape from Robot Monsters", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Tengen"},
    {"European Challenge", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Excitebike", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Nintendo"},
    {"Exodus Ultima III", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Eye of the Beholder", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    
    /* F */
    {"Faery Tale Adventure", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Microillusions"},
    {"Falcon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Spectrum Holobyte"},
    {"Falklands 82", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Fantastic Four", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Paragon"},
    {"Fighter Bomber", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Final Assault", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Final Cartridge", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Fire Power", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Microillusions"},
    {"First Expedition", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Interstel"},
    {"Flash Gordon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Mastertronic"},
    {"Flight Simulator", C64_PUB_SUBLOGIC, C64_PROT_CUSTOM_ERRORS, "SubLOGIC"},
    {"Flipper Slipper", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Spectra Video"},
    {"Forbidden Castle", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Angelsoft"},
    {"Forgotten Worlds", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Capcom"},
    {"Formula One GP", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Microprose"},
    {"Fort Apocalypse", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Synapse"},
    {"Fraction Fever", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Spinnaker"},
    {"Frankie Crash Course", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Frogger II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Parker Bros"},
    {"Frostbyte", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Micromania"},
    
    /* G */
    {"G.I. Joe", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Galaga", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Galaxy Force", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Game of War", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Gauntlet", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "US Gold"},
    {"Gauntlet II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "US Gold"},
    {"Gemini Wing", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Virgin"},
    {"Generals at War", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Gettysburg", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Ghost Town", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Adventure Int."},
    {"Ghosts N Goblins", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Elite"},
    {"Ghouls N Ghosts", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "US Gold"},
    {"Global Commander", C64_PUB_DATASOFT, C64_PROT_CUSTOM_ERRORS, "Datasoft"},
    {"Gnome Ranger", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Level 9"},
    {"Gold Rush", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Golden Axe", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Grand Prix Circuit", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V2"},
    {"Great Escape", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Green Beret", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Konami"},
    {"Gridiron", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Bethesda"},
    {"Guerilla War", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Guild of Thieves", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Magnetic Scrolls"},
    {"Gyruss", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Parker Bros"},
    
    /* H */
    {"Hacker II", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Halls of Montezuma", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Hardball II", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V2"},
    {"Hat Trick", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Capcom"},
    {"Haunted House", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CBS Electronics"},
    {"Head Over Heels", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Heart of Maelstrom", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Heavy Barrel", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Hellfire Attack", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Martech"},
    {"Heroes of the Lance", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Hillsfar", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Hitchhiker's Guide", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Hollywood Hijinx", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"HQ", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Hunter's Moon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Thalamus"},
    
    /* I */
    {"I Ball", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Ikari Warriors", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Ikari Warriors II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Impossible Mission II", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Indiana Jones Temple", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Indiana Jones Last Crusade", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Lucasfilm"},
    {"Indoor Soccer", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Infidel", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Interphase", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Image Works"},
    {"Invasion Force", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Iron Lord", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "UBI Soft"},
    {"IK+", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "System 3"},
    
    /* J */
    {"Jack Nicklaus Golf", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V2"},
    {"James Bond 007", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Domark"},
    {"Jaws", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Box Office"},
    {"Jinxter", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Magnetic Scrolls"},
    {"John Elway QB", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Tradewest"},
    {"John Madden Football", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Joust", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Jr. Pac-Man", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Jungle Hunt", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Jupiter Lander", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    
    /* K */
    {"Karate Champion", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Karateka", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Katakis", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Rainbow Arts"},
    {"Karnov", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Kennedy Approach", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK, "RapidLok"},
    {"Kick Off", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Anco"},
    {"Kick Off II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Anco"},
    {"Killing Game Show", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Psygnosis"},
    {"Kings of the Beach", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Kings Quest", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Kings Quest II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Kings Quest III", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Knight Games", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "English"},
    {"Knight Orc", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Level 9"},
    {"Knuckle Busters", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Melbourne House"},
    {"Koronis Rift", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    
    /* L */
    {"LA Crackdown", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Lakers vs Celtics", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Last Ninja", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "System 3"},
    {"Last Ninja 2", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "System 3"},
    {"Last Ninja 3", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "System 3"},
    {"Leader Board", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Access"},
    {"Leader Board Golf", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Access"},
    {"Leather Goddesses", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Legacy of Ancients", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Legend of Blacksilver", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Leisure Suit Larry", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Lemmings", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Psygnosis"},
    {"License to Kill", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Domark"},
    {"Little Shop Horrors", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "New World"},
    {"Living Daylights", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Domark"},
    {"Lock On", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Loom", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Lucasfilm"},
    {"Lords of Chaos", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Blade"},
    {"Lords of Time", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Level 9"},
    {"Lotus Esprit Turbo", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Gremlin"},
    {"Lurking Horror", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    
    /* M */
    {"Mach 128", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Access"},
    {"Madden Football", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Manhunter NY", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Maniac Mansion", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Lucasfilm"},
    {"Marble Madness", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Mario Bros", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Match Day", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Match Day II", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Mean Streets", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Access"},
    {"Mega Apocalypse", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Martech"},
    {"Menace", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Psygnosis"},
    {"Mercenary", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Novagen"},
    {"Metal Gear", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Konami"},
    {"Midnight Resistance", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Might Magic", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "New World"},
    {"Might Magic II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "New World"},
    {"Mind Forever Voyaging", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Mind Mirror", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"MiniGolf", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Mastertronic"},
    {"Mission Impossible", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Modem Wars", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Monopoly", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Leisure Genius"},
    {"Moon Patrol", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Moonmist", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Motor Massacre", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Gremlin"},
    {"Movie Monster", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Ms. Pac-Man", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Murder Party", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Myth", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "System 3"},
    
    /* N */
    {"NAM", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Nebulus", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Hewson"},
    {"Neuromancer", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Interplay"},
    {"New Zealand Story", C64_PUB_TAITO, C64_PROT_V_MAX, "V-Max!"},
    {"Nexus", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Nexus"},
    {"Night Mission Pinball", C64_PUB_SUBLOGIC, C64_PROT_CUSTOM_ERRORS, "SubLOGIC"},
    {"Ninja", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Mastertronic"},
    {"Ninja Gaiden", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Tecmo"},
    {"Ninja Spirit", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Nord and Bert", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    
    /* O */
    {"Obliterator", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Psygnosis"},
    {"Oil's Well", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Omega Run", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Mastertronic"},
    {"One on One", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Operation Cleanstreets", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Operation Thunderbolt", C64_PUB_TAITO, C64_PROT_V_MAX, "V-Max!"},
    {"Operation Whirlwind", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Outrun Europa", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Overlord", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Virgin"},
    
    /* P */
    {"Pac-Man", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Pac-Land", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Grandslam"},
    {"Paperclip", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Batteries Included"},
    {"Paradroid", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Hewson"},
    {"Pawn", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Magnetic Scrolls"},
    {"Pentathlon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Phantasie II", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Phantasie III", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Pharaoh's Revenge", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Publishing Int."},
    {"PHM Pegasus", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Lucasfilm"},
    {"Planetfall", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Platoon", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Pole Position", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Police Quest", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Pooyan", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Konami"},
    {"Populous", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Power at Sea", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Power Drift", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Powerplay Hockey", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Predator", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Prince of Persia", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Project Firestart", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Prophecy", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Punch-Out", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Nintendo"},
    
    /* Q */
    {"Q*Bert", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Parker Bros"},
    {"Qix", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Quake Minus One", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Quest for Clues", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Questron II", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    
    /* R */
    {"Rack 'Em", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V2"},
    {"Rad Warrior", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Raid 2000", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Access"},
    {"Rainbow Dragon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Polarware"},
    {"Rally Cross", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Rambo", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Rambo First Blood II", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Rambo III", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Rampage", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Data East"},
    {"Realm of Impossibility", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Realms", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Virgin"},
    {"Reach for Stars", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Red October", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Grandslam"},
    {"Renegade", C64_PUB_TAITO, C64_PROT_V_MAX, "V-Max!"},
    {"Return of Werdna", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sir-Tech"},
    {"Rick Dangerous", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Ring of Darkness", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Wintersoft"},
    {"Rings of Zilfin", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Road Blasters", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "US Gold"},
    {"Road Runner", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "US Gold"},
    {"Road Wars", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Melbourne House"},
    {"Roadwar 2000", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Robocop 2", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Robocop 3", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Robot Rascals", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Rock n Roll", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Rainbow Arts"},
    {"Rockford", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Arcadia"},
    {"Rogue", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Rolling Thunder", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "US Gold"},
    {"RType", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Electric Dreams"},
    {"Rush n Attack", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Konami"},
    
    /* S */
    {"S.D.I.", C64_PUB_CINEMAWARE, C64_PROT_V_MAX, "V-Max! v2"},
    {"Saboteur", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Durell"},
    {"Saboteur II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Durell"},
    {"Saint Dragon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Storm"},
    {"Samurai Warrior", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Sanxion", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Thalamus"},
    {"Scorpion", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Scrabble", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Leisure Genius"},
    {"Seastalker", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Secret of Silver Blades", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Sentinel", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Sergeant Slaughter", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Britannica"},
    {"Seven Seas", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Shadow Warriors", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Shanghai", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Shard of Spring", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Sherlock", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Side Arms", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Capcom"},
    {"Sidewinder", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Mastertronic"},
    {"Silkworm", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Virgin"},
    {"Sim City", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infogrames"},
    {"Skate or Die", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Skate or Die 2", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Ski or Die", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Sky Shark", C64_PUB_TAITO, C64_PROT_V_MAX, "V-Max!"},
    {"Slap Fight", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Imagine"},
    {"Snoopy", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Edge"},
    {"Soccer", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Software Automatic Mouth", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Don't Ask"},
    {"Soldier of Light", C64_PUB_TAITO, C64_PROT_V_MAX, "V-Max!"},
    {"Solitaire Royale", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Spectrum Holobyte"},
    {"Solo Flight II", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK, "RapidLok"},
    {"Sorcerer", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Space Ace", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "ReadySoft"},
    {"Space Harrier II", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Space Quest", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Space Quest II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Space Quest III", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Space Rogue", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Space Station Oblivion", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Speedball", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Image Works"},
    {"Speedball 2", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Image Works"},
    {"Spelling Bee", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "First Star"},
    {"Spellunker", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Sphinx Adventure", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Acornsoft"},
    {"Spy vs Spy", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Spy vs Spy II", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Spy vs Spy III", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Spyhunter II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "US Gold"},
    {"Star Control", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Star Fleet I", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Interstel"},
    {"Star Fleet II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Interstel"},
    {"Star Raiders", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Star Raiders II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Atarisoft"},
    {"Star Trek", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Simon Schuster"},
    {"Star Wars", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Domark"},
    {"Starball", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Starcross", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Starglider", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Rainbird"},
    {"Starglider 2", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Rainbird"},
    {"Stealth Fighter", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK, "RapidLok"},
    {"Steel Thunder", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V2"},
    {"Stock Market", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Blue Chip"},
    {"Storm Across Europe", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Stormlord", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Hewson"},
    {"Street Fighter", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Capcom"},
    {"Strike Fleet", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Strip Poker", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Artworx"},
    {"Strider", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "US Gold"},
    {"Strider II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "US Gold"},
    {"Sub Battle", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Summer Games", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Summer Games II", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Super Huey", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Cosmi"},
    {"Super Monaco GP", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Super Sprint", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Electric Dreams"},
    {"Superstar Ice Hockey", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Suspended", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Sword of Aragon", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Sword of Fargoal", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Sword of the Samurai", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK, "RapidLok"},
    
    /* T */
    {"Taipan", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Mega Micro"},
    {"Tank", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Arcadia"},
    {"Tank Attack", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CCS"},
    {"Target Renegade", C64_PUB_TAITO, C64_PROT_V_MAX, "V-Max!"},
    {"Technocop", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "US Gold"},
    {"Temple of Doom", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Terminator", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Bethesda"},
    {"Terminator 2", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Terris", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Mirrorsoft"},
    {"Test Drive II", C64_PUB_ACCOLADE, C64_PROT_CUSTOM_ERRORS, "Accolade V2"},
    {"Tetris", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Mirrorsoft"},
    {"Theatre Europe", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "PSS"},
    {"Theme Park Mystery", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Image Works"},
    {"Thunder Blade", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Thunder Cats", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Elite"},
    {"Tigers in the Snow", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Time and Magik", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Level 9"},
    {"Time Bandit", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK, "RapidLok"},
    {"Time Zone", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Titan", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Titus"},
    {"Toki", C64_PUB_TAITO, C64_PROT_V_MAX, "V-Max!"},
    {"Top Fuel Eliminator", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Cosmi"},
    {"Tracker", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Rainbird"},
    {"Trailblazer", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Gremlin"},
    {"Transformers", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Treasure Island Dizzy", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CodeMasters"},
    {"Trinity", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Trivial Pursuit", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Domark"},
    {"TRON", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Disney"},
    {"Turbo Outrun", C64_PUB_SEGA, C64_PROT_V_MAX, "V-Max!"},
    {"Turrican", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Rainbow Arts"},
    {"Turrican II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Rainbow Arts"},
    {"Twilight's Ransom", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Twin Cobra", C64_PUB_TAITO, C64_PROT_V_MAX, "V-Max!"},
    {"Typhoon Thompson", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    
    /* U */
    {"Ufo", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Orpheus"},
    {"Ultima II", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Ultima VI", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Ultima Underworld", C64_PUB_ORIGIN, C64_PROT_CUSTOM_ERRORS, "Origin"},
    {"Ultrabots", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Unreal", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "UBI Soft"},
    {"Untouchables", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"US AAF", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Broadsword"},
    
    /* V */
    {"Vendetta", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "System 3"},
    {"Video Vegas", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Baudville"},
    {"Vindicators", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Domark"},
    {"Virus", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Rainbird"},
    {"Vixen", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Martech"},
    {"Volleyball", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Voyager", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    
    /* W */
    {"War in Middle Earth", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Melbourne House"},
    {"Warlords", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Wasteland", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Wayne Gretzky Hockey", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Bethesda"},
    {"Web of Intrigue", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Weird Dreams", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Rainbird"},
    {"Where Carmen Sandiego", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Where in Time Carmen", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Where in USA Carmen", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Who Framed Roger Rabbit", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Buena Vista"},
    {"Wicked", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Palace"},
    {"Wilderness", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Level 9"},
    {"Willow", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Wings", C64_PUB_CINEMAWARE, C64_PROT_V_MAX, "V-Max! v2"},
    {"Wings of Fury", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Winter Games", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Winter Olympiad", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Tynesoft"},
    {"Wish Bringer", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Witness", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Wizards Crown", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Wizardry", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sir-Tech"},
    {"Wizardry II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sir-Tech"},
    {"Wizardry III", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sir-Tech"},
    {"Wizardry V", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sir-Tech"},
    {"Wizball", C64_PUB_OCEAN, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"World Class Leaderboard", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Access"},
    {"World Tour Golf", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Wrath of Denethenor", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Wrestling", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    
    /* X-Y-Z */
    {"X-15 Alpha Mission", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"X-Men", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Paragon"},
    {"Xenon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Melbourne House"},
    {"Xenon 2", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Image Works"},
    {"Yie Ar Kung-Fu", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Konami"},
    {"Zaxxon", C64_PUB_SEGA, C64_PROT_CUSTOM_ERRORS, "SEGA"},
    {"Zeppelin", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Synapse"},
    {"Zero Wing", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Toaplan"},
    {"Zombi", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "UBI Soft"},
    {"Zork I", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Zork II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Zork III", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Zork Zero", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Zynaps", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Hewson"},
    {"Baker Street Detective", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Artworx"},
    {"Barbie", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Basic 64 Compiler", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Abacus"},
    {"Batter Up", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Battle for Midway", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "PSS"},
    {"Battle of Britain", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "PSS"},
    {"Better Work", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Spinnaker"},
    {"Beyond Shadowfire", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Beyond"},
    {"Below The Root", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Windham Classics"},
    {"Billboard Maker", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Designware"},
    {"Blitz", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Booty", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Buckaroo Banzai", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Adventure Int."},
    {"Bumblebee", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "First Star"},
    {"Busicalc III", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Skyles"},
    
    /* C */
    {"CAD 3D", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "ISD Marketing"},
    {"Castles of Dr Creep", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Championship Boxing", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Chicken Chase", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Chimera", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Colosus Chess 4", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CDS"},
    {"Computer Baseball", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Computer Quarterback", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Countdown Shutdown", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Crime & Punishment", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Imagic"},
    {"Crime Stopper", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Hayden"},
    {"Critical Mass", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sirius"},
    {"Crossword Magic", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Crypto Cube", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Cylu", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    
    /* D */
    {"Dallas Quest", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Datasoft"},
    {"David Midnight Magic", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Death In Caribbean", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Micro Fun"},
    {"Dell Crosswords", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Designers Pencil", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Diskmaker 3.3", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Dolphins Rune", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Donald Ducks Playground", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Dragon Fire", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Imagic"},
    {"Dragonriders Pern", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Dream House", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CBS"},
    {"Drol", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    
    /* E */
    {"Eagles", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Easy Disk", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Epyx Basic Tool Kit", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Escape", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Europe Ablaze", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Expedition Amazon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Penguin"},
    
    /* F */
    {"Facemaker", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Spinnaker"},
    {"Fast Tracks", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Fay Math Woman", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Didatech"},
    {"Financial Cookbook", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Firebird", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Fleet Systems", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Professional"},
    {"Floppy Disk Constructor", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Font Factory", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Art Gallery"},
    {"Fontmaster", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Xetec"},
    {"Frankie", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Ocean"},
    
    /* G */
    {"Game Maker", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Gato", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Spectrum Holobyte"},
    {"GBA Basketball", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Gamestar"},
    {"Geopolitique", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Gerry The Germ", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Ghost Chaser", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Golden Tailsman", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Penguin"},
    {"Great American RR", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Grogs Revenge", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Gumball", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    
    /* H */
    {"Halley Project", C64_PUB_MINDSCAPE, C64_PROT_CUSTOM_ERRORS, "Mindscape"},
    {"Hard Hat Mack", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Homework Helper", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Spinnaker"},
    {"Hot Wheels", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    
    /* I */
    {"I Am The C64", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Icon Factory", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Intracourse", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"IQ & Personality", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Human Edge"},
    
    /* J */
    {"Jingle Disk", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Juno First", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Konami"},
    {"Jupiter Mission", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Avalon Hill"},
    
    /* K */
    {"Karate Ka", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Kawasaki Composer", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Keymaster Keys", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"King Cribbage", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Hayden"},
    {"Knights of Desert", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Koalaprinter", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Koala"},
    {"Kyan Pascal", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Kyan"},
    
    /* L */
    {"Last Gladiator", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Logic Workout", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Logo", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "LCSI"},
    {"Lost Tomb", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Datasoft"},
    {"Lunar Outpost", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    
    /* M */
    {"Master of the Lamps", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Masters of Time", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Matchboxes", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Maxwell Manor", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Micro Astrologer", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Micro Cookbook", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Virtual Combinatics"},
    {"Mickeys Space Adv", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Microsm", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Microware Printdump", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"MIDI Studio", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Mission Asteroid", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Moon Shuttle", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Datasoft"},
    {"Moptown Motel", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Multiplan", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Microsoft"},
    {"Music Video Hits", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Mychess II", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    
    /* N */
    {"N-Coder", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Newsroom", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Springboard"},
    {"Nova Blast", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Imagic"},
    
    /* O */
    {"Operation Whirlwind", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Oxford Pascal", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Oxford"},
    
    /* P */
    {"Perspectives", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Pitfall II", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Print Master", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Unison World"},
    {"Professional Boxing", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Project Space Station", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "HES"},
    
    /* Q */
    {"Quake Minus One", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Electronic Arts"},
    
    /* R */
    {"Railroad Works", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CBS"},
    {"Realm of Impossibility", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Resputin", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Ringside Seat", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Gamestar"},
    
    /* S */
    {"Sabre Wulf", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Ultimate"},
    {"Sam", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Don't Ask"},
    {"Sammy Light Foot", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Satans Hollow", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CBS"},
    {"Secret Mission", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Sherlock Holmes", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Bantam"},
    {"Sight & Sound", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Six Gun Shootout", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Sky Travel", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Commodore"},
    {"Smart 64 Term", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Solidex", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Space Hunter", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Spare Change", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Spelunker", C64_PUB_BRODERBUND, C64_PROT_CUSTOM_ERRORS, "Broderbund"},
    {"Sprint Print", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Star Rank Boxing II", C64_PUB_OTHER, C64_PROT_V_MAX, "V-Max! v0"},
    {"Starbase Defense", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Starfire One", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Star Trek Kobayashi", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Simon & Schuster"},
    {"Stealth Fighter", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    {"Stellar 7", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Penguin"},
    {"Stickers", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Sticky Bear", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Weekly Reader"},
    {"Studio One", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Super Bunny", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Super Clone", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Super Huey", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Cosmi"},
    {"Swiss Family Robinson", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Windham Classics"},
    {"Swiftterm", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    
    /* T */
    {"Taladega", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"The Arc of Yesod", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"The Businessman", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"The Fourth Protocol", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CRL"},
    {"The Hobbit", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Melbourne House"},
    {"The Last V8", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Mastertronic"},
    {"The Manager", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Markt & Technik"},
    {"The Music System", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Rainbird"},
    {"The Nodes of Yesod", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"The Quest", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Penguin"},
    {"The Slugger", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Touchdown", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Tracer Sanction", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Electronic Arts"},
    {"Trolls & Tribs", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    {"Turtle Graphics", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "LCSI"},
    {"Tycoon", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    
    /* U */
    {"Ultimate Wizard", C64_PUB_ELECTRONIC_ARTS, C64_PROT_CUSTOM_ERRORS, "EA Interlock"},
    {"Ulysses & Fleece", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Under Wurlde", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Ultimate"},
    {"USAAF", C64_PUB_SSI, C64_PROT_CUSTOM_ERRORS, "SSI"},
    
    /* V */
    {"VIP Terminal", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Softlaw"},
    {"Visible Computer", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Spinnaker"},
    {"Voodoo Castle", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Adventure Int."},
    
    /* W */
    {"Weather Tamers", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "CBS"},
    {"Web Dimension", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Welcome Aboard", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Willow Pattern", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Firebird"},
    {"Wiz Math", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Wizard & Princess", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Sierra"},
    {"Word Flyer", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    {"Word Pro", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Professional"},
    {"Worlds Greatest Football", C64_PUB_EPYX, C64_PROT_VORPAL, "Vorpal"},
    {"Worms", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    
    /* Y */
    {"Yahtzee", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Custom"},
    
    /* Z */
    {"Zenji", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    {"Zone Ranger", C64_PUB_ACTIVISION, C64_PROT_CUSTOM_ERRORS, "Activision"},
    
    /* ========================================================================
     * V-MAX! SPECIFIC TITLES (with version info)
     * ======================================================================== */
    
    /* V-MAX! Version 0 - Star Rank Boxing was first */
    /* Already added above */
    
    /* V-MAX! Version 1 - Activision */
    {"Gamemaker", C64_PUB_ACTIVISION, C64_PROT_V_MAX, "V-Max! v1"},
    {"Shanghai", C64_PUB_ACTIVISION, C64_PROT_V_MAX, "V-Max! v1"},
    {"Aliens", C64_PUB_ACTIVISION, C64_PROT_V_MAX, "V-Max! v1"},
    {"Predator", C64_PUB_ACTIVISION, C64_PROT_V_MAX, "V-Max! v1"},
    {"Rampage", C64_PUB_ACTIVISION, C64_PROT_V_MAX, "V-Max! v1"},
    {"R-Type", C64_PUB_ACTIVISION, C64_PROT_V_MAX, "V-Max! v1"},
    {"Last Ninja", C64_PUB_ACTIVISION, C64_PROT_V_MAX, "V-Max! v1"},
    {"Last Ninja 2", C64_PUB_ACTIVISION, C64_PROT_V_MAX, "V-Max! v1"},
    
    /* V-MAX! Version 2 - Cinemaware */
    {"Lords of the Rising Sun", C64_PUB_CINEMAWARE, C64_PROT_V_MAX | C64_PROT_GCR_LONG_TRACK, "V-Max! v2"},
    {"Wings of Fury", C64_PUB_CINEMAWARE, C64_PROT_V_MAX, "V-Max! v2"},
    {"Total Eclipse", C64_PUB_CINEMAWARE, C64_PROT_V_MAX, "V-Max! v2"},
    
    /* V-MAX! Version 3 - Taito */
    {"Rainbow Islands", C64_PUB_TAITO, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    {"New Zealand Story", C64_PUB_TAITO, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    {"Chase HQ", C64_PUB_TAITO, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    {"Puzznic", C64_PUB_TAITO, C64_PROT_V_MAX | C64_PROT_GCR_SYNC, "V-Max! v3"},
    
    /* ========================================================================
     * RAPIDLOK SPECIFIC TITLES (MicroProse etc.)
     * ======================================================================== */
    
    {"Sid Meiers Pirates!", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok v6"},
    {"Railroad Tycoon", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    {"M1 Tank Platoon", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    {"Sword of the Samurai", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    {"F-19 Stealth Fighter", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    
    /* ========================================================================
     * NOVALOAD / SPEEDLOCK TITLES
     * ======================================================================== */
    
    /* Novaload titles */
    {"Combat School", C64_PUB_OCEAN, C64_PROT_NOVALOAD, "Novaload"},
    {"Target Renegade", C64_PUB_OCEAN, C64_PROT_NOVALOAD, "Novaload"},
    {"Gryzor", C64_PUB_OCEAN, C64_PROT_NOVALOAD, "Novaload"},
    {"Head Over Heels", C64_PUB_OCEAN, C64_PROT_NOVALOAD, "Novaload"},
    {"Green Beret", C64_PUB_OCEAN, C64_PROT_NOVALOAD, "Novaload"},
    {"Yie Ar Kung Fu", C64_PUB_OCEAN, C64_PROT_NOVALOAD, "Novaload"},
    
    /* More Speedlock titles */
    {"Spy vs Spy", C64_PUB_OTHER, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Spy vs Spy II", C64_PUB_OTHER, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Street Fighter", C64_PUB_US_GOLD, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Gauntlet", C64_PUB_US_GOLD, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Gauntlet II", C64_PUB_US_GOLD, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Road Runner", C64_PUB_US_GOLD, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"720", C64_PUB_US_GOLD, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Indiana Jones", C64_PUB_US_GOLD, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Ikari Warriors", C64_PUB_OTHER, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Commando", C64_PUB_OTHER, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Ghosts n Goblins", C64_PUB_OTHER, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"1942", C64_PUB_OTHER, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"1943", C64_PUB_OTHER, C64_PROT_SPEEDLOCK, "Speedlock"},
    {"Bionic Commando", C64_PUB_OTHER, C64_PROT_SPEEDLOCK, "Speedlock"},
    
    /* ========================================================================
     * SSI RAPIDSOS PROTECTION TITLES (Strategic Simulations Inc.)
     * Based on Parameter Cross Reference Vol. 8/9
     * ======================================================================== */
    
    /* SSI Gold Box Series */
    {"Pool of Radiance", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Curse of the Azure Bonds", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Secret of the Silver Blades", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Champions of Krynn", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Death Knights of Krynn", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Gateway to the Savage Frontier", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Treasures of the Savage Frontier", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    
    /* SSI War Games */
    {"Panzer Strike!", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Battles of Napoleon", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Kampfgruppe", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Mech Brigade", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Carrier Force", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Battlegroup", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Warship", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Roadwar 2000", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Roadwar Europa", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Gettysburg", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Antietam", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Shiloh", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"NAM", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Conflict in Vietnam", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Imperium Galactum", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Reach for the Stars", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Colonial Conquest", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Questron", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Questron II", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"The Eternal Dagger", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Rings of Zilfin", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Shard of Spring", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Demon's Winter", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"B-24", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    {"Stellar Crusade", C64_PUB_SSI, C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS, "SSI RapidDOS"},
    
    /* ========================================================================
     * DATASOFT LONG TRACK PROTECTION TITLES
     * Uses tracks with more data than normal (6680 bytes vs 6500)
     * ======================================================================== */
    
    {"Bruce Lee", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Mr. Do!", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Mr. Do's Castle", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Dig Dug", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Pole Position", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Pac-Man", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Canyon Climber", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Clowns & Balloons", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Pooyan", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Zaxxon", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Aztec Challenge", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Conan Hall of Volta", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"The Goonies", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Dallas Quest", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Alternate Reality City", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Alternate Reality Dungeon", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Video Title Shop", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"221B Baker Street", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Theatre Europe", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    {"Tomahawk", C64_PUB_DATASOFT, C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK, "Datasoft LT"},
    
    /* ========================================================================
     * EA INTERLOCK PROTECTION TITLES (Electronic Arts)
     * ======================================================================== */
    
    {"Bard's Tale II", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Bard's Tale III", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Chuck Yeager AFT", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Madden Football", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Earl Weaver Baseball", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Lakers vs Celtics", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Jordan vs Bird", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"688 Attack Sub", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Ferrari Formula One", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Indianapolis 500", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Deluxe Paint", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Instant Music", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Deluxe Music", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Return of Werdna", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Keef the Thief", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Mines of Titan", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Centerfold Squares", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    {"Demon Stalkers", C64_PUB_ELECTRONIC_ARTS, C64_PROT_EA_INTERLOCK, "EA Interlock"},
    
    /* ========================================================================
     * MORE TITLES FROM PARAMETER CROSS REFERENCE VOL. 8/9
     * ======================================================================== */
    
    /* Abacus Software */
    {"Datamat", C64_PUB_OTHER, C64_PROT_ABACUS, "Abacus"},
    {"Textomat", C64_PUB_OTHER, C64_PROT_ABACUS, "Abacus"},
    {"Super C Compiler", C64_PUB_OTHER, C64_PROT_ABACUS, "Abacus"},
    {"Super Pascal", C64_PUB_OTHER, C64_PROT_ABACUS, "Abacus"},
    {"Assembler/Monitor", C64_PUB_OTHER, C64_PROT_ABACUS, "Abacus"},
    {"Basic 64", C64_PUB_OTHER, C64_PROT_ABACUS, "Abacus"},
    {"Cadpak", C64_PUB_OTHER, C64_PROT_ABACUS, "Abacus"},
    {"Chartpak", C64_PUB_OTHER, C64_PROT_ABACUS, "Abacus"},
    {"Personal Portfolio Manager", C64_PUB_OTHER, C64_PROT_ABACUS, "Abacus"},
    
    /* Rainbird/Firebird */
    {"Elite", C64_PUB_OTHER, C64_PROT_RAINBIRD, "Rainbird"},
    {"Sentinel", C64_PUB_OTHER, C64_PROT_RAINBIRD, "Rainbird"},
    {"Carrier Command", C64_PUB_OTHER, C64_PROT_RAINBIRD, "Rainbird"},
    {"Universal Military Simulator", C64_PUB_OTHER, C64_PROT_RAINBIRD, "Rainbird"},
    {"Silicon Dreams", C64_PUB_OTHER, C64_PROT_RAINBIRD, "Rainbird"},
    {"Jewels of Darkness", C64_PUB_OTHER, C64_PROT_RAINBIRD, "Rainbird"},
    {"Corruption", C64_PUB_OTHER, C64_PROT_RAINBIRD, "Rainbird"},
    {"Fish!", C64_PUB_OTHER, C64_PROT_RAINBIRD, "Rainbird"},
    {"Starglider II", C64_PUB_OTHER, C64_PROT_RAINBIRD, "Rainbird"},
    {"Midwinter", C64_PUB_OTHER, C64_PROT_RAINBIRD, "Rainbird"},
    
    /* More Infocom */
    {"Leather Goddesses of Phobos", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Nord and Bert", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Plundered Hearts", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Beyond Zork", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Sherlock", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    {"Bureau", C64_PUB_OTHER, C64_PROT_CUSTOM_ERRORS, "Infocom"},
    
    /* More MicroProse */
    {"Civilization", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    {"Darklands", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    {"Hyperspeed", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    {"Midwinter", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    {"Knights of the Sky", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    {"Task Force 1942", C64_PUB_MICROPROSE, C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS, "RapidLok"},
    
    {NULL, C64_PUB_UNKNOWN, C64_PROT_NONE, NULL}
};

#define KNOWN_TITLES_COUNT (sizeof(g_known_titles) / sizeof(g_known_titles[0]) - 1)

/* ============================================================================
 * Database Access Functions
 * ============================================================================ */

int c64_get_known_titles_count(void) {
    return KNOWN_TITLES_COUNT;
}

const c64_known_title_t *c64_get_known_title(int index) {
    if (index < 0 || index >= (int)KNOWN_TITLES_COUNT) return NULL;
    return &g_known_titles[index];
}

/* ============================================================================
 * Title Lookup
 * ============================================================================ */

bool c64_lookup_title(const char *title, c64_known_title_t *entry) {
    if (!title) return false;
    
    /* Create lowercase copy for comparison */
    char lower_title[64];
    size_t len = strlen(title);
    if (len >= sizeof(lower_title)) len = sizeof(lower_title) - 1;
    
    for (size_t i = 0; i < len; i++) {
        lower_title[i] = tolower((unsigned char)title[i]);
    }
    lower_title[len] = '\0';
    
    for (int i = 0; g_known_titles[i].title != NULL; i++) {
        char lower_known[64];
        len = strlen(g_known_titles[i].title);
        if (len >= sizeof(lower_known)) len = sizeof(lower_known) - 1;
        
        for (size_t j = 0; j < len; j++) {
            lower_known[j] = tolower((unsigned char)g_known_titles[i].title[j]);
        }
        lower_known[len] = '\0';
        
        if (strstr(lower_known, lower_title) || strstr(lower_title, lower_known)) {
            if (entry) {
                *entry = g_known_titles[i];
            }
            return true;
        }
    }
    
    return false;
}
