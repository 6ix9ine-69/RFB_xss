#pragma once

#include "sal.h"

typedef struct _IMAGE_DELAYLOAD_DESCRIPTOR {
    union {
        DWORD AllAttributes;
        struct {
            DWORD RvaBased : 1;             // Delay load version 2
            DWORD ReservedAttributes : 31;
        } DUMMYSTRUCTNAME;
    } Attributes;

    DWORD DllNameRVA;                       // RVA to the name of the target library (NULL-terminate ASCII string)
    DWORD ModuleHandleRVA;                  // RVA to the HMODULE caching location (PHMODULE)
    DWORD ImportAddressTableRVA;            // RVA to the start of the IAT (PIMAGE_THUNK_DATA)
    DWORD ImportNameTableRVA;               // RVA to the start of the name table (PIMAGE_THUNK_DATA::AddressOfData)
    DWORD BoundImportAddressTableRVA;       // RVA to an optional bound IAT
    DWORD UnloadInformationTableRVA;        // RVA to an optional unload info table
    DWORD TimeDateStamp;                    // 0 if not bound,
                                            // Otherwise, date/time of the target DLL

} IMAGE_DELAYLOAD_DESCRIPTOR, *PIMAGE_DELAYLOAD_DESCRIPTOR;

typedef const IMAGE_DELAYLOAD_DESCRIPTOR *PCIMAGE_DELAYLOAD_DESCRIPTOR;

typedef enum _FIRMWARE_TYPE {
    FirmwareTypeUnknown,
    FirmwareTypeBios,
    FirmwareTypeUefi,
    FirmwareTypeMax
} FIRMWARE_TYPE, *PFIRMWARE_TYPE;


typedef enum _PROCESS_MITIGATION_POLICY {
    ProcessDEPPolicy,
    ProcessASLRPolicy,
    ProcessDynamicCodePolicy,
    ProcessStrictHandleCheckPolicy,
    ProcessSystemCallDisablePolicy,
    ProcessMitigationOptionsMask,
    ProcessExtensionPointDisablePolicy,
    ProcessControlFlowGuardPolicy,
    ProcessSignaturePolicy,
    ProcessFontDisablePolicy,
    ProcessImageLoadPolicy,
    ProcessSystemCallFilterPolicy,
    ProcessPayloadRestrictionPolicy,
    ProcessChildProcessPolicy,
    ProcessSideChannelIsolationPolicy,
    MaxProcessMitigationPolicy
} PROCESS_MITIGATION_POLICY, *PPROCESS_MITIGATION_POLICY;

//
// N.B.  High entropy mode is read only and can only be set at creation time
//       and not via the ProcessMitigationPolicy APIs.
//

typedef struct _PROCESS_MITIGATION_ASLR_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD EnableBottomUpRandomization : 1;
            DWORD EnableForceRelocateImages : 1;
            DWORD EnableHighEntropy : 1;
            DWORD DisallowStrippedImages : 1;
            DWORD ReservedFlags : 28;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_ASLR_POLICY, *PPROCESS_MITIGATION_ASLR_POLICY;

typedef struct _PROCESS_MITIGATION_DEP_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD Enable : 1;
            DWORD DisableAtlThunkEmulation : 1;
            DWORD ReservedFlags : 30;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
    BOOLEAN Permanent;
} PROCESS_MITIGATION_DEP_POLICY, *PPROCESS_MITIGATION_DEP_POLICY;

typedef struct _PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD RaiseExceptionOnInvalidHandleReference : 1;
            DWORD HandleExceptionsPermanentlyEnabled : 1;
            DWORD ReservedFlags : 30;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY, *PPROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY;

typedef struct _PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD DisallowWin32kSystemCalls : 1;
            DWORD AuditDisallowWin32kSystemCalls : 1;
            DWORD ReservedFlags : 30;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY, *PPROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY;

typedef struct _PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD DisableExtensionPoints : 1;
            DWORD ReservedFlags : 31;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY, *PPROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY;

typedef struct _PROCESS_MITIGATION_DYNAMIC_CODE_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD ProhibitDynamicCode : 1;
            DWORD AllowThreadOptOut : 1;
            DWORD AllowRemoteDowngrade : 1;
            DWORD AuditProhibitDynamicCode : 1;
            DWORD ReservedFlags : 28;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_DYNAMIC_CODE_POLICY, *PPROCESS_MITIGATION_DYNAMIC_CODE_POLICY;

typedef struct _PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD EnableControlFlowGuard : 1;
            DWORD EnableExportSuppression : 1;
            DWORD StrictMode : 1;
            DWORD ReservedFlags : 29;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY, *PPROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY;

typedef struct _PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD MicrosoftSignedOnly : 1;
            DWORD StoreSignedOnly : 1;
            DWORD MitigationOptIn : 1;
            DWORD AuditMicrosoftSignedOnly : 1;
            DWORD AuditStoreSignedOnly : 1;
            DWORD ReservedFlags : 27;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY, *PPROCESS_MITIGATION_BINARY_SIGNATURE_POLICY;

