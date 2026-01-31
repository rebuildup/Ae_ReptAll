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

// ============================================================================
// Unified Parameter Indices
// ============================================================================
// Parameter indices used for array indexing - must match order in ParamsSetup
// Bounds checking is performed at runtime through parameter count validation

enum {
	REPTALL_INPUT = 0,           // Source layer input

	// Copy count parameters
	REPTALL_COPIES_X = 1,        // Number of copies in X
	REPTALL_COPIES_Y,            // Number of copies in Y
	REPTALL_COPIES_Z,            // Number of copies in Z

	// Transform step parameters
	REPTALL_STEP_X,              // X step per copy
	REPTALL_STEP_Y,              // Y step per copy
	REPTALL_STEP_Z,              // Z step per copy
	REPTALL_STEP_ROTATE_X,       // X rotation step per copy
	REPTALL_STEP_ROTATE_Y,       // Y rotation step per copy
	REPTALL_STEP_ROTATE_Z,       // Z rotation step per copy
	REPTALL_STEP_SCALE,          // Scale step per copy (%)
	REPTALL_STEP_OPACITY,        // Opacity step per copy

	// Base transform parameters
	REPTALL_BASE_POS_X,          // Base position X
	REPTALL_BASE_POS_Y,          // Base position Y
	REPTALL_BASE_POS_Z,          // Base position Z
	REPTALL_BASE_ROT_X,          // Base rotation X
	REPTALL_BASE_ROT_Y,          // Base rotation Y
	REPTALL_BASE_ROT_Z,          // Base rotation Z
	REPTALL_BASE_SCALE,          // Base scale
	REPTALL_BASE_OPACITY,        // Base opacity

	// Offset/distribution parameters
	REPTALL_OFFSET_MODE,         // Offset mode (linear/radial/etc)
	REPTALL_OFFSET_VALUE,        // Offset value

	// Camera/composite parameters
	REPTALL_COMP_MODE,           // Composite mode (add/screen/normal/etc)
	REPTALL_CAMERA_AWARE,        // Enable camera depth sorting

	REPTALL_NUM_PARAMS           // Must be last, represents total parameter count
};

// Disk IDs for parameter persistence
enum {
	COPIES_X_DISK_ID = 1,
	COPIES_Y_DISK_ID,
	COPIES_Z_DISK_ID,
	STEP_X_DISK_ID,
	STEP_Y_DISK_ID,
	STEP_Z_DISK_ID,
	STEP_ROTATE_X_DISK_ID,
	STEP_ROTATE_Y_DISK_ID,
	STEP_ROTATE_Z_DISK_ID,
	STEP_SCALE_DISK_ID,
	STEP_OPACITY_DISK_ID,
	BASE_POS_X_DISK_ID,
	BASE_POS_Y_DISK_ID,
	BASE_POS_Z_DISK_ID,
	BASE_ROT_X_DISK_ID,
	BASE_ROT_Y_DISK_ID,
	BASE_ROT_Z_DISK_ID,
	BASE_SCALE_DISK_ID,
	BASE_OPACITY_DISK_ID,
	OFFSET_MODE_DISK_ID,
	OFFSET_VALUE_DISK_ID,
	COMP_MODE_DISK_ID,
	CAMERA_AWARE_DISK_ID,
};

// ============================================================================
// Data Structures - Scalable architecture for full parameter support
// ============================================================================

// Per-copy transform state
// Represents the complete transform of a single copy
struct CopyTransform {
	PF_FpLong position[3];        // x, y, z position
	PF_FpLong rotation[3];        // x, y, z rotation (degrees)
	PF_FpLong scale;              // uniform scale (percent)
	PF_FpLong opacity;            // opacity (0-100)
	PF_FpLong world_matrix[16];   // 4x4 transformation matrix
	PF_FpLong view_scale;         // perspective scale factor
	A_Boolean visible;            // visibility flag
	PF_FpLong camera_depth;       // distance from camera for sorting

	// Constructor for auto-initialization
	CopyTransform() {
		Clear();
	}

	// Initialize to default values
	void Clear() {
		for (int i = 0; i < 3; i++) {
			position[i] = 0.0;
			rotation[i] = 0.0;
		}
		scale = 100.0;
		opacity = 100.0;
		for (int i = 0; i < 16; i++) {
			world_matrix[i] = (i % 5 == 0) ? 1.0 : 0.0;  // Identity matrix
		}
		view_scale = 1.0;
		visible = TRUE;
		camera_depth = 0.0;
	}
};

// Complete parameter state
// Holds all user-adjustable parameters from the effect UI
struct ReptAllState {
	// 3D grid dimensions
	A_long copies[3];             // copies in X, Y, Z directions

	// Offset/distribution
	PF_FpLong offset;             // global offset value

	// Anchor point
	PF_FpLong anchor[3];          // anchor x, y, z

	// Base transform (initial state before stepping)
	PF_FpLong position[3];        // base position x, y, z
	PF_FpLong scale;              // base uniform scale (percent)
	PF_FpLong rotation[3];        // base rotation x, y, z

	// Step values (increment per copy)
	PF_FpLong step_position[3];   // step x, y, z per copy
	PF_FpLong step_rotation[3];   // step rotation x, y, z per copy
	PF_FpLong step_scale;         // step uniform scale (percent)

	// Opacity
	PF_FpLong opacity_start;      // starting opacity (first copy)
	PF_FpLong opacity_end;        // ending opacity (last copy)

	// Rendering options
	A_Boolean camera_aware;       // enable depth-based sorting
	A_long composite_mode;        // blending mode

	// Initialize to defaults
	void Clear() {
		for (int i = 0; i < 3; i++) {
			copies[i] = (i == 0) ? REPTALL_COUNT_DFLT : 1;
			anchor[i] = 0.0;
			position[i] = 0.0;
			rotation[i] = 0.0;
			step_position[i] = (i == 0) ? REPTALL_TRANSLATE_DFLT : 0.0;
			step_rotation[i] = 0.0;
		}
		scale = 100.0;
		step_scale = 100.0;
		offset = 0.0;
		opacity_start = 100.0;
		opacity_end = 100.0;
		camera_aware = TRUE;
		composite_mode = 0;  // Normal blend
	}
};

// ============================================================================
// Helper Function Declarations - Split Render() into phases
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

	// Phase 1: Extract all parameters from UI into ReptAllState
	PF_Err ExtractParameters(
		PF_InData		*in_data,
		PF_ParamDef		*params[],
		ReptAllState	*outState);

	// Phase 2: Compute transform for each copy (handles stepping)
	PF_Err ComputeCopyTransforms(
		const ReptAllState	*state,
		CopyTransform		*transforms,
		A_long				*numTransforms,
		PF_InData			*in_data);

	// Phase 3: Sort copies by camera depth for proper Z-order
	void SortCopiesByDepth(
		CopyTransform	*transforms,
		A_long			count,
		PF_Boolean		cameraAware);

	// Phase 4: Render each copy with bilinear sampling
	PF_Err RenderCopies(
		PF_InData		*in_data,
		PF_OutData		*out_data,
		const ReptAllState	*state,
		const CopyTransform	*transforms,
		A_long			transformCount,
		PF_EffectWorld	*srcP,
		PF_LayerDef		*output);

#ifdef __cplusplus
}
#endif

// Main effect entry point
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