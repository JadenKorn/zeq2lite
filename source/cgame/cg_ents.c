// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_ents.c -- present snapshot entities, happens every single frame

#include "cg_local.h"

/*
======================
CG_GetTagPosition

Retrieves the position of a tag directly
======================
*/
void CG_GetTagPosition( refEntity_t *parent, char *tagName, vec3_t outpos) {
	int i;
	orientation_t lerped;

	// lerp the tag
	trap_R_LerpTag( &lerped, parent->hModel, parent->oldframe, parent->frame,
		1.0 - parent->backlerp, tagName );

	VectorCopy( parent->origin, outpos );
	for ( i = 0 ; i < 3 ; i++ ) {
		VectorMA( outpos, lerped.origin[i], parent->axis[i], outpos );
	}
}

/*
======================
CG_GetTagOrientation

Retrieves the orientation of a tag directly
======================
*/
void CG_GetTagOrientation( refEntity_t *parent, char *tagName, vec3_t dir) {
	orientation_t lerped;
	vec3_t	temp_axis[3];

	// lerp the tag
	trap_R_LerpTag( &lerped, parent->hModel, parent->oldframe, parent->frame,
		1.0 - parent->backlerp, tagName );

	MatrixMultiply( lerped.axis, ((refEntity_t *)parent)->axis, temp_axis );
	VectorCopy( temp_axis[0], dir );
}


/*
======================
CG_PositionEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent, 
							qhandle_t parentModel, char *tagName ) {
	int				i;
	orientation_t	lerped;
	
	// lerp the tag
	trap_R_LerpTag( &lerped, parentModel, parent->oldframe, parent->frame,
		1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );
	for ( i = 0 ; i < 3 ; i++ ) {
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// had to cast away the const to avoid compiler problems...
	MatrixMultiply( lerped.axis, ((refEntity_t *)parent)->axis, entity->axis );
	entity->backlerp = parent->backlerp;
}


/*
======================
CG_PositionRotatedEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent, 
							qhandle_t parentModel, char *tagName ) {
	int				i;
	orientation_t	lerped;
	vec3_t			tempAxis[3];

//AxisClear( entity->axis );
	// lerp the tag
	trap_R_LerpTag( &lerped, parentModel, parent->oldframe, parent->frame,
		1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );
	for ( i = 0 ; i < 3 ; i++ ) {
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// had to cast away the const to avoid compiler problems...
	MatrixMultiply( entity->axis, lerped.axis, tempAxis );
	MatrixMultiply( tempAxis, ((refEntity_t *)parent)->axis, entity->axis );
}



/*
==========================================================================

FUNCTIONS CALLED EACH FRAME

==========================================================================
*/

/*
======================
CG_SetEntitySoundPosition

Also called by event processing code
======================
*/
void CG_SetEntitySoundPosition( centity_t *cent ) {
	if ( cent->currentState.solid == SOLID_BMODEL ) {
		vec3_t	origin;
		float	*v;

		v = cgs.inlineModelMidpoints[ cent->currentState.modelindex ];
		VectorAdd( cent->lerpOrigin, v, origin );
		trap_S_UpdateEntityPosition( cent->currentState.number, origin );
	} else {
		trap_S_UpdateEntityPosition( cent->currentState.number, cent->lerpOrigin );
	}
}

/*
==================
CG_EntityEffects

Add continuous entity effects, like local entity emission and lighting
==================
*/
static void CG_EntityEffects( centity_t *cent ) {

	// update sound origins
	CG_SetEntitySoundPosition( cent );

	// add loop sound
	if ( cent->currentState.loopSound ) {
		if (cent->currentState.eType != ET_SPEAKER) {
			trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, 
				cgs.gameSounds[ cent->currentState.loopSound ] );
		} else {
			trap_S_AddRealLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, 
				cgs.gameSounds[ cent->currentState.loopSound ] );
		}
	}


	// constant light glow
	if ( cent->currentState.constantLight ) {
		int		cl;
		int		i, r, g, b;

		cl = cent->currentState.constantLight;
		r = cl & 255;
		g = ( cl >> 8 ) & 255;
		b = ( cl >> 16 ) & 255;
		i = ( ( cl >> 24 ) & 255 ) * 4;
		trap_R_AddLightToScene( cent->lerpOrigin, i, r, g, b );
	}

}


/*
==================
CG_General
==================
*/
static void CG_General( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// if set to invisible, skip
	if (!s1->modelindex) {
		return;
	}

	memset (&ent, 0, sizeof(ent));

	// set frame

	ent.frame = s1->frame;
	ent.oldframe = ent.frame;
	ent.backlerp = 0;

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	ent.hModel = cgs.gameModels[s1->modelindex];

	// player model
	if (s1->number == cg.snap->ps.clientNum) {
		ent.renderfx |= RF_THIRD_PERSON;	// only draw from mirrors
	}

	// convert angles to axis
	AnglesToAxis( cent->lerpAngles, ent.axis );

	// add to refresh list
	trap_R_AddRefEntityToScene (&ent);
}

/*
==================
CG_Speaker

Speaker entities can automatically play sounds
==================
*/
static void CG_Speaker( centity_t *cent ) {
	if ( ! cent->currentState.clientNum ) {	// FIXME: use something other than clientNum...
		return;		// not auto triggering
	}

	if ( cg.time < cent->miscTime ) {
		return;
	}

	trap_S_StartSound (NULL, cent->currentState.number, CHAN_ITEM, cgs.gameSounds[cent->currentState.eventParm] );

	//	ent->s.frame = ent->wait * 10;
	//	ent->s.clientNum = ent->random * 10;
	cent->miscTime = cg.time + cent->currentState.frame * 100 + cent->currentState.clientNum * 100 * crandom();
}

