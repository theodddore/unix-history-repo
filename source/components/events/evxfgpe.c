/******************************************************************************
 *
 * Module Name: evxfgpe - External Interfaces for General Purpose Events (GPEs)
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */


#define __EVXFGPE_C__

#include "acpi.h"
#include "accommon.h"
#include "acevents.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_EVENTS
        ACPI_MODULE_NAME    ("evxfgpe")


#if (!ACPI_REDUCED_HARDWARE) /* Entire module */
/*******************************************************************************
 *
 * FUNCTION:    AcpiUpdateAllGpes
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Complete GPE initialization and enable all GPEs that have
 *              associated _Lxx or _Exx methods and are not pointed to by any
 *              device _PRW methods (this indicates that these GPEs are
 *              generally intended for system or device wakeup. Such GPEs
 *              have to be enabled directly when the devices whose _PRW
 *              methods point to them are set up for wakeup signaling.)
 *
 * NOTE: Should be called after any GPEs are added to the system. Primarily,
 * after the system _PRW methods have been run, but also after a GPE Block
 * Device has been added or if any new GPE methods have been added via a
 * dynamic table load.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUpdateAllGpes (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiUpdateAllGpes);


    Status = AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (AcpiGbl_AllGpesInitialized)
    {
        goto UnlockAndExit;
    }

    Status = AcpiEvWalkGpeList (AcpiEvInitializeGpeBlock, NULL);
    if (ACPI_SUCCESS (Status))
    {
        AcpiGbl_AllGpesInitialized = TRUE;
    }

UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_EVENTS);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiUpdateAllGpes)


/*******************************************************************************
 *
 * FUNCTION:    AcpiEnableGpe
 *
 * PARAMETERS:  GpeDevice           - Parent GPE Device. NULL for GPE0/GPE1
 *              GpeNumber           - GPE level within the GPE block
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Add a reference to a GPE. On the first reference, the GPE is
 *              hardware-enabled.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEnableGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber)
{
    ACPI_STATUS             Status = AE_BAD_PARAMETER;
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_CPU_FLAGS          Flags;


    ACPI_FUNCTION_TRACE (AcpiEnableGpe);


    Flags = AcpiOsAcquireLock (AcpiGbl_GpeLock);

    /* Ensure that we have a valid GPE number */

    GpeEventInfo = AcpiEvGetGpeEventInfo (GpeDevice, GpeNumber);
    if (GpeEventInfo)
    {
        Status = AcpiEvAddGpeReference (GpeEventInfo);
    }

    AcpiOsReleaseLock (AcpiGbl_GpeLock, Flags);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiEnableGpe)


/*******************************************************************************
 *
 * FUNCTION:    AcpiDisableGpe
 *
 * PARAMETERS:  GpeDevice           - Parent GPE Device. NULL for GPE0/GPE1
 *              GpeNumber           - GPE level within the GPE block
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove a reference to a GPE. When the last reference is
 *              removed, only then is the GPE disabled (for runtime GPEs), or
 *              the GPE mask bit disabled (for wake GPEs)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDisableGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber)
{
    ACPI_STATUS             Status = AE_BAD_PARAMETER;
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_CPU_FLAGS          Flags;


    ACPI_FUNCTION_TRACE (AcpiDisableGpe);


    Flags = AcpiOsAcquireLock (AcpiGbl_GpeLock);

    /* Ensure that we have a valid GPE number */

    GpeEventInfo = AcpiEvGetGpeEventInfo (GpeDevice, GpeNumber);
    if (GpeEventInfo)
    {
        Status = AcpiEvRemoveGpeReference (GpeEventInfo);
    }

    AcpiOsReleaseLock (AcpiGbl_GpeLock, Flags);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiDisableGpe)


