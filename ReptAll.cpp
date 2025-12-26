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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

	// Count - number of copies
	AEFX_CLR_STRUCT(def);
	PF_ADD_SLIDER(	STR(StrID_Count_Param_Name), 
					REPTALL_COUNT_MIN, 
					REPTALL_COUNT_MAX, 
					REPTALL_COUNT_MIN, 
					REPTALL_COUNT_MAX,
					REPTALL_COUNT_DFLT,
					COUNT_DISK_ID);

	// Translate X
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_TranslateX_Param_Name), 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							TRANSLATE_X_DISK_ID);

	// Translate Y
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_TranslateY_Param_Name), 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							TRANSLATE_Y_DISK_ID);

	// Translate Z
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_TranslateZ_Param_Name), 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_MIN, 
							REPTALL_TRANSLATE_MAX, 
							REPTALL_TRANSLATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							TRANSLATE_Z_DISK_ID);

	// Rotate X
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_RotateX_Param_Name), 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							ROTATE_X_DISK_ID);

	// Rotate Y
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_RotateY_Param_Name), 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							ROTATE_Y_DISK_ID);

	// Rotate Z
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_RotateZ_Param_Name), 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_MIN, 
							REPTALL_ROTATE_MAX, 
							REPTALL_ROTATE_DFLT,
							PF_Precision_TENTHS,
							0,
							0,
							ROTATE_Z_DISK_ID);

	// Scale
	AEFX_CLR_STRUCT(def);
	PF_ADD_FLOAT_SLIDERX(	STR(StrID_Scale_Param_Name), 
							REPTALL_SCALE_MIN, 
							REPTALL_SCALE_MAX, 
							REPTALL_SCALE_MIN, 
							REPTALL_SCALE_MAX, 
							REPTALL_SCALE_DFLT,
							PF_Precision_TENTHS,
							0,
							PF_ValueDisplayFlag_PERCENT,
							SCALE_DISK_ID);

	out_data->num_params = REPTALL_SCALE + 1;  // No Use 3D checkbox - always enabled
	
	return err;
}

// Apply 2D affine transformation for rotation and scale
static void 
ApplyTransform2D(
	PF_FpLong srcX, 
	PF_FpLong srcY,
	PF_FpLong centerX,
	PF_FpLong centerY,
	PF_FpLong translateX,
	PF_FpLong translateY,
	PF_FpLong rotateZ,      // degrees
	PF_FpLong scale,        // percent (100 = 1.0)
	PF_FpLong *dstX,
	PF_FpLong *dstY)
{
	// Convert rotation to radians
	PF_FpLong radZ = rotateZ * M_PI / 180.0;
	PF_FpLong scaleF = scale / 100.0;
	
	// Translate to origin
	PF_FpLong x = srcX - centerX;
	PF_FpLong y = srcY - centerY;
	
	// Apply scale
	x *= scaleF;
	y *= scaleF;
	
	// Apply rotation
	PF_FpLong cosZ = cos(radZ);
	PF_FpLong sinZ = sin(radZ);
	PF_FpLong rx = x * cosZ - y * sinZ;
	PF_FpLong ry = x * sinZ + y * cosZ;
	
	// Translate back + apply translation offset
	*dstX = rx + centerX + translateX;
	*dstY = ry + centerY + translateY;
}

// Bilinear sampling function for 8-bit
static PF_Pixel
SampleBilinear8(
	PF_EffectWorld *srcP,
	PF_FpLong x,
	PF_FpLong y)
{
	PF_Pixel result = {0, 0, 0, 0};
	
	if (x < 0 || y < 0 || x >= srcP->width - 1 || y >= srcP->height - 1) {
		return result;
	}
	
	A_long x0 = (A_long)x;
	A_long y0 = (A_long)y;
	A_long x1 = x0 + 1;
	A_long y1 = y0 + 1;
	
	PF_FpLong fx = x - x0;
	PF_FpLong fy = y - y0;
	
	PF_Pixel *row0 = (PF_Pixel*)((char*)srcP->data + y0 * srcP->rowbytes);
	PF_Pixel *row1 = (PF_Pixel*)((char*)srcP->data + y1 * srcP->rowbytes);
	
	PF_Pixel p00 = row0[x0];
	PF_Pixel p10 = row0[x1];
	PF_Pixel p01 = row1[x0];
	PF_Pixel p11 = row1[x1];
	
	// Bilinear interpolation
	PF_FpLong w00 = (1.0 - fx) * (1.0 - fy);
	PF_FpLong w10 = fx * (1.0 - fy);
	PF_FpLong w01 = (1.0 - fx) * fy;
	PF_FpLong w11 = fx * fy;
	
	result.alpha = (A_u_char)(p00.alpha * w00 + p10.alpha * w10 + p01.alpha * w01 + p11.alpha * w11 + 0.5);
	result.red   = (A_u_char)(p00.red * w00 + p10.red * w10 + p01.red * w01 + p11.red * w11 + 0.5);
	result.green = (A_u_char)(p00.green * w00 + p10.green * w10 + p01.green * w01 + p11.green * w11 + 0.5);
	result.blue  = (A_u_char)(p00.blue * w00 + p10.blue * w10 + p01.blue * w01 + p11.blue * w11 + 0.5);
	
	return result;
}