/*
==================
CG_Item
==================
*/
static void CG_Item( centity_t *cent ) {
	refEntity_t		ent;
	entityState_t	*es;
	gitem_t			*item;
	int				msec;
	float			frac;
	float			scale;
	weaponInfo_t	*wi;

	es = &cent->currentState;
	if ( es->modelindex >= bg_numItems ) {
		CG_Error( "Bad item index %i on entity", es->modelindex );
	}

	// if set to invisible, skip
	if ( !es->modelindex || ( es->eFlags & EF_NODRAW ) ) {
		return;
	}

	item = &bg_itemlist[ es->modelindex ];
	if ( cg_simpleItems.integer && item->giType != IT_TEAM ) {
		memset( &ent, 0, sizeof( ent ) );
		ent.reType = RT_SPRITE;
		VectorCopy( cent->lerpOrigin, ent.origin );
		ent.radius = 14;
		ent.customShader = cg_items[es->modelindex].icon;
		ent.shaderRGBA[0] = 255;
		ent.shaderRGBA[1] = 255;
		ent.shaderRGBA[2] = 255;
		ent.shaderRGBA[3] = 255;
		trap_R_AddRefEntityToScene(&ent);
		return;
	}

	// items bob up and down continuously
	scale = 0.005 + cent->currentState.number * 0.00001;
	cent->lerpOrigin[2] += 4 + cos( ( cg.time + 1000 ) *  scale ) * 4;

	memset (&ent, 0, sizeof(ent));

	// autorotate at one of two speeds
	if ( item->giType == IT_HEALTH ) {
		VectorCopy( cg.autoAnglesFast, cent->lerpAngles );
		AxisCopy( cg.autoAxisFast, ent.axis );
	} else {
		VectorCopy( cg.autoAngles, cent->lerpAngles );
		AxisCopy( cg.autoAxis, ent.axis );
	}

	wi = NULL;
	// the weapons have their origin where they attatch to player
	// models, so we need to offset them or they will rotate
	// eccentricly
	if ( item->giType == IT_WEAPON ) {
		wi = &cg_weapons[item->giTag];
		cent->lerpOrigin[0] -= 
			wi->weaponMidpoint[0] * ent.axis[0][0] +
			wi->weaponMidpoint[1] * ent.axis[1][0] +
			wi->weaponMidpoint[2] * ent.axis[2][0];
		cent->lerpOrigin[1] -= 
			wi->weaponMidpoint[0] * ent.axis[0][1] +
			wi->weaponMidpoint[1] * ent.axis[1][1] +
			wi->weaponMidpoint[2] * ent.axis[2][1];
		cent->lerpOrigin[2] -= 
			wi->weaponMidpoint[0] * ent.axis[0][2] +
			wi->weaponMidpoint[1] * ent.axis[1][2] +
			wi->weaponMidpoint[2] * ent.axis[2][2];

		cent->lerpOrigin[2] += 8;	// an extra height boost
	}

	ent.hModel = cg_items[es->modelindex].models[0];

	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	ent.nonNormalizedAxes = qfalse;

	// if just respawned, slowly scale up
	msec = cg.time - cent->miscTime;
	if ( msec >= 0 && msec < ITEM_SCALEUP_TIME ) {
		frac = (float)msec / ITEM_SCALEUP_TIME;
		VectorScale( ent.axis[0], frac, ent.axis[0] );
		VectorScale( ent.axis[1], frac, ent.axis[1] );
		VectorScale( ent.axis[2], frac, ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;
	} else {
		frac = 1.0;
	}

	// items without glow textures need to keep a minimum light value
	// so they are always visible
	if ( ( item->giType == IT_WEAPON ) ||
		 ( item->giType == IT_ARMOR ) ) {
		ent.renderfx |= RF_MINLIGHT;
	}

	// increase the size of the weapons when they are presented as items
	if ( item->giType == IT_WEAPON ) {
		VectorScale( ent.axis[0], 1.5, ent.axis[0] );
		VectorScale( ent.axis[1], 1.5, ent.axis[1] );
		VectorScale( ent.axis[2], 1.5, ent.axis[2] );
		ent.nonNormalizedAxes = qtrue;
	}

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);

	// accompanying rings / spheres for powerups
	if ( !cg_simpleItems.integer ) 
	{
		vec3_t spinAngles;

		VectorClear( spinAngles );

		if ( item->giType == IT_HEALTH || item->giType == IT_POWERUP )
		{
			if ( ( ent.hModel = cg_items[es->modelindex].models[1] ) != 0 )
			{
				if ( item->giType == IT_POWERUP )
				{
					ent.origin[2] += 12;
					spinAngles[1] = ( cg.time & 1023 ) * 360 / -1024.0f;
				}
				AnglesToAxis( spinAngles, ent.axis );
				
				// scale up if respawning
				if ( frac != 1.0 ) {
					VectorScale( ent.axis[0], frac, ent.axis[0] );
					VectorScale( ent.axis[1], frac, ent.axis[1] );
					VectorScale( ent.axis[2], frac, ent.axis[2] );
					ent.nonNormalizedAxes = qtrue;
				}
				trap_R_AddRefEntityToScene( &ent );
			}
		}
	}
}

