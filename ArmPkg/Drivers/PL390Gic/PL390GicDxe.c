/*++

Copyright (c) 2009, Hewlett-Packard Company. All rights reserved.<BR>
Portions copyright (c) 2010, Apple Inc. All rights reserved.<BR>
Portions copyright (c) 2011-2012, ARM Ltd. All rights reserved.<BR> 
This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.   

Module Name:

  Gic.c

Abstract:

  Driver implementing the GIC interrupt controller protocol

--*/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <Library/IoLib.h>
#include <Library/ArmGicLib.h>

#include <Protocol/Cpu.h>
#include <Protocol/HardwareInterrupt.h>

// number of 32-bit registers needed to represent those interrupts as a bit
// (used for enable set, enable clear, pending set, pending clear, and active regs)
#define ARM_GIC_NUM_REG_PER_INT_BITS   (PcdGet32(PcdGicNumInterrupts) / 32)

// number of 32-bit registers needed to represent those interrupts as two bits
// (used for configuration reg)
#define ARM_GIC_NUM_REG_PER_INT_CFG    (PcdGet32(PcdGicNumInterrupts) / 16)

// number of 32-bit registers needed to represent interrupts as 8-bit priority field
// (used for priority regs)
#define ARM_GIC_NUM_REG_PER_INT_BYTES  (PcdGet32(PcdGicNumInterrupts) / 4)

#define ARM_GIC_DEFAULT_PRIORITY  0x80

extern EFI_HARDWARE_INTERRUPT_PROTOCOL gHardwareInterruptProtocol;

//
// Notifications
//
EFI_EVENT EfiExitBootServicesEvent      = (EFI_EVENT)NULL;

HARDWARE_INTERRUPT_HANDLER  gRegisteredInterruptHandlers[FixedPcdGet32(PcdGicNumInterrupts)];

/**
  Register Handler for the specified interrupt source.

  @param This     Instance pointer for this protocol
  @param Source   Hardware source of the interrupt
  @param Handler  Callback for interrupt. NULL to unregister

  @retval EFI_SUCCESS Source was updated to support Handler.
  @retval EFI_DEVICE_ERROR  Hardware could not be programmed.

**/
EFI_STATUS
EFIAPI
RegisterInterruptSource (
  IN EFI_HARDWARE_INTERRUPT_PROTOCOL    *This,
  IN HARDWARE_INTERRUPT_SOURCE          Source,
  IN HARDWARE_INTERRUPT_HANDLER         Handler
  )
{
  if (Source > PcdGet32(PcdGicNumInterrupts)) {
    ASSERT(FALSE);
    return EFI_UNSUPPORTED;
  }
  
  if ((Handler == NULL) && (gRegisteredInterruptHandlers[Source] == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Handler != NULL) && (gRegisteredInterruptHandlers[Source] != NULL)) {
    return EFI_ALREADY_STARTED;
  }

  gRegisteredInterruptHandlers[Source] = Handler;

  // If the interrupt handler is unregistered then disable the interrupt
  if (NULL == Handler){
  	return This->DisableInterruptSource (This, Source);
  } else {
  	return This->EnableInterruptSource (This, Source);
  }
}

/**
  Enable interrupt source Source.

  @param This     Instance pointer for this protocol
  @param Source   Hardware source of the interrupt

  @retval EFI_SUCCESS       Source interrupt enabled.
  @retval EFI_DEVICE_ERROR  Hardware could not be programmed.

**/
EFI_STATUS
EFIAPI
EnableInterruptSource (
  IN EFI_HARDWARE_INTERRUPT_PROTOCOL    *This,
  IN HARDWARE_INTERRUPT_SOURCE          Source
  )
{
  UINT32    RegOffset;
  UINTN     RegShift;
  
  if (Source > PcdGet32(PcdGicNumInterrupts)) {
    ASSERT(FALSE);
    return EFI_UNSUPPORTED;
  }
  
  // calculate enable register offset and bit position
  RegOffset = Source / 32;
  RegShift = Source % 32;

  // write set-enable register
  MmioWrite32 (PcdGet32(PcdGicDistributorBase) + ARM_GIC_ICDISER + (4*RegOffset), 1 << RegShift);
  
  return EFI_SUCCESS;
}