/*******************************************************************************
 *
 * FUNCTION:    AcpiSetGpe
 *
 * PARAMETERS:  GpeDevice           - Parent GPE Device. NULL for GPE0/GPE1
 *              GpeNumber           - GPE level within the GPE block
 *              Action              - ACPI_GPE_ENABLE or ACPI_GPE_DISABLE
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enable or disable an individual GPE. This function bypasses
 *              the reference count mechanism used in the AcpiEnableGpe and
 *              AcpiDisableGpe interfaces -- and should be used with care.
 *
 * Note: Typically used to disable a runtime GPE for short period of time,
 * then re-enable it, without disturbing the existing reference counts. This
 * is useful, for example, in the Embedded Controller (EC) driver.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT8                   Action)
{
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_STATUS             Status;
    ACPI_CPU_FLAGS          Flags;


    ACPI_FUNCTION_TRACE (AcpiSetGpe);


    Flags = AcpiOsAcquireLock (AcpiGbl_GpeLock);

    /* Ensure that we have a valid GPE number */

    GpeEventInfo = AcpiEvGetGpeEventInfo (GpeDevice, GpeNumber);
    if (!GpeEventInfo)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /* Perform the action */

    switch (Action)
    {
    case ACPI_GPE_ENABLE:
        Status = AcpiEvEnableGpe (GpeEventInfo);
        break;

    case ACPI_GPE_DISABLE:
        Status = AcpiHwLowSetGpe (GpeEventInfo, ACPI_GPE_DISABLE);
        break;

    default:
        Status = AE_BAD_PARAMETER;
        break;
    }

UnlockAndExit:
    AcpiOsReleaseLock (AcpiGbl_GpeLock, Flags);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiSetGpe)


/*******************************************************************************
 *
 * FUNCTION:    AcpiSetupGpeForWake
 *
 * PARAMETERS:  WakeDevice          - Device associated with the GPE (via _PRW)
 *              GpeDevice           - Parent GPE Device. NULL for GPE0/GPE1
 *              GpeNumber           - GPE level within the GPE block
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Mark a GPE as having the ability to wake the system. This
 *              interface is intended to be used as the host executes the
 *              _PRW methods (Power Resources for Wake) in the system tables.
 *              Each _PRW appears under a Device Object (The WakeDevice), and
 *              contains the info for the wake GPE associated with the
 *              WakeDevice.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetupGpeForWake (
    ACPI_HANDLE             WakeDevice,
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber)
{
    ACPI_STATUS             Status;
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_NAMESPACE_NODE     *DeviceNode;
    ACPI_GPE_NOTIFY_INFO    *Notify;
    ACPI_GPE_NOTIFY_INFO    *NewNotify;
    ACPI_CPU_FLAGS          Flags;


    ACPI_FUNCTION_TRACE (AcpiSetupGpeForWake);


    /* Parameter Validation */

    if (!WakeDevice)
    {
        /*
         * By forcing WakeDevice to be valid, we automatically enable the
         * implicit notify feature on all hosts.
         */
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Handle root object case */

    if (WakeDevice == ACPI_ROOT_OBJECT)
    {
        DeviceNode = AcpiGbl_RootNode;
    }
    else
    {
        DeviceNode = ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, WakeDevice);
    }

    /* Validate WakeDevice is of type Device */

    if (DeviceNode->Type != ACPI_TYPE_DEVICE)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * Allocate a new notify object up front, in case it is needed.
     * Memory allocation while holding a spinlock is a big no-no
     * on some hosts.
     */
    NewNotify = ACPI_ALLOCATE_ZEROED (sizeof (ACPI_GPE_NOTIFY_INFO));
    if (!NewNotify)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Flags = AcpiOsAcquireLock (AcpiGbl_GpeLock);

    /* Ensure that we have a valid GPE number */

    GpeEventInfo = AcpiEvGetGpeEventInfo (GpeDevice, GpeNumber);
    if (!GpeEventInfo)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /*
     * If there is no method or handler for this GPE, then the
     * WakeDevice will be notified whenever this GPE fires. This is
     * known as an "implicit notify". Note: The GPE is assumed to be
     * level-triggered (for windows compatibility).
     */
    if ((GpeEventInfo->Flags & ACPI_GPE_DISPATCH_MASK) ==
            ACPI_GPE_DISPATCH_NONE)
    {
        /*
         * This is the first device for implicit notify on this GPE.
         * Just set the flags here, and enter the NOTIFY block below.
         */
        GpeEventInfo->Flags =
            (ACPI_GPE_DISPATCH_NOTIFY | ACPI_GPE_LEVEL_TRIGGERED);
    }

    /*
     * If we already have an implicit notify on this GPE, add
     * this device to the notify list.
     */
    if ((GpeEventInfo->Flags & ACPI_GPE_DISPATCH_MASK) ==
            ACPI_GPE_DISPATCH_NOTIFY)
    {
        /* Ensure that the device is not already in the list */

        Notify = GpeEventInfo->Dispatch.NotifyList;
        while (Notify)
        {
            if (Notify->DeviceNode == DeviceNode)
            {
                Status = AE_ALREADY_EXISTS;
                goto UnlockAndExit;
            }
            Notify = Notify->Next;
        }

        /* Add this device to the notify list for this GPE */

        NewNotify->DeviceNode = DeviceNode;
        NewNotify->Next = GpeEventInfo->Dispatch.NotifyList;
        GpeEventInfo->Dispatch.NotifyList = NewNotify;
        NewNotify = NULL;
    }

    /* Mark the GPE as a possible wake event */

    GpeEventInfo->Flags |= ACPI_GPE_CAN_WAKE;
    Status = AE_OK;


