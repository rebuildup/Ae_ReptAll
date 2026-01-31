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

#include "ReptAll.h"

typedef struct {
	A_u_long	index;
	A_char		str[256];
} TableString;


const TableString		g_strs[StrID_NUMTYPES] = {
	StrID_NONE,						"",
	StrID_Name,						"3D Repeater",
	StrID_Description,				"3D camera-aware layer repeater effect.\\rCopyright 2024",
	StrID_CopiesX_Param_Name,		"Copies X",
	StrID_StepX_Param_Name,			"Step X",
	StrID_StepY_Param_Name,			"Step Y",
	StrID_StepZ_Param_Name,			"Step Z",
	StrID_StepRotateX_Param_Name,	"Step Rotate X",
	StrID_StepRotateY_Param_Name,	"Step Rotate Y",
	StrID_StepRotateZ_Param_Name,	"Step Rotate Z",
	StrID_StepScale_Param_Name,		"Step Scale",
};


char	*GetStringPtr(int strNum)
{
	// Add boundary check to prevent out-of-bounds access
	if (strNum < 0 || strNum >= StrID_NUMTYPES) {
		return const_cast<char*>("");
	}
	return const_cast<char*>(g_strs[strNum].str);
}