//	ebootLoader, Help needed to fix this broken mess, VS 2017 with ORBIS SDK needed
//
//	PURPOSE: Makes debugging a lot easier by loading an easily replaceable eboot from /data
//
//	Built with ORBIS SDK 8.008.051 

// headers (most of them aren't even needed or used but whatever)
#include <scebase.h>
#include <app_content.h>
#include <sceerror.h>
#include <kernel.h>
#include <stdio.h>
//#include <iostream>
#include <common_dialog.h>
#include <sys\file.h>
#include <unistd.h>
#include <app_content.h>
#include <libsysmodule.h>
#include <message_dialog.h>
#include <system_service.h>
#include <user_service.h>
#include <libime.h>
#include <string.h>
#include <_fs.h>
//#include <pad.h>

// needed libs 
#pragma comment(lib, "libc_stub_weak.a")
#pragma comment(lib, "libkernel_stub_weak.a")
#pragma comment(lib, "libSceCommonDialog_stub_weak.a")
#pragma comment(lib, "libSceSysmodule_stub_weak.a")
#pragma comment(lib, "libSceSystemService_stub_weak.a")
#pragma comment(lib, "libSceUserService_stub_weak.a")
#pragma comment(lib, "libScePosix_stub_weak.a")
#pragma comment(lib, "libSceMsgDialog_stub_weak.a")
//#pragma comment(lib,"libScePad_stub_weak.a")

#define TARGET_EXEC_PATH "/data/lso.bin"
#define FALLBACK_PATH "/app0/gtabin.bin"

// doesn't work
int ShowDialog(const char* message) {
	int loadmodule, msgdialogstatus, ret;
	unsigned int i = 0;
	printf("in ShowDialog\n");
	printf("message: %s\n", message);
	loadmodule = sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
	if (loadmodule < SCE_SYSMODULE_LOADED) {
		printf("Module not loaded\n");
		return loadmodule;
	}
	printf("loaded message dialog module\n");

	SceCommonDialogBaseParam baseParam;
	_sceCommonDialogBaseParamInit(&baseParam);
	SceMsgDialogParam dialogParam;


	sceMsgDialogParamInitialize(&dialogParam);
	printf("initing msgdialog\n");
	ret = sceMsgDialogInitialize();
	if (ret < 0) {
		printf("Failed to initialize Message Dialog ret %x", ret);
		return ret;
	}
	msgdialogstatus = sceMsgDialogGetStatus();
	printf("status after init %x\n", msgdialogstatus);

	SceMsgDialogUserMessageParam messageParam;
	memset(&messageParam, 0x00, sizeof(SceMsgDialogUserMessageParam));  // Ensures no leftover data
	messageParam.msg = message;
	messageParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;

	dialogParam.baseParam = baseParam;
	dialogParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	dialogParam.userMsgParam = &messageParam;
//	dialogParam.size = SCE_MSG_DIALOG_USER_MSG_SIZE;
	msgdialogstatus = sceMsgDialogGetStatus();
	printf("msgdialogstatus before alloc %x\n", msgdialogstatus);
	/*
	if (!dialogParam.userMsgParam) {
		printf("Allocating memory for userMsgParam\n");
		dialogParam.userMsgParam = (SceMsgDialogUserMessageParam*)malloc(sizeof(SceMsgDialogUserMessageParam));
		if (!dialogParam.userMsgParam) {
			printf("failed to allocate memory for userMsgParam\n");
			sceSysmoduleUnloadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
			return -1;
		}
		memset(dialogParam.userMsgParam, 0, sizeof(SceMsgDialogUserMessageParam));
	}
	*/
	//printf("allocated memory successfully\n");
//	dialogParam.userMsgParam = (SceMsgDialogUserMessageParam*)malloc(sizeof(SceMsgDialogUserMessageParam));
//	dialogParam.userMsgParam->msg = message;
	//dialogParam.userMsgParam->buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
	msgdialogstatus = sceMsgDialogGetStatus();
	printf("msgdialogstatus before open %x\n", msgdialogstatus);
	printf("opening dialog\n");
	ret = sceMsgDialogOpen(&dialogParam);
	if (ret < 0) {
		printf("failed to open dialog ret %x\n", ret);
		return ret;
	}
	msgdialogstatus = sceMsgDialogGetStatus();

	// ISSUE : Even after opening the dialog, the status remains SCE_COMMON_DIALOG_STATUS_INITIALIZED instead of SCE_COMMON_DIALOG_STATUS_RUNNING, and the dialog doesn't get opened and it gets stuck in the while loop
	// FIX : I have no fucking clue
	printf("msgdialogstatus after DialogOpen %x\n", msgdialogstatus);
	printf("about to start checking status\n");
	while (sceMsgDialogUpdateStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) {
		sceKernelUsleep(1000);
		printf("dialog status %x  %x", sceMsgDialogGetStatus(), sceMsgDialogUpdateStatus());
		i++;
		if (i >= 10000000) {
			printf("hang detected, aborting..\n");
			return -1;
		}
	}
	printf("outside of while\n");
	sceMsgDialogTerminate();
	sceMsgDialogClose();
	sceSysmoduleUnloadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
	return 0;
}

// works but only conditionally
int LoadExecutable(const char *path) {
	int ret, checkreachability, showdialogv, lpath = 0;
	printf("in LoadExecutable\n");
	checkreachability = sceKernelCheckReachability(path);
	if (checkreachability < SCE_OK) {
		printf("not reachable %x\n", checkreachability);
		showdialogv = ShowDialog("Could not reach /data, trying app0\n");
		if (showdialogv < 0) {
			printf("showdialog failed\n");
		}
		lpath = 1;
	}
	else {
		printf("is reachable? %x\n", checkreachability);
	}
	ret = ShowDialog("Launching Grand theft Auto V");
	if (ret < 0) {
		printf("showdialog failed\n");
		//return ret;
	}
	if (!lpath) {
		ret = sceSystemServiceLoadExec(path, NULL);
	}
	else {
		ret = sceSystemServiceLoadExec(FALLBACK_PATH, NULL);
	}
	if (ret < SCE_OK) {
		printf("failed to load executable %s %x\n", path, ret);
		ret = sceSystemServiceLoadExec(FALLBACK_PATH, NULL);
		if (ret < SCE_OK) {
			printf("failed to load exectuable %s %x\n", FALLBACK_PATH, ret);
			return ret;
		}
		else {
			printf("successfully loaded from fallback path\n");
			return ret;
		}
	}
	else {
		printf("successfully loaded executable %s ret %x?", path, ret);
		return ret;
	}
}

int main() {
	int ret;
	sceSystemServiceHideSplashScreen();
	printf("initing commondialog\n");
	ret = sceCommonDialogInitialize();
	if (ret != SCE_OK) {
		printf("Failed to initialize Common Dialog\n");
		return ret;
	}
	printf("Starting %s\n", TARGET_EXEC_PATH);
	//const char *dialogMessage = "Starting eboot";
	//ShowDialog(dialogMessage);
	//printf("Done with showdialog\n");
	return LoadExecutable(TARGET_EXEC_PATH);
}