//============================================================================

/*
===========================
CG_TrailFunc_StraightBeam
===========================
*/
void CG_TrailFunc_StraightBeam( centity_t *ent ) {
	entityState_t	*es;
	centity_t		*owner_ent;
	cg_userWeapon_t	*weaponGraphics;
	float			radius;
	orientation_t	orient;
	
	// Initialize some things for quick reference
	es = &ent->currentState;
	owner_ent = &cg_entities[es->clientNum];
	weaponGraphics = CG_FindUserWeaponGraphics( es->clientNum, es->weapon );

	if (!weaponGraphics->missileTrailShader) {
		return;
	}

	if (weaponGraphics->missileTrailRadius) {
		radius = weaponGraphics->missileTrailRadius;
	} else {
		radius = 10;
	}

	if ( CG_GetTagOrientationFromPlayerEntity( &cg_entities[es->clientNum], weaponGraphics->chargeTag[0], &orient )) {
		CG_DrawLine (orient.origin, ent->lerpOrigin, radius, weaponGraphics->missileTrailShader, 1);
	}
}

/*
========================
CG_TrailFunc_BendyBeam
========================
*/
void CG_TrailFunc_BendyBeam( centity_t *ent ) {
	entityState_t	*es;
	cg_userWeapon_t	*weaponGraphics;
	float			radius;
	//vec3_t			tangent;

	// Set up shortcut references
	es = &ent->currentState;
	weaponGraphics = CG_FindUserWeaponGraphics( es->clientNum, es->weapon );
	
	if ( weaponGraphics->missileTrailRadius ) {
		radius = weaponGraphics->missileTrailRadius;
	} else {
		radius = 10;
	}

	if ( weaponGraphics->missileTrailShader ) {
		CG_BeamTableUpdate( ent, radius, weaponGraphics->missileTrailShader, weaponGraphics->chargeTag[0] );
	}
}



/*
================================
CG_TrailFunc_SpiralBeam_Helper
================================
*/
// Helper function to make CG_TrailFunc_SpiralBeam
// more readable.
void CG_TrailFunc_SpiralBeam_Helper ( entityState_t *es, centity_t *ent, int time, vec3_t origin) {
	cg_userWeapon_t	*weaponGraphics;
	vec3_t			tmpAxis[3];


	weaponGraphics = CG_FindUserWeaponGraphics( es->clientNum, es->weapon );

	BG_EvaluateTrajectory( es, &es->pos, time, origin );

	// convert direction of travel into axis
	if (VectorNormalize2( es->pos.trDelta, tmpAxis[0] ) == 0 ) {
		tmpAxis[0][2] = 1;
	}
	RotateAroundDirection( tmpAxis, time * 2 );
	
	// upscale the offset of the coil to the main beam
	if ( weaponGraphics->missileTrailSpiralOffset ) {
		VectorScale( tmpAxis[2], weaponGraphics->missileTrailSpiralOffset, tmpAxis[2] );
	}
	VectorAdd( origin, tmpAxis[2], origin );
}
	


/*
=========================
CG_TrailFunc_SpiralBeam
=========================
*/
void CG_TrailFunc_SpiralBeam( centity_t *ent ) {
	vec3_t			lastPos, lastPos2;
	int				step, t;
	int				startTime;
	entityState_t	*es;
	localEntity_t	*le;
	refEntity_t		*re;
	cg_userWeapon_t	*weaponGraphics;

	// Draw the central beam
	CG_TrailFunc_StraightBeam( ent );

	step = cg_tailDetail.value / 4;

	es = &ent->currentState;
	weaponGraphics = CG_FindUserWeaponGraphics( es->clientNum, es->weapon );

	if (!weaponGraphics->missileTrailSpiralShader) {
		return;
	}

	startTime = ent->trailTime;
	ent->trailTime = cg.time;
	t = startTime + step;
	
	// if object (e.g. grenade) is stationary, don't show tail
	if ( es->pos.trType == TR_STATIONARY ) {
		return;
	}

	CG_TrailFunc_SpiralBeam_Helper ( es, ent, startTime, lastPos);
	
	// Build segments
		
	for (; t <= ent->trailTime; t += step) {
		CG_TrailFunc_SpiralBeam_Helper ( es, ent, t, lastPos2);

		le = CG_AllocLocalEntity();
		re = &le->refEntity;
		le->leType = LE_FADE_RGB;
		le->startTime = t;
		le->endTime = t + 1000;
		le->lifeRate = 1.0 / (le->endTime - le->startTime);
	
		re->reType = RT_RAIL_CORE;
		re->customShader = weaponGraphics->missileTrailSpiralShader;
	 
		VectorCopy(lastPos, re->origin);
		VectorCopy(lastPos2, re->oldorigin);
 
		re->shaderRGBA[0] = 0xff;
	    re->shaderRGBA[1] = 0xff;
	    re->shaderRGBA[2] = 0xff;
	    re->shaderRGBA[3] = 0xff;

		le->color[0] = 1.00f;
		le->color[1] = 1.00f;
		le->color[2] = 1.00f;
		le->color[3] = 1.00f;

		AxisClear( re->axis );

		VectorCopy(lastPos2, lastPos);
	}

	CG_TrailFunc_SpiralBeam_Helper ( es, ent, ent->trailTime, lastPos2);

	le = CG_AllocLocalEntity();
	re = &le->refEntity;
	le->leType = LE_FADE_RGB;
	le->startTime = ent->trailTime;
	le->endTime = ent->trailTime + 500;
	le->lifeRate = 1.0 / (le->endTime - le->startTime);
	
	re->reType = RT_RAIL_CORE;
	re->customShader = weaponGraphics->missileTrailSpiralShader;
	 
	VectorCopy(lastPos, re->origin);
	VectorCopy(lastPos2, re->oldorigin);
 
	re->shaderRGBA[0] = 0xff;
	re->shaderRGBA[1] = 0xff;
	re->shaderRGBA[2] = 0xff;
	re->shaderRGBA[3] = 0xff;

	le->color[0] = 1.00f;
	le->color[1] = 1.00f;
	le->color[2] = 1.00f;
	le->color[3] = 1.00f;

	AxisClear( re->axis );
}