// Bilinear sampling function for 16-bit
static PF_Pixel16
SampleBilinear16(
	PF_EffectWorld *srcP,
	PF_FpLong x,
	PF_FpLong y)
{
	PF_Pixel16 result = {0, 0, 0, 0};
	
	if (x < 0 || y < 0 || x >= srcP->width - 1 || y >= srcP->height - 1) {
		return result;
	}
	
	A_long x0 = (A_long)x;
	A_long y0 = (A_long)y;
	A_long x1 = x0 + 1;
	A_long y1 = y0 + 1;
	
	PF_FpLong fx = x - x0;
	PF_FpLong fy = y - y0;
	
	PF_Pixel16 *row0 = (PF_Pixel16*)((char*)srcP->data + y0 * srcP->rowbytes);
	PF_Pixel16 *row1 = (PF_Pixel16*)((char*)srcP->data + y1 * srcP->rowbytes);
	
	PF_Pixel16 p00 = row0[x0];
	PF_Pixel16 p10 = row0[x1];
	PF_Pixel16 p01 = row1[x0];
	PF_Pixel16 p11 = row1[x1];
	
	// Bilinear interpolation
	PF_FpLong w00 = (1.0 - fx) * (1.0 - fy);
	PF_FpLong w10 = fx * (1.0 - fy);
	PF_FpLong w01 = (1.0 - fx) * fy;
	PF_FpLong w11 = fx * fy;
	
	result.alpha = (A_u_short)(p00.alpha * w00 + p10.alpha * w10 + p01.alpha * w01 + p11.alpha * w11 + 0.5);
	result.red   = (A_u_short)(p00.red * w00 + p10.red * w10 + p01.red * w01 + p11.red * w11 + 0.5);
	result.green = (A_u_short)(p00.green * w00 + p10.green * w10 + p01.green * w01 + p11.green * w11 + 0.5);
	result.blue  = (A_u_short)(p00.blue * w00 + p10.blue * w10 + p01.blue * w01 + p11.blue * w11 + 0.5);
	
	return result;
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
	PF_FpLong oneMinusSrcA = 1.0 - srcA;
	
	dstP->alpha = (A_u_char)MIN(srcP->alpha + dstP->alpha * oneMinusSrcA + 0.5, PF_MAX_CHAN8);
	dstP->red   = (A_u_char)MIN(srcP->red + dstP->red * oneMinusSrcA + 0.5, PF_MAX_CHAN8);
	dstP->green = (A_u_char)MIN(srcP->green + dstP->green * oneMinusSrcA + 0.5, PF_MAX_CHAN8);
	dstP->blue  = (A_u_char)MIN(srcP->blue + dstP->blue * oneMinusSrcA + 0.5, PF_MAX_CHAN8);
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
	PF_FpLong oneMinusSrcA = 1.0 - srcA;
	
	dstP->alpha = (A_u_short)MIN(srcP->alpha + dstP->alpha * oneMinusSrcA + 0.5, PF_MAX_CHAN16);
	dstP->red   = (A_u_short)MIN(srcP->red + dstP->red * oneMinusSrcA + 0.5, PF_MAX_CHAN16);
	dstP->green = (A_u_short)MIN(srcP->green + dstP->green * oneMinusSrcA + 0.5, PF_MAX_CHAN16);
	dstP->blue  = (A_u_short)MIN(srcP->blue + dstP->blue * oneMinusSrcA + 0.5, PF_MAX_CHAN16);
}

