//	ebootLoader
//
//	PURPOSE: Makes debugging a lot easier by loading an easily replaceable eboot from /data
//
//	Build requirements: ORBIS SDK 1.750 or newer and Visual Studio 2017

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

// Shows a simple dialog message with an ok button
int ShowDialog(const char* message) {
	int32_t ret, status;
	//uint32_t i = 0;
	Displayf("in ShowDialog");
	if (!message) {
		Errorf("Invalid message string passed, aborting to prevent SIGSEGV..");
		return -1;
	}
	Displayf("message: %s", message);
	Displayf("initializing messagedialog");
	ret = sceMsgDialogInitialize();
	if (ret < SCE_OK) {
		Errorf("Failed to initialize Message Dialog ret %x", ret);
		return ret;
	}
	Displayf("Initialized Message Dialog");
	SceMsgDialogParam dialogParam;
	sceMsgDialogParamInitialize(&dialogParam);

	SceMsgDialogUserMessageParam messageParam;
	memset(&messageParam, 0, sizeof(SceMsgDialogUserMessageParam));  // Have to zero it out to not have random data (leading to a crash possibly)
	messageParam.msg = message;
	messageParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;

	dialogParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	dialogParam.userMsgParam = &messageParam;
	//dialogParam.userId = SCE_USER_SERVICE_USER_ID_EVERYONE;
	Displayf("opening dialog");
	ret = sceMsgDialogOpen(&dialogParam);
	if (ret < 0) {
		Errorf("failed to open dialog, ret = %x", ret);
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
	Displayf("Closing dialog and returning");
	sceMsgDialogClose();
	sceMsgDialogTerminate();
	return SCE_OK;
}

// Loads TARGET_EXEC_PATH (if it exists)
int LoadExecutable(const char *path) {
	int ret;
	Displayf("in LoadExecutable");
	ret = sceKernelCheckReachability(path);
	const char *execpath = (ret < SCE_OK) ? FALLBACK_PATH : path;
	if (strcmp(path, execpath)) {
		Warningf("%s isn't reachable, launching %s", path, execpath); 
		ShowDialog(static_cast<const char*>("Cannot launch from /data, trying /app0"));
	}
	ret = ShowDialog(static_cast<const char*>("Launching Grand Theft Auto V"));
	if (ret < 0) {
		Warningf("showdialog failed");
	}
	ret = sceSystemServiceLoadExec(execpath, NULL);
	if (ret < SCE_OK) {
		Errorf("failed to load executable %s %x", path, ret);
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
	Displayf("Starting %s", TARGET_EXEC_PATH);
	return LoadExecutable(TARGET_EXEC_PATH);
}