UnlockAndExit:
    AcpiOsReleaseLock (AcpiGbl_GpeLock, Flags);

    /* Delete the notify object if it was not used above */

    if (NewNotify)
    {
        ACPI_FREE (NewNotify);
    }
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiSetupGpeForWake)


/*******************************************************************************
 *
 * FUNCTION:    AcpiSetGpeWakeMask
 *
 * PARAMETERS:  GpeDevice           - Parent GPE Device. NULL for GPE0/GPE1
 *              GpeNumber           - GPE level within the GPE block
 *              Action              - Enable or Disable
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Set or clear the GPE's wakeup enable mask bit. The GPE must
 *              already be marked as a WAKE GPE.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiSetGpeWakeMask (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT8                   Action)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_GPE_REGISTER_INFO  *GpeRegisterInfo;
    ACPI_CPU_FLAGS          Flags;
    UINT32                  RegisterBit;


    ACPI_FUNCTION_TRACE (AcpiSetGpeWakeMask);


    Flags = AcpiOsAcquireLock (AcpiGbl_GpeLock);

    /*
     * Ensure that we have a valid GPE number and that this GPE is in
     * fact a wake GPE
     */
    GpeEventInfo = AcpiEvGetGpeEventInfo (GpeDevice, GpeNumber);
    if (!GpeEventInfo)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    if (!(GpeEventInfo->Flags & ACPI_GPE_CAN_WAKE))
    {
        Status = AE_TYPE;
        goto UnlockAndExit;
    }

    GpeRegisterInfo = GpeEventInfo->RegisterInfo;
    if (!GpeRegisterInfo)
    {
        Status = AE_NOT_EXIST;
        goto UnlockAndExit;
    }

    RegisterBit = AcpiHwGetGpeRegisterBit (GpeEventInfo);

    /* Perform the action */

    switch (Action)
    {
    case ACPI_GPE_ENABLE:
        ACPI_SET_BIT (GpeRegisterInfo->EnableForWake, (UINT8) RegisterBit);
        break;

    case ACPI_GPE_DISABLE:
        ACPI_CLEAR_BIT (GpeRegisterInfo->EnableForWake, (UINT8) RegisterBit);
        break;

    default:
        ACPI_ERROR ((AE_INFO, "%u, Invalid action", Action));
        Status = AE_BAD_PARAMETER;
        break;
    }

UnlockAndExit:
    AcpiOsReleaseLock (AcpiGbl_GpeLock, Flags);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiSetGpeWakeMask)


/*******************************************************************************
 *
 * FUNCTION:    AcpiClearGpe
 *
 * PARAMETERS:  GpeDevice           - Parent GPE Device. NULL for GPE0/GPE1
 *              GpeNumber           - GPE level within the GPE block
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Clear an ACPI event (general purpose)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiClearGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_CPU_FLAGS          Flags;


    ACPI_FUNCTION_TRACE (AcpiClearGpe);


    Flags = AcpiOsAcquireLock (AcpiGbl_GpeLock);

    /* Ensure that we have a valid GPE number */

    GpeEventInfo = AcpiEvGetGpeEventInfo (GpeDevice, GpeNumber);
    if (!GpeEventInfo)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    Status = AcpiHwClearGpe (GpeEventInfo);

