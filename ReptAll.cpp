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

/*	ReptAll.cpp	
	
	3D Repeater - A plugin that creates multiple copies of a layer
	with incremental 3D transforms, optionally using camera information.
	
	Based on Skeleton sample, with 3D camera/light support from Resizer sample.
	
	Revision History

	Version		Change												Engineer	Date
	=======		======												========	======
	1.0			Initial 3D Repeater implementation						2024

*/

#include "ReptAll.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cfloat>
#include "AE_EffectPixelFormat.h"

#ifdef _MSC_VER
// Suppress C4984: 'if constexpr' is a C++17 language extension
#pragma warning(disable : 4984)
// Suppress C4244: conversion from PF_FpLong (double) to PF_FpShort (float)
// This is safe for floating-point color channel assignments
#pragma warning(disable : 4244)
#elif defined(__clang__)
// Suppress Clang warnings (used by Mac/Linux builds)
#pragma clang diagnostic push
// Suppress -Wc++17-extensions: 'if constexpr' is a C++17 language extension
#pragma clang diagnostic ignored "-Wc++17-extensions"
// Suppress -Wfloat-conversion: conversion from double to float in color assignments
#pragma clang diagnostic ignored "-Wfloat-conversion"
// Suppress -Wunused-variable: template parameters may not be used in all instantiations
#pragma clang diagnostic ignored "-Wunused-variable"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Maximum copy count to prevent performance issues
#define MAX_COPIES 1000

// Precomputed transform parameters for optimization
struct TransformParams {
	PF_FpLong centerX;
	PF_FpLong centerY;
	PF_FpLong translateX;
	PF_FpLong translateY;
	PF_FpLong cosZ;      // Precomputed cosine
	PF_FpLong sinZ;      // Precomputed sine
	PF_FpLong scale;     // Scale factor (1.0 = 100%)
};

// Template for bilinear sampling - eliminates code duplication
// Use integer template parameter to avoid C++20 requirement
template<typename PixelType, int MaxChannelInt>
static PixelType
SampleBilinearTmpl(
	PF_EffectWorld *srcP,
	PF_FpLong x,
	PF_FpLong y)
{
	PixelType result = {0, 0, 0, 0};

	if (x < 0 || y < 0 || x >= srcP->width - 1 || y >= srcP->height - 1) {
		return result;
	}

	A_long x0 = (A_long)x;
	A_long y0 = (A_long)y;
	A_long x1 = x0 + 1;
	A_long y1 = y0 + 1;

	PF_FpLong fx = x - x0;
	PF_FpLong fy = y - y0;

	// Check for zero width/height
	if (srcP->width <= 0 || srcP->height <= 0) {
		return result;
	}

	// Overflow protection for rowbytes calculation
	// Use multiplication to detect overflow: if y * rowbytes > LONG_MAX
	if (srcP->rowbytes > 0 && ((y0 > LONG_MAX / srcP->rowbytes) || (y1 > LONG_MAX / srcP->rowbytes))) {
		return result;
	}

	PixelType *row0 = (PixelType*)((char*)srcP->data + y0 * srcP->rowbytes);
	PixelType *row1 = (PixelType*)((char*)srcP->data + y1 * srcP->rowbytes);

	PixelType p00 = row0[x0];
	PixelType p10 = row0[x1];
	PixelType p01 = row1[x0];
	PixelType p11 = row1[x1];

	// Bilinear interpolation weights
	PF_FpLong w00 = (1.0 - fx) * (1.0 - fy);
	PF_FpLong w10 = fx * (1.0 - fy);
	PF_FpLong w01 = (1.0 - fx) * fy;
	PF_FpLong w11 = fx * fy;

	// Apply interpolation
	// MaxChannelInt == 1 indicates float (1.0), other values indicate integer
	if constexpr (MaxChannelInt == 1) {
		// Float: no rounding, no clamping needed
		result.alpha = p00.alpha * w00 + p10.alpha * w10 + p01.alpha * w01 + p11.alpha * w11;
		result.red   = p00.red * w00 + p10.red * w10 + p01.red * w01 + p11.red * w11;
		result.green = p00.green * w00 + p10.green * w10 + p01.green * w01 + p11.green * w11;
		result.blue  = p00.blue * w00 + p10.blue * w10 + p01.blue * w01 + p11.blue * w11;
	} else {
		// Integer: round and clamp
		result.alpha = (decltype(result.alpha))(p00.alpha * w00 + p10.alpha * w10 + p01.alpha * w01 + p11.alpha * w11 + 0.5);
		result.red   = (decltype(result.red))(p00.red * w00 + p10.red * w10 + p01.red * w01 + p11.red * w11 + 0.5);
		result.green = (decltype(result.green))(p00.green * w00 + p10.green * w10 + p01.green * w01 + p11.green * w11 + 0.5);
		result.blue  = (decltype(result.blue))(p00.blue * w00 + p10.blue * w10 + p01.blue * w01 + p11.blue * w11 + 0.5);
	}

	return result;
}

