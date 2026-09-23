#ifndef UFT_AMIGA_RECIPE_PROFILES_H
#define UFT_AMIGA_RECIPE_PROFILES_H

#include "uft/core/uft_imaging_recipe.h"

extern const uft_imaging_recipe_t UFT_RECIPE_ARKANOID_AMIGA;
extern const uft_imaging_recipe_t UFT_RECIPE_GREMLIN_AMIGA;
extern const uft_imaging_recipe_t UFT_RECIPE_LOTUS2_AMIGA;
extern const uft_imaging_recipe_t UFT_RECIPE_IKPLUS_AMIGA;
extern const uft_imaging_recipe_t UFT_RECIPE_ROBOCOP2_AMIGA;
extern const uft_imaging_recipe_t UFT_RECIPE_WOODYS_WORLD_AMIGA;
extern const uft_imaging_recipe_t UFT_RECIPE_CAVITAS_AMIGA;
extern const uft_imaging_recipe_t UFT_RECIPE_WORLDS_OF_LEGEND_AMIGA;

const uft_imaging_recipe_t *const *uft_amiga_recipe_profiles(size_t *count);

#endif
