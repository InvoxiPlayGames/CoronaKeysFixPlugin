#include <xtl.h>
#include "ppcasm.h"

// patch address for 17559 - the instruction that loads the address of the 1bl key into r4 in set_title_key
#define SETTITLEKEY_INST 0x00003B8C

// struct for the kernel version
typedef struct _XBOX_KRNL_VERSION {
	WORD Major;
	WORD Minor;
	WORD Build;
	WORD Qfe;
} XBOX_KRNL_VERSION, *PXBOX_KRNL_VERSION;

// kernel imports that aren't normally exposed
EXTERN_C { 
	VOID DbgPrint(const char* s, ...);
	extern PXBOX_KRNL_VERSION XboxKrnlVersion;
	DWORD MmGetPhysicalAddress(void *buffer);
	VOID HalSendSMCMessage(LPVOID pCommandBuffer, LPVOID pRecvBuffer);
}

// freeboot memcpy lives in HvxGetVersions
static int __declspec(naked) HvxGetVersions(unsigned int magic, int op, unsigned long long source, unsigned long long dest, unsigned long long length) {
    __asm {
        li r0, 0
        sc
        blr
    }
}

// asks the SMC if the dvd drive tray is open
static BOOL IsTrayOpen() {
	BYTE smc_msg[0x10] = { 0xA };
	BYTE smc_ret[0x10] = { 0 };
	HalSendSMCMessage(smc_msg, smc_ret);
	return (smc_ret[1] == 0x60);
}

// dll entrypoint
BOOL APIENTRY DllMain(HANDLE hInstDLL, DWORD dwReason, LPVOID lpReserved) {
	if (dwReason == DLL_PROCESS_ATTACH) {
		// don't run if the tray is open, emergency hatch
		if (IsTrayOpen()) {
			DbgPrint("CoronaKeysFix | not patching: tray open\n");
			goto end;
		}
		// only patch on kernel 17559
		if (XboxKrnlVersion->Build == 17559) {
			// allocate a physical buffer
			unsigned char *buf = (unsigned char *)XPhysicalAlloc(0x1000, MAXULONG_PTR, 0, PAGE_READWRITE);
			unsigned long long buf_phy = 0x8000000000000000 + (unsigned int)MmGetPhysicalAddress(buf);
			DbgPrint("CoronaKeysFix | applying patch\n");
			// write the fixed instruction to set_title_key using freeboot memcpy
			*(unsigned int *)buf = ADDI(4, 31, 0x10);
			HvxGetVersions(0x72627472, 5, buf_phy, 0x8000010000000000ULL + SETTITLEKEY_INST, 0x4);
			// free our buffer
			DbgPrint("CoronaKeysFix | patched!\n");
			XPhysicalFree(buf);
		} else {
			DbgPrint("CoronaKeysFix | not patching: kernel is %i\n", XboxKrnlVersion->Build);
		}
	}
end:
	// in theory, this should unload the DLL, but it doesn't seem to. sorry.
	return FALSE;
}