static PF_Err 
About (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	AEGP_SuiteHandler suites(in_data->pica_basicP);
	
	suites.ANSICallbacksSuite1()->sprintf(
        out_data->return_msg,
        "%s v%d.%d\r%s",
        STR(StrID_Name),
        MAJOR_VERSION,
        MINOR_VERSION,
        STR(StrID_Description));
        
	return PF_Err_NONE;
}

static PF_Err 
GlobalSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	out_data->my_version = PF_VERSION(	MAJOR_VERSION, 
										MINOR_VERSION,
										BUG_VERSION, 
										STAGE_VERSION, 
										BUILD_VERSION);

	out_data->out_flags =  PF_OutFlag_DEEP_COLOR_AWARE;
	
	// 3D camera/light support - always enabled
	// PiPL flags (0x1400): I_USE_3D_CAMERA | I_USE_3D_LIGHTS
	out_data->out_flags2 = PF_OutFlag2_I_USE_3D_CAMERA |
						   PF_OutFlag2_I_USE_3D_LIGHTS;
	
	return PF_Err_NONE;
}

static PF_Err 
ParamsSetup (	
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err		err		= PF_Err_NONE;
	PF_ParamDef	def;	

	// Copies X - number of copies in X direction
	AEFX_CLR_STRUCT(def);
	PF_ADD_SLIDER(	STR(StrID_CopiesX_Param_Name), 
					REPTALL_COUNT_MIN, 
					REPTALL_COUNT_MAX, 
					REPTALL_COUNT_MIN, 
					REPTALL_COUNT_MAX, 
					REPTALL_COUNT_DFLT,
					COPIES_X_DISK_ID);

	// Step X
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_StepX_Param_Name), 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							STEP_X_DISK_ID);

	// Step Y
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_StepY_Param_Name), 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							STEP_Y_DISK_ID);

	// Step Z
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_StepZ_Param_Name), 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							STEP_Z_DISK_ID);

	// Step Rotate X
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_StepRotateX_Param_Name), 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							STEP_ROTATE_X_DISK_ID);

	// Step Rotate Y
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_StepRotateY_Param_Name), 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							STEP_ROTATE_Y_DISK_ID);

	// Step Rotate Z
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_StepRotateZ_Param_Name), 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							STEP_ROTATE_Z_DISK_ID);

	// Step Scale
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_StepScale_Param_Name), 
							REPTALL_SCALE_MIN, 
							REPTALL_SCALE_MAX, 
							REPTALL_SCALE_MIN, 
							REPTALL_SCALE_MAX, 
							REPTALL_SCALE_DFLT,
							PF_Precision_TENTHS,
							0,
							PF_ValueDisplayFlag_PERCENT,
							STEP_SCALE_DISK_ID);

	out_data->num_params = REPTALL_NUM_PARAMS;
	
	return err;
}

// Optimized 2D affine transformation with precomputed cos/sin
inline static void
ApplyTransform2DOptimized(
	PF_FpLong srcX,
	PF_FpLong srcY,
	const TransformParams& params,
	PF_FpLong *dstX,
	PF_FpLong *dstY)
{
	// Translate to origin
	PF_FpLong x = srcX - params.centerX;
	PF_FpLong y = srcY - params.centerY;

	// Apply rotation (using precomputed cos/sin)
	PF_FpLong rx = x * params.cosZ - y * params.sinZ;
	PF_FpLong ry = x * params.sinZ + y * params.cosZ;

	// Apply scale
	rx *= params.scale;
	ry *= params.scale;

	// Translate back + apply translation offset
	*dstX = rx + params.centerX + params.translateX;
	*dstY = ry + params.centerY + params.translateY;
}

// Wrapper functions for template-based bilinear sampling
static PF_Pixel
SampleBilinear8(PF_EffectWorld *srcP, PF_FpLong x, PF_FpLong y) {
	return SampleBilinearTmpl<PF_Pixel, PF_MAX_CHAN8>(srcP, x, y);
}

static PF_Pixel16
SampleBilinear16(PF_EffectWorld *srcP, PF_FpLong x, PF_FpLong y) {
	return SampleBilinearTmpl<PF_Pixel16, PF_MAX_CHAN16>(srcP, x, y);
}

