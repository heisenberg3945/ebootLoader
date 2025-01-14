//	ebootLoader
//
//	PURPOSE: Makes debugging a lot easier by loading an easily replaceable eboot from /data
//
//	Build requirements: ORBIS SDK 3.000 or newer and Visual Studio 2017

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
#include "Log.h"
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

int ShowTwoButtonDialog(const char *message, const char *firstbtn, const char *secondbtn) {
	int32_t ret, status;
	Displayf("2 button dialog");
	if (!message || !firstbtn || !secondbtn) {
		Errorf("Invalid parameters passed, aborting..");
		return -1;
	}
	ret = sceMsgDialogInitialize();
	if (ret != SCE_OK) {
		Errorf("Failed to initialize Message Dialog ret %x", ret);
		return -1;
	}
	SceMsgDialogParam dialogParam;
	sceMsgDialogParamInitialize(&dialogParam);

	SceMsgDialogButtonsParam buttonParams;
	memset(&buttonParams, 0, sizeof(SceMsgDialogButtonsParam));
	buttonParams.msg1 = firstbtn;
	buttonParams.msg2 = secondbtn;
	
	SceMsgDialogUserMessageParam messageParam;
	memset(&messageParam, 0, sizeof(SceMsgDialogUserMessageParam));
	messageParam.msg = message;
	messageParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_2BUTTONS;
	messageParam.buttonsParam = &buttonParams;

	dialogParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	dialogParam.userMsgParam = &messageParam;

	ret = sceMsgDialogOpen(&dialogParam);
	if (ret != SCE_OK) {
		Errorf("Failed to open dialog, ret = %x", ret);
		return -1;
	}
	while (true) {
		status = sceMsgDialogUpdateStatus();
		if (status == SCE_COMMON_DIALOG_STATUS_FINISHED) {
			break;
		}
		sceKernelUsleep(1000);
	}
	SceMsgDialogResult dialogResult;
	memset(&dialogResult, 0, sizeof(SceMsgDialogResult));
	sceMsgDialogGetResult(&dialogResult);

	sceMsgDialogClose();
	sceMsgDialogTerminate();
	if (dialogResult.buttonId == SCE_MSG_DIALOG_BUTTON_ID_BUTTON1) {
		Displayf("Button pressed: %s", firstbtn);
		return 1;
	}
	else if (dialogResult.buttonId == SCE_MSG_DIALOG_BUTTON_ID_BUTTON2) {
		Displayf("Button pressed: %s", secondbtn);
		return 0;
	}
	Warningf("Invalid selection, will assume %s", firstbtn);
	return 1;
	
}

// Shows a simple dialog message with an ok button
int ShowDialog(const char* message) {
	int32_t ret, status;
	//uint32_t i = 0;
	Displayf("in ShowDialog");
	if (!message) {
		Errorf("Invalid message string passed, aborting..");
		return -1;
	}
	Displayf("message: %s", message);
	Displayf("initializing messagedialog");
	ret = sceMsgDialogInitialize();
	if (ret != SCE_OK) {
		Errorf("Failed to initialize Message Dialog ret %x", ret);
		return ret;
	}
	Successf("Initialized Message Dialog");
	SceMsgDialogParam dialogParam;
	sceMsgDialogParamInitialize(&dialogParam);

	SceMsgDialogUserMessageParam messageParam;
	memset(&messageParam, 0, sizeof(SceMsgDialogUserMessageParam));  // Have to zero it out to not have random data (leading to a crash possibly)
	messageParam.msg = message;
	messageParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;

	dialogParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	dialogParam.userMsgParam = &messageParam;
	//dialogParam.userId = SCE_USER_SERVICE_USER_ID_EVERYONE;
	Displayf("Opening dialog");
	ret = sceMsgDialogOpen(&dialogParam);
	if (ret != SCE_OK) {
		Errorf("Failed to open dialog, ret = %x", ret);
		return ret;
	}
	Displayf("about to start checking status");
	while (true) {
		status = sceMsgDialogUpdateStatus();
		if (status == SCE_COMMON_DIALOG_STATUS_FINISHED) {
			break;
		}
		sceKernelUsleep(1000);
	}
	Successf("Closing dialog and returning");
	sceMsgDialogClose();
	sceMsgDialogTerminate();
	return SCE_OK;
}

// Loads TARGET_EXEC_PATH (if it exists)
int LoadExecutable(const char *path) {
	int ret;
	Displayf("in LoadExecutable");
	ret = ShowTwoButtonDialog("Launch from /data or /app0?", path, FALLBACK_PATH);
	const char *execpath = (ret == 0 || ret == -1) ? FALLBACK_PATH : path;
	ret = sceKernelCheckReachability(execpath);
	if (ret != SCE_OK) {
		Warningf("Selected path is not reachable, using fallback path");
		ret = ShowDialog("Selected path is not reachable, using fallback path");
		execpath = FALLBACK_PATH;
	}
	
	ret = ShowDialog("Launching Game eboot");
	if (ret != SCE_OK) {
		Errorf("showdialog failed");
	}
	ret = sceSystemServiceLoadExec(execpath, NULL);
	if (ret != SCE_OK) {
		Errorf("failed to load executable %s %x", path, ret);
		return ret;
	}
	else {
		Successf("successfully loaded executable %s ret %x?", path, ret);
		return ret;
	}
}

int main() {
	int32_t ret;
	sceSystemServiceHideSplashScreen();
	Displayf("initing commondialog\n");
	ret = sceCommonDialogInitialize();
	if (ret != SCE_OK) {
		Errorf("Failed to initialize Common Dialog");
		return ret;
	}
	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
	if (ret < SCE_SYSMODULE_LOADED) {
		Errorf("Failed to load Message Dialog PRX");
		return ret;
	}
	Successf("loaded message dialog module");
	Displayf("Starting %s", TARGET_EXEC_PATH);
	return LoadExecutable(TARGET_EXEC_PATH);
}