#include <3ds.h>

#include "text.h"
#include "player.h"
#include "world.h"
#include "gs.h"

void initPlayer(player_s* p)
{
	if(!p)return;

	initCamera(&p->camera);
	p->camera.position=vect3Df(0.0f, CHUNK_HEIGHT*CLUSTER_SIZE/2+CLUSTER_SIZE, 0.0f);
	loadIdentity44((float*)p->camera.orientation);
	p->velocity=vect3Df(0.0f, 0.0f, 0.0f);
	p->flying=false;

	initCursor(&p->cursor);
}

//TODO : cleanup ?
vect3Di_s performRayMarch(world_s* w, vect3Df_s localBlockPosf, vect3Df_s localBlockPosf2, vect3Df_s* out, bool* collided, u8* dirout)
{
	vect3Di_s cur=vf2i(localBlockPosf);
	vect3Df_s u=vnormf(vsubf(localBlockPosf2, localBlockPosf));
	const float d=vdistf(localBlockPosf2,localBlockPosf);
	
	if(!w)return cur;

	const int stepX=(localBlockPosf2.x>localBlockPosf.x)?1:-1;
	const int stepY=(localBlockPosf2.y>localBlockPosf.y)?1:-1;
	const int stepZ=(localBlockPosf2.z>localBlockPosf.z)?1:-1;

	const float tDeltaX=fabs(1.0f/u.x); // w/u.x
	const float tDeltaY=fabs(1.0f/u.y); // h/u.y
	const float tDeltaZ=fabs(1.0f/u.z); // z/u.z

	float tMaxX, tMaxY, tMaxZ;
	if(fabs(u.x)<0.001f)tMaxX=d;
	else tMaxX=fabs((localBlockPosf.x-floorf(localBlockPosf.x)+((localBlockPosf2.x>localBlockPosf.x)?-1.0f:0.0f))/u.x);
	if(fabs(u.y)<0.001f)tMaxY=d;
	else tMaxY=fabs((localBlockPosf.y-floorf(localBlockPosf.y)+((localBlockPosf2.y>localBlockPosf.y)?-1.0f:0.0f))/u.y);
	if(fabs(u.z)<0.001f)tMaxZ=d;
	else tMaxZ=fabs((localBlockPosf.z-floorf(localBlockPosf.z)+((localBlockPosf2.z>localBlockPosf.z)?-1.0f:0.0f))/u.z);

	if(collided)*collided=false;

	u8 dir=0;
	if(out)*out=localBlockPosf2;
	do{
		if(tMaxX>=d && tMaxY>=d && tMaxZ>=d)return cur; //finished without colliding with world
		if(tMaxX < tMaxY)
		{
			if(tMaxX < tMaxZ)
			{
				dir=0;
				cur.x+=stepX;
				tMaxX+=tDeltaX;
			}else{
				dir=2;
				cur.z+=stepZ;
				tMaxZ+=tDeltaZ;
			}   
		}else{
			if(tMaxY < tMaxZ) {
				dir=1;
				cur.y+=stepY;
				tMaxY+=tDeltaY;
			}else{
				dir=2;
				cur.z+=stepZ;
				tMaxZ+=tDeltaZ;
			}
		}
	}while(getWorldBlock(w, cur)==0);

	if(collided)*collided=true;
	if(dirout)*dirout=dir*2;

	switch(dir)
	{
		case 0:
			{
				float targetX=(cur.x)*1.0f;
				if(stepX<0)targetX+=1.0f;
				else if(dirout)(*dirout)++;
				if(out)
				{
					float r=(targetX-localBlockPosf.x)/u.x;
					targetX-=0.1f*stepX; //margin
					*out=vaddf(localBlockPosf, vmulf(u,r)); out->x=targetX;
				}
			}
			break;
		case 1:
			{
				float targetY=(cur.y)*1.0f;
				if(stepY<0)targetY+=1.0f;
				else if(dirout)(*dirout)++;
				if(out)
				{
					float r=(targetY-localBlockPosf.y)/u.y;
					targetY-=0.04f*stepY; //margin
					*out=vaddf(localBlockPosf, vmulf(u,r)); out->y=targetY;
				}
			}
			break;
		case 2:
			{
				float targetZ=(cur.z)*1.0f;
				if(stepZ<0)targetZ+=1.0f;
				else if(dirout)(*dirout)++;
				if(out)
				{
					float r=(targetZ-localBlockPosf.z)/u.z;
					targetZ-=0.1f*stepZ; //margin
					*out=vaddf(localBlockPosf, vmulf(u,r)); out->z=targetZ;
				}
			}
			break;
	}

	return cur;
}

const vect3Di_s directionVector[]=
{
	{+1,0,0},
	{-1,0,0},
	{0,+1,0},
	{0,-1,0},
	{0,0,+1},
	{0,0,-1},
};