// Alpha-over compositing for 8-bit (premultiplied alpha)
static void
CompositePremult8(
	PF_Pixel *dstP,
	const PF_Pixel *srcP)
{
	if (srcP->alpha == 0) return;
	if (srcP->alpha == PF_MAX_CHAN8) {
		*dstP = *srcP;
		return;
	}

	// Alpha-over: dst = src + dst * (1 - srcAlpha)
	PF_FpLong srcA = srcP->alpha / (PF_FpLong)PF_MAX_CHAN8;
	// Clamp srcA to prevent negative values
	if (srcA < 0.0) srcA = 0.0;
	if (srcA > 1.0) srcA = 1.0;
	PF_FpLong oneMinusSrcA = 1.0 - srcA;

	// Validate finite values
	if (!std::isfinite(oneMinusSrcA)) oneMinusSrcA = 0.0;

	PF_FpLong newAlpha = srcP->alpha + dstP->alpha * oneMinusSrcA;
	PF_FpLong newRed = srcP->red + dstP->red * oneMinusSrcA;
	PF_FpLong newGreen = srcP->green + dstP->green * oneMinusSrcA;
	PF_FpLong newBlue = srcP->blue + dstP->blue * oneMinusSrcA;

	// Clamp to valid range
	dstP->alpha = (A_u_char)MIN(MAX(newAlpha + 0.5, 0.0), PF_MAX_CHAN8);
	dstP->red   = (A_u_char)MIN(MAX(newRed + 0.5, 0.0), PF_MAX_CHAN8);
	dstP->green = (A_u_char)MIN(MAX(newGreen + 0.5, 0.0), PF_MAX_CHAN8);
	dstP->blue  = (A_u_char)MIN(MAX(newBlue + 0.5, 0.0), PF_MAX_CHAN8);
}

// Alpha-over compositing for 16-bit (premultiplied alpha)
static void
CompositePremult16(
	PF_Pixel16 *dstP,
	const PF_Pixel16 *srcP)
{
	if (srcP->alpha == 0) return;
	if (srcP->alpha == PF_MAX_CHAN16) {
		*dstP = *srcP;
		return;
	}

	PF_FpLong srcA = srcP->alpha / (PF_FpLong)PF_MAX_CHAN16;
	// Clamp srcA to prevent negative values
	if (srcA < 0.0) srcA = 0.0;
	if (srcA > 1.0) srcA = 1.0;
	PF_FpLong oneMinusSrcA = 1.0 - srcA;

	// Validate finite values
	if (!std::isfinite(oneMinusSrcA)) oneMinusSrcA = 0.0;

	PF_FpLong newAlpha = srcP->alpha + dstP->alpha * oneMinusSrcA;
	PF_FpLong newRed = srcP->red + dstP->red * oneMinusSrcA;
	PF_FpLong newGreen = srcP->green + dstP->green * oneMinusSrcA;
	PF_FpLong newBlue = srcP->blue + dstP->blue * oneMinusSrcA;

	// Clamp to valid range
	dstP->alpha = (A_u_short)MIN(MAX(newAlpha + 0.5, 0.0), PF_MAX_CHAN16);
	dstP->red   = (A_u_short)MIN(MAX(newRed + 0.5, 0.0), PF_MAX_CHAN16);
	dstP->green = (A_u_short)MIN(MAX(newGreen + 0.5, 0.0), PF_MAX_CHAN16);
	dstP->blue  = (A_u_short)MIN(MAX(newBlue + 0.5, 0.0), PF_MAX_CHAN16);
}

// Wrapper for 32-bit Float
static PF_PixelFloat
SampleBilinearFloat(PF_EffectWorld *srcP, PF_FpLong x, PF_FpLong y) {
	return SampleBilinearTmpl<PF_PixelFloat, 1>(srcP, x, y);
}

