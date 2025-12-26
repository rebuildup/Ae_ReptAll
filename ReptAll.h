/*******************************************************************/
/*                                                                 */
/*                      ADOBE CONFIDENTIAL                         */
/*                   _ _ _ _ _ _ _ _ _ _ _ _ _                     */
/*                                                                 */
/* Copyright 2007-2023 Adobe Inc.                                  */
/* All Rights Reserved.                                            */
/*                                                                 */
/* NOTICE:  All information contained herein is, and remains the   */
/* property of Adobe Inc. and its suppliers, if                    */
/* any.  The intellectual and technical concepts contained         */
/* herein are proprietary to Adobe Inc. and its                    */
/* suppliers and may be covered by U.S. and Foreign Patents,       */
/* patents in process, and are protected by trade secret or        */
/* copyright law.  Dissemination of this information or            */
/* reproduction of this material is strictly forbidden unless      */
/* prior written permission is obtained from Adobe Inc.            */
/* Incorporated.                                                   */
/*                                                                 */
/*******************************************************************/

/*
	ReptAll.h
*/

#pragma once

#ifndef REPTALL_H
#define REPTALL_H

typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned short		u_int16;
typedef unsigned long		u_long;
typedef short int			int16;
#define PF_TABLE_BITS	12
#define PF_TABLE_SZ_16	4096

#define PF_DEEP_COLOR_AWARE 1	// make sure we get 16bpc pixels; 
								// AE_Effect.h checks for this.

#include "AEConfig.h"

#ifdef AE_OS_WIN
	typedef unsigned short PixelType;
	#include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "String_Utils.h"
#include "AE_GeneralPlug.h"
#include "AEFX_ChannelDepthTpl.h"
#include "AEGP_SuiteHandler.h"

#include "ReptAll_Strings.h"

/* Versioning information */

#define	MAJOR_VERSION	1
#define	MINOR_VERSION	1
#define	BUG_VERSION		0
#define	STAGE_VERSION	PF_Stage_DEVELOP
#define	BUILD_VERSION	1


/* Parameter defaults */

// 3D Repeater Parameter Defaults
#define REPTALL_COUNT_MIN       1
#define REPTALL_COUNT_MAX       10
#define REPTALL_COUNT_DFLT      3

#define REPTALL_TRANSLATE_MIN   -500.0
#define REPTALL_TRANSLATE_MAX   500.0
#define REPTALL_TRANSLATE_DFLT  0.0

#define REPTALL_ROTATE_MIN      -360.0
#define REPTALL_ROTATE_MAX      360.0
#define REPTALL_ROTATE_DFLT     0.0

#define REPTALL_SCALE_MIN       10.0
#define REPTALL_SCALE_MAX       200.0
#define REPTALL_SCALE_DFLT      100.0

// Parameter indices
enum {
	REPTALL_INPUT = 0,
	REPTALL_COPIES_X,        // Number of copies in X
	REPTALL_STEP_X,          // X step per copy
	REPTALL_STEP_Y,          // Y step per copy
	REPTALL_STEP_Z,          // Z step per copy
	REPTALL_STEP_ROTATE_X,   // X rotation step per copy
	REPTALL_STEP_ROTATE_Y,   // Y rotation step per copy
	REPTALL_STEP_ROTATE_Z,   // Z rotation step per copy
	REPTALL_STEP_SCALE,      // Scale step per copy (%)
	REPTALL_NUM_PARAMS
};

// Disk IDs for parameter persistence
enum {
	COPIES_X_DISK_ID = 1,
	STEP_X_DISK_ID,
	STEP_Y_DISK_ID,
	STEP_Z_DISK_ID,
	STEP_ROTATE_X_DISK_ID,
	STEP_ROTATE_Y_DISK_ID,
	STEP_ROTATE_Z_DISK_ID,
	STEP_SCALE_DISK_ID,
};

// 3D Transform info structure for render
typedef struct ReptAllInfo {
	A_long     count;
	PF_FpLong  translate_x;
	PF_FpLong  translate_y;
	PF_FpLong  translate_z;
	PF_FpLong  rotate_x;
	PF_FpLong  rotate_y;
	PF_FpLong  rotate_z;
	PF_FpLong  scale;        // percent (100 = 1.0)
} ReptAllInfo, *ReptAllInfoP, **ReptAllInfoH;


extern "C" {

	DllExport
	PF_Err
	EffectMain(
		PF_Cmd			cmd,
		PF_InData		*in_data,
		PF_OutData		*out_data,
		PF_ParamDef		*params[],
		PF_LayerDef		*output,
		void			*extra);

}

#endif // REPTALL_H