UnlockAndExit:
    AcpiOsReleaseLock (AcpiGbl_GpeLock, Flags);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiClearGpe)


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetGpeStatus
 *
 * PARAMETERS:  GpeDevice           - Parent GPE Device. NULL for GPE0/GPE1
 *              GpeNumber           - GPE level within the GPE block
 *              EventStatus         - Where the current status of the event
 *                                    will be returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get the current status of a GPE (signalled/not_signalled)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetGpeStatus (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    ACPI_EVENT_STATUS       *EventStatus)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_CPU_FLAGS          Flags;


    ACPI_FUNCTION_TRACE (AcpiGetGpeStatus);


    Flags = AcpiOsAcquireLock (AcpiGbl_GpeLock);

    /* Ensure that we have a valid GPE number */

    GpeEventInfo = AcpiEvGetGpeEventInfo (GpeDevice, GpeNumber);
    if (!GpeEventInfo)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /* Obtain status on the requested GPE number */

    Status = AcpiHwGetGpeStatus (GpeEventInfo, EventStatus);

UnlockAndExit:
    AcpiOsReleaseLock (AcpiGbl_GpeLock, Flags);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiGetGpeStatus)


/*******************************************************************************
 *
 * FUNCTION:    AcpiFinishGpe
 *
 * PARAMETERS:  GpeDevice           - Namespace node for the GPE Block
 *                                    (NULL for FADT defined GPEs)
 *              GpeNumber           - GPE level within the GPE block
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Clear and conditionally reenable a GPE. This completes the GPE
 *              processing. Intended for use by asynchronous host-installed
 *              GPE handlers. The GPE is only reenabled if the EnableForRun bit
 *              is set in the GPE info.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiFinishGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber)
{
    ACPI_GPE_EVENT_INFO     *GpeEventInfo;
    ACPI_STATUS             Status;
    ACPI_CPU_FLAGS          Flags;


    ACPI_FUNCTION_TRACE (AcpiFinishGpe);


    Flags = AcpiOsAcquireLock (AcpiGbl_GpeLock);

    /* Ensure that we have a valid GPE number */

    GpeEventInfo = AcpiEvGetGpeEventInfo (GpeDevice, GpeNumber);
    if (!GpeEventInfo)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    Status = AcpiEvFinishGpe (GpeEventInfo);

UnlockAndExit:
    AcpiOsReleaseLock (AcpiGbl_GpeLock, Flags);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiFinishGpe)