/*
=======================
CG_TrailFunc_FadeTail
=======================
*/
void CG_TrailFunc_FadeTail( centity_t *cent ) {
	entityState_t	*es;
	cg_userWeapon_t	*weaponGraphics;
	vec3_t			delta;

	es = &cent->currentState;
	
	weaponGraphics = CG_FindUserWeaponGraphics( es->clientNum, es->weapon );
	
	if ( !weaponGraphics->missileTrailShader || !weaponGraphics->missileTrailRadius ) {
		return;
	}

	// If we didn't draw the tail last frame this is a new instantiation
	// of the entity and we will have to reset the tail positions.
	if ( cent->lastTrailTime < (cg.time - cg.frametime - 200) ) { // -200; give 0.2 sec leeway, just in case
		BG_EvaluateTrajectoryDelta( es, &es->pos, cg.time, delta );
		CG_ResetTrail( es->number, cent->lerpOrigin, VectorLength( delta ),
			weaponGraphics->missileTrailRadius, weaponGraphics->missileTrailShader, NULL );
	}
	
	CG_UpdateTrailHead( es->number, cent->lerpOrigin );

	cent->lastTrailTime = cg.time;
}


/*
===============
CG_Torch
===============
*/
static void CG_Torch( centity_t *cent ) {
	entityState_t		*s1;
	cg_userWeapon_t		*weaponGraphics;

	s1 = &cent->currentState;
	weaponGraphics = CG_FindUserWeaponGraphics(s1->clientNum, s1->weapon);
}


