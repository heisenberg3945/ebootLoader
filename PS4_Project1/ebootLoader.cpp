//	ebootLoader PoC
//
//	PURPOSE: Makes debugging a lot easier by loading an easily replaceable eboot from /data
//
//	Built with ORBIS SDK 1.750.111 because Visual Studio is retarded and won't use headers from newer PS4 SDKs

// headers (most of them aren't even needed or used but whatever)
#include <scebase.h>
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

// shows a simple dialog message with an ok button
void ShowDialog(const char* message) {
	printf("in ShowDialog\n");
	sceSystemServiceHideSplashScreen();
	printf("message: %s\n",message);
	if (sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG) != SCE_SYSMODULE_LOADED) {
		printf("Module not loaded\n");
		return;
	}
	SceCommonDialogBaseParam baseParam;
	memset(&baseParam, 0, sizeof(SceCommonDialogBaseParam));
	_sceCommonDialogBaseParamInit(&baseParam);
	SceMsgDialogParam dialogParam;
	memset(&dialogParam, 0, sizeof(SceMsgDialogParam));
	sceMsgDialogParamInitialize(&dialogParam);
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
	printf("outside of if\n");
	dialogParam.userMsgParam->msg = message;
	dialogParam.userMsgParam->buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
	printf("opening dialog\n");
	printf("initing commondialog\n");
	sceCommonDialogInitialize();
	printf("initing msgdialog\n");
	sceMsgDialogInitialize();
	msgdialogstatus = sceMsgDialogGetStatus();
	printf("msgdialogstatus after Init %x\n", msgdialogstatus);
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

void LoadExecutable(const char *path) {
	int ret, checkreachability;
	printf("in LoadExecutable\n");
	checkreachability = sceKernelCheckReachability(path);
	printf("is reachable %x", checkreachability);
	ret = sceSystemServiceLoadExec(path,NULL);
	printf("Trying to load executable %s rc %x", path, ret);
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