// Alpha-over compositing for 32-bit Float (premultiplied alpha)
static void
CompositePremultFloat(
	PF_PixelFloat *dstP,
	const PF_PixelFloat *srcP)
{
	if (srcP->alpha <= 0.0) return;
	if (srcP->alpha >= 1.0) {
		*dstP = *srcP;
		return;
	}

	// Alpha-over: dst = src + dst * (1 - srcAlpha)
	PF_FpLong srcA = srcP->alpha;
	// Clamp srcA to prevent negative values
	if (srcA < 0.0) srcA = 0.0;
	if (srcA > 1.0) srcA = 1.0;
	PF_FpLong oneMinusSrcA = 1.0 - srcA;

	// Validate finite values
	if (!std::isfinite(oneMinusSrcA)) oneMinusSrcA = 0.0;

	dstP->alpha = srcP->alpha + dstP->alpha * oneMinusSrcA;
	dstP->red   = srcP->red + dstP->red * oneMinusSrcA;
	dstP->green = srcP->green + dstP->green * oneMinusSrcA;
	dstP->blue  = srcP->blue + dstP->blue * oneMinusSrcA;

	// Clamp to prevent negative values
	if (dstP->alpha < 0.0) dstP->alpha = 0.0;
	if (dstP->red < 0.0) dstP->red = 0.0;
	if (dstP->green < 0.0) dstP->green = 0.0;
	if (dstP->blue < 0.0) dstP->blue = 0.0;
}

// ============================================================================
// PHASE 1: Extract all parameters from UI into ReptAllState
// ============================================================================
PF_Err
ExtractParameters(
	PF_InData		*in_data,
	PF_ParamDef		*params[],
	ReptAllState	*outState)
{
	PF_Err err = PF_Err_NONE;

	if (!params || !outState) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	// Initialize state to defaults
	outState->Clear();

	// Validate required parameters
	if (!params[REPTALL_INPUT] || !params[REPTALL_COPIES_X] ||
		!params[REPTALL_STEP_X] || !params[REPTALL_STEP_Y] ||
		!params[REPTALL_STEP_Z] || !params[REPTALL_STEP_ROTATE_X] ||
		!params[REPTALL_STEP_ROTATE_Y] || !params[REPTALL_STEP_ROTATE_Z] ||
		!params[REPTALL_STEP_SCALE]) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	// Extract 3D grid count (currently only X supported, Y/Z reserved for future)
	outState->copies[0] = params[REPTALL_COPIES_X]->u.sd.value;
	outState->copies[1] = 1;  // Y dimension - future expansion
	outState->copies[2] = 1;  // Z dimension - future expansion

	// Extract step parameters
	outState->step_position[0] = params[REPTALL_STEP_X]->u.fs_d.value;
	outState->step_position[1] = params[REPTALL_STEP_Y]->u.fs_d.value;
	outState->step_position[2] = params[REPTALL_STEP_Z]->u.fs_d.value;
	outState->step_rotation[0] = params[REPTALL_STEP_ROTATE_X]->u.fs_d.value;
	outState->step_rotation[1] = params[REPTALL_STEP_ROTATE_Y]->u.fs_d.value;
	outState->step_rotation[2] = params[REPTALL_STEP_ROTATE_Z]->u.fs_d.value;

	// Extract scale step (uniform)
	outState->step_scale = params[REPTALL_STEP_SCALE]->u.fs_d.value;

	// Set defaults for other parameters (will be exposed in future parameters)
	outState->offset = 0.0;
	for (int i = 0; i < 3; i++) {
		outState->anchor[i] = 0.0;
		outState->position[i] = 0.0;
		outState->rotation[i] = 0.0;
	}
	outState->scale = 100.0;
	outState->opacity_start = 100.0;
	outState->opacity_end = 100.0;
	outState->camera_aware = TRUE;
	outState->composite_mode = 0;

	return err;
}