/**
  Disable interrupt source Source.

  @param This     Instance pointer for this protocol
  @param Source   Hardware source of the interrupt

  @retval EFI_SUCCESS       Source interrupt disabled.
  @retval EFI_DEVICE_ERROR  Hardware could not be programmed.

**/
EFI_STATUS
EFIAPI
DisableInterruptSource (
  IN EFI_HARDWARE_INTERRUPT_PROTOCOL    *This,
  IN HARDWARE_INTERRUPT_SOURCE          Source
  )
{
  UINT32    RegOffset;
  UINTN     RegShift;
  
  if (Source > PcdGet32(PcdGicNumInterrupts)) {
    ASSERT(FALSE);
    return EFI_UNSUPPORTED;
  }
  
  // Calculate enable register offset and bit position
  RegOffset = Source / 32;
  RegShift = Source % 32;

  // Write set-enable register
  MmioWrite32 (PcdGet32(PcdGicDistributorBase) + ARM_GIC_ICDICER + (4*RegOffset), 1 << RegShift);
  
  return EFI_SUCCESS;
}

/**
  Return current state of interrupt source Source.

  @param This     Instance pointer for this protocol
  @param Source   Hardware source of the interrupt
  @param InterruptState  TRUE: source enabled, FALSE: source disabled.

  @retval EFI_SUCCESS       InterruptState is valid
  @retval EFI_DEVICE_ERROR  InterruptState is not valid

**/
EFI_STATUS
EFIAPI
GetInterruptSourceState (
  IN EFI_HARDWARE_INTERRUPT_PROTOCOL    *This,
  IN HARDWARE_INTERRUPT_SOURCE          Source,
  IN BOOLEAN                            *InterruptState
  )
{
  UINT32    RegOffset;
  UINTN     RegShift;
  
  if (Source > PcdGet32(PcdGicNumInterrupts)) {
    ASSERT(FALSE);
    return EFI_UNSUPPORTED;
  }
  
  // calculate enable register offset and bit position
  RegOffset = Source / 32;
  RegShift = Source % 32;
    
  if ((MmioRead32 (PcdGet32(PcdGicDistributorBase) + ARM_GIC_ICDISER + (4*RegOffset)) & (1<<RegShift)) == 0) {
    *InterruptState = FALSE;
  } else {
    *InterruptState = TRUE;
  }
  
  return EFI_SUCCESS;
}

/**
  Signal to the hardware that the End Of Intrrupt state 
  has been reached.

  @param This     Instance pointer for this protocol
  @param Source   Hardware source of the interrupt

  @retval EFI_SUCCESS       Source interrupt EOI'ed.
  @retval EFI_DEVICE_ERROR  Hardware could not be programmed.

**/
EFI_STATUS
EFIAPI
EndOfInterrupt (
  IN EFI_HARDWARE_INTERRUPT_PROTOCOL    *This,
  IN HARDWARE_INTERRUPT_SOURCE          Source
  )
{
  if (Source > PcdGet32(PcdGicNumInterrupts)) {
    ASSERT(FALSE);
    return EFI_UNSUPPORTED;
  }

  MmioWrite32 (PcdGet32(PcdGicInterruptInterfaceBase) + ARM_GIC_ICCEIOR, Source);
  return EFI_SUCCESS;
}

/**
  EFI_CPU_INTERRUPT_HANDLER that is called when a processor interrupt occurs.

  @param  InterruptType    Defines the type of interrupt or exception that
                           occurred on the processor.This parameter is processor architecture specific.
  @param  SystemContext    A pointer to the processor context when
                           the interrupt occurred on the processor.

  @return None

**/
VOID
EFIAPI
IrqInterruptHandler (
  IN EFI_EXCEPTION_TYPE           InterruptType,
  IN EFI_SYSTEM_CONTEXT           SystemContext
  )
{
  UINT32                      GicInterrupt;
  HARDWARE_INTERRUPT_HANDLER  InterruptHandler;

  GicInterrupt = MmioRead32 (PcdGet32(PcdGicInterruptInterfaceBase) + ARM_GIC_ICCIAR);

  // Special Interrupts (ID1020-ID1023) have an Interrupt ID greater than the number of interrupt (ie: Spurious interrupt).
  if (GicInterrupt >= PcdGet32(PcdGicNumInterrupts)) {
    // The special interrupt do not need to be acknowledge
    return;
  }
  
  InterruptHandler = gRegisteredInterruptHandlers[GicInterrupt];
  if (InterruptHandler != NULL) {
    // Call the registered interrupt handler.
    InterruptHandler (GicInterrupt, SystemContext);
  } else {
    DEBUG ((EFI_D_ERROR, "Spurious GIC interrupt: 0x%x\n", GicInterrupt));
  }

  EndOfInterrupt (&gHardwareInterruptProtocol, GicInterrupt);
}

//
// Making this global saves a few bytes in image size
//
EFI_HANDLE  gHardwareInterruptHandle = NULL;

//
// The protocol instance produced by this driver
//
EFI_HARDWARE_INTERRUPT_PROTOCOL gHardwareInterruptProtocol = {
  RegisterInterruptSource,
  EnableInterruptSource,
  DisableInterruptSource,
  GetInterruptSourceState,
  EndOfInterrupt
};

