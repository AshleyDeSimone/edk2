/** @file
  Produces Simple Text Input Protocl, Simple Text Input Extended Protocol and
  Simple Text Output Protocol upon Serial IO Protocol.

Copyright (c) 2006 - 2008, Intel Corporation. <BR>
All rights reserved. This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#include "Terminal.h"

STATIC
EFI_STATUS
TerminalFreeNotifyList (
  IN OUT LIST_ENTRY           *ListHead
  );

//
// Globals
//
EFI_DRIVER_BINDING_PROTOCOL gTerminalDriverBinding = {
  TerminalDriverBindingSupported,
  TerminalDriverBindingStart,
  TerminalDriverBindingStop,
  0xa,
  NULL,
  NULL
};


EFI_GUID  *gTerminalType[] = {
  &gEfiPcAnsiGuid,
  &gEfiVT100Guid,
  &gEfiVT100PlusGuid,
  &gEfiVTUTF8Guid
};


TERMINAL_DEV  gTerminalDevTemplate = {
  TERMINAL_DEV_SIGNATURE,
  NULL,
  0,
  NULL,
  NULL,
  {   // SimpleTextInput
    TerminalConInReset,
    TerminalConInReadKeyStroke,
    NULL
  },
  {   // SimpleTextOutput
    TerminalConOutReset,
    TerminalConOutOutputString,
    TerminalConOutTestString,
    TerminalConOutQueryMode,
    TerminalConOutSetMode,
    TerminalConOutSetAttribute,
    TerminalConOutClearScreen,
    TerminalConOutSetCursorPosition,
    TerminalConOutEnableCursor,
    NULL
  },
  {   // SimpleTextOutputMode
    1,                                           // MaxMode
    0,                                           // Mode?
    EFI_TEXT_ATTR (EFI_LIGHTGRAY, EFI_BLACK),    // Attribute
    0,                                           // CursorColumn
    0,                                           // CursorRow
    TRUE                                         // CursorVisible
  },
  0,
  {
    0,
    0,
    { 0 }
  },
  {
    0,
    0,
    { 0 }
  },
  {
    0,
    0,
    { {0} }
  },
  NULL, // ControllerNameTable
  NULL,
  INPUT_STATE_DEFAULT,
  RESET_STATE_DEFAULT,
  FALSE,
  {   // SimpleTextInputEx
    TerminalConInResetEx,
    TerminalConInReadKeyStrokeEx,
    NULL,
    TerminalConInSetState,
    TerminalConInRegisterKeyNotify,
    TerminalConInUnregisterKeyNotify,
  },
  {
    NULL,
    NULL,
  }
};



EFI_STATUS
EFIAPI
TerminalDriverBindingSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath;
  EFI_SERIAL_IO_PROTOCOL    *SerialIo;
  VENDOR_DEVICE_PATH        *Node;

  //
  // If remaining device path is not NULL, then make sure it is a
  // device path that describes a terminal communications protocol.
  //
  if (RemainingDevicePath != NULL) {

    Node = (VENDOR_DEVICE_PATH *) RemainingDevicePath;

    if (Node->Header.Type != MESSAGING_DEVICE_PATH ||
        Node->Header.SubType != MSG_VENDOR_DP ||
        DevicePathNodeLength(&Node->Header) != sizeof(VENDOR_DEVICE_PATH)) {

      return EFI_UNSUPPORTED;

    }
    //
    // only supports PC ANSI, VT100, VT100+ and VT-UTF8 terminal types
    //
    if (!CompareGuid (&Node->Guid, &gEfiPcAnsiGuid) &&
        !CompareGuid (&Node->Guid, &gEfiVT100Guid) &&
        !CompareGuid (&Node->Guid, &gEfiVT100PlusGuid) &&
        !CompareGuid (&Node->Guid, &gEfiVTUTF8Guid)) {

      return EFI_UNSUPPORTED;
    }
  }
  //
  // Open the IO Abstraction(s) needed to perform the supported test
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (Status == EFI_ALREADY_STARTED) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->CloseProtocol (
        Controller,
        &gEfiDevicePathProtocolGuid,
        This->DriverBindingHandle,
        Controller
        );

  //
  // The Controller must support the Serial I/O Protocol.
  // This driver is a bus driver with at most 1 child device, so it is
  // ok for it to be already started.
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiSerialIoProtocolGuid,
                  (VOID **) &SerialIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (Status == EFI_ALREADY_STARTED) {
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    return Status;
  }
  //
  // Close the I/O Abstraction(s) used to perform the supported test
  //
  gBS->CloseProtocol (
        Controller,
        &gEfiSerialIoProtocolGuid,
        This->DriverBindingHandle,
        Controller
        );

  return Status;
}

EFI_STATUS
EFIAPI
TerminalDriverBindingStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
/*++

  Routine Description:

    Start the controller.

  Arguments:

    This                - A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
    Controller          - The handle of the controller to start.
    RemainingDevicePath - A pointer to the remaining portion of a devcie path.

  Returns:

    EFI_SUCCESS.

--*/
{
  EFI_STATUS                          Status;
  EFI_SERIAL_IO_PROTOCOL              *SerialIo;
  EFI_DEVICE_PATH_PROTOCOL            *ParentDevicePath;
  VENDOR_DEVICE_PATH                  *Node;
  VENDOR_DEVICE_PATH                  *DefaultNode;
  EFI_SERIAL_IO_MODE                  *Mode;
  UINTN                               SerialInTimeOut;
  TERMINAL_DEV                        *TerminalDevice;
  UINT8                               TerminalType;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *OpenInfoBuffer;
  UINTN                               EntryCount;
  UINTN                               Index;
  EFI_DEVICE_PATH_PROTOCOL            *DevicePath;

  TerminalDevice = NULL;
  DefaultNode    = NULL;
  //
  // Get the Device Path Protocol to build the device path of the child device
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &ParentDevicePath,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status) && Status != EFI_ALREADY_STARTED) {
    return Status;
  }

  //
  // Open the Serial I/O Protocol BY_DRIVER.  It might already be started.
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiSerialIoProtocolGuid,
                  (VOID **) &SerialIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status) && Status != EFI_ALREADY_STARTED) {
    return Status;
  }

  if (Status != EFI_ALREADY_STARTED) {
    //
    // If Serial I/O is not already open by this driver, then tag the handle
    // with the Terminal Driver GUID and update the ConInDev, ConOutDev, and
    // StdErrDev variables with the list of possible terminal types on this
    // serial port.
    //
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiCallerIdGuid,
                    NULL,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &Controller,
                      &gEfiCallerIdGuid,
                      DuplicateDevicePath (ParentDevicePath),
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        goto Error;
      }
      //
      // if the serial device is a hot plug device, do not update the
      // ConInDev, ConOutDev, and StdErrDev variables.
      //
      Status = gBS->OpenProtocol (
                      Controller,
                      &gEfiHotPlugDeviceGuid,
                      NULL,
                      This->DriverBindingHandle,
                      Controller,
                      EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                      );
      if (EFI_ERROR (Status)) {
        TerminalUpdateConsoleDevVariable (L"ConInDev", ParentDevicePath);
        TerminalUpdateConsoleDevVariable (L"ConOutDev", ParentDevicePath);
        TerminalUpdateConsoleDevVariable (L"ErrOutDev", ParentDevicePath);
      }
    }
  }
  //
  // Make sure a child handle does not already exist.  This driver can only
  // produce one child per serial port.
  //
  Status = gBS->OpenProtocolInformation (
                  Controller,
                  &gEfiSerialIoProtocolGuid,
                  &OpenInfoBuffer,
                  &EntryCount
                  );
  if (!EFI_ERROR (Status)) {
    Status = EFI_SUCCESS;
    for (Index = 0; Index < EntryCount; Index++) {
      if (OpenInfoBuffer[Index].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) {
        Status = EFI_ALREADY_STARTED;
      }
    }

    FreePool (OpenInfoBuffer);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }
  //
  // If RemainingDevicePath is NULL, then create default device path node
  //
  if (RemainingDevicePath == NULL) {
    DefaultNode = AllocateZeroPool (sizeof (VENDOR_DEVICE_PATH));
    if (DefaultNode == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Error;
    }

    TerminalType = FixedPcdGet8 (PcdDefaultTerminalType);
    // must be between PcAnsiType (0) and VTUTF8Type (3)
    ASSERT (TerminalType <= VTUTF8Type);

    CopyMem (&DefaultNode->Guid, gTerminalType[TerminalType], sizeof (EFI_GUID));
    RemainingDevicePath = (EFI_DEVICE_PATH_PROTOCOL*)DefaultNode;
  } else {
    //
    // Use the RemainingDevicePath to determine the terminal type
    //
    Node = (VENDOR_DEVICE_PATH *)RemainingDevicePath;
    if (CompareGuid (&Node->Guid, &gEfiPcAnsiGuid)) {
      TerminalType = PcAnsiType;
    } else if (CompareGuid (&Node->Guid, &gEfiVT100Guid)) {
      TerminalType = VT100Type;
    } else if (CompareGuid (&Node->Guid, &gEfiVT100PlusGuid)) {
      TerminalType = VT100PlusType;
    } else if (CompareGuid (&Node->Guid, &gEfiVTUTF8Guid)) {
      TerminalType = VTUTF8Type;
    } else {
      goto Error;
    }
  }

  //
  // Initialize the Terminal Dev
  //
  TerminalDevice = AllocateCopyPool (sizeof (TERMINAL_DEV), &gTerminalDevTemplate);
  if (TerminalDevice == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Error;
  }

  TerminalDevice->TerminalType  = TerminalType;
  TerminalDevice->SerialIo      = SerialIo;

  InitializeListHead (&TerminalDevice->NotifyList);
  Status = gBS->CreateEvent (
                  EVT_NOTIFY_WAIT,
                  TPL_NOTIFY,
                  TerminalConInWaitForKeyEx,
                  &TerminalDevice->SimpleInputEx,
                  &TerminalDevice->SimpleInputEx.WaitForKeyEx
                  );
  if (EFI_ERROR (Status)) {
    goto Error;
  }


  Status = gBS->CreateEvent (
                  EVT_NOTIFY_WAIT,
                  TPL_NOTIFY,
                  TerminalConInWaitForKey,
                  &TerminalDevice->SimpleInput,
                  &TerminalDevice->SimpleInput.WaitForKey
                  );
  if (EFI_ERROR (Status)) {
    goto Error;
  }
  //
  // initialize the FIFO buffer used for accommodating
  // the pre-read pending characters
  //
  InitializeRawFiFo (TerminalDevice);
  InitializeUnicodeFiFo (TerminalDevice);
  InitializeEfiKeyFiFo (TerminalDevice);

  //
  // Set the timeout value of serial buffer for
  // keystroke response performance issue
  //
  Mode            = TerminalDevice->SerialIo->Mode;

  SerialInTimeOut = 0;
  if (Mode->BaudRate != 0) {
    SerialInTimeOut = (1 + Mode->DataBits + Mode->StopBits) * 2 * 1000000 / (UINTN) Mode->BaudRate;
  }

  Status = TerminalDevice->SerialIo->SetAttributes (
                                      TerminalDevice->SerialIo,
                                      Mode->BaudRate,
                                      Mode->ReceiveFifoDepth,
                                      (UINT32) SerialInTimeOut,
                                      (EFI_PARITY_TYPE) (Mode->Parity),
                                      (UINT8) Mode->DataBits,
                                      (EFI_STOP_BITS_TYPE) (Mode->StopBits)
                                      );
  if (EFI_ERROR (Status)) {
    //
    // if set attributes operation fails, invalidate
    // the value of SerialInTimeOut,thus make it
    // inconsistent with the default timeout value
    // of serial buffer. This will invoke the recalculation
    // in the readkeystroke routine.
    //
    TerminalDevice->SerialInTimeOut = 0;
  } else {
    TerminalDevice->SerialInTimeOut = SerialInTimeOut;
  }
  //
  // Build the device path for the child device
  //
  Status = SetTerminalDevicePath (
            TerminalDevice->TerminalType,
            ParentDevicePath,
            &TerminalDevice->DevicePath
            );
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  DevicePath = TerminalDevice->DevicePath;

  Status = TerminalDevice->SimpleInput.Reset (
                                        &TerminalDevice->SimpleInput,
                                        FALSE
                                        );
  if (EFI_ERROR (Status)) {
    //
    // Need to report Error Code first
    //
    goto ReportError;
  }
  //
  // Simple Text Output Protocol
  //
  TerminalDevice->SimpleTextOutput.Reset              = TerminalConOutReset;
  TerminalDevice->SimpleTextOutput.OutputString       = TerminalConOutOutputString;
  TerminalDevice->SimpleTextOutput.TestString         = TerminalConOutTestString;
  TerminalDevice->SimpleTextOutput.QueryMode          = TerminalConOutQueryMode;
  TerminalDevice->SimpleTextOutput.SetMode            = TerminalConOutSetMode;
  TerminalDevice->SimpleTextOutput.SetAttribute       = TerminalConOutSetAttribute;
  TerminalDevice->SimpleTextOutput.ClearScreen        = TerminalConOutClearScreen;
  TerminalDevice->SimpleTextOutput.SetCursorPosition  = TerminalConOutSetCursorPosition;
  TerminalDevice->SimpleTextOutput.EnableCursor       = TerminalConOutEnableCursor;
  TerminalDevice->SimpleTextOutput.Mode               = &TerminalDevice->SimpleTextOutputMode;

  TerminalDevice->SimpleTextOutputMode.MaxMode        = 3;
  //
  // For terminal devices, cursor is always visible
  //
  TerminalDevice->SimpleTextOutputMode.CursorVisible  = TRUE;
  Status = TerminalDevice->SimpleTextOutput.SetAttribute (
                                                      &TerminalDevice->SimpleTextOutput,
                                                      EFI_TEXT_ATTR (EFI_LIGHTGRAY, EFI_BLACK)
                                                      );
  if (EFI_ERROR (Status)) {
    goto ReportError;
  }

  Status = TerminalDevice->SimpleTextOutput.Reset (
                                              &TerminalDevice->SimpleTextOutput,
                                              FALSE
                                              );
  if (EFI_ERROR (Status)) {
    goto ReportError;
  }

  Status = TerminalDevice->SimpleTextOutput.SetMode (
                                              &TerminalDevice->SimpleTextOutput,
                                              0
                                              );
  if (EFI_ERROR (Status)) {
    goto ReportError;
  }

  Status = TerminalDevice->SimpleTextOutput.EnableCursor (
                                              &TerminalDevice->SimpleTextOutput,
                                              TRUE
                                              );
  if (EFI_ERROR (Status)) {
    goto ReportError;
  }

  Status = gBS->CreateEvent (
                  EVT_TIMER,
                  TPL_CALLBACK,
                  NULL,
                  NULL,
                  &TerminalDevice->TwoSecondTimeOut
                  );

  //
  // Build the component name for the child device
  //
  TerminalDevice->ControllerNameTable = NULL;
  switch (TerminalDevice->TerminalType) {
  case PcAnsiType:
    AddUnicodeString2 (
      "eng",
      gTerminalComponentName.SupportedLanguages,
      &TerminalDevice->ControllerNameTable,
      (CHAR16 *)L"PC-ANSI Serial Console",
      TRUE
      );
    AddUnicodeString2 (
      "en",
      gTerminalComponentName2.SupportedLanguages,
      &TerminalDevice->ControllerNameTable,
      (CHAR16 *)L"PC-ANSI Serial Console",
      FALSE
      );

    break;

  case VT100Type:
    AddUnicodeString2 (
      "eng",
      gTerminalComponentName.SupportedLanguages,
      &TerminalDevice->ControllerNameTable,
      (CHAR16 *)L"VT-100 Serial Console",
      TRUE
      );
    AddUnicodeString2 (
      "en",
      gTerminalComponentName2.SupportedLanguages,
      &TerminalDevice->ControllerNameTable,
      (CHAR16 *)L"VT-100 Serial Console",
      FALSE
      );

    break;

  case VT100PlusType:
    AddUnicodeString2 (
      "eng",
      gTerminalComponentName.SupportedLanguages,
      &TerminalDevice->ControllerNameTable,
      (CHAR16 *)L"VT-100+ Serial Console",
      TRUE
      );
    AddUnicodeString2 (
      "en",
      gTerminalComponentName2.SupportedLanguages,
      &TerminalDevice->ControllerNameTable,
      (CHAR16 *)L"VT-100+ Serial Console",
      FALSE
      );

    break;

  case VTUTF8Type:
    AddUnicodeString2 (
      "eng",
      gTerminalComponentName.SupportedLanguages,
      &TerminalDevice->ControllerNameTable,
      (CHAR16 *)L"VT-UTF8 Serial Console",
      TRUE
      );
    AddUnicodeString2 (
      "en",
      gTerminalComponentName2.SupportedLanguages,
      &TerminalDevice->ControllerNameTable,
      (CHAR16 *)L"VT-UTF8 Serial Console",
      FALSE
      );

    break;
  }
  //
  // Install protocol interfaces for the serial device.
  //
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &TerminalDevice->Handle,
                  &gEfiDevicePathProtocolGuid,
                  TerminalDevice->DevicePath,
                  &gEfiSimpleTextInProtocolGuid,
                  &TerminalDevice->SimpleInput,
                  &gEfiSimpleTextInputExProtocolGuid,
                  &TerminalDevice->SimpleInputEx,
                  &gEfiSimpleTextOutProtocolGuid,
                  &TerminalDevice->SimpleTextOutput,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    goto Error;
  }
  //
  // if the serial device is a hot plug device, attaches the HotPlugGuid
  // onto the terminal device handle.
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiHotPlugDeviceGuid,
                  NULL,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &TerminalDevice->Handle,
                    &gEfiHotPlugDeviceGuid,
                    NULL,
                    NULL
                    );
  }
  //
  // Register the Parent-Child relationship via
  // EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.
  //
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiSerialIoProtocolGuid,
                  (VOID **) &TerminalDevice->SerialIo,
                  This->DriverBindingHandle,
                  TerminalDevice->Handle,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    goto Error;
  }

  if (DefaultNode != NULL) {
    FreePool (DefaultNode);
  }

  return EFI_SUCCESS;

