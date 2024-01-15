#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h> // sceCtrl*()
#include <stdio.h> // sprintf()
#include <string.h> // mem*()

#define VER_MAJOR 2
#define VER_MINOR 0
#define VER_BUILD "-QA"

#define VAL_LENGTH 0x10
#define VAL_PUBLIC 0x0A
#define VAL_PRIVATE 0x06

PSP_MODULE_INFO("cex2dex", 0, VER_MAJOR, VER_MINOR);
PSP_MAIN_THREAD_ATTR(0);
PSP_HEAP_SIZE_KB(1024);

#include "../kernel_prx/kernel_prx.h"
#define printf pspDebugScreenPrintf

#define key_number 0x0100 // 100/120, 101/121
#define key_offset 0x0038 // 100@38, 100@f0, 100@1a8, 101@60, 101@118

SceCtrlData pad;

void zeco(uint64_t fuseid, uint8_t *buffer, uint8_t *crtbuf);

int cmpstr(const unsigned char *s1, const unsigned char *s2, size_t n) {
	unsigned char u1, u2;
	while (n-- > 0) {
		u1 = (unsigned char) *s1++;
		u2 = (unsigned char) *s2++;
		if (u1 != u2)
			return u1 - u2;
		if (u1 == '\0')
			return 0;
	}
	return 0;
}

void ContCircle(void) {
	printf("Press O to continue or X to exit...\n\n");
	for(;;) {
		sceCtrlReadBufferPositive(&pad, 1);
		sceKernelDelayThread(0.05*1000*1000);
		if (pad.Buttons & PSP_CTRL_CROSS)
			sceKernelExitGame();
		else if (pad.Buttons & PSP_CTRL_CIRCLE)
			break;
	}
}

void ExitCross(char*text) {
	printf("%s, press X to exit...\n", text);
	do {
		sceCtrlReadBufferPositive(&pad, 1);
		sceKernelDelayThread(0.05*1000*1000);
	} while(!(pad.Buttons & PSP_CTRL_CROSS));
	sceKernelExitGame();
}

void ExitError(char*text, int delay, int error) {
	printf(text, error);
	sceKernelDelayThread(delay*1000*1000);
	sceKernelExitGame();
}

void DeleteLeaf(u16 from) {
	unsigned char key_buffer[512];

	printf(" Removing 0x%03x... ", from);
	if (prxIdStorageReadLeaf(from, key_buffer) == 0) {
		if (prxIdStorageDeleteLeaf(from) == 0) {
			printf("OK");
		} else {
			printf("NG");
			ExitCross("\nAborting");
		}
	}
	printf("\n");
}

void WriteLeaf(u16 from, u16 to) {
	unsigned char key_buffer[512];

	printf(" Writing 0x%03x to 0x%03x... ", from, to);
	if (prxIdStorageReadLeaf(from, key_buffer) == 0) {
		if (prxIdStorageWriteLeaf(to, key_buffer) == 0) {
			printf("OK");
		} else {
			printf("NG");
			ExitCross("\nAborting");
		}
	}
	printf("\n");
}