/*
===============
CG_Missile
===============
*/
static void CG_Missile( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;
	cg_userWeapon_t		*weaponGraphics;
	float				missileScale;
	int					missileChargeLvl;
	

	s1 = &cent->currentState;
	weaponGraphics = CG_FindUserWeaponGraphics(s1->clientNum, s1->weapon);

	// The missile's charge level was stored in this field. We hijacked it on the
	// server to be able to transmit the missile's own charge level.
	missileChargeLvl = s1->powerups;

	// Obtain the scale the missile must have.
	if (weaponGraphics->chargeGrowth) {
		// below the start, we use the lowest form
		if (weaponGraphics->chargeStartPct >= missileChargeLvl) {
			missileScale = weaponGraphics->chargeStartsize;
		} else {
			// above the end, we use the highest form
			if (weaponGraphics->chargeEndPct <= missileChargeLvl) {
				missileScale = weaponGraphics->chargeEndsize;
			} else {
				float	PctRange;
				float	PctVal;
				float	SizeRange;
				float	SizeVal;

				// inbetween, we work out the value
				PctRange = weaponGraphics->chargeEndPct - weaponGraphics->chargeStartPct;
				PctVal = missileChargeLvl - weaponGraphics->chargeStartPct;

				SizeRange = weaponGraphics->chargeEndsize - weaponGraphics->chargeStartsize;
				SizeVal = (PctVal / PctRange) * SizeRange;
				missileScale = SizeVal + weaponGraphics->chargeStartsize;
			}
		}
	} else {
		missileScale = 1;
	}
	missileScale = missileScale * weaponGraphics->missileSize;

	// calculate the axis
	 VectorCopy( s1->angles, cent->lerpAngles);
	
	// If it's a guided missile, and belongs to this client, return its position to cg.
	if ((cent->currentState.clientNum == cg.clientNum) && (cent->currentState.eFlags & EF_GUIDED)) {
		VectorCopy(cent->lerpOrigin, cg.guide_target);
		cg.guide_view = qtrue;
	}


	// add trails
	if ( weaponGraphics->missileTrailShader && weaponGraphics->missileTrailRadius ) {
		if ( cent->currentState.eType == ET_MISSILE ) {
			CG_TrailFunc_FadeTail( cent );

		} else if ( cent->currentState.eType == ET_BEAMHEAD ) {
			if ( cent->currentState.eFlags & EF_GUIDED ) {
				CG_TrailFunc_BendyBeam( cent );

			} else if ( weaponGraphics->missileTrailSpiralShader &&
				        weaponGraphics->missileTrailSpiralRadius &&
						weaponGraphics->missileTrailSpiralOffset ) {
				CG_TrailFunc_SpiralBeam( cent );

			} else {
				CG_TrailFunc_StraightBeam( cent );
			}
		}
	}

	// add dynamic light
	if ( weaponGraphics->missileDlightRadius ) {
		trap_R_AddLightToScene(cent->lerpOrigin,
			100 * weaponGraphics->missileDlightRadius, 
			weaponGraphics->missileDlightColor[0],
			weaponGraphics->missileDlightColor[1],
			weaponGraphics->missileDlightColor[2] );
	}

	// add missile sound
	if ( weaponGraphics->missileSound ) {
		vec3_t	velocity;

		BG_EvaluateTrajectoryDelta( &cent->currentState, &cent->currentState.pos, cg.time, velocity );

		trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, velocity, weaponGraphics->missileSound );
	}

	
	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);


	if ( ! (weaponGraphics->missileModel && weaponGraphics->missileSkin) ) {
		ent.reType = RT_SPRITE;
		ent.radius = 4 * missileScale;
		ent.rotation = 0;
		ent.customShader = weaponGraphics->missileShader;
		trap_R_AddRefEntityToScene( &ent );
	} else {
		ent.reType = RT_MODEL;
		ent.hModel = weaponGraphics->missileModel;
		ent.customSkin = weaponGraphics->missileSkin;
		ent.renderfx = RF_NOSHADOW;
		
		
		
		// We want something simple for simple roll-spinning missiles,
		// but will use quaternions to get a correct yaw rotation for
		// disk attacks and the like.
		if ( !weaponGraphics->missileSpin[0] && !weaponGraphics->missileSpin[1] ) {
					
			// convert direction of travel into axis
			if ( VectorNormalize2( s1->pos.trDelta, ent.axis[0] ) == 0 ) {
				ent.axis[0][2] = 1;
			}
			// spin as it moves
			if ( s1->pos.trType != TR_STATIONARY ) {
				RotateAroundDirection( ent.axis, weaponGraphics->missileSpin[2] * cg.time / 4.0f );
			} else {
				RotateAroundDirection( ent.axis, weaponGraphics->missileSpin[2] * s1->time );
			}
		} else {
			vec3_t tempAngles;
			vec4_t quatOrient;
			vec4_t quatRot;
			vec4_t quatResult;
			vec3_t spinRotate;

			// convert direction of travel into axis
			if ( VectorNormalize2( s1->pos.trDelta, ent.axis[0] ) == 0 ) {
				ent.axis[0][0] = 1;
			}

			if ( s1->pos.trType != TR_STATIONARY ) {
				VectorSet( spinRotate, cg.time / 4.0f, cg.time / 4.0f, cg.time / 4.0f );
			} else {
				VectorSet( spinRotate, s1->time, s1->time, s1->time );
			}
			VectorPieceWiseMultiply( spinRotate, weaponGraphics->missileSpin, spinRotate );

			vectoangles( ent.axis[0], tempAngles );
			AnglesToQuat( tempAngles, quatOrient );
			AnglesToQuat( spinRotate, quatRot );
			QuatMul( quatOrient, quatRot, quatResult );
			QuatToAxis( quatResult, ent.axis );
		}

		ent.nonNormalizedAxes = qtrue;
		VectorScale(ent.axis[0], missileScale, ent.axis[0]);
		VectorScale(ent.axis[1], missileScale, ent.axis[1]);
		VectorScale(ent.axis[2], missileScale, ent.axis[2]);

		trap_R_AddRefEntityToScene( &ent );
	}

	// Check if it is a groundskimmer missile and activate a new debris trail
	// if the entity is a entering into the PVS this frame.
	if ( s1->eType == ET_SKIMMER ) {
		//if ( cent->lastPVSTime < ( cg.time - cg.frametime - 100 )) { // 100 msec leeway for inconsistency in engine timing
		if ( !CG_FrameHist_WasInPVS( s1->number )) {
			PSys_SpawnCachedSystem( "TrailDebris", cent->lerpOrigin, NULL, cent, NULL, qfalse, qfalse );
		}
	}

	// Check if we should activate a missile specific particle system
	//if ( cent->lastPVSTime < ( cg.time - cg.frametime - 100) ) {
	if ( !CG_FrameHist_WasInPVS( s1->number )) {
		if ( weaponGraphics->missileParticleSystem[0] ) {
			vec3_t tempAxis[3];

			AnglesToAxis( cent->lerpAngles, tempAxis );
			PSys_SpawnCachedSystem( weaponGraphics->missileParticleSystem, cent->lerpOrigin, tempAxis, cent, NULL, qfalse, qfalse );
		}
	}
}

/*
===============
CG_Grapple

This is called when the grapple is sitting up against the wall
===============
*/
static void CG_Grapple( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;
	const weaponInfo_t		*weapon;

	s1 = &cent->currentState;
	if ( s1->weapon > WP_NUM_WEAPONS ) {
		s1->weapon = 0;
	}
	weapon = &cg_weapons[s1->weapon];

	// calculate the axis
	VectorCopy( s1->angles, cent->lerpAngles);

#if 0 // FIXME add grapple pull sound here..?
	// add missile sound
	if ( weapon->missileSound ) {
		trap_S_AddLoopingSound( cent->currentState.number, cent->lerpOrigin, vec3_origin, weapon->missileSound );
	}
#endif

	// Will draw cable if needed
	CG_GrappleTrail ( cent, weapon );

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	// flicker between two skins
	ent.skinNum = cg.clientFrame & 1;
	ent.hModel = weapon->missileModel;
	ent.renderfx = weapon->missileRenderfx | RF_NOSHADOW;

	// convert direction of travel into axis
	if ( VectorNormalize2( s1->pos.trDelta, ent.axis[0] ) == 0 ) {
		ent.axis[0][2] = 1;
	}

	trap_R_AddRefEntityToScene( &ent );
}

