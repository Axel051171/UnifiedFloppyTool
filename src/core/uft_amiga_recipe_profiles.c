#include "uft/core/uft_amiga_recipe_profiles.h"

/* Acht Amiga-Leseprofile — Anordnungszahlen, keine Zeilen.
 *
 * Hier stand: "These profiles contain independently expressed layout
 * facts only. They do not copy WHDLoad/RawDIC implementation code."
 * Der Satz bleibt richtig und wird nicht zurueckgenommen — aber eine
 * VERNEINUNG ist keine Attribution (MF-636: wer eine setzt, nennt die
 * Quelle UND ihre Lizenz).
 *
 * QUELLE:
 *   WHDLoad Development Package, Bert Jahn, Copyright 1995-2025.
 *   Die Vorlagen sind die RawDIC-Imager-Beispiele unter
 *   `Src/imager-examples/` (im Baum: `neue-ideen/fertige/2/WHDLoad_dev/`).
 *
 * LIZENZ, an den DATEIEN gemessen, nicht an einem Projektfeld:
 *   - `Docs/en/copy.html` (Paketlizenz), woertlich: "All rights
 *     reserved. Commercial use is only allowed with an explicit,
 *     written permission by the author. It is strictly prohibited to
 *     modify any part of the WHDLoad package, EXCEPT THE SOURCE FILES
 *     WHICH MAY BE MODIFIED FOR REUSE." Disassemblierung wird NICHT
 *     untersagt (anders als bei x50conv).
 *   - Fuenf der acht Vorlagen tragen selbst `:Copyright. Public Domain`
 *     (arkanoid, ik+, Lotus2, robocop2, worldsoflegend).
 *   - `Cavitas.islave.s` und `Gremlin.islave.s` tragen KEINE Lizenzzeile
 *     und fallen damit unter die Paketlizenz.
 *   - `WoodysWorld.islave.s:2` traegt ein FREMDES Copyright:
 *     "(c) 1993 Vision".
 *
 * KANAL nach MF-695: *Nachbau*, nicht *Port*. Uebernommen sind Zahlen —
 * Spurbereiche, Syncworte, Laengen, Seitenreihenfolge —, keine Zeilen
 * Quelltext. Dasselbe Verfahren wie bei OS_VOLUME aus SDISK (MF-1176).
 * Eigentuemer-Entscheidung vom 2026-09-21, woertlich "los A bis E" auf
 * eine Liste, in der dieser Punkt als offene Lizenzfrage stand.
 *
 * WAS DAMIT NICHT ENTSCHIEDEN IST, und das bleibt benannt: "non-
 * commercial only" ist GPL-unvertraeglich. Die Entscheidung deckt die
 * ZAHLEN; sie deckt keine Uebernahme von Quelltext aus dem Paket und
 * nicht die Weitergabe der Vorlagen selbst.
 *
 * BELEGSTAND: jedes Profil traegt `verified_with_real_media = false`.
 * Es gibt fuer keines eine echte Aufnahme und keinen Abgleich gegen ein
 * fremdes Werkzeug. Die Profile sind Bestand, nicht Faehigkeit.
 *
 * SEITENREIHENFOLGE — die einzige Zahl, die hier nachgemessen ist:
 * `UFT_RECIPE_SIDE_SWAPPED` bei Gremlin und Lotus 2 ist KEIN Versehen.
 * Beide Vorlagen setzen die Flagge ausdruecklich:
 *   - `Gremlin.islave.s:53` und `:64`  — `dc.w DFLG_SWAPSIDES`
 *   - `Lotus2.islave.asm:66`           — `dc.w DFLG_SWAPSIDES|DFLG_NORESTRICTIONS`
 * Was die Flagge GENAU bedeutet, ist im Paket nicht dokumentiert —
 * `Src/resource/RawDICDiskFlags.s:11` listet nur ihren Namen. Die
 * Abbildung auf SWAPPED folgt also dem NAMEN, nicht einer Beschreibung,
 * und mehr wird hier nicht behauptet. Der Baum rechnet Amiga sonst als
 * `cyl*2+head` (an fremder Hand abgenommen, 1760 von 1760 Sektoren in
 * `adf_ext`); diese zwei Profile weichen davon mit Beleg ab.
 * Nebenbei aus derselben Flaggenliste: `DFLG_DOUBLEINC` ist der
 * `track_step = 2`-Fall, an dem die Rezept-Maschine bis zu dieser Runde
 * einen gelungenen Lauf als `UFT_R_EBOUNDS` gemeldet hat. */

