//	ebootLoader, Help needed to fix this broken mess, VS 2017 with ORBIS SDK needed
//
//	PURPOSE: Makes debugging a lot easier by loading an easily replaceable eboot from /data
//
//	Built with ORBIS SDK 1.750.111 because Visual Studio is retarded and won't use headers from newer PS4 SDKs

// headers (most of them aren't even needed or used but whatever)
#include <scebase.h>
#include <sceerror.h>
#include <kernel.h>
#include <stdio.h>
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

// needed libs 
#pragma comment(lib, "libSceCommonDialog_stub_weak.a")
#pragma comment(lib, "libSceSysmodule_stub_weak.a")
#pragma comment(lib, "libSceSystemService_stub_weak.a")
#pragma comment(lib, "libSceUserService_stub_weak.a")
#pragma comment(lib, "libScePosix_stub_weak.a")
#pragma comment(lib, "libSceMsgDialog_stub_weak.a")

#define TARGET_EXEC_PATH "/data/ebootGTA.bin"

// shows a simple dialog message with an ok button  (NOT WORKING YET, WIP)
void ShowDialog(const char* message) {
	int loadmodule;
	printf("in ShowDialog\n");
	sceSystemServiceHideSplashScreen();
	printf("message: %s\n",message);

	loadmodule = sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
	if (loadmodule < SCE_SYSMODULE_LOADED) {
		printf("Module not loaded\n");
		return;
	}
	SceCommonDialogBaseParam baseParam;
	memset(&baseParam, 0, sizeof(SceCommonDialogBaseParam));
	_sceCommonDialogBaseParamInit(&baseParam);

	SceMsgDialogParam dialogParam;
	memset(&dialogParam, 0, sizeof(SceMsgDialogParam));
	dialogParam.baseParam = baseParam;
	dialogParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;

	int msgdialogstatus = sceMsgDialogGetStatus();
	printf("msgdialogstatus before alloc %x\n", msgdialogstatus);

	if (!dialogParam.userMsgParam) {
		printf("userMsgParam is NULL. Allocating memory...\n");
		dialogParam.userMsgParam = (SceMsgDialogUserMessageParam*)malloc(sizeof(SceMsgDialogUserMessageParam));
		if (!dialogParam.userMsgParam) {
			printf("Failed to allocate memory for userMsgParam.\n");
			sceSysmoduleUnloadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
			return;
		}
		memset(dialogParam.userMsgParam, 0, sizeof(SceMsgDialogUserMessageParam));
		printf("memset\n");
	}

	sceMsgDialogParamInitialize(&dialogParam);
	printf("outside of if\n");
	dialogParam.userMsgParam->msg = message;
	dialogParam.userMsgParam->buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
	
	printf("initing commondialog\n");
	sceCommonDialogInitialize();
	printf("initing msgdialog\n");
	sceMsgDialogInitialize();
	msgdialogstatus = sceMsgDialogGetStatus();
	printf("msgdialogstatus after Init %x\n", msgdialogstatus);

	printf("opening dialog\n");
	sceMsgDialogOpen(&dialogParam);
	sceMsgDialogUpdateStatus();
	msgdialogstatus = sceMsgDialogGetStatus();
	printf("msgdialogstatus after DialogOpen %x\n", msgdialogstatus);
	//sceMsgDialogOpen(&dialogParam);
	printf("about to start checking status\n");
	while (sceMsgDialogUpdateStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) {
		sceKernelUsleep(1000);
	}
	printf("outside of while\n");
	sceMsgDialogTerminate();
	free(dialogParam.userMsgParam);
	sceSysmoduleUnloadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
}

// ALSO NOT WORKING
void LoadExecutable(const char *path) {
	int ret, checkreachability;
	printf("in LoadExecutable\n");
	checkreachability = sceKernelCheckReachability(path);
	if (checkreachability < SCE_OK) {
		printf("not reachable\n");
		return;
	}
	else {
		printf("is reachable?\n");
	}
	ret = sceSystemServiceLoadExec(path,NULL);
	if (ret < SCE_OK) {
		printf("failed to load executable %x\n",ret);
		return;
	}
	else {
		printf("successfully loaded executable %s ret %x?",path,ret);
	}
}

int main() {
	sceSystemServiceHideSplashScreen();
	printf("Starting %s\n", TARGET_EXEC_PATH);
	const char *execpath = TARGET_EXEC_PATH;
	//const char *dialogMessage = "Starting eboot";
	//ShowDialog(dialogMessage);
	printf("Done with showdialog\n");
	LoadExecutable(execpath);
	return 0;
}