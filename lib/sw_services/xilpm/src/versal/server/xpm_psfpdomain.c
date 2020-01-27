/******************************************************************************
*
* Copyright (C) 2019-2020 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
*
*
******************************************************************************/

#include "xpm_common.h"
#include "xpm_psfpdomain.h"
#include "xpm_bisr.h"
#include "xpm_regs.h"
#include "xpm_psm.h"
#include "xpm_device.h"

static XStatus FpdInitStart(u32 *Args, u32 NumOfArgs)
{
	XStatus Status = XST_FAILURE;
	u32 Payload[PAYLOAD_ARG_CNT] = {0};

	(void)Args;
	(void)NumOfArgs;

	/* Check vccint_fpd first to make sure power is on */
	if (XST_SUCCESS != XPmPower_CheckPower(PMC_GLOBAL_PWR_SUPPLY_STATUS_VCCINT_FPD_MASK)) {
		/* TODO: Request PMC to power up VCCINT_FP rail and wait for the acknowledgement.*/
		Status = XST_SUCCESS;
		goto done;
	}

	if (XPmPsm_FwIsPresent() != TRUE) {
		Status = XST_NOT_ENABLED;
		goto done;
	}

	Payload[0] = PSM_API_FPD_HOUSECLEAN;
	Payload[1] = (u32)FUNC_INIT_START;

	Status = XPm_IpiSend(PSM_IPI_INT_MASK, Payload);
	if (XST_SUCCESS != Status) {
		goto done;
	}

	Status = XPm_IpiReadStatus(PSM_IPI_INT_MASK);
	if (XST_SUCCESS != Status) {
		goto done;
	}
	/* Release POR for PS-FPD */
	Status = XPmReset_AssertbyId(PM_RST_FPD_POR, (u32)PM_RESET_ACTION_RELEASE);
done:
	return Status;
}

static XStatus FpdInitFinish(u32 *Args, u32 NumOfArgs)
{
	XStatus Status = XST_FAILURE;

	(void)Args;
	(void)NumOfArgs;

	Status = XST_SUCCESS;

	return Status;
}

static XStatus FpdHcComplete(u32 *Args, u32 NumOfArgs)
{
	XStatus Status = XST_FAILURE;
	u32 Payload[PAYLOAD_ARG_CNT] = {0};

	(void)Args;
	(void)NumOfArgs;

	/* Release SRST for PS-FPD - in case Bisr and Mbist are skipped */
	Status = XPmReset_AssertbyId(PM_RST_FPD, (u32)PM_RESET_ACTION_RELEASE);

	Payload[0] = PSM_API_FPD_HOUSECLEAN;
	Payload[1] = (u32)FUNC_INIT_FINISH;

	Status = XPm_IpiSend(PSM_IPI_INT_MASK, Payload);
	if (XST_SUCCESS != Status) {
		goto done;
	}

	Status = XPm_IpiReadStatus(PSM_IPI_INT_MASK);
	if (Status != XST_SUCCESS)
		goto done;

	/* Remove FPD SOC domains isolation */
	Status = XPmDomainIso_Control((u32)XPM_NODEIDX_ISO_FPD_SOC, FALSE_VALUE);
	if (Status != XST_SUCCESS)
		goto done;

	/* Copy sysmon data */
	XPmPowerDomain_ApplyAmsTrim(SysmonAddresses[XPM_NODEIDX_MONITOR_SYSMON_PS_FPD], PM_POWER_FPD, 0);
done:
	return Status;
}

static XStatus FpdScanClear(u32 *Args, u32 NumOfArgs)
{
	XStatus Status = XST_FAILURE;
	XPm_Psm *Psm;

	(void)Args;
	(void)NumOfArgs;

        if (PLATFORM_VERSION_SILICON != Platform) {
                Status = XST_SUCCESS;
                goto done;
        }

	Psm = (XPm_Psm *)XPmDevice_GetById(PM_DEV_PSM_PROC);;
	if (NULL == Psm) {
		Status = XST_FAILURE;
		goto done;
	}

        /* Trigger scan clear */
        PmRmw32(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_SCAN_CLEAR_FPD_OFFSET,
		PSM_GLOBAL_SCAN_CLEAR_TRIGGER, PSM_GLOBAL_SCAN_CLEAR_TRIGGER);

        Status = XPm_PollForMask(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_SCAN_CLEAR_FPD_OFFSET,
				 PSM_GLOBAL_SCAN_CLEAR_DONE_STATUS, 0x10000U);
        if (XST_SUCCESS != Status) {
                goto done;
        }

        Status = XPm_PollForMask(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_SCAN_CLEAR_FPD_OFFSET,
				 PSM_GLOBAL_SCAN_CLEAR_PASS_STATUS, 0x10000U);
        if (XST_SUCCESS != Status) {
                goto done;
        }

	/* Unwrite trigger bits */
        PmRmw32(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_SCAN_CLEAR_FPD_OFFSET,
		PSM_GLOBAL_SCAN_CLEAR_TRIGGER, 0);

done:
        return Status;
}