/*
===============
CG_Mover
===============
*/
static void CG_Mover( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);
	AnglesToAxis( cent->lerpAngles, ent.axis );

	ent.renderfx = RF_NOSHADOW;

	// flicker between two skins (FIXME?)
	ent.skinNum = ( cg.time >> 6 ) & 1;

	// get the model, either as a bmodel or a modelindex
	if ( s1->solid == SOLID_BMODEL ) {
		ent.hModel = cgs.inlineDrawModel[s1->modelindex];
	} else {
		ent.hModel = cgs.gameModels[s1->modelindex];
	}

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);

	// add the secondary model
	if ( s1->modelindex2 ) {
		ent.skinNum = 0;
		ent.hModel = cgs.gameModels[s1->modelindex2];
		trap_R_AddRefEntityToScene(&ent);
	}

}

/*
===============
CG_Beam

Also called as an event
===============
*/
void CG_Beam( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( s1->pos.trBase, ent.origin );
	VectorCopy( s1->origin2, ent.oldorigin );
	AxisClear( ent.axis );
	ent.reType = RT_BEAM;

	ent.renderfx = RF_NOSHADOW;

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);
}


/*
===============
CG_Portal
===============
*/
static void CG_Portal( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;

	s1 = &cent->currentState;

	// create the render entity
	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin );
	VectorCopy( s1->origin2, ent.oldorigin );
	ByteToDir( s1->eventParm, ent.axis[0] );
	PerpendicularVector( ent.axis[1], ent.axis[0] );

	// negating this tends to get the directions like they want
	// we really should have a camera roll value
	VectorSubtract( vec3_origin, ent.axis[1], ent.axis[1] );

	CrossProduct( ent.axis[0], ent.axis[1], ent.axis[2] );
	ent.reType = RT_PORTALSURFACE;
	ent.oldframe = s1->powerups;
	ent.frame = s1->frame;		// rotation speed
	ent.skinNum = s1->clientNum/256.0 * 360;	// roll offset

	// add to refresh list
	trap_R_AddRefEntityToScene(&ent);
}


/*
=========================
CG_AdjustPositionForMover

Also called by client movement prediction code
=========================
*/
void CG_AdjustPositionForMover( const vec3_t in, int moverNum, int fromTime, int toTime, vec3_t out ) {
	centity_t	*cent;
	vec3_t	oldOrigin, origin, deltaOrigin;
	vec3_t	oldAngles, angles, deltaAngles;

	if ( moverNum <= 0 || moverNum >= ENTITYNUM_MAX_NORMAL ) {
		VectorCopy( in, out );
		return;
	}

	cent = &cg_entities[ moverNum ];
	if ( cent->currentState.eType != ET_MOVER ) {
		VectorCopy( in, out );
		return;
	}

	BG_EvaluateTrajectory( &cent->currentState, &cent->currentState.pos, fromTime, oldOrigin );
	BG_EvaluateTrajectory( &cent->currentState, &cent->currentState.apos, fromTime, oldAngles );

	BG_EvaluateTrajectory( &cent->currentState, &cent->currentState.pos, toTime, origin );
	BG_EvaluateTrajectory( &cent->currentState, &cent->currentState.apos, toTime, angles );

	VectorSubtract( origin, oldOrigin, deltaOrigin );
	VectorSubtract( angles, oldAngles, deltaAngles );

	VectorAdd( in, deltaOrigin, out );

	// FIXME: origin change when on a rotating object
}


/*
=============================
CG_InterpolateEntityPosition
=============================
*/
static void CG_InterpolateEntityPosition( centity_t *cent ) {
	vec3_t		current, next;
	float		f;

	// it would be an internal error to find an entity that interpolates without
	// a snapshot ahead of the current one
	if ( cg.nextSnap == NULL ) {
		CG_Error( "CG_InterpolateEntityPosition: cg.nextSnap == NULL" );
	}

	f = cg.frameInterpolation;

	// this will linearize a sine or parabolic curve, but it is important
	// to not extrapolate player positions if more recent data is available
	BG_EvaluateTrajectory( &cent->currentState, &cent->currentState.pos, cg.snap->serverTime, current );
	BG_EvaluateTrajectory( &cent->nextState, &cent->nextState.pos, cg.nextSnap->serverTime, next );

	cent->lerpOrigin[0] = current[0] + f * ( next[0] - current[0] );
	cent->lerpOrigin[1] = current[1] + f * ( next[1] - current[1] );
	cent->lerpOrigin[2] = current[2] + f * ( next[2] - current[2] );

	BG_EvaluateTrajectory( &cent->currentState, &cent->currentState.apos, cg.snap->serverTime, current );
	BG_EvaluateTrajectory( &cent->nextState, &cent->nextState.apos, cg.nextSnap->serverTime, next );

	cent->lerpAngles[0] = LerpAngle( current[0], next[0], f );
	cent->lerpAngles[1] = LerpAngle( current[1], next[1], f );
	cent->lerpAngles[2] = LerpAngle( current[2], next[2], f );
}