// ============================================================================
// PHASE 2: Compute transform for each copy (handles stepping)
// ============================================================================
PF_Err
ComputeCopyTransforms(
	const ReptAllState	*state,
	CopyTransform		*transforms,
	A_long				*numTransforms,
	PF_InData			*in_data)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	if (!state || !transforms || !numTransforms) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	// Validate maximum copy count
	for (int i = 0; i < 3; i++) {
		if (state->copies[i] < 1 || state->copies[i] > MAX_COPIES) {
			return PF_Err_BAD_CALLBACK_PARAM;
		}
	}

	// Get total number of copies with overflow check
	A_long totalX = state->copies[0];
	A_long totalY = state->copies[1];
	A_long totalZ = state->copies[2];

	// Check for multiplication overflow
	if (totalX > 0 && totalY > LONG_MAX / totalX) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}
	A_long totalXY = totalX * totalY;

	if (totalXY > 0 && totalZ > LONG_MAX / totalXY) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	*numTransforms = totalXY * totalZ;

	// Additional maximum copy count validation
	if (*numTransforms > MAX_COPIES) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	// ===== Get 3D Camera Information =====
	A_Matrix4 camera_matrix;
	A_FpLong focal_length = 0.0;
	PF_Boolean has_camera = FALSE;
	AEFX_CLR_STRUCT(camera_matrix);

	PF_FpLong camera_x = 0.0, camera_y = 0.0, camera_z = 0.0;
	PF_FpLong camera_fwd_x = 0.0, camera_fwd_y = 0.0, camera_fwd_z = 0.0;

	if (state->camera_aware && in_data->appl_id != 'PrMr') {
		A_Time comp_timeT = {0, 1};
		AEGP_LayerH camera_layerH = NULL;

		ERR(suites.PFInterfaceSuite1()->AEGP_ConvertEffectToCompTime(
			in_data->effect_ref,
			in_data->current_time,
			in_data->time_scale,
			&comp_timeT));
		if (!err) {
			ERR(suites.PFInterfaceSuite1()->AEGP_GetEffectCamera(
				in_data->effect_ref,
				&comp_timeT,
				&camera_layerH));
		}

		if (camera_layerH && !err) {
			has_camera = TRUE;

			ERR(suites.LayerSuite5()->AEGP_GetLayerToWorldXform(
				camera_layerH,
				&comp_timeT,
				&camera_matrix));

			if (!err) {
				AEGP_StreamVal stream_val;
				AEFX_CLR_STRUCT(stream_val);

				ERR(suites.StreamSuite2()->AEGP_GetLayerStreamValue(
					camera_layerH,
					AEGP_LayerStream_ZOOM,
					AEGP_LTimeMode_CompTime,
					&comp_timeT,
					FALSE,
					&stream_val,
					NULL));

				if (!err) {
					focal_length = stream_val.one_d;
					// AEGP_StreamVal is a union, not a pointer-allocated struct
					// No disposal needed for values returned by AEGP_GetLayerStreamValue
				}

				// Extract camera position and direction
				camera_x = camera_matrix.mat[3][0];
				camera_y = camera_matrix.mat[3][1];
				camera_z = camera_matrix.mat[3][2];

				camera_fwd_x = -camera_matrix.mat[2][0];
				camera_fwd_y = -camera_matrix.mat[2][1];
				camera_fwd_z = -camera_matrix.mat[2][2];

				PF_FpLong fwd_len = sqrt(camera_fwd_x*camera_fwd_x +
										  camera_fwd_y*camera_fwd_y +
										  camera_fwd_z*camera_fwd_z);
				if (fwd_len > 0.0001) {
					camera_fwd_x /= fwd_len;
					camera_fwd_y /= fwd_len;
					camera_fwd_z /= fwd_len;
				}
			}
		}
	}

	// ===== Compute transform for each copy =====
	A_long transformIndex = 0;

	for (A_long z = 0; z < state->copies[2]; z++) {
		for (A_long y = 0; y < state->copies[1]; y++) {
			for (A_long x = 0; x < state->copies[0]; x++) {
				A_long copyIndex = z * state->copies[0] * state->copies[1] +
								   y * state->copies[0] + x;

				CopyTransform& transform = transforms[transformIndex++];
				transform.Clear();

				// Calculate cumulative position
				transform.position[0] = state->position[0] + state->step_position[0] * x;
				transform.position[1] = state->position[1] + state->step_position[1] * y;
				transform.position[2] = state->position[2] + state->step_position[2] * z;

				// Calculate cumulative rotation
				transform.rotation[0] = state->rotation[0] + state->step_rotation[0] * copyIndex;
				transform.rotation[1] = state->rotation[1] + state->step_rotation[1] * copyIndex;
				transform.rotation[2] = state->rotation[2] + state->step_rotation[2] * copyIndex;

				// Calculate cumulative scale (compound) - use exponential formula instead of O(n^2) loop
				// scale = base_scale * (step_scale / 100.0) ^ copyIndex
				PF_FpLong stepScaleRatio = state->step_scale / 100.0;
				PF_FpLong baseScale = state->scale;

				// Validate inputs
				if (!std::isfinite(stepScaleRatio) || stepScaleRatio < 0.001) stepScaleRatio = 0.001;
				if (stepScaleRatio > 10.0) stepScaleRatio = 10.0;
				if (!std::isfinite(baseScale) || baseScale < 0.001) baseScale = 0.001;
				if (baseScale > 1000.0) baseScale = 1000.0;

				if (copyIndex > 0) {
					// Use pow for exponential calculation: base * ratio^copyIndex
					transform.scale = baseScale * std::pow(stepScaleRatio, copyIndex);

					// Clamp result to reasonable range
					if (!std::isfinite(transform.scale) || transform.scale < 0.001) transform.scale = 0.001;
					if (transform.scale > 10000.0) transform.scale = 10000.0;
				} else {
					transform.scale = baseScale;
				}

				// Calculate opacity (linear interpolation)
				if (*numTransforms > 1) {
					transform.opacity = state->opacity_start +
						(state->opacity_end - state->opacity_start) * copyIndex / (*numTransforms - 1);
				} else {
					transform.opacity = state->opacity_start;
				}

				// Calculate camera depth for sorting
				if (has_camera) {
					PF_FpLong dx = transform.position[0] - camera_x;
					PF_FpLong dy = transform.position[1] - camera_y;
					PF_FpLong dz = transform.position[2] - camera_z;

					transform.camera_depth = dx * camera_fwd_x +
											 dy * camera_fwd_y +
											 dz * camera_fwd_z;

					// Apply perspective scaling
					if (focal_length > 0.0) {
						PF_FpLong depth = transform.camera_depth;
						PF_FpLong perspectiveScale = 1.0;
						PF_FpLong denominator = focal_length - depth;

						if (denominator > 1.0) {
							perspectiveScale = focal_length / denominator;
						} else if (denominator < -1.0) {
							perspectiveScale = 0.001;
						} else {
							perspectiveScale = (denominator > 0) ? 10.0 : 0.001;
						}

						if (perspectiveScale < 0.001) perspectiveScale = 0.001;
						if (perspectiveScale > 100.0) perspectiveScale = 100.0;

						transform.scale *= perspectiveScale;
						transform.view_scale = perspectiveScale;
					}
				} else {
					transform.camera_depth = transform.position[2];
					transform.view_scale = 1.0;
				}

				transform.visible = (transform.opacity > 0.0 && transform.scale > 0.001);

				// Validate and clamp opacity
				if (!std::isfinite(transform.opacity)) transform.opacity = 100.0;
				if (transform.opacity < 0.0) transform.opacity = 0.0;
				if (transform.opacity > 100.0) transform.opacity = 100.0;

				// Store precomputed 2D transform params for rendering
				// (legacy compatibility for current rendering code)
				PF_FpLong invRotateZ = -transform.rotation[2];
				PF_FpLong radZ = invRotateZ * M_PI / 180.0;

				transform.world_matrix[0] = cos(radZ);  // cosZ
				transform.world_matrix[1] = sin(radZ);  // sinZ
				transform.world_matrix[2] = 0.0;  // centerX - set by caller
				transform.world_matrix[3] = 0.0;  // centerY - set by caller

				if (has_camera) {
					transform.world_matrix[4] = transform.position[0] - camera_x;  // translateX
					transform.world_matrix[5] = transform.position[1] - camera_y;  // translateY
				} else {
					transform.world_matrix[4] = transform.position[0];
					transform.world_matrix[5] = transform.position[1];
				}
			}
		}
	}

	return err;
}