ReportError:
  //
  // Report error code before exiting
  //
  REPORT_STATUS_CODE_WITH_DEVICE_PATH (
    EFI_ERROR_CODE | EFI_ERROR_MINOR,
    PcdGet32 (PcdStatusCodeValueRemoteConsoleError),
    DevicePath
    );

Error:
  //
  // Use the Stop() function to free all resources allocated in Start()
  //
  if (TerminalDevice != NULL) {

    if (TerminalDevice->Handle != NULL) {
      This->Stop (This, Controller, 1, &TerminalDevice->Handle);
    } else {

      if (TerminalDevice->TwoSecondTimeOut != NULL) {
        gBS->CloseEvent (TerminalDevice->TwoSecondTimeOut);
      }

      if (TerminalDevice->SimpleInput.WaitForKey != NULL) {
        gBS->CloseEvent (TerminalDevice->SimpleInput.WaitForKey);
      }

      if (TerminalDevice->SimpleInputEx.WaitForKeyEx != NULL) {
        gBS->CloseEvent (TerminalDevice->SimpleInputEx.WaitForKeyEx);
      }

      TerminalFreeNotifyList (&TerminalDevice->NotifyList);

      if (TerminalDevice->ControllerNameTable != NULL) {
        FreeUnicodeStringTable (TerminalDevice->ControllerNameTable);
      }

      if (TerminalDevice->DevicePath != NULL) {
        FreePool (TerminalDevice->DevicePath);
      }

      FreePool (TerminalDevice);
    }
  }

  if (DefaultNode != NULL) {
    FreePool (DefaultNode);
  }

  This->Stop (This, Controller, 0, NULL);

  return Status;
}