typedef struct _PROCESS_MITIGATION_FONT_DISABLE_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD DisableNonSystemFonts     : 1;
            DWORD AuditNonSystemFontLoading : 1;
            DWORD ReservedFlags             : 30;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_FONT_DISABLE_POLICY, *PPROCESS_MITIGATION_FONT_DISABLE_POLICY;

typedef struct _PROCESS_MITIGATION_IMAGE_LOAD_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD NoRemoteImages : 1;
            DWORD NoLowMandatoryLabelImages : 1;
            DWORD PreferSystem32Images : 1;
            DWORD AuditNoRemoteImages : 1;
            DWORD AuditNoLowMandatoryLabelImages : 1;
            DWORD ReservedFlags : 27;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_IMAGE_LOAD_POLICY, *PPROCESS_MITIGATION_IMAGE_LOAD_POLICY;

typedef struct _PROCESS_MITIGATION_SYSTEM_CALL_FILTER_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD FilterId: 4;
            DWORD ReservedFlags : 28;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_SYSTEM_CALL_FILTER_POLICY, *PPROCESS_MITIGATION_SYSTEM_CALL_FILTER_POLICY;

typedef struct _PROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD EnableExportAddressFilter     : 1;
            DWORD AuditExportAddressFilter      : 1;

            DWORD EnableExportAddressFilterPlus : 1;
            DWORD AuditExportAddressFilterPlus  : 1;

            DWORD EnableImportAddressFilter     : 1;
            DWORD AuditImportAddressFilter      : 1;

            DWORD EnableRopStackPivot           : 1;
            DWORD AuditRopStackPivot            : 1;

            DWORD EnableRopCallerCheck          : 1;
            DWORD AuditRopCallerCheck           : 1;

            DWORD EnableRopSimExec              : 1;
            DWORD AuditRopSimExec               : 1;

            DWORD ReservedFlags                 : 20;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY, *PPROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY;

typedef struct _PROCESS_MITIGATION_CHILD_PROCESS_POLICY {
    union {
        DWORD Flags;
        struct {
            DWORD NoChildProcessCreation : 1;
            DWORD AuditNoChildProcessCreation : 1;
            DWORD AllowSecureProcessCreation : 1;
            DWORD ReservedFlags : 29;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_CHILD_PROCESS_POLICY, *PPROCESS_MITIGATION_CHILD_PROCESS_POLICY;

typedef struct _PROCESS_MITIGATION_SIDE_CHANNEL_ISOLATION_POLICY {
    union {
        DWORD Flags;
        struct {

            //
            // Prevent branch target pollution cross-SMT-thread in user mode.
            //

            DWORD SmtBranchTargetIsolation : 1;

            //
            // Isolate this process into a distinct security domain, even from
            // other processes running as the same security context.  This
            // prevents branch target injection cross-process (normally such
            // branch target injection is only inhibited across different
            // security contexts).
            //
            // Page combining is limited to processes within the same security
            // domain.  This flag thus also effectively limits the process to
            // only being able to combine internally to the process itself,
            // except for common pages (unless further restricted by the
            // DisablePageCombine policy).
            //

            DWORD IsolateSecurityDomain : 1;

            //
            // Disable all page combining for this process, even internally to
            // the process itself, except for common pages (zeroes or ones).
            //

            DWORD DisablePageCombine : 1;

            //
            // Memory Disambiguation Disable.
            //

            DWORD SpeculativeStoreBypassDisable : 1;

            DWORD ReservedFlags : 28;

        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
} PROCESS_MITIGATION_SIDE_CHANNEL_ISOLATION_POLICY, *PPROCESS_MITIGATION_SIDE_CHANNEL_ISOLATION_POLICY;


typedef struct _CFG_CALL_TARGET_INFO {
    ULONG_PTR Offset;
    ULONG_PTR Flags;
} CFG_CALL_TARGET_INFO, *PCFG_CALL_TARGET_INFO;


typedef struct _FILE_ID_128 {
    BYTE  Identifier[16];
} FILE_ID_128, *PFILE_ID_128;


typedef struct _RTL_BARRIER {
            DWORD Reserved1;
            DWORD Reserved2;
            ULONG_PTR Reserved3[2];
            DWORD Reserved4;
            DWORD Reserved5;
} RTL_BARRIER, *PRTL_BARRIER;

#ifndef _Frees_ptr_
#define _Frees_ptr_               _SAL_L_Source_(_Frees_ptr_, (),  _Pre_notnull_ _Post_ptr_invalid_ __drv_freesMem(Mem))
#endif
#ifndef _Frees_ptr_opt_
#define _Frees_ptr_opt_           _SAL_L_Source_(_Frees_ptr_opt_, (),  _Pre_maybenull_ _Post_ptr_invalid_ __drv_freesMem(Mem))
#endif


typedef BYTE  SE_SIGNING_LEVEL, *PSE_SIGNING_LEVEL;