/**
  Shutdown our hardware
  
  DXE Core will disable interrupts and turn off the timer and disable interrupts
  after all the event handlers have run.

  @param[in]  Event   The Event that is being processed
  @param[in]  Context Event Context
**/
VOID
EFIAPI
ExitBootServicesEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINTN    Index;
  
  // Acknowledge all pending interrupts
  for (Index = 0; Index < PcdGet32(PcdGicNumInterrupts); Index++) {
    DisableInterruptSource (&gHardwareInterruptProtocol, Index);
  }

  for (Index = 0; Index < PcdGet32(PcdGicNumInterrupts); Index++) {
    EndOfInterrupt (&gHardwareInterruptProtocol, Index);
  }

  // Disable Gic Interface
  MmioWrite32 (PcdGet32(PcdGicInterruptInterfaceBase) + ARM_GIC_ICCICR, 0x0);
  MmioWrite32 (PcdGet32(PcdGicInterruptInterfaceBase) + ARM_GIC_ICCPMR, 0x0);

  // Disable Gic Distributor
  MmioWrite32 (PcdGet32(PcdGicDistributorBase) + ARM_GIC_ICDDCR, 0x0);
}

/**
  Initialize the state information for the CPU Architectural Protocol

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
InterruptDxeInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS              Status;
  UINTN                   Index;
  UINT32                  RegOffset;
  UINTN                   RegShift;
  EFI_CPU_ARCH_PROTOCOL   *Cpu;
  
  // Make sure the Interrupt Controller Protocol is not already installed in the system.
  ASSERT_PROTOCOL_ALREADY_INSTALLED (NULL, &gHardwareInterruptProtocolGuid);

  for (Index = 0; Index < PcdGet32(PcdGicNumInterrupts); Index++) {
    DisableInterruptSource (&gHardwareInterruptProtocol, Index);
    
    // Set Priority 
    RegOffset = Index / 4;
    RegShift = (Index % 4) * 8;
    MmioAndThenOr32 (
      PcdGet32(PcdGicDistributorBase) + ARM_GIC_ICDIPR + (4*RegOffset),
      ~(0xff << RegShift), 
      ARM_GIC_DEFAULT_PRIORITY << RegShift
      );
  }

  // Configure interrupts for cpu 0
  for (Index = 0; Index < ARM_GIC_NUM_REG_PER_INT_BYTES; Index++) {
    MmioWrite32 (PcdGet32(PcdGicDistributorBase) + ARM_GIC_ICDIPTR + (Index*4), 0x01010101);
  }

  // Set binary point reg to 0x7 (no preemption)
  MmioWrite32 (PcdGet32(PcdGicInterruptInterfaceBase) + ARM_GIC_ICCBPR, 0x7);

  // Set priority mask reg to 0xff to allow all priorities through
  MmioWrite32 (PcdGet32(PcdGicInterruptInterfaceBase) + ARM_GIC_ICCPMR, 0xff);
  
  // Enable gic cpu interface
  MmioWrite32 (PcdGet32(PcdGicInterruptInterfaceBase) + ARM_GIC_ICCICR, 0x1);

  // Enable gic distributor
  MmioWrite32 (PcdGet32(PcdGicDistributorBase) + ARM_GIC_ICDDCR, 0x1);
  
  ZeroMem (&gRegisteredInterruptHandlers, sizeof (gRegisteredInterruptHandlers));
  
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &gHardwareInterruptHandle,
                  &gHardwareInterruptProtocolGuid,   &gHardwareInterruptProtocol,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);
  
  //
  // Get the CPU protocol that this driver requires.
  //
  Status = gBS->LocateProtocol(&gEfiCpuArchProtocolGuid, NULL, (VOID **)&Cpu);
  ASSERT_EFI_ERROR(Status);

  //
  // Unregister the default exception handler.
  //
  Status = Cpu->RegisterInterruptHandler(Cpu, EXCEPT_ARM_IRQ, NULL);
  ASSERT_EFI_ERROR(Status);

  //
  // Register to receive interrupts
  //
  Status = Cpu->RegisterInterruptHandler(Cpu, EXCEPT_ARM_IRQ, IrqInterruptHandler);
  ASSERT_EFI_ERROR(Status);

  // Register for an ExitBootServicesEvent
  Status = gBS->CreateEvent (EVT_SIGNAL_EXIT_BOOT_SERVICES, TPL_NOTIFY, ExitBootServicesEvent, NULL, &EfiExitBootServicesEvent);
  ASSERT_EFI_ERROR (Status);

  return Status;
}
