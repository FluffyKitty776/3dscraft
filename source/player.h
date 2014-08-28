#ifndef PLAYER_H
#define PLAYER_H

#include <3ds/types.h>
#include "math.h"

typedef struct
{
	vect3Df_s position;
	vect3Df_s velocity;
	mtx44 orientation;
}player_s;

void initPlayer(player_s* p);
void controlsPlayer(player_s* p);
void updatePlayer(player_s* p);
void setCameraPlayer(player_s* p);

#endif