/******************************************************************************
 *
 * FUNCTION:    AcpiDisableAllGpes
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disable and clear all GPEs in all GPE blocks
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDisableAllGpes (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiDisableAllGpes);


    Status = AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiHwDisableAllGpes ();
    (void) AcpiUtReleaseMutex (ACPI_MTX_EVENTS);

    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiDisableAllGpes)


/******************************************************************************
 *
 * FUNCTION:    AcpiEnableAllRuntimeGpes
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enable all "runtime" GPEs, in all GPE blocks
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEnableAllRuntimeGpes (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiEnableAllRuntimeGpes);


    Status = AcpiUtAcquireMutex (ACPI_MTX_EVENTS);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiHwEnableAllRuntimeGpes ();
    (void) AcpiUtReleaseMutex (ACPI_MTX_EVENTS);

    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiEnableAllRuntimeGpes)


/*******************************************************************************
 *
 * FUNCTION:    AcpiInstallGpeBlock
 *
 * PARAMETERS:  GpeDevice           - Handle to the parent GPE Block Device
 *              GpeBlockAddress     - Address and SpaceID
 *              RegisterCount       - Number of GPE register pairs in the block
 *              InterruptNumber     - H/W interrupt for the block
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create and Install a block of GPE registers. The GPEs are not
 *              enabled here.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiInstallGpeBlock (
    ACPI_HANDLE             GpeDevice,
    ACPI_GENERIC_ADDRESS    *GpeBlockAddress,
    UINT32                  RegisterCount,
    UINT32                  InterruptNumber)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_GPE_BLOCK_INFO     *GpeBlock;


    ACPI_FUNCTION_TRACE (AcpiInstallGpeBlock);


    if ((!GpeDevice)       ||
        (!GpeBlockAddress) ||
        (!RegisterCount))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Node = AcpiNsValidateHandle (GpeDevice);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /*
     * For user-installed GPE Block Devices, the GpeBlockBaseNumber
     * is always zero
     */
    Status = AcpiEvCreateGpeBlock (Node, GpeBlockAddress, RegisterCount,
                0, InterruptNumber, &GpeBlock);
    if (ACPI_FAILURE (Status))
    {
        goto UnlockAndExit;
    }

    /* Install block in the DeviceObject attached to the node */

    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (!ObjDesc)
    {
        /*
         * No object, create a new one (Device nodes do not always have
         * an attached object)
         */
        ObjDesc = AcpiUtCreateInternalObject (ACPI_TYPE_DEVICE);
        if (!ObjDesc)
        {
            Status = AE_NO_MEMORY;
            goto UnlockAndExit;
        }

        Status = AcpiNsAttachObject (Node, ObjDesc, ACPI_TYPE_DEVICE);

        /* Remove local reference to the object */

        AcpiUtRemoveReference (ObjDesc);
        if (ACPI_FAILURE (Status))
        {
            goto UnlockAndExit;
        }
    }

    /* Now install the GPE block in the DeviceObject */

    ObjDesc->Device.GpeBlock = GpeBlock;


UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiInstallGpeBlock)


/*******************************************************************************
 *
 * FUNCTION:    AcpiRemoveGpeBlock
 *
 * PARAMETERS:  GpeDevice           - Handle to the parent GPE Block Device
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove a previously installed block of GPE registers
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRemoveGpeBlock (
    ACPI_HANDLE             GpeDevice)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;


    ACPI_FUNCTION_TRACE (AcpiRemoveGpeBlock);


    if (!GpeDevice)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Node = AcpiNsValidateHandle (GpeDevice);
    if (!Node)
    {
        Status = AE_BAD_PARAMETER;
        goto UnlockAndExit;
    }

    /* Get the DeviceObject attached to the node */

    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (!ObjDesc ||
        !ObjDesc->Device.GpeBlock)
    {
        return_ACPI_STATUS (AE_NULL_OBJECT);
    }

    /* Delete the GPE block (but not the DeviceObject) */

    Status = AcpiEvDeleteGpeBlock (ObjDesc->Device.GpeBlock);
    if (ACPI_SUCCESS (Status))
    {
        ObjDesc->Device.GpeBlock = NULL;
    }

UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    return_ACPI_STATUS (Status);
}

ACPI_EXPORT_SYMBOL (AcpiRemoveGpeBlock)


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetGpeDevice
 *
 * PARAMETERS:  Index               - System GPE index (0-CurrentGpeCount)
 *              GpeDevice           - Where the parent GPE Device is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Obtain the GPE device associated with the input index. A NULL
 *              gpe device indicates that the gpe number is contained in one of
 *              the FADT-defined gpe blocks. Otherwise, the GPE block device.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetGpeDevice (
    UINT32                  Index,
    ACPI_HANDLE             *GpeDevice)
{
    ACPI_GPE_DEVICE_INFO    Info;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (AcpiGetGpeDevice);


    if (!GpeDevice)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    if (Index >= AcpiCurrentGpeCount)
    {
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    /* Setup and walk the GPE list */

    Info.Index = Index;
    Info.Status = AE_NOT_EXIST;
    Info.GpeDevice = NULL;
    Info.NextBlockBaseIndex = 0;

    Status = AcpiEvWalkGpeList (AcpiEvGetGpeDevice, &Info);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    *GpeDevice = ACPI_CAST_PTR (ACPI_HANDLE, Info.GpeDevice);
    return_ACPI_STATUS (Info.Status);
}

ACPI_EXPORT_SYMBOL (AcpiGetGpeDevice)

#endif /* !ACPI_REDUCED_HARDWARE */