// ============================================================================
// PHASE 3: Sort copies by camera depth for proper Z-order
// ============================================================================
void
SortCopiesByDepth(
	CopyTransform	*transforms,
	A_long			count,
	PF_Boolean		cameraAware)
{
	if (!transforms || count <= 1) {
		return;
	}

	// Sort by camera depth (ascending - furthest first)
	std::sort(transforms, transforms + count,
		[](const CopyTransform& a, const CopyTransform& b) {
			return a.camera_depth < b.camera_depth;
		});
}

// ============================================================================
// PHASE 4: Render each copy with bilinear sampling
// ============================================================================
PF_Err
RenderCopies(
	PF_InData			*in_data,
	PF_OutData			*out_data,
	const ReptAllState	*state,
	const CopyTransform	*transforms,
	A_long				transformCount,
	PF_EffectWorld		*srcP,
	PF_LayerDef			*output)
{
	PF_Err err = PF_Err_NONE;
	AEGP_SuiteHandler suites(in_data->pica_basicP);

	if (!state || !transforms || !srcP || !output) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	// Check bit depth using pixel format
	// PF_WORLD_IS_DEEP checks for 16-bit (ARGB64)
	// For 32-bit float (ARGB128), we need to use PF_WorldSuite2->PF_GetPixelFormat()
	PF_Boolean deepB = PF_WORLD_IS_DEEP(output);
	PF_Boolean floatB = FALSE;

	// Detect 32-bit float format using PF_WorldSuite2
	PF_PixelFormat pixfmt = PF_PixelFormat_INVALID;
	PF_WorldSuite2 *world_suiteP = NULL;
	A_Err suite_err = in_data->pica_basicP->AcquireSuite(
		kPFWorldSuite,
		kPFWorldSuiteVersion2,
		(const void**)&world_suiteP);

	if (suite_err == A_Err_NONE && world_suiteP) {
		PF_Err fmt_err = world_suiteP->PF_GetPixelFormat(output, &pixfmt);
		if (fmt_err == PF_Err_NONE && pixfmt == PF_PixelFormat_ARGB128) {
			floatB = TRUE;
		}
	}

	// Calculate center point
	PF_FpLong centerX = srcP->width / 2.0;
	PF_FpLong centerY = srcP->height / 2.0;

	// Clear output
	if (floatB) {
		PF_PixelFloat clearColor = {0, 0, 0, 0};
		ERR(suites.FillMatteSuite2()->fill_float(in_data->effect_ref, &clearColor, NULL, output));
	} else if (deepB) {
		PF_Pixel16 clearColor = {0, 0, 0, 0};
		ERR(suites.FillMatteSuite2()->fill16(in_data->effect_ref, &clearColor, NULL, output));
	} else {
		PF_Pixel clearColor = {0, 0, 0, 0};
		ERR(suites.FillMatteSuite2()->fill(in_data->effect_ref, &clearColor, NULL, output));
	}

	// Render each copy in sorted order
	for (A_long i = 0; i < transformCount && !err; i++) {
		const CopyTransform& transform = transforms[i];

		if (!transform.visible) {
			continue;
		}

		// Build transform params from precomputed data
		TransformParams params;
		params.centerX = centerX;
		params.centerY = centerY;
		params.cosZ = transform.world_matrix[0];
		params.sinZ = transform.world_matrix[1];
		params.translateX = transform.world_matrix[4];
		params.translateY = transform.world_matrix[5];
		params.scale = transform.scale / 100.0;

		// Validate finite values for scale
		PF_FpLong safeScale = transform.scale;
		if (!std::isfinite(safeScale) || safeScale < 0.001) safeScale = 0.001;
		if (safeScale > 1000.0) safeScale = 1000.0;

		PF_FpLong invScale = 100.0 / safeScale;
		if (!std::isfinite(invScale) || invScale < 0.001) invScale = 0.001;
		if (invScale > 1000.0) invScale = 1000.0;

		// Iterate through output pixels
		for (A_long y = 0; y < output->height && !err; y++) {
			if (floatB) {
				PF_PixelFloat *dstRow = (PF_PixelFloat*)((char*)output->data + y * output->rowbytes);
				for (A_long x = 0; x < output->width; x++) {
					PF_FpLong srcX, srcY;
					ApplyTransform2DOptimized((PF_FpLong)x, (PF_FpLong)y, params, &srcX, &srcY);
					srcX = centerX + (srcX - centerX) * invScale;
					srcY = centerY + (srcY - centerY) * invScale;

					PF_PixelFloat srcPix = SampleBilinearFloat(srcP, srcX, srcY);
					if (srcPix.alpha > 0.0) {
						// Apply opacity with clamping
						PF_FpLong opacity = transform.opacity;
						if (!std::isfinite(opacity)) opacity = 100.0;
						if (opacity < 0.0) opacity = 0.0;
						if (opacity > 100.0) opacity = 100.0;

						if (opacity < 100.0) {
							srcPix.alpha *= opacity / 100.0;
						}
						CompositePremultFloat(&dstRow[x], &srcPix);
					}
				}
			} else if (deepB) {
				PF_Pixel16 *dstRow = (PF_Pixel16*)((char*)output->data + y * output->rowbytes);
				for (A_long x = 0; x < output->width; x++) {
					PF_FpLong srcX, srcY;
					ApplyTransform2DOptimized((PF_FpLong)x, (PF_FpLong)y, params, &srcX, &srcY);
					srcX = centerX + (srcX - centerX) * invScale;
					srcY = centerY + (srcY - centerY) * invScale;

					PF_Pixel16 srcPix = SampleBilinear16(srcP, srcX, srcY);
					if (srcPix.alpha > 0) {
						// Apply opacity with clamping
						PF_FpLong opacity = transform.opacity;
						if (!std::isfinite(opacity)) opacity = 100.0;
						if (opacity < 0.0) opacity = 0.0;
						if (opacity > 100.0) opacity = 100.0;

						if (opacity < 100.0) {
							srcPix.alpha = (A_u_short)(srcPix.alpha * opacity / 100.0);
						}
						CompositePremult16(&dstRow[x], &srcPix);
					}
				}
			} else {
				PF_Pixel *dstRow = (PF_Pixel*)((char*)output->data + y * output->rowbytes);
				for (A_long x = 0; x < output->width; x++) {
					PF_FpLong srcX, srcY;
					ApplyTransform2DOptimized((PF_FpLong)x, (PF_FpLong)y, params, &srcX, &srcY);
					srcX = centerX + (srcX - centerX) * invScale;
					srcY = centerY + (srcY - centerY) * invScale;

					PF_Pixel srcPix = SampleBilinear8(srcP, srcX, srcY);
					if (srcPix.alpha > 0) {
						// Apply opacity with clamping
						PF_FpLong opacity = transform.opacity;
						if (!std::isfinite(opacity)) opacity = 100.0;
						if (opacity < 0.0) opacity = 0.0;
						if (opacity > 100.0) opacity = 100.0;

						if (opacity < 100.0) {
							srcPix.alpha = (A_u_char)(srcPix.alpha * opacity / 100.0);
						}
						CompositePremult8(&dstRow[x], &srcPix);
					}
				}
			}

			ERR(PF_ABORT(in_data));
		}
	}

	return err;
}

