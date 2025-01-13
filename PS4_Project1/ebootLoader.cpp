//	ebootLoader, Help needed to fix this broken mess
//
//	PURPOSE: Makes debugging a lot easier by loading an easily replaceable eboot from /data
//
//	Build reqs: ORBIS SDK 1.750 or newer and Visual Studio 2017

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
#include "displayf.h"
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
#define HANG_TIMEOUT 1000000

int ShowDialog(const char* message) {
	int32_t ret;
	uint32_t i = 0;
	Displayf("in ShowDialog");
	if (!message) {
		Displayf("Invalid message string passed, aborting to prevent SIGSEGV fault..");
		return -1;
	}
	Displayf("message: %s", message);
	Displayf("initializing messagedialog");
	ret = sceMsgDialogInitialize();
	if (ret < SCE_OK) {
		Displayf("Failed to initialize Message Dialog ret %x", ret);
		return ret;
	}
	Displayf("Initialized Message Dialog");

	SceCommonDialogBaseParam baseParam;
	_sceCommonDialogBaseParamInit(&baseParam);
	SceMsgDialogParam dialogParam;


	sceMsgDialogParamInitialize(&dialogParam);
	//printf("initing msgdialog\n");
	
	//msgdialogstatus = sceMsgDialogGetStatus();
	//printf("status after init %x\n", msgdialogstatus);

	SceMsgDialogUserMessageParam messageParam;
	messageParam = {};  // Have to zero it out to not have random data (leading to a crash possibly)
	messageParam.msg = message;
	messageParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
	Displayf("Zeroing out messageParam.reserved");
	memset(messageParam.reserved, 0x0, sizeof(messageParam.reserved));

	dialogParam.baseParam = baseParam;
	dialogParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	dialogParam.userMsgParam = &messageParam;
	dialogParam.size = sizeof(SceMsgDialogParam);
	Displayf("Zeroing out dialogParam.reserved");
	memset(dialogParam.reserved, 0x0, sizeof(dialogParam.reserved));
	//msgdialogstatus = sceMsgDialogGetStatus();
	//printf("msgdialogstatus before alloc %x\n", msgdialogstatus);
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
	//msgdialogstatus = sceMsgDialogGetStatus();
	//printf("msgdialogstatus before open %x\n", msgdialogstatus);
	Displayf("opening dialog");
	ret = sceMsgDialogOpen(&dialogParam);
	if (ret < 0) {
		Displayf("failed to open dialog, ret = %x", ret);
		return ret;
	}
	//msgdialogstatus = sceMsgDialogGetStatus();
	// ISSUE : Dialog Open always fails due to "Invalid Param"
	// FIX : I have no fucking clue
	//printf("msgdialogstatus after DialogOpen %x\n", msgdialogstatus);
	Displayf("about to start checking status");
	while (sceMsgDialogUpdateStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) {
		sceKernelUsleep(1000);	// time is measured in microseconds
		Displayf("dialog status %x  %x", sceMsgDialogGetStatus(), sceMsgDialogUpdateStatus());
		if (i++ >= HANG_TIMEOUT && sceMsgDialogUpdateStatus() != SCE_COMMON_DIALOG_STATUS_RUNNING) {	// have to make sure that the dialog is running or else it will return when the user waits too long to respond (~10 seconds)
			Displayf("hang detected, aborting..");
			return -1;
		}
	}
	Displayf("Closing dialog and returning");
	sceMsgDialogClose();
	sceMsgDialogTerminate();
	return SCE_OK;
}

// works but only conditionally
int LoadExecutable(const char *path) {
	int ret;
	Displayf("in LoadExecutable");
	ret = sceKernelCheckReachability(path);
	const char *execpath = (ret < SCE_OK) ? FALLBACK_PATH : path;
	if (strcmp(path, execpath)) {
		Displayf("%s wasn't reachable launching %s", path, execpath); 
		ShowDialog(static_cast<const char*>("Cannot launch from /data, trying /app0"));
	}
	ret = ShowDialog(static_cast<const char*>("Launching Grand Theft Auto V"));
	if (ret < 0) {
		Displayf("showdialog failed");
		//return ret;
	}
	ret = sceSystemServiceLoadExec(execpath, NULL);
	if (ret < SCE_OK) {
		Displayf("failed to load executable %s %x", path, ret);
		return ret;
	}
	else {
		Displayf("successfully loaded executable %s ret %x?", path, ret);
		return ret;
	}
}

int main() {
	int32_t ret;
	sceSystemServiceHideSplashScreen();
	Displayf("initing commondialog\n");
	ret = sceCommonDialogInitialize();
	if (ret != SCE_OK) {
		Displayf("Failed to initialize Common Dialog");
		return ret;
	}
	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
	if (ret < SCE_SYSMODULE_LOADED) {
		Displayf("Failed to load Message Dialog PRX");
		return ret;
	}
	Displayf("loaded message dialog module");
	
	/*
	ret = sceMsgDialogInitialize();
	if (ret < SCE_OK) {
		printf("Failed to initialize Message Dialog ret %x", ret);
		return ret;
	}
	printf("loaded message dialog module\n");
	*/
	Displayf("Starting %s", TARGET_EXEC_PATH);
	//const char *dialogMessage = "Starting eboot";
	//ShowDialog(dialogMessage);
	//printf("Done with showdialog\n");
	return LoadExecutable(TARGET_EXEC_PATH);
}