/*
===============
CG_CalcEntityLerpPositions

===============
*/
static void CG_CalcEntityLerpPositions( centity_t *cent ) {

	// if this player does not want to see extrapolated players
	if ( !cg_smoothClients.integer ) {
		// make sure the clients use TR_INTERPOLATE
		if ( cent->currentState.number < MAX_CLIENTS ) {
			cent->currentState.pos.trType = TR_INTERPOLATE;
			cent->nextState.pos.trType = TR_INTERPOLATE;
		}
	}

	if ( cent->interpolate && cent->currentState.pos.trType == TR_INTERPOLATE ) {
		CG_InterpolateEntityPosition( cent );
		return;
	}

	// first see if we can interpolate between two snaps for
	// linear extrapolated clients
	if ( cent->interpolate && cent->currentState.pos.trType == TR_LINEAR_STOP &&
											cent->currentState.number < MAX_CLIENTS) {
		CG_InterpolateEntityPosition( cent );
		return;
	}

	// just use the current frame and evaluate as best we can
	BG_EvaluateTrajectory( &cent->currentState, &cent->currentState.pos, cg.time, cent->lerpOrigin );
	BG_EvaluateTrajectory( &cent->currentState, &cent->currentState.apos, cg.time, cent->lerpAngles );

	// adjust for riding a mover if it wasn't rolled into the predicted
	// player state
	if ( cent != &cg.predictedPlayerEntity ) {
		CG_AdjustPositionForMover( cent->lerpOrigin, cent->currentState.groundEntityNum, 
		cg.snap->serverTime, cg.time, cent->lerpOrigin );
	}
}