// ============================================================================
// MAIN RENDER FUNCTION - Orchestrates all phases
// ============================================================================
static PF_Err
Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err				err		= PF_Err_NONE;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	// Validate inputs
	if (!params || !output) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	// Get source layer
	PF_EffectWorld *srcP = &params[REPTALL_INPUT]->u.ld;
	if (!srcP) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	// ========================================================================
	// PHASE 1: Extract parameters from UI
	// ========================================================================
	ReptAllState state;
	state.Clear();

	ERR(ExtractParameters(in_data, params, &state));
	if (err) return err;

	// ========================================================================
	// PHASE 2: Compute transforms for all copies
	// ========================================================================
	// Calculate total copies with overflow check
	A_long totalX = state.copies[0];
	A_long totalY = state.copies[1];
	A_long totalZ = state.copies[2];

	// Validate individual dimensions
	if (totalX < 1 || totalX > MAX_COPIES ||
		totalY < 1 || totalY > MAX_COPIES ||
		totalZ < 1 || totalZ > MAX_COPIES) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	// Check for multiplication overflow
	if (totalX > 0 && totalY > LONG_MAX / totalX) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}
	A_long totalXY = totalX * totalY;

	if (totalXY > 0 && totalZ > LONG_MAX / totalXY) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	A_long totalCopies = totalXY * totalZ;

	// Validate maximum copy count
	if (totalCopies > MAX_COPIES) {
		return PF_Err_BAD_CALLBACK_PARAM;
	}

	// Use std::vector for automatic memory management (RAII pattern)
	std::vector<CopyTransform> transformStorage(totalCopies);
	A_long transformCount = 0;
	ERR(ComputeCopyTransforms(&state, transformStorage.data(), &transformCount, in_data));
	if (err) {
		return err;
	}

	// ========================================================================
	// PHASE 3: Sort copies by depth
	// ========================================================================
	SortCopiesByDepth(transformStorage.data(), transformCount, state.camera_aware);

	// ========================================================================
	// PHASE 4: Render all copies
	// ========================================================================
	ERR(RenderCopies(in_data, out_data, &state, transformStorage.data(), transformCount, srcP, output));

	// std::vector handles cleanup automatically (RAII)

	return err;
}


