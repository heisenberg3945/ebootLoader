//	ebootLoader
//
//	PURPOSE: Makes debugging a lot easier on retail consoles by loading an easily replaceable eboot from a chosen path
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
#include <ime_dialog.h>
#include <string.h>
#include <_fs.h>
#include "Log.h"	// make sure it's in the root dir
//#include <pad.h>

#if SCE_ORBIS_SDK_VERSION < (0x03000000u)
#error Outdated SDK in use, unable to build
#endif

// needed libs 
#pragma comment(lib, "libc_stub_weak.a")
#pragma comment(lib, "libkernel_stub_weak.a")
#pragma comment(lib, "libSceCommonDialog_stub_weak.a")
#pragma comment(lib, "libSceSysmodule_stub_weak.a")
#pragma comment(lib, "libSceSystemService_stub_weak.a")
#pragma comment(lib, "libSceUserService_stub_weak.a")
#pragma comment(lib, "libScePosix_stub_weak.a")
#pragma comment(lib, "libSceMsgDialog_stub_weak.a")
#pragma comment(lib, "libSceIme_stub_weak.a")
#pragma comment(lib, "libSceImeDialog_stub_weak.a")
#pragma comment(lib, "libSceImeBackend_stub_weak.a")
//#pragma comment(lib,"libScePad_stub_weak.a")

#define TARGET_EXEC_PATH "/data/game.bin"
#define FALLBACK_PATH "/app0/game.bin"
#define HANG_TIMEOUT 1000000
#define MAX_LENGTH 256

// Inputs a string using IME Dialog library
int InputString(char *outputBuffer, size_t maxLength, const char *dialogTitle) {
	int32_t ret;
	SceUserServiceInitializeParams userServiceParams;
	memset(&userServiceParams, 0, sizeof(SceUserServiceInitializeParams));
	sceUserServiceInitialize(&userServiceParams);

	SceUserServiceUserId userId;
	ret = sceUserServiceGetInitialUser(&userId);
	if (ret != SCE_OK) {
		Errorf("Failed to get the userId of the user that started ebootLoader, ret = %x", ret);
		userId = SCE_USER_SERVICE_USER_ID_EVERYONE;
	}

	SceImeDialogParam imeParam;
	sceImeDialogParamInit(&imeParam);
	
	wchar_t inputTextBuffer[MAX_LENGTH + 1] = { 0 };
	wchar_t utf16Title[256] = { 0 };

	// Convert dialogTitle to UTF-16
	mbstowcs(utf16Title, dialogTitle, sizeof(utf16Title) / sizeof(wchar_t));

	imeParam.userId = userId;
	imeParam.type = SCE_IME_TYPE_DEFAULT;
	imeParam.supportedLanguages = SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
	imeParam.enterLabel = SCE_IME_ENTER_LABEL_DEFAULT;
	imeParam.inputMethod = SCE_IME_INPUT_METHOD_DEFAULT;
	imeParam.filter = NULL;
	imeParam.option = SCE_IME_OPTION_DEFAULT;
	imeParam.maxTextLength = maxLength; 
	imeParam.inputTextBuffer = inputTextBuffer;
	imeParam.posx = 960.0f; // Center position (half of 1920)
	imeParam.posy = 540.0f; // Center position (half of 1080)
	imeParam.horizontalAlignment = SCE_IME_HALIGN_CENTER;
	imeParam.verticalAlignment = SCE_IME_VALIGN_CENTER;
	imeParam.placeholder = NULL; 
	imeParam.title = utf16Title; 
	SceImeParamExtended extendedParam;
	memset(&extendedParam, 0, sizeof(SceImeParamExtended));
	ret = sceImeDialogInit(&imeParam, &extendedParam);
	if (ret != SCE_OK) {
		Errorf("Failed to initialize and run IME Dialog ret = %x",ret);
		return -1;
	}
	while (true) {
		if (sceImeDialogGetStatus() == SCE_IME_DIALOG_STATUS_FINISHED) {
			break;
		}
		sceKernelUsleep(1000);
	}
	
	SceImeDialogResult imeResult;
	memset(&imeResult, 0, sizeof(SceImeDialogResult));
	sceImeDialogGetResult(&imeResult);
	if (imeResult.endstatus == SCE_IME_DIALOG_END_STATUS_OK) {
		Successf("Ime Dialog succeeded");
		wcstombs(outputBuffer, inputTextBuffer, maxLength + 1);
		outputBuffer[maxLength + 1] = '\0';
		return SCE_OK;
	}
	else {
		Warningf("Ime Dialog canceled or aborted");
		outputBuffer[0] = '\0';
		return -1;
	}
}

// Shows a dialog message with 2 buttons for the user to pick
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
	memset(&messageParam, 0, sizeof(SceMsgDialogUserMessageParam)); 
	messageParam.msg = message;
	messageParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;

	dialogParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	dialogParam.userMsgParam = &messageParam;
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
	const char* execpath = nullptr;
	ret = ShowTwoButtonDialog("Would you like to input a custom path or use presets", "Custom path", "Presets");
	if (ret == 0) {
		ret = ShowTwoButtonDialog("Select the path you would like to launch from", path, FALLBACK_PATH);
		execpath = (ret == 0 || ret == -1) ? FALLBACK_PATH : path;
	}
	else {
		Warningf("Work in progress");
		char userPath[MAX_LENGTH] = { 0 };
		ret = InputString(userPath, MAX_LENGTH, "Enter Executable Path");
		if (ret == SCE_OK && strlen(userPath) > 0) {
			execpath = userPath;
			Displayf("User entered path: %s", execpath);
		}
		else {
			Warningf("Failed to get input or user canceled, defaulting to %s", FALLBACK_PATH);
			ret = ShowDialog("Failed to get input or user canceled, using fallback path");
			execpath = FALLBACK_PATH;
		}

	}
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
	ret = sceSystemServiceLoadExec(execpath, nullptr);
	if (ret == SCE_OK) {
		Successf("successfully loaded executable %s ret %x?", execpath, ret);
		return ret;
	}
	else {
		Errorf("failed to load executable %s %x", execpath, ret);
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
	if (ret != SCE_SYSMODULE_LOADED) {
		Errorf("Failed to load Message Dialog PRX");
		return ret;
	}
	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_LIBIME);
	if (ret != SCE_SYSMODULE_LOADED) {
		Errorf("Failed to load IME library");
		return -1;
	}
	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_IME_DIALOG);
	if (ret != SCE_SYSMODULE_LOADED) {
		Errorf("Failed to load IME Dialog PRX");
		return ret;
	}
	Successf("Loaded all modules");
	Displayf("Starting %s", TARGET_EXEC_PATH);
	return LoadExecutable(TARGET_EXEC_PATH);
}