void controlsPlayer(player_s* p, world_s* w)
{
	if(!p)return;

	const u32 PAD=hidKeysHeld();
	circlePosition cstick;
	hidCstickRead(&cstick);

	vect3Df_s vx=vnormf(getMatrixColumn((float*)p->camera.orientation, 0));
	vect3Df_s vy=vnormf(getMatrixColumn((float*)p->camera.orientation, 1));
	vect3Df_s vz=vnormf(getMatrixColumn((float*)p->camera.orientation, 2));
	vect3Df_s vz2=(!p->flying)?vnormf(vect3Df(vz.x,0.0f,vz.z)):vz;

	if(PAD&KEY_UP)p->velocity=vaddf(p->velocity, vmulf(vz2, -0.2f));
	if(PAD&KEY_DOWN)p->velocity=vaddf(p->velocity, vmulf(vz2, 0.2f));
	if(PAD&KEY_LEFT)p->velocity=vaddf(p->velocity, vmulf(vx, -0.2f));
	if(PAD&KEY_RIGHT)p->velocity=vaddf(p->velocity, vmulf(vx, 0.2f));

	if(hidKeysDown()&KEY_ZL)p->flying^=1;

	if(!p->flying)
	{
		if(hidKeysDown()&KEY_L)p->velocity=vaddf(p->velocity, vmulf(vy, 0.6f));

		if(w)
		{
			const vect3Df_s v=vmulf(vz, -5.0f);
			bool collided=false;
			u8 dir;
			vect3Di_s out=performRayMarch(w, p->camera.position, vaddf(p->camera.position, v), NULL, &collided, &dir);
			if(collided)
			{
				p->cursor.active=true;
				p->cursor.position=out;
				p->cursor.direction=dir;
				if(hidKeysDown()&KEY_R)alterWorldBlock(w, vaddi(out, directionVector[dir]), 1, true);
				if(hidKeysDown()&KEY_ZR)alterWorldBlock(w, out, 0, true);
			}else p->cursor.active=false;
		}
	}else{
		if(PAD&KEY_L)p->velocity=vaddf(p->velocity, vmulf(vy, 0.2f));
		if(PAD&KEY_R)p->velocity=vaddf(p->velocity, vmulf(vy, -0.2f));
	}

	rotateMatrixX((float*)p->camera.orientation, (cstick.dy*0.07f)/0x9c, true);
	rotateMatrixY((float*)p->camera.orientation, (cstick.dx*0.07f)/0x9c, false);
}

extern u32 debugValue[];

#define PLAYERSIZE (0.25f)
#define PLAYERHEIGHT (0.4f)
vect3Df_s playerBox[]={(vect3Df_s){-PLAYERSIZE,-PLAYERHEIGHT,-PLAYERSIZE},
						(vect3Df_s){+PLAYERSIZE,-PLAYERHEIGHT,-PLAYERSIZE},
						(vect3Df_s){-PLAYERSIZE,+PLAYERHEIGHT,-PLAYERSIZE},
						(vect3Df_s){-PLAYERSIZE,-PLAYERHEIGHT,+PLAYERSIZE},
						(vect3Df_s){+PLAYERSIZE,+PLAYERHEIGHT,-PLAYERSIZE},
						(vect3Df_s){+PLAYERSIZE,-PLAYERHEIGHT,+PLAYERSIZE},
						(vect3Df_s){-PLAYERSIZE,+PLAYERHEIGHT,+PLAYERSIZE},
						(vect3Df_s){+PLAYERSIZE,+PLAYERHEIGHT,+PLAYERSIZE}};

void updatePlayer(player_s* p, world_s* w)
{
	if(!p)return;

	//gravity
	if(!p->flying)p->velocity=vaddf(p->velocity, vect3Df(0.0f, -0.05f, 0.0f));

	//collisions
	if(vmagf(p->velocity)>0.0001f)
	{
		vect3Df_s v=p->velocity;
		int i;
		for(i=0; i<8; i++)
		{
			vect3Df_s out;
			vect3Df_s pt=vaddf(p->camera.position, playerBox[i]);
			performRayMarch(w, pt, vaddf(pt, v), &out, NULL, NULL);
			v=vsubf(out,pt);
			if(vmagf(v)<=0.0001f)break;
		}
		p->velocity=v;
	}

	p->camera.position=vaddf(p->camera.position, p->velocity);

	if(!p->flying)p->velocity=vect3Df(0.0f, p->velocity.y, 0.0f);
	else p->velocity=vect3Df(0.0f, 0.0f, 0.0f);

	//world streaming
	if(w)
	{
		vect3Di_s off=vsubi(vf2i(vmulf(p->camera.position, 1.0f/CLUSTER_SIZE)), vaddi(w->position, vect3Di(WORLD_SIZE/2,0,WORLD_SIZE/2)));
		if(off.x<=-2)translateWorld(w, vect3Di(-1,0,0));
		if(off.x>=2)translateWorld(w, vect3Di(1,0,0));
		if(off.z<=-2)translateWorld(w, vect3Di(0,0,-1));
		if(off.z>=2)translateWorld(w, vect3Di(0,0,1));
	}

	updateCamera(&p->camera);
}

void setCameraPlayer(player_s* p)
{
	if(!p)return;

	setCamera(&p->camera);
}