int main(int argc, char*argv[]) {
	int i = 0;
	int paranoid = 0;
	uint64_t fuseid = 0;
	char idps_buffer[16];
	unsigned char key_buffer[512];
	unsigned char crt_buf_A8[0xA8];
	unsigned char crt_buf_B8[0xB8];

	sceKernelDelayThread(1000000);
	sceCtrlReadBufferPositive(&pad, 1);
	sceKernelDelayThread(1000000);
	if (pad.Buttons & PSP_CTRL_LTRIGGER)
		paranoid = 1;

	pspDebugScreenInit();
	pspDebugScreenClear();
	printf("PSP CEX2DEX v%i.%i%s by Yoti\n\n", VER_MAJOR, VER_MINOR, VER_BUILD);

	if (VAL_PUBLIC + VAL_PRIVATE != VAL_LENGTH)
		ExitError("Length error 0x%02x", 5, VAL_PUBLIC + VAL_PRIVATE);

	SceUID mod = pspSdkLoadStartModule("kernel.prx", PSP_MEMORY_PARTITION_KERNEL);
	if (mod < 0)
		ExitError("Error: LoadStart() returned 0x%08x\n", 3, mod);

	if (prxIdStorageLookup(key_number, key_offset, idps_buffer, sizeof(idps_buffer)) != 0) {
		ExitCross("Lookup() error");
	}

	printf(" Your IDPS is: ");
	for (i=0; i<VAL_PUBLIC; i++) {
		if (i == 0x04)
			pspDebugScreenSetTextColor(0xFF0000FF); // red #1
		else if (i == 0x05)
			pspDebugScreenSetTextColor(0xFFFF0000); // blue #2
		else if (i == 0x06)
			pspDebugScreenSetTextColor(0xFF0000FF); // red #3
		else if (i == 0x07)
			pspDebugScreenSetTextColor(0xFF00FF00); // green #4
		else
			pspDebugScreenSetTextColor(0xFFFFFFFF); // white
		printf("%02X", (u8)idps_buffer[i]);
	}
	if (paranoid == 1) {
		for (i=0; i<VAL_PRIVATE; i++) {
			pspDebugScreenSetTextColor(0xFF777777); // gray
			printf("XX");
			pspDebugScreenSetTextColor(0xFFFFFFFF); // white
		}
	} else {
		for (i=0; i<VAL_PRIVATE; i++) {
			pspDebugScreenSetTextColor(0xFFFFFFFF); // white
			printf("%02X", (u8)idps_buffer[VAL_PUBLIC+i]);
		}
	}
	printf("\n\n");

	printf(" It seems that you are using ");
	pspDebugScreenSetTextColor(0xFF0000FF); // red
	if (idps_buffer[0x04] == 0x00)
		printf("PlayStation Portable");
	else if (idps_buffer[0x04] == 0x01) { // psv, vtv/pstv
		if (idps_buffer[0x06] == 0x00)
			printf("PlayStation Vita"); // fatWF/fat3G, slim
		else if (idps_buffer[0x06] == 0x02)
			printf("PlayStation/Vita TV"); // vtv, pstv
		else if (idps_buffer[0x06] == 0x06)
			printf("PlayStation/Vita TV"); // vtv, pstv (testkit)
		else
			printf("Unknown Vita 0x%02X", idps_buffer[0x06]);
	} else {
		printf("Unknown PS 0x%02X", idps_buffer[0x04]);
	}
	pspDebugScreenSetTextColor(0xFFFFFFFF); // white
	printf("\n");

	printf(" Your motherboard is ");
	pspDebugScreenSetTextColor(0xFF00FF00); // green
	if (idps_buffer[0x06] == 0x00) { // portable
		switch(idps_buffer[0x07]) {
			case 0x01:
				printf("TA-079/081 (PSP-1000)");
				break;
			case 0x02:
				printf("TA-082/086 (PSP-1000)");
				break;
			case 0x03:
				printf("TA-085/088 (PSP-2000)");
				break;
			case 0x04:
				printf("TA-090/092 (PSP-3000)");
				break;
			case 0x05:
				printf("TA-091 (PSP-N1000)");
				break;
			case 0x06:
				printf("TA-093 (PSP-3000)");
				break;
			//case 0x07:
			//	printf("TA-094 (PSP-N1000)");
			//	break;
			case 0x08:
				printf("TA-095 (PSP-3000)");
				break;
			case 0x09:
				printf("TA-096/097 (PSP-E1000)");
				break;
			case 0x10:
				printf("IRS-002 (PCH-1000/1100)");
				break;
			case 0x11: // 3G?
			case 0x12: // WF?
				printf("IRS-1001 (PCH-1000/1100)");
				break;
			case 0x14:
				printf("USS-1001 (PCH-2000)");
				break;
			case 0x18:
				printf("USS-1002 (PCH-2000)");
				break;
			default:
				printf("Unknown MoBo 0x%02X", idps_buffer[0x07]);
				break;
		}
	} else if ((idps_buffer[0x06] == 0x02) || (idps_buffer[0x06] == 0x06)) { // home system
		switch(idps_buffer[0x07]) {
			case 0x01:
				printf("DOL-1001 (VTE-1000)");
				break;
			case 0x02:
				printf("DOL-1002 (VTE-1000)");
				break;
			default:
				printf("Unknown MoBo 0x%02X", idps_buffer[0x07]);
				break;
		}
	} else {
		printf("Unknown type 0x%02X", idps_buffer[0x06]);
	}
	pspDebugScreenSetTextColor(0xFFFFFFFF); // white
	printf("\n");

	printf(" And your region is ");
	pspDebugScreenSetTextColor(0xFFFF0000); // blue
	switch(idps_buffer[0x05]) {
		case 0x00:
			printf("Proto");
			break;
		case 0x01:
			printf("DevKit");
			break;
		case 0x02:
			printf("TestKit");
			break;
		case 0x03:
			printf("Japan");
			break;
		case 0x04:
			printf("North America");
			break;
		case 0x05:
			printf("Europe/East/Africa");
			break;
		case 0x06:
			printf("Korea");
			break;
		case 0x07: // PCH-xx03 VTE-1016
			printf("Great Britain/United Kingdom");
			break;
		case 0x08:
			printf("Mexica/Latin America");
			break;
		case 0x09:
			printf("Australia/New Zeland");
			break;
		case 0x0A:
			printf("Hong Kong/Singapore");
			break;
		case 0x0B:
			printf("Taiwan");
			break;
		case 0x0C:
			printf("Russia");
			break;
		case 0x0D:
			printf("China");
			break;
		default:
			printf("Unknown region 0x%02X", idps_buffer[0x05]);
			break;
	}
	pspDebugScreenSetTextColor(0xFFFFFFFF); // white
	printf("\n");

	printf(" This console FuseID ");
	pspDebugScreenSetTextColor(0xFF007FFF); // orange
	fuseid = prxSysregGetFuseId();
	printf("0x%llx", fuseid);
	pspDebugScreenSetTextColor(0xFFFFFFFF); // white
	printf("\n\n");

	if (prxIdStorageReadLeaf(key_number, key_buffer) != 0) {
		ExitCross("ReadLeaf() error");
	}
	if (key_buffer[key_offset+0x06] > 0) {
		ExitCross("Wrong console");
	}
	if (key_buffer[key_offset+0x05] < 3) {
		for (i = 0; i < 3; i++) {
			if (prxIdStorageReadLeaf(0x150 + i, key_buffer) != 0) {
				printf(" Backup key 0x%03x is missing!\n", 0x150 + i);
				ExitCross("Can't uninstall");
			}
			if (prxIdStorageReadLeaf(0x170 + i, key_buffer) != 0) {
				printf(" Backup key 0x%03x is missing!\n", 0x170 + i);
				ExitCross("Can't uninstall");
			}
		}
		printf("Do you want to uninstall TestKit certificate?\n");
		ContCircle();
		for (i = 0; i < 3; i++) {
			WriteLeaf(0x150 + i, 0x100 + i);
			WriteLeaf(0x170 + i, 0x120 + i);
		}
		printf("\n");
		sceKernelDelayThread(1 * 1000 * 1000);
		printf("Do you want to remove backup IDStorage leafs?\n");
		ContCircle();
		for (i = 0; i < 3; i++) {
			DeleteLeaf(0x150 + i);
			DeleteLeaf(0x170 + i);
		}
		printf("\n");

		printf("https://github.com/yoti/psp_cex2dex/\n");

		ExitCross("\nDone");
	}

	printf("Do you want to convert your PSP to TestKit?\n");
	ContCircle();

	for (i = 0; i < 3; i++) {
		WriteLeaf(0x100 + i, 0x150 + i);
		WriteLeaf(0x120 + i, 0x170 + i);
	}

	prxIdStorageReadLeaf(key_number, key_buffer);
	memcpy(crt_buf_A8, key_buffer + key_offset, sizeof(crt_buf_A8));
	memset(crt_buf_B8, 0, sizeof(crt_buf_B8));
	zeco(fuseid, crt_buf_A8, crt_buf_B8);
	printf(" Checking current certificate... ");
	if (cmpstr(&key_buffer[key_offset + 0xA8], &crt_buf_B8[0xA8], 16) == 0) {
		printf("OK");
	} else {
		printf("NG");
		ExitCross("\nAborting");
	}
	printf("\n");

	memcpy(crt_buf_A8, key_buffer + key_offset, sizeof(crt_buf_A8));
	memset(crt_buf_B8, 0, sizeof(crt_buf_B8));
	printf(" Will patch from 0x%02x to 0x02\n\n", crt_buf_A8[5]);
	crt_buf_A8[5] = 0x02;
	printf(" Will patch from 0x%02x to 0x8C\n\n", crt_buf_A8[8]);
	crt_buf_A8[8] = 0x8C;
	zeco(fuseid, crt_buf_A8, crt_buf_B8);
	memcpy(key_buffer + key_offset, crt_buf_B8, sizeof(crt_buf_B8));
	prxIdStorageWriteLeaf(key_number, key_buffer);
	prxIdStorageWriteLeaf(key_number+0x20, key_buffer);

	printf(" https://github.com/yoti/psp_cex2dex/\n");

	ExitCross("\nDone");
	return 0;
}