EFI_STATUS
EFIAPI
TerminalDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL   *This,
  IN  EFI_HANDLE                    Controller,
  IN  UINTN                         NumberOfChildren,
  IN  EFI_HANDLE                    *ChildHandleBuffer
  )
/*++

  Routine Description:

    Stop a device controller.

  Arguments:

    This              - A pointer to the EFI_DRIVER_BINDING_PROTOCOL instance.
    Controller        - A handle to the device being stopped.
    NumberOfChildren  - The number of child device handles in ChildHandleBuffer.
    ChildHandleBuffer - An array of child handles to be freed.

  Returns:

    EFI_SUCCESS      - Operation successful.
    EFI_DEVICE_ERROR - Devices error.

--*/
{
  EFI_STATUS                       Status;
  UINTN                            Index;
  BOOLEAN                          AllChildrenStopped;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *SimpleTextOutput;
  TERMINAL_DEV                     *TerminalDevice;
  EFI_DEVICE_PATH_PROTOCOL         *ParentDevicePath;
  EFI_SERIAL_IO_PROTOCOL           *SerialIo;
  EFI_DEVICE_PATH_PROTOCOL         *DevicePath;

  Status = gBS->HandleProtocol (
                  Controller,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &DevicePath
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Complete all outstanding transactions to Controller.
  // Don't allow any new transaction to Controller to be started.
  //
  if (NumberOfChildren == 0) {
    //
    // Close the bus driver
    //
    Status = gBS->OpenProtocol (
                    Controller,
                    &gEfiCallerIdGuid,
                    (VOID **) &ParentDevicePath,
                    This->DriverBindingHandle,
                    Controller,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (!EFI_ERROR (Status)) {
      //
      // Remove Parent Device Path from
      // the Console Device Environment Variables
      //
      TerminalRemoveConsoleDevVariable (L"ConInDev", ParentDevicePath);
      TerminalRemoveConsoleDevVariable (L"ConOutDev", ParentDevicePath);
      TerminalRemoveConsoleDevVariable (L"ErrOutDev", ParentDevicePath);

      //
      // Uninstall the Terminal Driver's GUID Tag from the Serial controller
      //
      Status = gBS->UninstallMultipleProtocolInterfaces (
                      Controller,
                      &gEfiCallerIdGuid,
                      ParentDevicePath,
                      NULL
                      );

      //
      // Free the ParentDevicePath that was duplicated in Start()
      //
      if (!EFI_ERROR (Status)) {
        FreePool (ParentDevicePath);
      }
    }

    gBS->CloseProtocol (
          Controller,
          &gEfiSerialIoProtocolGuid,
          This->DriverBindingHandle,
          Controller
          );

    gBS->CloseProtocol (
          Controller,
          &gEfiDevicePathProtocolGuid,
          This->DriverBindingHandle,
          Controller
          );

    return EFI_SUCCESS;
  }

  AllChildrenStopped = TRUE;

  for (Index = 0; Index < NumberOfChildren; Index++) {

    Status = gBS->OpenProtocol (
                    ChildHandleBuffer[Index],
                    &gEfiSimpleTextOutProtocolGuid,
                    (VOID **) &SimpleTextOutput,
                    This->DriverBindingHandle,
                    ChildHandleBuffer[Index],
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (!EFI_ERROR (Status)) {

      TerminalDevice = TERMINAL_CON_OUT_DEV_FROM_THIS (SimpleTextOutput);

      gBS->CloseProtocol (
            Controller,
            &gEfiSerialIoProtocolGuid,
            This->DriverBindingHandle,
            ChildHandleBuffer[Index]
            );

      Status = gBS->UninstallMultipleProtocolInterfaces (
                      ChildHandleBuffer[Index],
                      &gEfiSimpleTextInProtocolGuid,
                      &TerminalDevice->SimpleInput,
                      &gEfiSimpleTextInputExProtocolGuid,
                      &TerminalDevice->SimpleInputEx,
                      &gEfiSimpleTextOutProtocolGuid,
                      &TerminalDevice->SimpleTextOutput,
                      &gEfiDevicePathProtocolGuid,
                      TerminalDevice->DevicePath,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        gBS->OpenProtocol (
              Controller,
              &gEfiSerialIoProtocolGuid,
              (VOID **) &SerialIo,
              This->DriverBindingHandle,
              ChildHandleBuffer[Index],
              EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
              );
      } else {

        if (TerminalDevice->ControllerNameTable != NULL) {
          FreeUnicodeStringTable (TerminalDevice->ControllerNameTable);
        }

        Status = gBS->OpenProtocol (
                        ChildHandleBuffer[Index],
                        &gEfiHotPlugDeviceGuid,
                        NULL,
                        This->DriverBindingHandle,
                        Controller,
                        EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                        );
        if (!EFI_ERROR (Status)) {
          Status = gBS->UninstallMultipleProtocolInterfaces (
                          ChildHandleBuffer[Index],
                          &gEfiHotPlugDeviceGuid,
                          NULL,
                          NULL
                          );
        } else {
          Status = EFI_SUCCESS;
        }

        gBS->CloseEvent (TerminalDevice->TwoSecondTimeOut);
        gBS->CloseEvent (TerminalDevice->SimpleInput.WaitForKey);
        gBS->CloseEvent (TerminalDevice->SimpleInputEx.WaitForKeyEx);
        TerminalFreeNotifyList (&TerminalDevice->NotifyList);
        FreePool (TerminalDevice->DevicePath);
        FreePool (TerminalDevice);
      }
    }

    if (EFI_ERROR (Status)) {
      AllChildrenStopped = FALSE;
    }
  }

  if (!AllChildrenStopped) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
TerminalFreeNotifyList (
  IN OUT LIST_ENTRY           *ListHead
  )
/*++

Routine Description:

Arguments:

  ListHead   - The list head

Returns:

  EFI_SUCCESS           - Free the notify list successfully
  EFI_INVALID_PARAMETER - ListHead is invalid.

--*/
{
  TERMINAL_CONSOLE_IN_EX_NOTIFY *NotifyNode;

  if (ListHead == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  while (!IsListEmpty (ListHead)) {
    NotifyNode = CR (
                   ListHead->ForwardLink,
                   TERMINAL_CONSOLE_IN_EX_NOTIFY,
                   NotifyEntry,
                   TERMINAL_CONSOLE_IN_EX_NOTIFY_SIGNATURE
                   );
    RemoveEntryList (ListHead->ForwardLink);
    gBS->FreePool (NotifyNode);
  }

  return EFI_SUCCESS;
}



VOID
TerminalUpdateConsoleDevVariable (
  IN CHAR16                    *VariableName,
  IN EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath
  )
{
  EFI_STATUS                Status;
  UINTN                     VariableSize;
  UINT8                     TerminalType;
  EFI_DEVICE_PATH_PROTOCOL  *Variable;
  EFI_DEVICE_PATH_PROTOCOL  *NewVariable;
  EFI_DEVICE_PATH_PROTOCOL  *TempDevicePath;

  Variable = NULL;

  //
  // Get global variable and its size according to the name given.
  //
  Variable = TerminalGetVariableAndSize (
              VariableName,
              &gEfiGlobalVariableGuid,
              &VariableSize
              );
  //
  // Append terminal device path onto the variable.
  //
  for (TerminalType = PcAnsiType; TerminalType <= VTUTF8Type; TerminalType++) {
    SetTerminalDevicePath (TerminalType, ParentDevicePath, &TempDevicePath);
    NewVariable = AppendDevicePathInstance (Variable, TempDevicePath);
    if (Variable != NULL) {
      FreePool (Variable);
    }

    if (TempDevicePath != NULL) {
      FreePool (TempDevicePath);
    }

    Variable = NewVariable;
  }

  VariableSize = GetDevicePathSize (Variable);

  Status = gRT->SetVariable (
                  VariableName,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  VariableSize,
                  Variable
                  );
  ASSERT_EFI_ERROR (Status);
  FreePool (Variable);

  return ;
}

VOID
TerminalRemoveConsoleDevVariable (
  IN CHAR16                    *VariableName,
  IN EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath
  )
/*++

  Routine Description:

    Remove console device variable.

  Arguments:

    VariableName     - A pointer to the variable name.
    ParentDevicePath - A pointer to the parent device path.

  Returns:

--*/
{
  EFI_STATUS                Status;
  BOOLEAN                   FoundOne;
  BOOLEAN                   Match;
  UINTN                     VariableSize;
  UINTN                     InstanceSize;
  UINT8                     TerminalType;
  EFI_DEVICE_PATH_PROTOCOL  *Instance;
  EFI_DEVICE_PATH_PROTOCOL  *Variable;
  EFI_DEVICE_PATH_PROTOCOL  *OriginalVariable;
  EFI_DEVICE_PATH_PROTOCOL  *NewVariable;
  EFI_DEVICE_PATH_PROTOCOL  *SavedNewVariable;
  EFI_DEVICE_PATH_PROTOCOL  *TempDevicePath;

  Variable  = NULL;
  Instance  = NULL;

  //
  // Get global variable and its size according to the name given.
  //
  Variable = TerminalGetVariableAndSize (
              VariableName,
              &gEfiGlobalVariableGuid,
              &VariableSize
              );
  if (Variable == NULL) {
    return ;
  }

  FoundOne          = FALSE;
  OriginalVariable  = Variable;
  NewVariable       = NULL;

  //
  // Get first device path instance from Variable
  //
  Instance = GetNextDevicePathInstance (&Variable, &InstanceSize);
  if (Instance == NULL) {
    FreePool (OriginalVariable);
    return ;
  }
  //
  // Loop through all the device path instances of Variable
  //
  do {
    //
    // Loop through all the terminal types that this driver supports
    //
    Match = FALSE;
    for (TerminalType = PcAnsiType; TerminalType <= VTUTF8Type; TerminalType++) {

      SetTerminalDevicePath (TerminalType, ParentDevicePath, &TempDevicePath);

      //
      // Compare the genterated device path to the current device path instance
      //
      if (TempDevicePath != NULL) {
        if (CompareMem (Instance, TempDevicePath, InstanceSize) == 0) {
          Match     = TRUE;
          FoundOne  = TRUE;
        }

        FreePool (TempDevicePath);
      }
    }
    //
    // If a match was not found, then keep the current device path instance
    //
    if (!Match) {
      SavedNewVariable  = NewVariable;
      NewVariable       = AppendDevicePathInstance (NewVariable, Instance);
      if (SavedNewVariable != NULL) {
        FreePool (SavedNewVariable);
      }
    }
    //
    // Get next device path instance from Variable
    //
    FreePool (Instance);
    Instance = GetNextDevicePathInstance (&Variable, &InstanceSize);
  } while (Instance != NULL);

  FreePool (OriginalVariable);

  if (FoundOne) {
    VariableSize = GetDevicePathSize (NewVariable);

    Status = gRT->SetVariable (
                    VariableName,
                    &gEfiGlobalVariableGuid,
                    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                    VariableSize,
                    NewVariable
                    );
    ASSERT_EFI_ERROR (Status);
  }

  if (NewVariable != NULL) {
    FreePool (NewVariable);
  }

  return ;
}

VOID *
TerminalGetVariableAndSize (
  IN  CHAR16              *Name,
  IN  EFI_GUID            *VendorGuid,
  OUT UINTN               *VariableSize
  )
/*++

Routine Description:
  Read the EFI variable (VendorGuid/Name) and return a dynamically allocated
  buffer, and the size of the buffer. On failure return NULL.

Arguments:
  Name       - String part of EFI variable name

  VendorGuid - GUID part of EFI variable name

  VariableSize - Returns the size of the EFI variable that was read

Returns:
  Dynamically allocated memory that contains a copy of the EFI variable.
  Caller is repsoncible freeing the buffer.

  NULL - Variable was not read

--*/
{
  EFI_STATUS  Status;
  UINTN       BufferSize;
  VOID        *Buffer;

  Buffer = NULL;

  //
  // Pass in a small size buffer to find the actual variable size.
  //
  BufferSize  = 1;
  Buffer      = AllocatePool (BufferSize);
  if (Buffer == NULL) {
    *VariableSize = 0;
    return NULL;
  }

  Status = gRT->GetVariable (Name, VendorGuid, NULL, &BufferSize, Buffer);

  if (Status == EFI_SUCCESS) {
    *VariableSize = BufferSize;
    return Buffer;

  } else if (Status == EFI_BUFFER_TOO_SMALL) {
    //
    // Allocate the buffer to return
    //
    FreePool (Buffer);
    Buffer = AllocatePool (BufferSize);
    if (Buffer == NULL) {
      *VariableSize = 0;
      return NULL;
    }
    //
    // Read variable into the allocated buffer.
    //
    Status = gRT->GetVariable (Name, VendorGuid, NULL, &BufferSize, Buffer);
    if (EFI_ERROR (Status)) {
      BufferSize = 0;
      FreePool (Buffer);
      Buffer = NULL;
    }
  } else {
    //
    // Variable not found or other errors met.
    //
    BufferSize = 0;
    FreePool (Buffer);
    Buffer = NULL;
  }

  *VariableSize = BufferSize;
  return Buffer;
}

EFI_STATUS
SetTerminalDevicePath (
  IN  UINT8                       TerminalType,
  IN  EFI_DEVICE_PATH_PROTOCOL    *ParentDevicePath,
  OUT EFI_DEVICE_PATH_PROTOCOL    **TerminalDevicePath
  )
{
  VENDOR_DEVICE_PATH  Node;

  *TerminalDevicePath = NULL;
  Node.Header.Type    = MESSAGING_DEVICE_PATH;
  Node.Header.SubType = MSG_VENDOR_DP;

  //
  // generate terminal device path node according to terminal type.
  //
  switch (TerminalType) {

  case PcAnsiType:
    CopyMem (
      &Node.Guid,
      &gEfiPcAnsiGuid,
      sizeof (EFI_GUID)
      );
    break;

  case VT100Type:
    CopyMem (
      &Node.Guid,
      &gEfiVT100Guid,
      sizeof (EFI_GUID)
      );
    break;

  case VT100PlusType:
    CopyMem (
      &Node.Guid,
      &gEfiVT100PlusGuid,
      sizeof (EFI_GUID)
      );
    break;

  case VTUTF8Type:
    CopyMem (
      &Node.Guid,
      &gEfiVTUTF8Guid,
      sizeof (EFI_GUID)
      );
    break;

  default:
    return EFI_UNSUPPORTED;
    break;
  }

  SetDevicePathNodeLength (
    &Node.Header,
    sizeof (VENDOR_DEVICE_PATH)
    );
  //
  // append the terminal node onto parent device path
  // to generate a complete terminal device path.
  //
  *TerminalDevicePath = AppendDevicePathNode (
                          ParentDevicePath,
                          (EFI_DEVICE_PATH_PROTOCOL *) &Node
                          );
  if (*TerminalDevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}

VOID
InitializeRawFiFo (
  IN  TERMINAL_DEV  *TerminalDevice
  )
{
  //
  // Make the raw fifo empty.
  //
  TerminalDevice->RawFiFo.Head = TerminalDevice->RawFiFo.Tail;
}

VOID
InitializeUnicodeFiFo (
  IN  TERMINAL_DEV  *TerminalDevice
  )
{
  //
  // Make the unicode fifo empty
  //
  TerminalDevice->UnicodeFiFo.Head = TerminalDevice->UnicodeFiFo.Tail;
}

VOID
InitializeEfiKeyFiFo (
  IN  TERMINAL_DEV  *TerminalDevice
  )
{
  //
  // Make the efi key fifo empty
  //
  TerminalDevice->EfiKeyFiFo.Head = TerminalDevice->EfiKeyFiFo.Tail;
}


/**
  The user Entry Point for module Terminal. The user code starts with this function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
InitializeTerminal(
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
  EFI_STATUS              Status;

  //
  // Install driver model protocol(s).
  //
  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gTerminalDriverBinding,
             ImageHandle,
             &gTerminalComponentName,
             &gTerminalComponentName2
             );
  ASSERT_EFI_ERROR (Status);


  return Status;
}
