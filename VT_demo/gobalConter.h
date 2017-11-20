#include "ntddk.h"
#include "ntstrsafe.h"
ULONG64 G_GuestCr3;
VOID NTAPI HookEptMemoryPage(PEPROCESS Process, ULONG64 Address);
BOOLEAN NTAPI UnHookEptPage(PVOID GuestPA);
ULONG64 Search64Process(HANDLE hProcess);
PEPROCESS TargetProcess;
ULONG64 oldGuestRip;
ULONG64 oldCr3;
ULONG64 TargetAddress;
typedef struct _DBGSTRUCT {
  ULONG64 Pte;
  BOOLEAN addrHook;
  ULONG64 Addr;

} DBGSTRUCT, *PDBGSTRUCT;
DBGSTRUCT Bpaddr[6];
ULONG64 Guest_CR3;

#define DEVICE_NAME L"\\Device\\vmmHardBreak"
#define LINK_NAME L"\\DosDevices\\vmmHardBreak"
#define LINK_GLOBAL_NAME L"\\DosDevices\\Global\\vmmHardBreak"

#define Hookpage                                                               \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x650, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define Unhookpage                                                             \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x651, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TargetHookAddress                                                      \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x652, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define TargetProcessId                                                        \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x653, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define ISrecv                                                                 \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x654, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_notiyenable                                                      \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_ObDuplicateObject                                                \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x907, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_KiSaveDebugRegisterState                                         \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x917, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KiUmsCallEntry                                                   \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x918, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KiSystemServiceExit                                              \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x919, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KeGdiFlushUserBatch                                              \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x920, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KiSystemServiceRepeat                                            \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x921, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_KeServiceDescriptorTable                                         \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KeServiceDescriptorTableShadow                                   \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KiSystemServiceCopyEnd                                           \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x922, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DbgkDebugObjectType                                              \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x923, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DbgkpPostFakeThreadMessages                                      \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x893, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DbgkpPostModuleMessages                                          \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x897, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KeFreezeAllThreads                                               \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PsResumeThread                                                   \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x811, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KeThawAllThreads                                                 \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PsGetNextProcessThread                                           \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x885, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PsSuspendThread                                                  \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x924, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_LpcRequestWaitReplyPortEx                                        \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x925, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DbgkpProcessDebugPortMutex                                       \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x889, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_KiConvertToGuiThread                                             \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x930, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KiRetireDpcList                                                  \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x911, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DbgkCopyProcessDebugPort                                         \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x850, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KiDispatchException                                              \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x854, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DbgkForwardException                                             \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x852, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MmGetFileNameForSection                                          \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_PsGetNextProcess                                                 \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x931, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PsTerminateProcess                                               \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x932, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DbgkpSendApiMessage                                              \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x934, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DbgkOpenProcessDebugPort                                         \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x858, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DbgkUnMapViewOfSection                                           \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x855, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DbgkMapViewOfSection                                             \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x856, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ObTypeIndexTable                                                 \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x933, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DbgkExitProcess                                                  \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x862, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DbgkExitThread                                                   \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x861, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ObpCallPreOperationCallbacks                                     \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ExGetCallBackBlockRoutine                                        \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x898, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_dbgProcessId                                                     \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x881, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DbgkpQueueMessage                                                \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x853, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DbgkpSetProcessDebugObject                                       \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x851, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DbgkpPostFakeProcessCreateMessages                               \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x892, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_MmGetFileNameForAddress                                          \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x935, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PspSystemDlls                                                    \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x904, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DbgkpWakeTarget                                                  \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x891, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NtQueryInformationThread                                         \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x914, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_ExCompareExchangeCallBack                                        \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x936, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_KiRestoreDebugRegisterState                                      \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x937, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_RtlpCopyLegacyContextX86                                         \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x938, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_keycode                                                          \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x939, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_keycheck                                                         \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x940, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NtReadVirtualMemory                                              \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x941, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NtWriteVirtualMemory                                             \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x942, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NtOpenProcess                                                    \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x943, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KiAttachProcess                                                  \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x944, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NtCreateDebugObject                                              \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x895, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NtWaitForDebugEvent                                              \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x910, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NtDebugContinue                                                  \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x909, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_NtDebugActiveProcess                                             \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x894, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_NtRemoveProcessDebug                                             \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x908, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DbgLol                                                           \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x950, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SwapContext_PatchXRstor                                          \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x945, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SwapContext                                                      \
  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x946, METHOD_BUFFERED, FILE_ANY_ACCESS)

extern ULONG64 SwapContext_PatchXRstor;
extern ULONG64 SwapContext;

BOOLEAN DbgConter = FALSE;
extern ULONG64 myNtRemoveProcessDebug;
extern ULONG64 NtDebugActiveProcess;
extern ULONG64 NtDebugContinue;
extern ULONG64 NtWaitForDebugEvent;
extern ULONG64 NtCreateDebugObject;

ULONG64 NtReadVirtualMemory;
ULONG64 NtWriteVirtualMemory;
ULONG64 XNtOpenProcess;
extern ULONG64 KiAttachProcess;

char keycode[1024] = {0};
char keycheck[1024] = {0};

extern ULONG64 RtlpCopyLegacyContextX86;
extern ULONG64 KiRestoreDebugRegisterState;
extern ULONG64 ExCompareExchangeCallBack;
extern ULONG64 NtQueryInformationThread;
extern ULONG64 DbgkpWakeTarget_2;
extern ULONG64 MmGetFileNameForAddress;
extern ULONG64 ExGetCallBackBlockRoutine;
extern ULONG64 ObpCallPreOperationCallbacks;
extern ULONG64 ObTypeIndexTable;
extern ULONG64 DbgkOpenProcessDebugPort;
extern ULONG64 DbgkUnMapViewOfSection;
extern ULONG64 DbgkMapViewOfSection;

extern ULONG64 DbgkExitProcess;
extern ULONG64 DbgkExitThread;
extern ULONG64 PspSystemDlls;
extern ULONG64 DbgkpPostFakeProcessCreateMessages;
extern ULONG64 DbgkpSetProcessDebugObject;
extern ULONG64 PsTerminateProcess;
extern ULONG64 PsGetNextProcess;
extern ULONG64 MmGetFileNameForSection;
extern ULONG64 DbgkForwardException;
extern ULONG64 KiDispatchException;
ULONG64 KiConvertToGuiThread;
extern ULONG64 ObDuplicateObject;
extern ULONG64 KiRetireDpcList;
extern ULONG64 DbgkCopyProcessDebugPort;
extern ULONG64 KiSaveDebugRegisterState;
extern ULONG64 KiUmsCallEntry;
extern ULONG64 KiSystemServiceExit;
extern ULONG64 KeGdiFlushUserBatch;
extern ULONG64 KiSystemServiceRepeat;
extern ULONG64 KeServiceDescriptorTable;
extern ULONG64 KeServiceDescriptorTableShadow;
extern ULONG64 KiSystemServiceCopyEnd;
extern ULONG64 DbgkDebugObjectType;
extern ULONG64 PsGetNextProcessThread;
extern ULONG64 DbgkpPostModuleMessages;
extern ULONG64 PsSuspendThread;
extern ULONG64 KeFreezeAllThreads;
extern ULONG64 DbgkpPostFakeThreadMessages;
extern ULONG64 KeThawAllThreads;
extern ULONG64 PsResumeThread;
extern ULONG64 LpcRequestWaitReplyPortEx;
extern ULONG64 DbgkpProcessDebugPortMutex;
extern ULONG64 DbgkpSendApiMessage;
extern ULONG64 DbgkpQueueMessage;