#define RULE(F,L,LEN,S0,S1) { \
    .first_track=(F), .last_track=(L), .track_step=1, .decoded_bytes=0, \
    .raw_min_bytes=(LEN)*2u, .raw_max_bytes=0x10000, \
    .input_level=UFT_RECIPE_INPUT_BITSTREAM, \
    .sync_kind=UFT_RECIPE_SYNC_WORD, .sync={(S0),(S1)}, .sync_bits=16, \
    .sync_occurrence=1, .decoder_id="raw-copy", \
    .output_mode=UFT_RECIPE_OUTPUT_ANALYSIS_ONLY, \
    .error_policy=UFT_RECIPE_ERROR_RETRY, .retries=4, .revolutions=3, \
    .preserve_bad_capture=true }

#define RECIPE(NAME,TITLE,HEADS,ORDER,VARS) { \
    .api_version=UFT_RECIPE_API_VERSION, .id=(NAME), .title=(TITLE), \
    .platform="Commodore Amiga", \
    .provenance="Clean-room layout profile; source observations require real-media verification", \
    .cylinders=80, .heads=(HEADS), .side_order=(ORDER), \
    .variants=(VARS), .variant_count=sizeof(VARS)/sizeof((VARS)[0]), \
    .required_capabilities=UFT_RECIPE_CAP_BITSTREAM|UFT_RECIPE_CAP_MULTI_REV, \
    .preservation_recipe=true, .verified_with_real_media=false }

static const uft_recipe_track_rule_t arkanoid_rules[] = {
    RULE(2,61,0x1a20,0x95,0x21)
};
static const uft_recipe_variant_t arkanoid_vars[] = {
    {"observed-layout","Tracks 2-61; initial sync 0x9521; later sync transitions need decoder fixture",
     arkanoid_rules,1,NULL,0,"arkanoid-file-chain-unimplemented"}
};
const uft_imaging_recipe_t UFT_RECIPE_ARKANOID_AMIGA =
    RECIPE("amiga.arkanoid.capture","Arkanoid capture profile",2,
           UFT_RECIPE_SIDE_INTERLEAVED,arkanoid_vars);

static const uft_recipe_track_rule_t gremlin_rules[] = {
    RULE(1,1,0x1800,0x41,0x24),
    RULE(2,159,0x1800,0x44,0x89)
};
static const uft_recipe_variant_t gremlin_vars[] = {
    {"protected-track-1","Track 1 protection candidate plus custom game tracks",
     gremlin_rules,2,NULL,0,"gremlin-directory-unimplemented"}
};
const uft_imaging_recipe_t UFT_RECIPE_GREMLIN_AMIGA =
    RECIPE("amiga.gremlin.capture","Gremlin family capture profile",2,
           UFT_RECIPE_SIDE_SWAPPED,gremlin_vars);

static const uft_recipe_track_rule_t lotus_rules[] = {
    RULE(0,1,0x1600,0x44,0x89),
    RULE(2,157,0x1800,0x44,0x89),
    { .first_track=158,.last_track=159,.track_step=1,.decoded_bytes=0,
      .raw_min_bytes=0x3200,.raw_max_bytes=0x10000,
      .input_level=UFT_RECIPE_INPUT_BITSTREAM,
      .sync_kind=UFT_RECIPE_SYNC_SEQUENCE,
      .sync={0x41,0x24,0x41,0x24},.sync_bits=32,.sync_occurrence=1,
      .decoder_id="raw-copy",.output_mode=UFT_RECIPE_OUTPUT_ANALYSIS_ONLY,
      .error_policy=UFT_RECIPE_ERROR_RETRY,.retries=7,.revolutions=5,
      .preserve_bad_capture=true }
};
static const uft_recipe_variant_t lotus_vars[] = {
    {"custom-long-track","Custom tracks plus long protection tracks 158-159",
     lotus_rules,3,NULL,0,"lotus-directory-unimplemented"}
};
const uft_imaging_recipe_t UFT_RECIPE_LOTUS2_AMIGA =
    RECIPE("amiga.lotus2.capture","Lotus 2 capture profile",2,
           UFT_RECIPE_SIDE_SWAPPED,lotus_vars);