/*
===============
CG_TeamBase
===============
*/
static void CG_TeamBase( centity_t *cent ) {
	refEntity_t model;
#ifdef MISSIONPACK
	vec3_t angles;
	int t, h;
	float c;

	if ( cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF ) {
#else
	if ( cgs.gametype == GT_CTF) {
#endif
		// show the flag base
		memset(&model, 0, sizeof(model));
		model.reType = RT_MODEL;
		VectorCopy( cent->lerpOrigin, model.lightingOrigin );
		VectorCopy( cent->lerpOrigin, model.origin );
		AnglesToAxis( cent->currentState.angles, model.axis );
		if ( cent->currentState.modelindex == TEAM_RED ) {
			model.hModel = cgs.media.redFlagBaseModel;
		}
		else if ( cent->currentState.modelindex == TEAM_BLUE ) {
			model.hModel = cgs.media.blueFlagBaseModel;
		}
		else {
			model.hModel = cgs.media.neutralFlagBaseModel;
		}
		trap_R_AddRefEntityToScene( &model );
	}
#ifdef MISSIONPACK
	else if ( cgs.gametype == GT_OBELISK ) {
		// show the obelisk
		memset(&model, 0, sizeof(model));
		model.reType = RT_MODEL;
		VectorCopy( cent->lerpOrigin, model.lightingOrigin );
		VectorCopy( cent->lerpOrigin, model.origin );
		AnglesToAxis( cent->currentState.angles, model.axis );

		model.hModel = cgs.media.overloadBaseModel;
		trap_R_AddRefEntityToScene( &model );
		// if hit
		if ( cent->currentState.frame == 1) {
			// show hit model
			// modelindex2 is the health value of the obelisk
			c = cent->currentState.modelindex2;
			model.shaderRGBA[0] = 0xff;
			model.shaderRGBA[1] = c;
			model.shaderRGBA[2] = c;
			model.shaderRGBA[3] = 0xff;
			//
			model.hModel = cgs.media.overloadEnergyModel;
			trap_R_AddRefEntityToScene( &model );
		}
		// if respawning
		if ( cent->currentState.frame == 2) {
			if ( !cent->miscTime ) {
				cent->miscTime = cg.time;
			}
			t = cg.time - cent->miscTime;
			h = (cg_obeliskRespawnDelay.integer - 5) * 1000;
			//
			if (t > h) {
				c = (float) (t - h) / h;
				if (c > 1)
					c = 1;
			}
			else {
				c = 0;
			}
			// show the lights
			AnglesToAxis( cent->currentState.angles, model.axis );
			//
			model.shaderRGBA[0] = c * 0xff;
			model.shaderRGBA[1] = c * 0xff;
			model.shaderRGBA[2] = c * 0xff;
			model.shaderRGBA[3] = c * 0xff;

			model.hModel = cgs.media.overloadLightsModel;
			trap_R_AddRefEntityToScene( &model );
			// show the target
			if (t > h) {
				if ( !cent->muzzleFlashTime ) {
					trap_S_StartSound (cent->lerpOrigin, ENTITYNUM_NONE, CHAN_BODY,  cgs.media.obeliskRespawnSound);
					cent->muzzleFlashTime = 1;
				}
				VectorCopy(cent->currentState.angles, angles);
				angles[YAW] += (float) 16 * acos(1-c) * 180 / M_PI;
				AnglesToAxis( angles, model.axis );

				VectorScale( model.axis[0], c, model.axis[0]);
				VectorScale( model.axis[1], c, model.axis[1]);
				VectorScale( model.axis[2], c, model.axis[2]);

				model.shaderRGBA[0] = 0xff;
				model.shaderRGBA[1] = 0xff;
				model.shaderRGBA[2] = 0xff;
				model.shaderRGBA[3] = 0xff;
				//
				model.origin[2] += 56;
				model.hModel = cgs.media.overloadTargetModel;
				trap_R_AddRefEntityToScene( &model );
			}
			else {
				//FIXME: show animated smoke
			}
		}
		else {
			cent->miscTime = 0;
			cent->muzzleFlashTime = 0;
			// modelindex2 is the health value of the obelisk
			c = cent->currentState.modelindex2;
			model.shaderRGBA[0] = 0xff;
			model.shaderRGBA[1] = c;
			model.shaderRGBA[2] = c;
			model.shaderRGBA[3] = 0xff;
			// show the lights
			model.hModel = cgs.media.overloadLightsModel;
			trap_R_AddRefEntityToScene( &model );
			// show the target
			model.origin[2] += 56;
			model.hModel = cgs.media.overloadTargetModel;
			trap_R_AddRefEntityToScene( &model );
		}
	}
	else if ( cgs.gametype == GT_HARVESTER ) {
		// show harvester model
		memset(&model, 0, sizeof(model));
		model.reType = RT_MODEL;
		VectorCopy( cent->lerpOrigin, model.lightingOrigin );
		VectorCopy( cent->lerpOrigin, model.origin );
		AnglesToAxis( cent->currentState.angles, model.axis );

		if ( cent->currentState.modelindex == TEAM_RED ) {
			model.hModel = cgs.media.harvesterModel;
			model.customSkin = cgs.media.harvesterRedSkin;
		}
		else if ( cent->currentState.modelindex == TEAM_BLUE ) {
			model.hModel = cgs.media.harvesterModel;
			model.customSkin = cgs.media.harvesterBlueSkin;
		}
		else {
			model.hModel = cgs.media.harvesterNeutralModel;
			model.customSkin = 0;
		}
		trap_R_AddRefEntityToScene( &model );
	}
#endif
}

/*
===============
CG_AddCEntity

===============
*/
static void CG_AddCEntity( centity_t *cent ) {
	// event-only entities will have been dealt with already
	if ( cent->currentState.eType >= ET_EVENTS ) {
		return;
	}

	// calculate the current origin
	CG_CalcEntityLerpPositions( cent );

	// add automatic effects
	CG_EntityEffects( cent );

	switch ( cent->currentState.eType ) {
	default:
		CG_Error( "Bad entity type: %i\n", cent->currentState.eType );
		break;
	case ET_INVISIBLE:
	case ET_PUSH_TRIGGER:
	case ET_TELEPORT_TRIGGER:
		break;
	case ET_GENERAL:
		CG_General( cent );
		break;
	case ET_PLAYER:
		//CG_CalcEntityLerpWeaponCharges( cent );
		CG_Player( cent );
		break;
	case ET_ITEM:
		CG_Item( cent );
		break;
	case ET_MISSILE:
		CG_Missile( cent );		
		break;
	case ET_MOVER:
		CG_Mover( cent );
		break;
	case ET_BEAM:
		CG_Beam( cent );
		break;
	case ET_PORTAL:
		CG_Portal( cent );
		break;
	case ET_SPEAKER:
		CG_Speaker( cent );
		break;
	case ET_GRAPPLE:
		CG_Grapple( cent );
		break;
	case ET_TEAM:
		CG_TeamBase( cent );
		break;
	// ADDING FOR ZEQ2
	case ET_BEAMHEAD:
		CG_Missile( cent );
		break;
	case ET_SKIMMER:
		CG_Missile( cent );
		break;
	case ET_TORCH:
		CG_Torch( cent );
		break;
	// END ADDING
	}

	//cent->lastPVSTime = cg.time;
	CG_FrameHist_SetPVS( cent->currentState.number );
}

/*
===============
CG_AddPacketEntities

===============
*/
void CG_AddPacketEntities( void ) {
	int					num;
	centity_t			*cent;
	playerState_t		*ps;

	// set cg.frameInterpolation
	if ( cg.nextSnap ) {
		int		delta;

		delta = (cg.nextSnap->serverTime - cg.snap->serverTime);
		if ( delta == 0 ) {
			cg.frameInterpolation = 0;
		} else {
			cg.frameInterpolation = (float)( cg.time - cg.snap->serverTime ) / delta;
		}
	} else {
		cg.frameInterpolation = 0;	// actually, it should never be used, because 
									// no entities should be marked as interpolating
	}

	// the auto-rotating items will all have the same axis
	cg.autoAngles[0] = 0;
	cg.autoAngles[1] = ( cg.time & 2047 ) * 360 / 2048.0;
	cg.autoAngles[2] = 0;

	cg.autoAnglesFast[0] = 0;
	cg.autoAnglesFast[1] = ( cg.time & 1023 ) * 360 / 1024.0f;
	cg.autoAnglesFast[2] = 0;

	AnglesToAxis( cg.autoAngles, cg.autoAxis );
	AnglesToAxis( cg.autoAnglesFast, cg.autoAxisFast );

	// generate and add the entity from the playerstate
	ps = &cg.predictedPlayerState;
	BG_PlayerStateToEntityState( ps, &cg.predictedPlayerEntity.currentState, qfalse );
	CG_AddCEntity( &cg.predictedPlayerEntity );

	// lerp the non-predicted value for lightning gun origins
	CG_CalcEntityLerpPositions( &cg_entities[ cg.snap->ps.clientNum ] );

	// add each entity sent over by the server
	for ( num = 0 ; num < cg.snap->numEntities ; num++ ) {
		cent = &cg_entities[ cg.snap->entities[ num ].number ];
		CG_AddCEntity( cent );
	}
}