static XStatus FpdBisr(u32 *Args, u32 NumOfArgs)
{
	XStatus Status = XST_FAILURE;
	u32 Payload[PAYLOAD_ARG_CNT] = {0};

	(void)Args;
	(void)NumOfArgs;

	/* Release SRST for PS-FPD */
	Status = XPmReset_AssertbyId(PM_RST_FPD, (u32)PM_RESET_ACTION_RELEASE);

	/* Call PSM to execute pre bisr requirements */
	Payload[0] = PSM_API_FPD_HOUSECLEAN;
	Payload[1] = (u32)FUNC_BISR;

	Status = XPm_IpiSend(PSM_IPI_INT_MASK, Payload);
	if (XST_SUCCESS != Status) {
		goto done;
	}

	Status = XPm_IpiReadStatus(PSM_IPI_INT_MASK);
	if (XST_SUCCESS != Status) {
		goto done;
	}

	/* Trigger Bisr repair */
	Status = XPmBisr_Repair(FPD_TAG_ID);

done:
	return Status;
}

static XStatus FpdMbistClear(u32 *Args, u32 NumOfArgs)
{
        XStatus Status = XST_FAILURE;
        u32 Payload[PAYLOAD_ARG_CNT] = {0};
	XPm_Psm *Psm;

	(void)Args;
	(void)NumOfArgs;

	Psm = (XPm_Psm *)XPmDevice_GetById(PM_DEV_PSM_PROC);;
	if (NULL == Psm) {
		Status = XST_FAILURE;
		goto done;
	}

	/* Release SRST for PS-FPD */
	Status = XPmReset_AssertbyId(PM_RST_FPD, (u32)PM_RESET_ACTION_RELEASE);

        Payload[0] = PSM_API_FPD_HOUSECLEAN;
        Payload[1] = (u32)FUNC_MBIST_CLEAR;

        Status = XPm_IpiSend(PSM_IPI_INT_MASK, Payload);
        if (XST_SUCCESS != Status) {
                goto done;
        }

        Status = XPm_IpiReadStatus(PSM_IPI_INT_MASK);

        if (PLATFORM_VERSION_SILICON != Platform) {
                Status = XST_SUCCESS;
                goto done;
        }

        PmRmw32(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_MBIST_RST_OFFSET,
		PSM_GLOBAL_MBIST_RST_FPD_MASK, PSM_GLOBAL_MBIST_RST_FPD_MASK);

        PmRmw32(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_MBIST_SETUP_OFFSET,
		PSM_GLOBAL_MBIST_SETUP_FPD_MASK, PSM_GLOBAL_MBIST_SETUP_FPD_MASK);

        PmRmw32(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_MBIST_PG_EN_OFFSET,
		PSM_GLOBAL_MBIST_PG_EN_FPD_MASK, PSM_GLOBAL_MBIST_PG_EN_FPD_MASK);

        Status = XPm_PollForMask(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_MBIST_DONE_OFFSET,
				 PSM_GLOBAL_MBIST_DONE_FPD_MASK, 0x10000U);
        if (XST_SUCCESS != Status) {
                goto done;
        }

        if (PSM_GLOBAL_MBIST_GO_FPD_MASK !=
            (XPm_In32(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_MBIST_GO_OFFSET) &
             PSM_GLOBAL_MBIST_GO_FPD_MASK)) {
                Status = XST_FAILURE;
        }

	/* Unwrite trigger bits */
	PmRmw32(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_MBIST_RST_OFFSET,
		PSM_GLOBAL_MBIST_RST_FPD_MASK, 0);

        PmRmw32(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_MBIST_SETUP_OFFSET,
		PSM_GLOBAL_MBIST_SETUP_FPD_MASK, 0);

        PmRmw32(Psm->PsmGlobalBaseAddr + PSM_GLOBAL_MBIST_PG_EN_OFFSET,
		PSM_GLOBAL_MBIST_PG_EN_FPD_MASK, 0);

	/* EDT-997247: Mem clear introduces apu gic ecc error,
	so pulse gic reset as a work around to fix it */
	XPmReset_AssertbyId(PM_RST_ACPU_GIC, (u32)PM_RESET_ACTION_ASSERT);
	XPmReset_AssertbyId(PM_RST_ACPU_GIC, (u32)PM_RESET_ACTION_RELEASE);

done:
        return Status;
}

struct XPm_PowerDomainOps FpdOps = {
	.InitStart = FpdInitStart,
	.InitFinish = FpdInitFinish,
	.ScanClear = FpdScanClear,
	.Bisr = FpdBisr,
	.Mbist = FpdMbistClear,
	.HcComplete = FpdHcComplete,
};

XStatus XPmPsFpDomain_Init(XPm_PsFpDomain *PsFpd, u32 Id, u32 BaseAddress,
			   XPm_Power *Parent,  u32 *OtherBaseAddresses,
			   u32 OtherBaseAddressCnt)
{
	XStatus Status = XST_FAILURE;

	XPmPowerDomain_Init(&PsFpd->Domain, Id, BaseAddress, Parent, &FpdOps);

	/* Make sure enough base addresses are being passed */
	if (1 <= OtherBaseAddressCnt) {
		PsFpd->FpdSlcrBaseAddr = OtherBaseAddresses[0];
		Status = XST_SUCCESS;
	} else {
		Status = XST_FAILURE;
	}

	return Status;
}