static const uft_recipe_track_rule_t ik_rules[] = {
    RULE(0,0,0x1600,0x44,0x89),
    RULE(1,60,0x1800,0x89,0x44)
};
static const uft_recipe_variant_t ik_vars[] = {
    {"single-side","Single-side observed layout",ik_rules,2,NULL,0,
     "ikplus-decoder-unimplemented"}
};
const uft_imaging_recipe_t UFT_RECIPE_IKPLUS_AMIGA =
    RECIPE("amiga.ikplus.capture","IK+ capture profile",1,
           UFT_RECIPE_SIDE_SINGLE_0,ik_vars);

static const uft_recipe_track_rule_t robocop_rules[] = {
    RULE(0,159,0x1800,0x89,0x44)
};
static const uft_recipe_variant_t robocop_vars[] = {
    {"12-sector-directory","12-sector custom layout; directory observed on track 80",
     robocop_rules,1,NULL,0,"robocop2-directory-unimplemented"}
};
const uft_imaging_recipe_t UFT_RECIPE_ROBOCOP2_AMIGA =
    RECIPE("amiga.robocop2.capture","RoboCop 2 capture profile",2,
           UFT_RECIPE_SIDE_INTERLEAVED,robocop_vars);

static const uft_recipe_track_rule_t woody_rules[] = {
    RULE(1,159,0x1600,0x44,0x8a)
};
static const uft_recipe_variant_t woody_vars[] = {
    {"three-disk-layout","Custom 0x448A sync family",woody_rules,1,NULL,0,
     "woodys-world-decoder-unimplemented"}
};
const uft_imaging_recipe_t UFT_RECIPE_WOODYS_WORLD_AMIGA =
    RECIPE("amiga.woodys-world.capture","Woody's World capture profile",2,
           UFT_RECIPE_SIDE_INTERLEAVED,woody_vars);

static const uft_recipe_track_rule_t cavitas_rules[] = {
    RULE(0,159,0x1600,0x44,0x89)
};
static const uft_recipe_variant_t cavitas_vars[] = {
    {"filesystem-chain","Standard tracks with proprietary file-chain extraction",
     cavitas_rules,1,NULL,0,"cavitas-chain-unimplemented"}
};
const uft_imaging_recipe_t UFT_RECIPE_CAVITAS_AMIGA =
    RECIPE("amiga.cavitas.capture","Cavitas capture profile",2,
           UFT_RECIPE_SIDE_INTERLEAVED,cavitas_vars);

static const uft_recipe_track_rule_t wol_rules[] = {
    RULE(0,146,0x1600,0x44,0x89)
};
static const uft_recipe_variant_t wol_vars[] = {
    {"save-disk","Observed save-disk range",wol_rules,1,NULL,0,NULL}
};
const uft_imaging_recipe_t UFT_RECIPE_WORLDS_OF_LEGEND_AMIGA =
    RECIPE("amiga.worlds-of-legend.capture","Worlds of Legend capture profile",2,
           UFT_RECIPE_SIDE_INTERLEAVED,wol_vars);

static const uft_imaging_recipe_t *const all_profiles[] = {
    &UFT_RECIPE_ARKANOID_AMIGA,
    &UFT_RECIPE_GREMLIN_AMIGA,
    &UFT_RECIPE_LOTUS2_AMIGA,
    &UFT_RECIPE_IKPLUS_AMIGA,
    &UFT_RECIPE_ROBOCOP2_AMIGA,
    &UFT_RECIPE_WOODYS_WORLD_AMIGA,
    &UFT_RECIPE_CAVITAS_AMIGA,
    &UFT_RECIPE_WORLDS_OF_LEGEND_AMIGA
};

const uft_imaging_recipe_t *const *uft_amiga_recipe_profiles(size_t *count)
{
    if (count) *count = sizeof(all_profiles)/sizeof(all_profiles[0]);
    return all_profiles;
}