static PF_Err 
Render (
	PF_InData		*in_data,
	PF_OutData		*out_data,
	PF_ParamDef		*params[],
	PF_LayerDef		*output )
{
	PF_Err				err		= PF_Err_NONE;
	AEGP_SuiteHandler	suites(in_data->pica_basicP);

	// Get parameter values
	ReptAllInfo info;
	AEFX_CLR_STRUCT(info);
	
	info.count       = params[REPTALL_COUNT]->u.sd.value;
	info.translate_x = params[REPTALL_TRANSLATE_X]->u.fs_d.value;
	info.translate_y = params[REPTALL_TRANSLATE_Y]->u.fs_d.value;
	info.translate_z = params[REPTALL_TRANSLATE_Z]->u.fs_d.value;
	info.rotate_x    = params[REPTALL_ROTATE_X]->u.fs_d.value;
	info.rotate_y    = params[REPTALL_ROTATE_Y]->u.fs_d.value;
	info.rotate_z    = params[REPTALL_ROTATE_Z]->u.fs_d.value;
	info.scale       = params[REPTALL_SCALE]->u.fs_d.value;
	
	// Get source layer
	PF_EffectWorld *srcP = &params[REPTALL_INPUT]->u.ld;
	
	// Check bit depth
	PF_Boolean deepB = PF_WORLD_IS_DEEP(output);
	
	// Calculate center point
	PF_FpLong centerX = srcP->width / 2.0;
	PF_FpLong centerY = srcP->height / 2.0;
	
	// ===== 3D Camera Information (Phase 2) =====
	A_Matrix4		camera_matrix;
	A_FpLong		focal_length = 0.0;
	PF_Boolean		has_camera = FALSE;
	
	AEFX_CLR_STRUCT(camera_matrix);
	
	// Only in After Effects (not Premiere Pro)
	if (in_data->appl_id != 'PrMr') {
		A_Time comp_timeT = {0, 1};
		AEGP_LayerH camera_layerH = NULL;
		
		// Convert effect time to comp time
		ERR(suites.PFInterfaceSuite1()->AEGP_ConvertEffectToCompTime(
			in_data->effect_ref,
			in_data->current_time,
			in_data->time_scale,
			&comp_timeT));
		
		// Get camera layer
		if (!err) {
			ERR(suites.PFInterfaceSuite1()->AEGP_GetEffectCamera(
				in_data->effect_ref,
				&comp_timeT,
				&camera_layerH));
		}
		
		// If camera exists, get its properties
		if (!err && camera_layerH) {
			has_camera = TRUE;
			
			// Get camera transform matrix (world space)
			ERR(suites.LayerSuite5()->AEGP_GetLayerToWorldXform(
				camera_layerH,
				&comp_timeT,
				&camera_matrix));
			
			// Get focal length (zoom)
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
				}
			}
		}
	}
	
	// Extract camera position from matrix (Phase 4a)
	PF_FpLong camera_x = 0.0;
	PF_FpLong camera_y = 0.0;
	PF_FpLong camera_z = 0.0;
	
	if (has_camera) {
		// Camera position is in the last column of the 4x4 matrix
		// mat[row][col], position is [0][3], [1][3], [2][3]
		camera_x = camera_matrix.mat[0][3];
		camera_y = camera_matrix.mat[1][3];
		camera_z = camera_matrix.mat[2][3];
	}
	
	// ===== End of 3D Camera Information =====
	
	// Clear output (will be filled by compositing)
	if (deepB) {
		PF_Pixel16 clearColor = {0, 0, 0, 0};
		ERR(suites.FillMatteSuite2()->fill16(in_data->effect_ref, &clearColor, NULL, output));
	} else {
		PF_Pixel clearColor = {0, 0, 0, 0};
		ERR(suites.FillMatteSuite2()->fill(in_data->effect_ref, &clearColor, NULL, output));
	}
	
	// ===== Phase 4b: Calculate Z-order sorting =====
	// Structure to hold copy index and camera distance
	struct CopyOrder {
		A_long index;
		PF_FpLong camera_distance;
	};
	
	CopyOrder render_order[10];  // Max 10 copies (REPTALL_COUNT_MAX)
	
	// Calculate camera distance for each copy
	for (A_long i = 0; i < info.count; i++) {
		PF_FpLong totalTranslateX = info.translate_x * i;
		PF_FpLong totalTranslateY = info.translate_y * i;
		PF_FpLong totalTranslateZ = info.translate_z * i;
		
		// Calculate distance from camera
		PF_FpLong dx = totalTranslateX - camera_x;
		PF_FpLong dy = totalTranslateY - camera_y;
		PF_FpLong dz = totalTranslateZ - camera_z;
		
		render_order[i].index = i;
		render_order[i].camera_distance = sqrt(dx*dx + dy*dy + dz*dz);
	}
	
	// Simple bubble sort (small array, not performance critical)
	// Sort furthest first (descending distance)
	for (A_long i = 0; i < info.count - 1; i++) {
		for (A_long j = 0; j < info.count - i - 1; j++) {
			if (render_order[j].camera_distance < render_order[j+1].camera_distance) {
				CopyOrder temp = render_order[j];
				render_order[j] = render_order[j+1];
				render_order[j+1] = temp;
			}
		}
	}
	// ===== End: Z-order sorting =====
	
	// Render copies in sorted order (furthest first for proper Z-order)
	for (A_long render_i = 0; render_i < info.count && !err; render_i++) {
		A_long i = render_order[render_i].index;  // Get actual copy index
		// Calculate cumulative transform for this copy
		PF_FpLong totalTranslateX = info.translate_x * i;
		PF_FpLong totalTranslateY = info.translate_y * i;
		PF_FpLong totalTranslateZ = info.translate_z * i;
		PF_FpLong totalRotateZ    = info.rotate_z * i;
		PF_FpLong totalScale      = 100.0;
		
		// Apply cumulative scale (each copy reduces by scale%)
		for (A_long j = 0; j < i; j++) {
			totalScale *= (info.scale / 100.0);
		}
		
		// ===== Phase 3: Apply perspective scaling based on Z =====
		if (has_camera && focal_length > 0.0 && totalTranslateZ != 0.0) {
			// Simple perspective: scale = focal_length / (focal_length - z)
			// Positive Z moves away from camera (gets smaller)
			// Negative Z moves toward camera (gets larger)
			PF_FpLong z_distance = totalTranslateZ;
			PF_FpLong perspectiveScale = 1.0;
			
			// Calculate perspective scale
			PF_FpLong denominator = focal_length - z_distance;
			
			// Prevent division by zero and extreme values
			if (denominator > 1.0) {
				perspectiveScale = focal_length / denominator;
			} else if (denominator < -1.0) {
				perspectiveScale = focal_length / denominator;
			} else {
				// Too close to or behind camera - make very small
				perspectiveScale = 0.01;
			}
			
			// Clamp to reasonable range (prevent extreme scaling)
			if (perspectiveScale < 0.01) perspectiveScale = 0.01;
			if (perspectiveScale > 10.0) perspectiveScale = 10.0;
			
			// Apply perspective to total scale
			totalScale *= perspectiveScale;
		}
		// ===== End: Perspective scaling =====
		
		// Iterate through output pixels
		for (A_long y = 0; y < output->height && !err; y++) {
			for (A_long x = 0; x < output->width; x++) {
				// Inverse transform: output -> source coordinates
				PF_FpLong srcX, srcY;
				
				// Apply inverse transform (reverse order)
				PF_FpLong invScale = 100.0 / totalScale;
				PF_FpLong invRotateZ = -totalRotateZ;
				
				// Phase 4a: Apply camera position offset
				// Camera offset affects the apparent position of copies
				PF_FpLong invTranslateX = -totalTranslateX;
				PF_FpLong invTranslateY = -totalTranslateY;
				
				if (has_camera) {
					// Adjust translation based on camera position
					// When camera moves right (+X), scene appears to move left
					invTranslateX += camera_x;
					invTranslateY += camera_y;
				}
				
				ApplyTransform2D(
					(PF_FpLong)x, (PF_FpLong)y,
					centerX, centerY,
					invTranslateX, invTranslateY,
					invRotateZ,
					100.0,  // Scale is applied separately
					&srcX, &srcY);
				
				// Apply inverse scale from center
				srcX = centerX + (srcX - centerX) * invScale;
				srcY = centerY + (srcY - centerY) * invScale;
				
				// Sample and composite
				if (deepB) {
					PF_Pixel16 srcPix = SampleBilinear16(srcP, srcX, srcY);
					if (srcPix.alpha > 0) {
						PF_Pixel16 *dstRow = (PF_Pixel16*)((char*)output->data + y * output->rowbytes);
						CompositePremult16(&dstRow[x], &srcPix);
					}
				} else {
					PF_Pixel srcPix = SampleBilinear8(srcP, srcX, srcY);
					if (srcPix.alpha > 0) {
						PF_Pixel *dstRow = (PF_Pixel*)((char*)output->data + y * output->rowbytes);
						CompositePremult8(&dstRow[x], &srcPix);
					}
				}
			}
		}
		
		ERR(PF_ABORT(in_data));
	}

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
	
	try {
		switch (cmd) {
			case PF_Cmd_ABOUT:

				err = About(in_data,
							out_data,
							params,
							output);
				break;
				
			case PF_Cmd_GLOBAL_SETUP:

				err = GlobalSetup(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_PARAMS_SETUP:

				err = ParamsSetup(	in_data,
									out_data,
									params,
									output);
				break;
				
			case PF_Cmd_RENDER:

				err = Render(	in_data,
								out_data,
								params,
								output);
				break;
		}
	}
	catch(PF_Err &thrown_err){
		err = thrown_err;
	}
	return err;
}
