/** @file
*
*  Copyright (c) 2011, ARM Limited. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <PiPei.h>

#include <Library/ArmPlatformLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>

EFI_STATUS
EFIAPI
PlatformPeim (
  VOID
  )
{
  // Initialize the platform specific controllers
  ArmPlatformNormalInitialize ();

  BuildFvHob (PcdGet32(PcdFvBaseAddress), PcdGet32(PcdFvSize));

  return EFI_SUCCESS;
}
