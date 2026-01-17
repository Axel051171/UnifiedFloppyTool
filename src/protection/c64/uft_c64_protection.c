/**
 * @file uft_c64_protection.c
 * @brief C64/1541 Copy Protection Detection Implementation
 * @version 4.1.5
 * 
 * Based on Super-Kit 1541 V2.0 documentation and reverse engineering.
 */

#include "uft/protection/uft_c64_protection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ============================================================================
 * D64 Image Size Constants
 * ============================================================================ */

#define D64_35_TRACKS           174848   /* Standard 35 tracks */
#define D64_35_TRACKS_ERRORS    175531   /* 35 tracks + 683 error bytes */
#define D64_40_TRACKS           196608   /* Extended 40 tracks */
#define D64_40_TRACKS_ERRORS    197376   /* 40 tracks + 768 error bytes */

#define D64_SECTOR_SIZE         256
#define D64_SECTORS_35          683      /* Total sectors in 35 tracks */
#define D64_SECTORS_40          768      /* Total sectors in 40 tracks */

/* ============================================================================
 * Known Title Database (Based on Super-Kit 1541 V2.0 Parameters)
 * ============================================================================ */

static const c64_known_title_t g_known_titles[] = {
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
 * Error Code Strings
 * ============================================================================ */

const char *c64_error_to_string(c64_error_code_t error_code) {
    switch (error_code) {
        case C64_ERR_OK: return "OK - No error";
        case C64_ERR_HEADER_NOT_FOUND: return "Error 20: Header block not found";
        case C64_ERR_NO_SYNC: return "Error 21: No sync found (unformatted sector)";
        case C64_ERR_DATA_NOT_FOUND: return "Error 22: Data block not found";
        case C64_ERR_CHECKSUM: return "Error 23: Data block checksum error";
        case C64_ERR_VERIFY: return "Error 25: Verify error after write";
        case C64_ERR_WRITE_PROTECT: return "Error 26: Write protect error";
        case C64_ERR_HEADER_CHECKSUM: return "Error 27: Header checksum error";
        case C64_ERR_LONG_DATA: return "Error 28: Long data block";
        case C64_ERR_ID_MISMATCH: return "Error 29: Disk ID mismatch";
        default: return "Unknown error";
    }
}

/* ============================================================================
 * Protection Type to String
 * ============================================================================ */

int c64_protection_to_string(uint32_t protection_type, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return 0;
    
    buffer[0] = '\0';
    int written = 0;
    
    if (protection_type == C64_PROT_NONE) {
        return snprintf(buffer, buffer_size, "No protection detected");
    }
    
    #define ADD_FLAG(flag, name) \
        if (protection_type & (flag)) { \
            if (written > 0) written += snprintf(buffer + written, buffer_size - written, ", "); \
            written += snprintf(buffer + written, buffer_size - written, "%s", name); \
        }
    
    ADD_FLAG(C64_PROT_ERRORS_T18, "Directory Errors");
    ADD_FLAG(C64_PROT_ERRORS_T36_40, "Extended Track Errors");
    ADD_FLAG(C64_PROT_CUSTOM_ERRORS, "Custom Errors");
    ADD_FLAG(C64_PROT_EXTRA_TRACKS, "Extra Tracks (36-40)");
    ADD_FLAG(C64_PROT_HALF_TRACKS, "Half-Tracks");
    ADD_FLAG(C64_PROT_KILLER_TRACKS, "Killer Tracks");
    ADD_FLAG(C64_PROT_EXTRA_SECTORS, "Extra Sectors");
    ADD_FLAG(C64_PROT_MISSING_SECTORS, "Missing Sectors");
    ADD_FLAG(C64_PROT_INTERLEAVE, "Non-standard Interleave");
    ADD_FLAG(C64_PROT_GCR_TIMING, "GCR Timing");
    ADD_FLAG(C64_PROT_GCR_DENSITY, "GCR Density");
    ADD_FLAG(C64_PROT_GCR_SYNC, "GCR Sync Marks");
    ADD_FLAG(C64_PROT_GCR_LONG_TRACK, "Long Track");
    ADD_FLAG(C64_PROT_GCR_BAD_GCR, "Bad GCR Patterns");
    ADD_FLAG(C64_PROT_VORPAL, "Vorpal (Epyx)");
    ADD_FLAG(C64_PROT_V_MAX, "V-Max!");
    ADD_FLAG(C64_PROT_RAPIDLOK, "RapidLok");
    ADD_FLAG(C64_PROT_FAT_TRACK, "Fat Track");
    ADD_FLAG(C64_PROT_SPEEDLOCK, "Speedlock");
    ADD_FLAG(C64_PROT_NOVALOAD, "Novaload");
    ADD_FLAG(C64_PROT_DATASOFT, "Datasoft Long Track");
    ADD_FLAG(C64_PROT_SSI_RDOS, "SSI RapidDOS");
    ADD_FLAG(C64_PROT_EA_INTERLOCK, "EA Interlock");
    ADD_FLAG(C64_PROT_ABACUS, "Abacus");
    ADD_FLAG(C64_PROT_RAINBIRD, "Rainbird/Firebird");
    
    #undef ADD_FLAG
    
    return written;
}

/* ============================================================================
 * D64 Sector Offset Calculation
 * ============================================================================ */

static size_t d64_get_sector_offset(int track, int sector) {
    if (track < 1 || track > 40) return (size_t)-1;
    if (sector < 0 || sector >= c64_sectors_per_track[track]) return (size_t)-1;
    
    size_t offset = 0;
    for (int t = 1; t < track; t++) {
        offset += c64_sectors_per_track[t] * D64_SECTOR_SIZE;
    }
    offset += sector * D64_SECTOR_SIZE;
    
    return offset;
}

/* ============================================================================
 * BAM Analysis
 * ============================================================================ */

static void analyze_bam(const uint8_t *data, size_t size, c64_protection_analysis_t *result) {
    size_t bam_offset = d64_get_sector_offset(C64_BAM_TRACK, C64_BAM_SECTOR);
    if (bam_offset == (size_t)-1 || bam_offset + 256 > size) {
        result->bam_valid = false;
        return;
    }
    
    const uint8_t *bam = data + bam_offset;
    
    /* Check BAM structure */
    result->bam_valid = true;
    result->bam_free_blocks = 0;
    result->bam_allocated_blocks = 0;
    
    /* BAM entries start at offset 4, 4 bytes per track */
    for (int track = 1; track <= 35; track++) {
        int bam_idx = 4 + (track - 1) * 4;
        int free_sectors = bam[bam_idx];
        
        result->bam_free_blocks += free_sectors;
        result->bam_allocated_blocks += c64_sectors_per_track[track] - free_sectors;
    }
    
    /* Check for extended BAM (tracks 36-40) */
    /* Extended BAM is at sector 18,0 offset 0xDD-0xFF or sector 18,1 */
    if (size >= D64_40_TRACKS) {
        result->bam_track_36_40 = true;
    }
}

/* ============================================================================
 * D64 Error Analysis
 * ============================================================================ */

static void analyze_d64_errors(const uint8_t *data, size_t size, 
                                size_t error_offset, int sector_count,
                                c64_protection_analysis_t *result) {
    if (error_offset >= size) return;
    
    const uint8_t *errors = data + error_offset;
    
    /* Map sector index to track */
    int sector_idx = 0;
    for (int track = 1; track <= 40 && sector_idx < sector_count; track++) {
        for (int sector = 0; sector < c64_sectors_per_track[track] && sector_idx < sector_count; sector++) {
            if (sector_idx < (int)(size - error_offset)) {
                uint8_t err = errors[sector_idx];
                if (err != C64_ERR_OK && err != 0x00) {
                    result->total_errors++;
                    if (err < 16) result->error_counts[err]++;
                    result->error_tracks[track] = 1;
                    
                    /* Flag specific error-based protection */
                    if (track == C64_DIR_TRACK) {
                        result->protection_flags |= C64_PROT_ERRORS_T18;
                    }
                    if (track >= 36) {
                        result->protection_flags |= C64_PROT_ERRORS_T36_40;
                    }
                }
            }
            sector_idx++;
        }
    }
    
    if (result->total_errors > 0) {
        result->protection_flags |= C64_PROT_CUSTOM_ERRORS;
    }
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

/* ============================================================================
 * Main D64 Analysis
 * ============================================================================ */

int c64_analyze_d64(const uint8_t *data, size_t size, c64_protection_analysis_t *result) {
    if (!data || !result) return -1;
    
    memset(result, 0, sizeof(c64_protection_analysis_t));
    
    /* Determine image type */
    bool has_errors = false;
    int tracks = 35;
    int sectors = D64_SECTORS_35;
    
    switch (size) {
        case D64_35_TRACKS:
            tracks = 35;
            sectors = D64_SECTORS_35;
            break;
        case D64_35_TRACKS_ERRORS:
            tracks = 35;
            sectors = D64_SECTORS_35;
            has_errors = true;
            break;
        case D64_40_TRACKS:
            tracks = 40;
            sectors = D64_SECTORS_40;
            result->uses_track_36_40 = true;
            result->protection_flags |= C64_PROT_EXTRA_TRACKS;
            break;
        case D64_40_TRACKS_ERRORS:
            tracks = 40;
            sectors = D64_SECTORS_40;
            has_errors = true;
            result->uses_track_36_40 = true;
            result->protection_flags |= C64_PROT_EXTRA_TRACKS;
            break;
        default:
            /* Non-standard size */
            if (size > D64_40_TRACKS) {
                tracks = 40;
                sectors = D64_SECTORS_40;
                has_errors = (size > D64_40_TRACKS);
            }
            break;
    }
    
    result->tracks_used = tracks;
    result->total_sectors = sectors;
    
    /* Analyze BAM */
    analyze_bam(data, size, result);
    
    /* Analyze errors if present */
    if (has_errors) {
        size_t error_offset = (tracks == 40) ? D64_40_TRACKS : D64_35_TRACKS;
        analyze_d64_errors(data, size, error_offset, sectors, result);
    }
    
    /* Try to extract disk name from directory */
    size_t bam_offset = d64_get_sector_offset(C64_BAM_TRACK, C64_BAM_SECTOR);
    if (bam_offset != (size_t)-1 && bam_offset + 256 <= size) {
        const uint8_t *bam = data + bam_offset;
        
        /* Disk name is at offset 0x90-0x9F (16 chars, padded with 0xA0) */
        char disk_name[17];
        for (int i = 0; i < 16; i++) {
            uint8_t c = bam[0x90 + i];
            if (c == 0xA0) c = ' ';  /* Convert PETSCII padding */
            if (c >= 0xC1 && c <= 0xDA) c = c - 0xC1 + 'A';  /* PETSCII uppercase */
            if (c < 0x20 || c > 0x7E) c = '?';
            disk_name[i] = c;
        }
        disk_name[16] = '\0';
        
        /* Trim trailing spaces */
        for (int i = 15; i >= 0 && disk_name[i] == ' '; i--) {
            disk_name[i] = '\0';
        }
        
        strncpy(result->title, disk_name, sizeof(result->title) - 1);
        
        /* Look up in database */
        c64_known_title_t known;
        if (c64_lookup_title(disk_name, &known)) {
            result->publisher = known.publisher;
            result->protection_flags |= known.protection_flags;
            strncpy(result->protection_name, known.protection_name, 
                    sizeof(result->protection_name) - 1);
            result->confidence = 85;
        }
    }
    
    /* Calculate confidence based on findings */
    if (result->confidence == 0) {
        result->confidence = 50;  /* Base confidence */
        if (result->total_errors > 0) result->confidence += 20;
        if (result->uses_track_36_40) result->confidence += 10;
        if (result->protection_flags != C64_PROT_NONE) result->confidence += 10;
    }
    
    return 0;
}

/* ============================================================================
 * D64 with explicit error info
 * ============================================================================ */

int c64_analyze_d64_errors(const uint8_t *data, size_t size, c64_protection_analysis_t *result) {
    /* Same as c64_analyze_d64, which handles error bytes automatically */
    return c64_analyze_d64(data, size, result);
}

/* ============================================================================
 * G64 Analysis (GCR-level)
 * ============================================================================ */

int c64_analyze_g64(const uint8_t *data, size_t size, c64_protection_analysis_t *result) {
    if (!data || !result || size < 12) return -1;
    
    memset(result, 0, sizeof(c64_protection_analysis_t));
    result->has_gcr_data = true;
    
    /* Check G64 header */
    if (memcmp(data, "GCR-1541", 8) != 0) {
        return -2;  /* Not a valid G64 */
    }
    
    uint8_t version = data[8];
    uint8_t track_count = data[9];
    uint16_t max_track_size = data[10] | (data[11] << 8);
    
    result->tracks_used = track_count / 2;  /* G64 counts half-tracks */
    
    /* Scan track table for half-tracks and anomalies */
    const uint32_t *track_offsets = (const uint32_t *)(data + 12);
    const uint32_t *speed_zones = (const uint32_t *)(data + 12 + track_count * 4);
    
    for (int i = 0; i < track_count && i < 84; i++) {
        uint32_t offset = track_offsets[i];
        
        if (offset != 0) {
            /* Track exists */
            if (i % 2 == 1) {
                /* Half-track */
                result->uses_half_tracks = true;
                result->half_track_count++;
                result->protection_flags |= C64_PROT_HALF_TRACKS;
            }
            
            if (i / 2 >= 36) {
                result->uses_track_36_40 = true;
                result->protection_flags |= C64_PROT_EXTRA_TRACKS;
            }
            
            /* Check track data for sync anomalies */
            if (offset < size) {
                uint16_t track_size = data[offset] | (data[offset + 1] << 8);
                const uint8_t *track_data = data + offset + 2;
                
                /* Count sync marks (0xFF bytes) */
                int sync_count = 0;
                int long_sync = 0;
                int consecutive_ff = 0;
                
                for (uint16_t j = 0; j < track_size && offset + 2 + j < size; j++) {
                    if (track_data[j] == 0xFF) {
                        consecutive_ff++;
                        if (consecutive_ff >= 5) sync_count++;
                        if (consecutive_ff > 10) long_sync++;
                    } else {
                        consecutive_ff = 0;
                    }
                }
                
                /* Non-standard sync patterns */
                if (sync_count == 0 && track_size > 100) {
                    result->protection_flags |= C64_PROT_KILLER_TRACKS;
                }
                if (long_sync > 5) {
                    result->sync_anomalies++;
                    result->protection_flags |= C64_PROT_GCR_SYNC;
                }
                
                /* Check for long track */
                int expected_size = 7692;  /* Standard track size */
                if (i / 2 < 18) expected_size = 7692;
                else if (i / 2 < 25) expected_size = 7142;
                else if (i / 2 < 31) expected_size = 6666;
                else expected_size = 6250;
                
                if (track_size > expected_size + 200) {
                    result->protection_flags |= C64_PROT_GCR_LONG_TRACK;
                }
            }
        }
    }
    
    /* Check for non-standard speed zones */
    for (int i = 0; i < track_count && i < 84; i++) {
        uint32_t speed = speed_zones[i];
        if (speed > 3) {  /* Custom speed zone */
            result->density_anomalies++;
            result->protection_flags |= C64_PROT_GCR_DENSITY;
        }
    }
    
    result->confidence = 60;
    if (result->uses_half_tracks) result->confidence += 15;
    if (result->sync_anomalies > 0) result->confidence += 10;
    if (result->density_anomalies > 0) result->confidence += 10;
    
    return 0;
}

/* ============================================================================
 * Report Generation
 * ============================================================================ */

int c64_generate_report(const c64_protection_analysis_t *analysis, char *buffer, size_t buffer_size) {
    if (!analysis || !buffer || buffer_size == 0) return 0;
    
    int written = 0;
    
    written += snprintf(buffer + written, buffer_size - written,
        "╔══════════════════════════════════════════════════════════════════╗\n"
        "║          C64 COPY PROTECTION ANALYSIS REPORT                    ║\n"
        "╚══════════════════════════════════════════════════════════════════╝\n\n");
    
    if (analysis->title[0]) {
        written += snprintf(buffer + written, buffer_size - written,
            "Disk Title: %s\n", analysis->title);
    }
    
    if (analysis->protection_name[0]) {
        written += snprintf(buffer + written, buffer_size - written,
            "Protection: %s\n", analysis->protection_name);
    }
    
    written += snprintf(buffer + written, buffer_size - written,
        "Confidence: %d%%\n\n", analysis->confidence);
    
    /* Protection types */
    char prot_str[512];
    c64_protection_to_string(analysis->protection_flags, prot_str, sizeof(prot_str));
    written += snprintf(buffer + written, buffer_size - written,
        "Protection Types Detected:\n  %s\n\n", prot_str);
    
    /* Track info */
    written += snprintf(buffer + written, buffer_size - written,
        "Track Analysis:\n"
        "  Tracks Used: %d\n"
        "  Extended Tracks (36-40): %s\n"
        "  Half-Tracks: %s (%d found)\n\n",
        analysis->tracks_used,
        analysis->uses_track_36_40 ? "Yes" : "No",
        analysis->uses_half_tracks ? "Yes" : "No",
        analysis->half_track_count);
    
    /* Error analysis */
    if (analysis->total_errors > 0) {
        written += snprintf(buffer + written, buffer_size - written,
            "Error Analysis:\n"
            "  Total Error Sectors: %d\n", analysis->total_errors);
        
        for (int i = 0; i < 16; i++) {
            if (analysis->error_counts[i] > 0) {
                written += snprintf(buffer + written, buffer_size - written,
                    "    %s: %d\n", 
                    c64_error_to_string((c64_error_code_t)i),
                    analysis->error_counts[i]);
            }
        }
        
        written += snprintf(buffer + written, buffer_size - written,
            "  Tracks with Errors: ");
        bool first = true;
        for (int t = 1; t <= 40; t++) {
            if (analysis->error_tracks[t]) {
                written += snprintf(buffer + written, buffer_size - written,
                    "%s%d", first ? "" : ", ", t);
                first = false;
            }
        }
        written += snprintf(buffer + written, buffer_size - written, "\n\n");
    }
    
    /* BAM info */
    written += snprintf(buffer + written, buffer_size - written,
        "BAM Analysis:\n"
        "  Valid: %s\n"
        "  Free Blocks: %d\n"
        "  Allocated Blocks: %d\n",
        analysis->bam_valid ? "Yes" : "No",
        analysis->bam_free_blocks,
        analysis->bam_allocated_blocks);
    
    /* GCR info */
    if (analysis->has_gcr_data) {
        written += snprintf(buffer + written, buffer_size - written,
            "\nGCR Analysis:\n"
            "  Sync Anomalies: %d\n"
            "  Density Anomalies: %d\n"
            "  Timing Anomalies: %d\n",
            analysis->sync_anomalies,
            analysis->density_anomalies,
            analysis->timing_anomalies);
    }
    
    if (analysis->notes[0]) {
        written += snprintf(buffer + written, buffer_size - written,
            "\nNotes: %s\n", analysis->notes);
    }
    
    return written;
}

/* ============================================================================
 * Database Access
 * ============================================================================ */

int c64_get_known_titles_count(void) {
    return KNOWN_TITLES_COUNT;
}

const c64_known_title_t *c64_get_known_title(int index) {
    if (index < 0 || index >= (int)KNOWN_TITLES_COUNT) return NULL;
    return &g_known_titles[index];
}

/* ============================================================================
 * V-MAX! Version Detection
 * Based on Pete Rittwage and Lord Crass documentation
 * ============================================================================ */

const char *c64_vmax_version_string(c64_vmax_version_t version) {
    switch (version) {
        case VMAX_VERSION_0: return "V-Max! v0 (Star Rank Boxing - CBM DOS, checksums)";
        case VMAX_VERSION_1: return "V-Max! v1 (Activision - CBM DOS, byte counting)";
        case VMAX_VERSION_2A: return "V-Max! v2a (Cinemaware - single EOR, CBM DOS)";
        case VMAX_VERSION_2B: return "V-Max! v2b (Cinemaware - dual EOR, custom sectors)";
        case VMAX_VERSION_3A: return "V-Max! v3a (Taito - variable sectors, normal syncs)";
        case VMAX_VERSION_3B: return "V-Max! v3b (Taito - variable sectors, short syncs)";
        case VMAX_VERSION_4: return "V-Max! v4 (4 marker bytes variant)";
        default: return "V-Max! (unknown version)";
    }
}

c64_vmax_version_t c64_detect_vmax_version(const uint8_t *data, size_t size,
                                            c64_protection_analysis_t *result) {
    if (!data || size < 12) return VMAX_VERSION_UNKNOWN;
    
    /* Check for G64 signature */
    if (memcmp(data, "GCR-1541", 8) != 0) {
        /* Not G64 - try D64 directory check */
        if (c64_check_vmax_directory(data, size)) {
            result->protection_flags |= C64_PROT_V_MAX;
            result->vmax_version = VMAX_VERSION_2B; /* Likely v2 based on directory */
            return VMAX_VERSION_2B;
        }
        return VMAX_VERSION_UNKNOWN;
    }
    
    /* G64 analysis for V-MAX detection */
    uint8_t num_tracks = data[9];
    
    /* Track offset table starts at byte 12 */
    const uint8_t *track_offsets = data + 12;
    
    /* Check track 20 (V-MAX loader track) */
    uint32_t track20_offset = 0;
    int track20_idx = 19 * 2; /* Track 20, full track (not half) */
    
    if (track20_idx < num_tracks) {
        track20_offset = track_offsets[track20_idx * 4] |
                        (track_offsets[track20_idx * 4 + 1] << 8) |
                        (track_offsets[track20_idx * 4 + 2] << 16) |
                        (track_offsets[track20_idx * 4 + 3] << 24);
    }
    
    if (track20_offset == 0 || track20_offset >= size) {
        return VMAX_VERSION_UNKNOWN;
    }
    
    /* Analyze track 20 for V-MAX signatures */
    uint16_t track20_size = data[track20_offset] | (data[track20_offset + 1] << 8);
    const uint8_t *track20_data = data + track20_offset + 2;
    
    if (track20_size < 100) return VMAX_VERSION_UNKNOWN;
    
    /* Look for V-MAX signatures */
    int sync_count = 0;
    int eor_sections = 0;
    int marker_49_count = 0;
    int marker_5a_count = 0;
    bool found_ee_marker = false;
    
    for (uint16_t i = 0; i < track20_size - 10 && (track20_offset + 2 + i + 10) < size; i++) {
        /* Count sync marks */
        if (track20_data[i] == 0xFF) {
            int sync_len = 0;
            while (i + sync_len < track20_size && track20_data[i + sync_len] == 0xFF) {
                sync_len++;
            }
            if (sync_len >= 5) sync_count++;
            i += sync_len - 1;
        }
        
        /* Count $5A runs (V-MAX v2 signature) */
        if (track20_data[i] == 0x5A) {
            marker_5a_count++;
        }
        
        /* Count $49 markers (V-MAX v3/v4 signature) */
        if (track20_data[i] == 0x49 && i + 1 < track20_size && 
            track20_data[i + 1] == 0x49) {
            marker_49_count++;
        }
        
        /* Check for $EE end-of-header marker (v3) */
        if (track20_data[i] == 0xEE) {
            found_ee_marker = true;
        }
    }
    
    /* Determine V-MAX version based on signatures */
    c64_vmax_version_t detected = VMAX_VERSION_UNKNOWN;
    
    if (marker_49_count > 5 && found_ee_marker) {
        /* V-MAX v3/v4 - check for short syncs */
        int short_sync_count = 0;
        for (uint16_t i = 0; i < track20_size - 5; i++) {
            if (track20_data[i] == 0xFF && track20_data[i + 1] != 0xFF &&
                (i == 0 || track20_data[i - 1] != 0xFF)) {
                short_sync_count++;
            }
        }
        
        if (short_sync_count > 10) {
            detected = VMAX_VERSION_3B;  /* Short syncs = v3b */
        } else {
            detected = VMAX_VERSION_3A;  /* Normal syncs = v3a */
        }
    } else if (marker_5a_count > 20) {
        /* V-MAX v2 - check for EOR sections */
        detected = VMAX_VERSION_2B;  /* Assume v2b (custom sectors) */
    } else if (sync_count > 0 && track20_size > 5000) {
        /* Could be v0 or v1 */
        detected = VMAX_VERSION_1;
    }
    
    if (detected != VMAX_VERSION_UNKNOWN && result) {
        result->protection_flags |= C64_PROT_V_MAX;
        result->vmax_version = detected;
        result->vmax_loader_blocks = (detected >= VMAX_VERSION_3A) ? 8 : 7;
        snprintf(result->protection_name, sizeof(result->protection_name),
                 "%s", c64_vmax_version_string(detected));
    }
    
    return detected;
}

bool c64_check_vmax_directory(const uint8_t *data, size_t size) {
    /* Check for V-MAX v2 "!" only directory signature */
    size_t dir_offset = d64_get_sector_offset(18, 1); /* Directory starts at 18/1 */
    if (dir_offset == (size_t)-1 || dir_offset + 256 > size) return false;
    
    const uint8_t *dir = data + dir_offset;
    
    /* Check first directory entry */
    /* Entry starts at offset 2, filename at offset 5 */
    if (dir[2] == 0x82 && dir[5] == '!' && dir[6] == 0xA0) {
        /* File type PRG, name is "!" padded with shifted space */
        return true;
    }
    
    return false;
}

/* ============================================================================
 * RapidLok Version Detection
 * Based on Pete Rittwage, Banguibob, and Kracker Jax documentation
 * ============================================================================ */

const char *c64_rapidlok_version_string(c64_rapidlok_version_t version) {
    switch (version) {
        case RAPIDLOK_VERSION_1: return "RapidLok v1 (patch keycheck works)";
        case RAPIDLOK_VERSION_2: return "RapidLok v2 (patch keycheck works)";
        case RAPIDLOK_VERSION_3: return "RapidLok v3 (patch keycheck works)";
        case RAPIDLOK_VERSION_4: return "RapidLok v4 (patch keycheck works)";
        case RAPIDLOK_VERSION_5: return "RapidLok v5 (intermittent in VICE)";
        case RAPIDLOK_VERSION_6: return "RapidLok v6 (intermittent in VICE)";
        case RAPIDLOK_VERSION_7: return "RapidLok v7 (requires additional crack)";
        default: return "RapidLok (unknown version)";
    }
}

c64_rapidlok_version_t c64_detect_rapidlok_version(const uint8_t *data, size_t size,
                                                    c64_protection_analysis_t *result) {
    if (!data || size < 12) return RAPIDLOK_VERSION_UNKNOWN;
    
    /* Check for G64 signature */
    if (memcmp(data, "GCR-1541", 8) != 0) {
        return RAPIDLOK_VERSION_UNKNOWN;
    }
    
    uint8_t num_tracks = data[9];
    const uint8_t *track_offsets = data + 12;
    
    /* Check for track 36 (RapidLok key track) */
    int track36_idx = 35 * 2; /* Track 36, full track */
    if (track36_idx >= num_tracks) {
        return RAPIDLOK_VERSION_UNKNOWN;
    }
    
    uint32_t track36_offset = track_offsets[track36_idx * 4] |
                              (track_offsets[track36_idx * 4 + 1] << 8) |
                              (track_offsets[track36_idx * 4 + 2] << 16) |
                              (track_offsets[track36_idx * 4 + 3] << 24);
    
    if (track36_offset == 0 || track36_offset >= size) {
        return RAPIDLOK_VERSION_UNKNOWN;
    }
    
    /* Analyze track 36 for RapidLok key sector */
    uint16_t track36_size = data[track36_offset] | (data[track36_offset + 1] << 8);
    const uint8_t *track36_data = data + track36_offset + 2;
    
    if (track36_size < 100) return RAPIDLOK_VERSION_UNKNOWN;
    
    /* Look for RapidLok signatures */
    bool found_long_sync = false;
    bool found_key_data = false;
    bool found_bad_gcr = false;
    int sync_bits = 0;
    
    /* Count initial sync length */
    for (uint16_t i = 0; i < track36_size && track36_data[i] == 0xFF; i++) {
        sync_bits += 8;
    }
    
    if (sync_bits >= 40) {
        found_long_sync = true;
    }
    
    /* Look for key data pattern and bad GCR ($00) */
    for (uint16_t i = 0; i < track36_size - 10 && (track36_offset + 2 + i + 10) < size; i++) {
        if (track36_data[i] == 0x00) {
            found_bad_gcr = true;
        }
        
        /* Look for encoded key bytes after sync */
        if (track36_data[i] != 0xFF && track36_data[i] != 0x00 &&
            i > 5 && track36_data[i - 5] == 0xFF) {
            found_key_data = true;
        }
    }
    
    if (!found_long_sync || !found_key_data) {
        return RAPIDLOK_VERSION_UNKNOWN;
    }
    
    /* Now check other tracks for RapidLok signatures */
    int rapidlok_track_count = 0;
    int total_7b_count = 0;
    
    for (int track = 1; track <= 35; track++) {
        int track_idx = (track - 1) * 2;
        if (track_idx >= num_tracks) continue;
        
        uint32_t offset = track_offsets[track_idx * 4] |
                         (track_offsets[track_idx * 4 + 1] << 8) |
                         (track_offsets[track_idx * 4 + 2] << 16) |
                         (track_offsets[track_idx * 4 + 3] << 24);
        
        if (offset == 0 || offset >= size) continue;
        
        uint16_t track_size = data[offset] | (data[offset + 1] << 8);
        const uint8_t *track_data = data + offset + 2;
        
        /* Count $7B bytes (extra sector signature) */
        int count_7b = 0;
        for (uint16_t i = 0; i < track_size && (offset + 2 + i) < size; i++) {
            if (track_data[i] == RAPIDLOK_EXTRA_SECTOR) {
                count_7b++;
            }
        }
        
        if (count_7b > 10) {
            rapidlok_track_count++;
            total_7b_count += count_7b;
            if (result && track <= 35) {
                result->rapidlok_7b_counts[track] = count_7b;
            }
        }
        
        /* Look for $75 (sector header) and $6B (data block) markers */
        bool found_75 = false;
        bool found_6b = false;
        for (uint16_t i = 0; i < track_size - 1 && (offset + 2 + i + 1) < size; i++) {
            if (track_data[i] == RAPIDLOK_SECTOR_HEADER) found_75 = true;
            if (track_data[i] == RAPIDLOK_DATA_BLOCK) found_6b = true;
        }
        
        if (found_75 && found_6b) {
            rapidlok_track_count++;
        }
    }
    
    if (rapidlok_track_count < 5) {
        return RAPIDLOK_VERSION_UNKNOWN;
    }
    
    /* Determine RapidLok version based on characteristics */
    c64_rapidlok_version_t detected = RAPIDLOK_VERSION_UNKNOWN;
    
    /* Version detection heuristics based on sync lengths and structure */
    if (total_7b_count > 500) {
        /* Later versions have more complex $7B patterns */
        detected = RAPIDLOK_VERSION_6;  /* Common in MicroProse games like Pirates! */
    } else if (total_7b_count > 300) {
        detected = RAPIDLOK_VERSION_5;
    } else if (rapidlok_track_count > 20) {
        detected = RAPIDLOK_VERSION_4;
    } else {
        detected = RAPIDLOK_VERSION_3;
    }
    
    if (result) {
        result->protection_flags |= C64_PROT_RAPIDLOK | C64_PROT_EXTRA_TRACKS;
        result->rapidlok_version = detected;
        result->rapidlok_key_valid = found_key_data;
        result->rapidlok_sync_track_start = sync_bits;
        snprintf(result->protection_name, sizeof(result->protection_name),
                 "%s", c64_rapidlok_version_string(detected));
    }
    
    return detected;
}

bool c64_extract_rapidlok_key(const uint8_t *data, size_t size, uint8_t *key_table) {
    if (!data || !key_table || size < 12) return false;
    
    /* Check for G64 signature */
    if (memcmp(data, "GCR-1541", 8) != 0) return false;
    
    uint8_t num_tracks = data[9];
    const uint8_t *track_offsets = data + 12;
    
    /* Get track 36 offset */
    int track36_idx = 35 * 2;
    if (track36_idx >= num_tracks) return false;
    
    uint32_t track36_offset = track_offsets[track36_idx * 4] |
                              (track_offsets[track36_idx * 4 + 1] << 8) |
                              (track_offsets[track36_idx * 4 + 2] << 16) |
                              (track_offsets[track36_idx * 4 + 3] << 24);
    
    if (track36_offset == 0 || track36_offset >= size) return false;
    
    uint16_t track36_size = data[track36_offset] | (data[track36_offset + 1] << 8);
    const uint8_t *track36_data = data + track36_offset + 2;
    
    /* Find key data after sync mark */
    int key_start = 0;
    for (uint16_t i = 0; i < track36_size && track36_data[i] == 0xFF; i++) {
        key_start = i + 1;
    }
    
    if (key_start == 0 || key_start + 35 > track36_size) return false;
    
    /* Extract 35 key values (one per track) */
    /* Note: In real RapidLok, this is encrypted/encoded */
    for (int i = 0; i < 35 && (key_start + i) < track36_size; i++) {
        key_table[i] = track36_data[key_start + i];
    }
    
    return true;
}

/* ============================================================================
 * Datasoft Long Track Protection Detection
 * Technical: Uses tracks with more data than normal (6680 bytes vs 6500)
 * Titles: Bruce Lee, Mr. Do!, Dig Dug, Pac-Man, Conan, etc.
 * ============================================================================ */

bool c64_detect_datasoft(const uint8_t *data, size_t size,
                          c64_protection_analysis_t *result) {
    if (!data || size < 12) return false;
    
    /* Check for G64 signature */
    if (memcmp(data, "GCR-1541", 8) != 0) {
        /* For D64, check directory for Datasoft titles */
        return c64_detect_datasoft_d64(data, size, result);
    }
    
    /* G64 analysis - look for long tracks */
    uint8_t num_tracks = data[9];
    const uint8_t *track_offsets = data + 12;
    
    int long_track_count = 0;
    int max_track_bytes = 0;
    
    for (int track = 1; track <= 35; track++) {
        int track_idx = (track - 1) * 2;
        if (track_idx >= num_tracks) continue;
        
        uint32_t offset = track_offsets[track_idx * 4] |
                         (track_offsets[track_idx * 4 + 1] << 8) |
                         (track_offsets[track_idx * 4 + 2] << 16) |
                         (track_offsets[track_idx * 4 + 3] << 24);
        
        if (offset == 0 || offset >= size) continue;
        
        uint16_t track_size = data[offset] | (data[offset + 1] << 8);
        
        /* Standard track sizes by zone:
         * Zone 1 (tracks 1-17):  ~7692 bytes
         * Zone 2 (tracks 18-24): ~7142 bytes
         * Zone 3 (tracks 25-30): ~6666 bytes
         * Zone 4 (tracks 31-35): ~6250 bytes
         */
        int expected_max;
        if (track <= 17) expected_max = 7800;
        else if (track <= 24) expected_max = 7250;
        else if (track <= 30) expected_max = 6750;
        else expected_max = 6350;
        
        if (track_size > expected_max) {
            long_track_count++;
            if (track_size > max_track_bytes) {
                max_track_bytes = track_size;
            }
        }
    }
    
    /* Datasoft protection uses tracks with > 6680 bytes (vs ~6500 normal) */
    if (long_track_count >= 3 && max_track_bytes > DATASOFT_LONG_TRACK_BYTES - 200) {
        if (result) {
            result->protection_flags |= C64_PROT_DATASOFT | C64_PROT_GCR_LONG_TRACK;
            result->confidence = 75 + (long_track_count * 2);
            if (result->confidence > 95) result->confidence = 95;
            snprintf(result->protection_name, sizeof(result->protection_name),
                     "Datasoft Long Track (%d tracks, max %d bytes)", 
                     long_track_count, max_track_bytes);
        }
        return true;
    }
    
    return false;
}

bool c64_detect_datasoft_d64(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result) {
    /* For D64 images, we can't detect long tracks directly
     * Instead check directory for known Datasoft titles and patterns */
    
    if (size < D64_35_TRACKS) return false;
    
    /* Check directory track 18 for Datasoft patterns */
    size_t dir_offset = d64_get_sector_offset(18, 1);
    if (dir_offset == (size_t)-1 || dir_offset + 256 > size) return false;
    
    /* Check BAM for Datasoft signature patterns */
    size_t bam_offset = d64_get_sector_offset(18, 0);
    if (bam_offset == (size_t)-1 || bam_offset + 256 > size) return false;
    
    const uint8_t *bam = data + bam_offset;
    
    /* Datasoft often has specific BAM patterns */
    /* Check disk name offset 0x90 for "DATASOFT" or known titles */
    char disk_name[17];
    memcpy(disk_name, bam + 0x90, 16);
    disk_name[16] = '\0';
    
    /* Check for known Datasoft disk names */
    const char *datasoft_names[] = {
        "BRUCE LEE", "MR. DO", "DIG DUG", "PAC-MAN", "CONAN",
        "POLE POSITION", "ZAXXON", "POOYAN", "AZTEC", "GOONIES",
        "DALLAS", "ALTERNATE", NULL
    };
    
    for (int i = 0; datasoft_names[i] != NULL; i++) {
        if (strstr(disk_name, datasoft_names[i]) != NULL) {
            if (result) {
                result->protection_flags |= C64_PROT_DATASOFT;
                result->publisher = C64_PUB_DATASOFT;
                result->confidence = 80;
                snprintf(result->protection_name, sizeof(result->protection_name),
                         "Datasoft (detected from disk name)");
            }
            return true;
        }
    }
    
    return false;
}

/* ============================================================================
 * SSI RapidDOS Protection Detection
 * Technical: Custom DOS with track 36 key, 10 sectors per track
 * Titles: Pool of Radiance, Curse of Azure Bonds, War games
 * ============================================================================ */

bool c64_detect_ssi_rdos(const uint8_t *data, size_t size,
                          c64_protection_analysis_t *result) {
    if (!data || size < 12) return false;
    
    bool is_g64 = (memcmp(data, "GCR-1541", 8) == 0);
    
    if (is_g64) {
        return c64_detect_ssi_rdos_g64(data, size, result);
    } else {
        return c64_detect_ssi_rdos_d64(data, size, result);
    }
}

bool c64_detect_ssi_rdos_g64(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result) {
    /* G64 analysis for SSI RapidDOS */
    uint8_t num_tracks = data[9];
    const uint8_t *track_offsets = data + 12;
    
    /* Check for track 36 (SSI key track) */
    int track36_idx = 35 * 2;
    if (track36_idx >= num_tracks) return false;
    
    uint32_t track36_offset = track_offsets[track36_idx * 4] |
                              (track_offsets[track36_idx * 4 + 1] << 8) |
                              (track_offsets[track36_idx * 4 + 2] << 16) |
                              (track_offsets[track36_idx * 4 + 3] << 24);
    
    if (track36_offset == 0 || track36_offset >= size) return false;
    
    uint16_t track36_size = data[track36_offset] | (data[track36_offset + 1] << 8);
    const uint8_t *track36_data = data + track36_offset + 2;
    
    /* Look for SSI RapidDOS signatures */
    /* SSI uses custom header marker $4B instead of standard $08 */
    int ssi_header_count = 0;
    bool found_key_pattern = false;
    
    for (uint16_t i = 0; i < track36_size - 10 && (track36_offset + 2 + i + 10) < size; i++) {
        /* Look for SSI custom header marker */
        if (track36_data[i] == SSI_RDOS_HEADER_MARKER) {
            ssi_header_count++;
        }
        
        /* SSI key pattern: specific byte sequence after sync */
        if (i > 5 && track36_data[i - 1] == 0xFF && 
            track36_data[i] != 0xFF && track36_data[i] != 0x00) {
            /* Check for 10-sector structure */
            int sector_count = 0;
            for (uint16_t j = i; j < track36_size - 1 && j < i + 3000; j++) {
                if (track36_data[j] == 0x08 || track36_data[j] == SSI_RDOS_HEADER_MARKER) {
                    sector_count++;
                }
            }
            if (sector_count >= 8 && sector_count <= 12) {
                found_key_pattern = true;
            }
        }
    }
    
    /* Check other tracks for 10-sector structure (instead of standard 17-21) */
    int tracks_with_10_sectors = 0;
    
    for (int track = 1; track <= 35; track++) {
        int track_idx = (track - 1) * 2;
        if (track_idx >= num_tracks) continue;
        
        uint32_t offset = track_offsets[track_idx * 4] |
                         (track_offsets[track_idx * 4 + 1] << 8) |
                         (track_offsets[track_idx * 4 + 2] << 16) |
                         (track_offsets[track_idx * 4 + 3] << 24);
        
        if (offset == 0 || offset >= size) continue;
        
        uint16_t track_size = data[offset] | (data[offset + 1] << 8);
        const uint8_t *track_data = data + offset + 2;
        
        /* Count sector headers (standard $08 or custom) */
        int sector_headers = 0;
        for (uint16_t i = 0; i < track_size - 5 && (offset + 2 + i + 5) < size; i++) {
            /* Standard sector header after sync: FF FF FF FF FF 08 */
            if (track_data[i] == 0x08 && i >= 5 && track_data[i - 1] == 0xFF) {
                sector_headers++;
            }
        }
        
        /* SSI RapidDOS uses 10 sectors per track instead of standard */
        if (sector_headers >= 9 && sector_headers <= 11) {
            tracks_with_10_sectors++;
        }
    }
    
    if ((found_key_pattern && tracks_with_10_sectors >= 5) || ssi_header_count >= 5) {
        if (result) {
            result->protection_flags |= C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS;
            result->publisher = C64_PUB_SSI;
            result->confidence = 80 + (tracks_with_10_sectors / 2);
            if (result->confidence > 95) result->confidence = 95;
            snprintf(result->protection_name, sizeof(result->protection_name),
                     "SSI RapidDOS (10 sectors/track, track 36 key)");
        }
        return true;
    }
    
    return false;
}

bool c64_detect_ssi_rdos_d64(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result) {
    /* For D64, check for SSI titles in directory or known patterns */
    if (size < D64_35_TRACKS) return false;
    
    /* Check BAM for SSI disk name patterns */
    size_t bam_offset = d64_get_sector_offset(18, 0);
    if (bam_offset == (size_t)-1 || bam_offset + 256 > size) return false;
    
    const uint8_t *bam = data + bam_offset;
    char disk_name[17];
    memcpy(disk_name, bam + 0x90, 16);
    disk_name[16] = '\0';
    
    /* Check for known SSI disk names */
    const char *ssi_names[] = {
        "POOL OF RAD", "CURSE", "AZURE", "CHAMPIONS", "KRYNN",
        "DEATH KNIGHT", "GATEWAY", "SAVAGE", "QUESTRON", 
        "PANZER", "KAMPFGRUPPE", "CARRIER", "ROADWAR",
        "GETTYSBURG", "ANTIETAM", "SHILOH", "NAM", NULL
    };
    
    for (int i = 0; ssi_names[i] != NULL; i++) {
        if (strstr(disk_name, ssi_names[i]) != NULL) {
            if (result) {
                result->protection_flags |= C64_PROT_SSI_RDOS;
                result->publisher = C64_PUB_SSI;
                result->confidence = 75;
                snprintf(result->protection_name, sizeof(result->protection_name),
                         "SSI RapidDOS (detected from disk name)");
            }
            return true;
        }
    }
    
    /* Check for 40-track D64 (SSI often uses extended tracks) */
    if (size >= D64_40_TRACKS) {
        /* Has extended tracks - more likely to be SSI */
        /* Additional heuristic: check if tracks 36-40 have data */
        bool has_track_36_data = false;
        for (int track = 36; track <= 40; track++) {
            size_t track_offset = d64_get_sector_offset(track, 0);
            if (track_offset != (size_t)-1 && track_offset + 256 <= size) {
                const uint8_t *sector = data + track_offset;
                /* Check if sector has non-zero data */
                for (int i = 0; i < 256; i++) {
                    if (sector[i] != 0x00 && sector[i] != 0x01) {
                        has_track_36_data = true;
                        break;
                    }
                }
            }
        }
        
        if (has_track_36_data) {
            if (result) {
                result->protection_flags |= C64_PROT_SSI_RDOS | C64_PROT_EXTRA_TRACKS;
                result->publisher = C64_PUB_SSI;
                result->confidence = 60;
                snprintf(result->protection_name, sizeof(result->protection_name),
                         "SSI RapidDOS (40-track image with extended data)");
            }
            return true;
        }
    }
    
    return false;
}

/* ============================================================================
 * EA Interlock Protection Detection
 * Technical: Custom DOS with interleave and specific boot sequence
 * Titles: Bard's Tale, Archon, Seven Cities of Gold, etc.
 * ============================================================================ */

bool c64_detect_ea_interlock(const uint8_t *data, size_t size,
                              c64_protection_analysis_t *result) {
    if (!data || size < D64_35_TRACKS) return false;
    
    /* Check BAM for EA disk patterns */
    size_t bam_offset = d64_get_sector_offset(18, 0);
    if (bam_offset == (size_t)-1 || bam_offset + 256 > size) return false;
    
    const uint8_t *bam = data + bam_offset;
    
    /* EA Interlock characteristics:
     * 1. Specific disk ID patterns
     * 2. Custom sector interleave
     * 3. Boot sector at track 1, sector 0 with EA signature
     */
    
    /* Check boot sector for EA signature */
    size_t boot_offset = d64_get_sector_offset(1, 0);
    if (boot_offset == (size_t)-1 || boot_offset + 256 > size) return false;
    
    const uint8_t *boot = data + boot_offset;
    
    /* EA boot sectors often have specific patterns */
    bool found_ea_boot = false;
    
    /* Look for "EA" or Electronic Arts signatures in boot sector */
    for (int i = 0; i < 200; i++) {
        if (boot[i] == 'E' && boot[i + 1] == 'A' && boot[i + 2] == ' ') {
            found_ea_boot = true;
            break;
        }
        /* Also check for common EA loader patterns */
        if (boot[i] == 0x4C && (boot[i + 2] == 0x08 || boot[i + 2] == 0x03)) {
            /* JMP instruction to common EA loader address */
            found_ea_boot = true;
        }
    }
    
    /* Check disk name for EA titles */
    char disk_name[17];
    memcpy(disk_name, bam + 0x90, 16);
    disk_name[16] = '\0';
    
    const char *ea_names[] = {
        "ARCHON", "BARD", "SEVEN CITIES", "STARFLIGHT", "SKYFOX",
        "MULE", "MAIL ORDER", "RACING DEST", "MOVIE MAKER",
        "MARBLE", "WASTELAND", "ULTIMA", "CHUCK YEAGER",
        "MADDEN", "EARL WEAVER", "DELUXE", NULL
    };
    
    bool found_ea_name = false;
    for (int i = 0; ea_names[i] != NULL; i++) {
        if (strstr(disk_name, ea_names[i]) != NULL) {
            found_ea_name = true;
            break;
        }
    }
    
    /* Check sector interleave pattern (EA uses non-standard interleave) */
    int interleave_anomalies = 0;
    size_t dir_offset = d64_get_sector_offset(18, 1);
    if (dir_offset != (size_t)-1 && dir_offset + 256 <= size) {
        const uint8_t *dir = data + dir_offset;
        
        /* Check directory chain for non-standard sector links */
        int prev_sector = 1;
        int curr_track = 18;
        int curr_sector = dir[0] == 18 ? dir[1] : -1;
        
        while (curr_sector > 0 && curr_sector < 19) {
            int expected_next = (prev_sector + 10) % 19; /* Standard interleave 10 */
            if (curr_sector != expected_next && curr_sector != (prev_sector + 1) % 19) {
                interleave_anomalies++;
            }
            
            size_t next_offset = d64_get_sector_offset(curr_track, curr_sector);
            if (next_offset == (size_t)-1 || next_offset + 256 > size) break;
            
            prev_sector = curr_sector;
            const uint8_t *next_sector = data + next_offset;
            curr_track = next_sector[0];
            curr_sector = next_sector[1];
            
            if (curr_track != 18) break;
        }
    }
    
    if (found_ea_boot || found_ea_name || interleave_anomalies >= 3) {
        if (result) {
            result->protection_flags |= C64_PROT_EA_INTERLOCK;
            result->publisher = C64_PUB_ELECTRONIC_ARTS;
            result->confidence = 60;
            if (found_ea_boot) result->confidence += 15;
            if (found_ea_name) result->confidence += 15;
            if (interleave_anomalies >= 3) result->confidence += 10;
            if (result->confidence > 95) result->confidence = 95;
            snprintf(result->protection_name, sizeof(result->protection_name),
                     "EA Interlock");
        }
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Novaload Protection Detection (Tape, but also disk variants)
 * Technical: Fast loader with anti-tampering, stack manipulation
 * Titles: Combat School, Target Renegade, Gryzor, etc. (Ocean/Imagine)
 * ============================================================================ */

bool c64_detect_novaload(const uint8_t *data, size_t size,
                          c64_protection_analysis_t *result) {
    if (!data || size < D64_35_TRACKS) return false;
    
    /* Novaload is primarily a tape protection, but disk versions exist
     * Key characteristics:
     * 1. Specific loader signature in boot sector
     * 2. Stack manipulation code
     * 3. Self-wiping on tampering
     */
    
    /* Check boot sector for Novaload loader */
    size_t boot_offset = d64_get_sector_offset(1, 0);
    if (boot_offset == (size_t)-1 || boot_offset + 256 > size) return false;
    
    const uint8_t *boot = data + boot_offset;
    
    bool found_novaload = false;
    int novaload_score = 0;
    
    /* Look for Novaload signature patterns */
    /* Novaload often uses these opcodes for its fast loader:
     * LDA #$xx, STA $01 - switch memory config
     * SEI - disable interrupts
     * LDA $DD00, AND #$03, ORA #$04, STA $DD00 - set VIC bank
     */
    
    for (int i = 0; i < 200; i++) {
        /* SEI instruction (disable interrupts) - common in Novaload */
        if (boot[i] == 0x78) novaload_score++;
        
        /* STA $01 (memory config) after LDA #xx */
        if (boot[i] == 0x85 && boot[i + 1] == 0x01 && i > 0 && boot[i - 2] == 0xA9) {
            novaload_score += 2;
        }
        
        /* LDA $DD00 (VIC bank register) */
        if (boot[i] == 0xAD && boot[i + 1] == 0x00 && boot[i + 2] == 0xDD) {
            novaload_score += 2;
        }
        
        /* Stack pointer manipulation: TXS after LDX #$xx */
        if (boot[i] == 0x9A && i > 0 && boot[i - 2] == 0xA2) {
            novaload_score += 3;  /* Stack manipulation is key Novaload feature */
        }
        
        /* Look for "NOVALOAD" or "NOVA" string */
        if (i < 196 && boot[i] == 'N' && boot[i + 1] == 'O' && 
            boot[i + 2] == 'V' && boot[i + 3] == 'A') {
            found_novaload = true;
            novaload_score += 10;
        }
    }
    
    /* Check disk name for known Novaload/Ocean titles */
    size_t bam_offset = d64_get_sector_offset(18, 0);
    if (bam_offset != (size_t)-1 && bam_offset + 256 <= size) {
        const uint8_t *bam = data + bam_offset;
        char disk_name[17];
        memcpy(disk_name, bam + 0x90, 16);
        disk_name[16] = '\0';
        
        const char *novaload_names[] = {
            "COMBAT SCHOOL", "TARGET RENE", "GRYZOR", "HEAD OVER",
            "GREEN BERET", "YIE AR", "IMAGINE", "OCEAN", NULL
        };
        
        for (int i = 0; novaload_names[i] != NULL; i++) {
            if (strstr(disk_name, novaload_names[i]) != NULL) {
                novaload_score += 5;
                break;
            }
        }
    }
    
    /* Detection threshold */
    if (found_novaload || novaload_score >= 8) {
        if (result) {
            result->protection_flags |= C64_PROT_NOVALOAD;
            result->publisher = C64_PUB_OCEAN;
            result->confidence = 50 + novaload_score * 3;
            if (result->confidence > 95) result->confidence = 95;
            snprintf(result->protection_name, sizeof(result->protection_name),
                     "Novaload (fast loader with anti-tampering)");
        }
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Speedlock Protection Detection
 * Technical: Custom loader with encrypted code, timing checks
 * Titles: Many Ocean, US Gold titles
 * ============================================================================ */

bool c64_detect_speedlock(const uint8_t *data, size_t size,
                           c64_protection_analysis_t *result) {
    if (!data || size < D64_35_TRACKS) return false;
    
    /* Check boot sector for Speedlock loader signatures */
    size_t boot_offset = d64_get_sector_offset(1, 0);
    if (boot_offset == (size_t)-1 || boot_offset + 256 > size) return false;
    
    const uint8_t *boot = data + boot_offset;
    
    int speedlock_score = 0;
    
    /* Speedlock characteristics:
     * 1. Decryption loop at start
     * 2. Timing-based checks
     * 3. Specific byte patterns
     */
    
    for (int i = 0; i < 200; i++) {
        /* EOR (Exclusive OR) decryption - common in Speedlock */
        if (boot[i] == 0x49 || boot[i] == 0x45 || boot[i] == 0x55) {
            /* EOR #xx, EOR zp, EOR zp,X */
            speedlock_score++;
        }
        
        /* DEY/DEX in tight loop (decryption counter) */
        if ((boot[i] == 0x88 || boot[i] == 0xCA) && 
            i > 0 && (boot[i - 1] == 0xD0 || boot[i + 1] == 0xD0)) {
            speedlock_score += 2;
        }
        
        /* CIA timer access ($DC04-$DC05) for timing checks */
        if (boot[i] == 0xAD && boot[i + 2] == 0xDC &&
            (boot[i + 1] == 0x04 || boot[i + 1] == 0x05)) {
            speedlock_score += 3;
        }
    }
    
    /* Check disk name for known Speedlock/Ocean/US Gold titles */
    size_t bam_offset = d64_get_sector_offset(18, 0);
    if (bam_offset != (size_t)-1 && bam_offset + 256 <= size) {
        const uint8_t *bam = data + bam_offset;
        char disk_name[17];
        memcpy(disk_name, bam + 0x90, 16);
        disk_name[16] = '\0';
        
        const char *speedlock_names[] = {
            "GAUNTLET", "ROAD RUNNER", "720", "INDIANA", "IKARI",
            "COMMANDO", "GHOSTS", "1942", "1943", "BIONIC",
            "WIZBALL", "TRANSFORMERS", "TERMINATOR", NULL
        };
        
        for (int i = 0; speedlock_names[i] != NULL; i++) {
            if (strstr(disk_name, speedlock_names[i]) != NULL) {
                speedlock_score += 5;
                break;
            }
        }
    }
    
    if (speedlock_score >= 7) {
        if (result) {
            result->protection_flags |= C64_PROT_SPEEDLOCK;
            result->publisher = C64_PUB_US_GOLD;
            result->confidence = 50 + speedlock_score * 3;
            if (result->confidence > 95) result->confidence = 95;
            snprintf(result->protection_name, sizeof(result->protection_name),
                     "Speedlock (encrypted loader with timing checks)");
        }
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Unified Protection Detection - Run all detectors
 * ============================================================================ */

void c64_detect_all_protections(const uint8_t *data, size_t size,
                                 c64_protection_analysis_t *result) {
    if (!data || !result) return;
    
    memset(result, 0, sizeof(*result));
    
    /* Run base analysis first */
    bool is_g64 = (size >= 12 && memcmp(data, "GCR-1541", 8) == 0);
    
    if (is_g64) {
        c64_analyze_g64(data, size, result);
    } else {
        c64_analyze_d64(data, size, result);
    }
    
    /* Now run specific protection detectors */
    
    /* V-MAX! */
    c64_detect_vmax_version(data, size, result);
    
    /* RapidLok */
    c64_detect_rapidlok_version(data, size, result);
    
    /* Datasoft */
    c64_detect_datasoft(data, size, result);
    
    /* SSI RapidDOS */
    c64_detect_ssi_rdos(data, size, result);
    
    /* EA Interlock */
    c64_detect_ea_interlock(data, size, result);
    
    /* Novaload */
    c64_detect_novaload(data, size, result);
    
    /* Speedlock */
    c64_detect_speedlock(data, size, result);
    
    /* Try to match known title if no protection name set */
    if (result->protection_name[0] == '\0' && result->protection_flags != C64_PROT_NONE) {
        c64_protection_to_string(result->protection_flags, result->protection_name, 
                                  sizeof(result->protection_name));
    }
}