extern "C" DllExport
PF_Err PluginDataEntryFunction2(
	PF_PluginDataPtr inPtr,
	PF_PluginDataCB2 inPluginDataCallBackPtr,
	SPBasicSuite* inSPBasicSuitePtr,
	const char* inHostName,
	const char* inHostVersion)
{
	PF_Err result = PF_Err_INVALID_CALLBACK;

	result = PF_REGISTER_EFFECT_EXT2(
		inPtr,
		inPluginDataCallBackPtr,
		"ReptAll", // Name
		"361do ReptAll", // Match Name
		"361do_plugins", // Category
		AE_RESERVED_INFO, // Reserved Info
		"EffectMain",	// Entry point
		"https://github.com/rebuildup/Ae_ReptAll");	// support URL

	return result;
}


PF_Err
EffectMain(
	PF_Cmd			cmd,
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output,
	void			*extra)
{
	PF_Err		err = PF_Err_NONE;

	// PF_Err is NOT meant to be thrown as exception - return errors directly
	switch (cmd) {
		case PF_Cmd_ABOUT:
			err = About(in_data,
						out_data,
						params,
						output);
			break;

		case PF_Cmd_GLOBAL_SETUP:
			err = GlobalSetup(in_data,
								out_data,
								params,
								output);
			break;

		case PF_Cmd_PARAMS_SETUP:
			err = ParamsSetup(in_data,
								out_data,
								params,
								output);
			break;

		case PF_Cmd_RENDER:
			err = Render(in_data,
						out_data,
						params,
						output);
			break;
	}

	return err;
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
