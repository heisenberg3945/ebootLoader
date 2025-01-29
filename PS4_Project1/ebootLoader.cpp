//	ebootLoader
//
//	PURPOSE: Makes debugging a lot easier on retail consoles by loading an easily replaceable eboot from a chosen path
//
//	Build requirements: ORBIS SDK 3.000 or newer and Visual Studio 2017

// headers (most of them aren't even needed or used but whatever)
#include <scebase.h>
#include <kernel.h>			// Using Sony's pthread instead of the POSIX standard one
#include <stdio.h>
//#include <iostream>
#include <common_dialog.h>
#include <sys\file.h>
#include <libsysmodule.h>
#include <message_dialog.h>
#include <system_service.h>
#include <user_service.h>
#include <libime.h>
#include <ime_dialog.h>
#include <string.h>
#include <net.h>
#include <libssl.h>
#include <libhttp.h>
#include "Log.h"	// make sure it's in the root dir
#ifdef _DEBUG
#include <libdbg.h>
#endif

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
#pragma comment(lib, "libSceHttp_stub_weak.a")
#pragma comment(lib, "libSceNet_stub_weak.a")
#pragma comment(lib, "libSceSsl_stub_weak.a")
//#pragma comment(lib,"libScePad_stub_weak.a")

#define TARGET_EXEC_PATH "/data/game.bin"
#define FALLBACK_PATH "/app0/game.bin"
#define HANG_TIMEOUT 1000000
#define MAX_LENGTH 256
#define BUFFER_SIZE 8192
#define SSL_HEAP_SIZE	(304 * 1024)
#define HTTP_HEAP_SIZE	(48 * 1024)
#define NET_HEAP_SIZE	(16 * 1024)
#define TEST_URL	"https://sample.siedev.net/api_libhttp-http_get-https.html"

void FormatString(char *tbc, size_t size, const char* format, ...) {
	Warningf("Work in progress");
	va_list args;
	va_start(args, format);
	vsnprintf(tbc, size, format, args);
	va_end(args);
}

// Inputs a string using IME Dialog library
int InputString(char *outputBuffer, size_t maxLength, const char *dialogTitle, const wchar_t *placeholder, SceImeType imeType) {
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
	imeParam.type = imeType;
	imeParam.supportedLanguages = SCE_SYSTEM_PARAM_LANG_GERMAN;	// For some reason on a european PS4 German = English xd
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
	imeParam.placeholder = placeholder; 
	imeParam.title = utf16Title; 
	SceImeParamExtended extendedParam;
	memset(&extendedParam, 0, sizeof(SceImeParamExtended));
	ret = sceImeDialogInit(&imeParam, &extendedParam);
	if (ret != SCE_OK) {
		Errorf("Failed to initialize and run IME Dialog ret = %x",ret);
		return ret;
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
		wcstombs(outputBuffer, inputTextBuffer, maxLength - 1);
		outputBuffer[maxLength - 1] = '\0';
		for (size_t i = 0; i < strlen(outputBuffer); i++) {
			Displayf("%02X ", outputBuffer[i]); 
		}
		sceImeDialogTerm();
		return SCE_OK;
	}
	else {
		Warningf("Ime Dialog cancelled or aborted");
		outputBuffer[0] = '\0';
		sceImeDialogTerm();
		return -1;
	}
	
}

int ShowYesNoDialog(const char *message) {
	int32_t ret, status;
	if (!message) {
		Errorf("Invalid message string passed");
		return -1;
	}
	ret = sceMsgDialogInitialize();
	if (ret != SCE_OK) {
		Errorf("Failed to initialize Message Dialog ret %x", ret);
		return ret;
	}
	SceMsgDialogParam dialogParam;
	sceMsgDialogParamInitialize(&dialogParam);

	SceMsgDialogUserMessageParam messageParam;
	memset(&messageParam, 0, sizeof(SceMsgDialogUserMessageParam));

	messageParam.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_YESNO_FOCUS_NO;
	messageParam.msg = message;
	dialogParam.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	dialogParam.userMsgParam = &messageParam;

	ret = sceMsgDialogOpen(&dialogParam);
	if (ret != SCE_OK) {
		Errorf("Failed to open dialog, ret = %x", ret);
		return ret;
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
	if (dialogResult.buttonId == SCE_MSG_DIALOG_BUTTON_ID_YES) {
		Displayf("Pressed Yes");
		return 1;
	}
	else if (dialogResult.buttonId == SCE_MSG_DIALOG_BUTTON_ID_NO) {
		Displayf("Pressed No");
		return 0;
	}
	Warningf("Cancelled dialog");
	return -1;
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
		return ret;
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
		return ret;
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

volatile int DownloadInProgress;	// flag for download progress
									// has to be volatile so the compiler won't optimize the while statement (making it while(true))

int ShowProgressBar(const char *message, float *progress /*,const char *title*/) {
	int32_t ret, status;
	Displayf("Showing ProgressBar");
	if (!message || !progress) {
		Errorf("Invalid parameters passed");
		return -1;
	}
	ret = sceMsgDialogInitialize();
	if (ret != SCE_OK) {
		Errorf("Failed to initialize Message Dialog ret %x", ret);
		return ret;
	}
	SceMsgDialogProgressBarTarget progressbarTarget;
	SceMsgDialogParam dialogParam;
	sceMsgDialogParamInitialize(&dialogParam);

	SceMsgDialogProgressBarParam progbarParam;
	memset(&progbarParam, 0, sizeof(SceMsgDialogProgressBarParam));
	
	progbarParam.barType = SCE_MSG_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE;
	progbarParam.msg = message;

	dialogParam.mode = SCE_MSG_DIALOG_MODE_PROGRESS_BAR;
	dialogParam.progBarParam = &progbarParam;

	ret = sceMsgDialogOpen(&dialogParam);
	if (ret < SCE_OK) {
		Errorf("Couldn't open dialog ret %x", ret);
		return ret;
	}
	progressbarTarget = SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT;

	while (DownloadInProgress) {
		status = sceMsgDialogUpdateStatus();
		if (status == SCE_COMMON_DIALOG_STATUS_FINISHED) {
			Displayf("Finished with progress bar");
			break;
		}
		uint32_t progressRate = (uint32_t)(*progress * 100.0f);
		if (progressRate > 100) {
			progressRate = 100;
		}
		ret = sceMsgDialogProgressBarSetValue(progressbarTarget, progressRate);
		if (ret != SCE_OK) {
			Warningf("Failed to update progress bar, ret = %x", ret);
		}
		sceKernelUsleep(10000);
	}
	Displayf("Closing ProgressBar dialog");
	sceMsgDialogClose();
	sceMsgDialogTerminate();

	Successf("Progress Bar closed");
	return SCE_OK;
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

void *progressBarThread(void *arg) {
	float *progress = (float*)arg;
	ShowProgressBar("Downloading File", progress);
	return NULL;
}

// Downloads an executable from the internet
int DownloadExec(const char *url, const char *outputPath) {
	Warningf("Work in progress");
	pthread_t progressThread;
	int32_t ret;
	int32_t netPoolId = -1, sslCtxId = -1, httpCtxId = -1, httpTemplateId = -1, httpConnectionId = -1, httpRequestId;
	FILE *outputFile = nullptr;
	const char *actualurl = url;
	float progress = 0.0f;
	ret = sceNetInit();
	if (ret != SCE_OK) {
		Errorf("Failed to initialize Net Library ret %x", ret);
		return ret;
	}
	netPoolId = sceNetPoolCreate("simple", NET_HEAP_SIZE, 0);
	if (netPoolId < 0) {
		Errorf("Failed to create Net Pool ret %x", netPoolId);
		return netPoolId;
	}
	sslCtxId = sceSslInit(SSL_HEAP_SIZE);
	if (sslCtxId < 0) {
		Errorf("Failed to initialize SSL, cannot download ret %x", sslCtxId);
		sceNetPoolDestroy(netPoolId);
		sceNetTerm();
		return sslCtxId;
	}
	httpCtxId = sceHttpInit(netPoolId, sslCtxId, HTTP_HEAP_SIZE);
	if (sslCtxId < 0) {
		Errorf("Failed to initialize HTTP, cannot download");
		sceNetPoolDestroy(netPoolId);
		sceSslTerm(sslCtxId);
		sceNetTerm();
		return sslCtxId;
	}
	httpTemplateId = sceHttpCreateTemplate(httpCtxId,"ebootLoader/1.0", SCE_HTTP_VERSION_1_1, SCE_TRUE);
	if (httpTemplateId < 0) {
		Errorf("Failed to create Template ID ret %x", httpTemplateId);
		sceHttpTerm(httpCtxId);
		sceSslTerm(sslCtxId);
		sceNetPoolDestroy(netPoolId);
		sceNetTerm();
		return httpTemplateId;
	}
	sceHttpEnableRedirect(httpTemplateId);
	sceHttpsDisableOption(httpTemplateId, SCE_HTTPS_FLAG_SERVER_VERIFY);
	httpConnectionId = sceHttpCreateConnectionWithURL(httpTemplateId, actualurl, SCE_TRUE);
	if (httpConnectionId < 0) {
		Errorf("Failed to create Connection ID 0x%x", httpConnectionId);
		ShowDialog("Failed to create Connection with your inputted URL, trying other URLs");
		httpConnectionId = sceHttpCreateConnectionWithURL(httpTemplateId, TEST_URL, SCE_TRUE);
		if (httpConnectionId < 0) {
			Errorf("Failed to create Connection ID 0x%x", httpConnectionId);
			sceHttpDeleteTemplate(httpTemplateId);
			sceHttpTerm(httpCtxId);
			sceSslTerm(sslCtxId);
			sceNetPoolDestroy(netPoolId);
			sceNetTerm();
			return httpConnectionId;
		}
		actualurl = TEST_URL;
		Displayf("Somehow works with %s", TEST_URL);
	}
	httpRequestId = sceHttpCreateRequestWithURL(httpConnectionId, SCE_HTTP_METHOD_GET, actualurl, 0);
	if (httpRequestId < 0) {
		Errorf("Failed to create Request ID %x", httpRequestId);
		sceHttpDeleteConnection(httpConnectionId);
		sceHttpDeleteTemplate(httpTemplateId);
		sceHttpTerm(httpCtxId);
		sceSslTerm(sslCtxId);
		sceNetPoolDestroy(netPoolId);
		sceNetTerm();
		return httpRequestId;
	}
	ret = sceHttpSendRequest(httpRequestId, nullptr, 0);
	if (ret < 0) {
		Errorf("Failed to send request %x", ret);
		sceHttpDeleteRequest(httpRequestId);
		sceHttpDeleteConnection(httpConnectionId);
		sceHttpDeleteTemplate(httpTemplateId);
		sceHttpTerm(httpCtxId);
		sceSslTerm(sslCtxId);
		sceNetPoolDestroy(netPoolId);
		sceNetTerm();
		return ret;
	}
	
	int result;
	uint64_t contentLength = 0;
	uint64_t totalDownloaded = 0;
	ret = sceHttpGetResponseContentLength(httpRequestId, &result, &contentLength);
	if (ret < 0 || result != SCE_HTTP_CONTENTLEN_EXIST || contentLength == 0) {
		Errorf("Failed to get content length ret = %x, result = %x",ret, result);
		contentLength = 1; // Prevent division by zero
	}
	outputFile = fopen(outputPath, "wb");
	if (!outputFile) {
		Errorf("Failed to open file for writing: %s", outputPath);
		sceHttpDeleteRequest(httpRequestId);
		sceHttpDeleteConnection(httpConnectionId);
		sceHttpDeleteTemplate(httpTemplateId);
		sceHttpTerm(httpCtxId);
		sceSslTerm(sslCtxId);
		sceNetPoolDestroy(netPoolId);
		sceNetTerm();
		return -1;
	}
	Displayf("About to create progressThread");
	DownloadInProgress = 1;
	pthread_create(&progressThread, NULL, progressBarThread, &progress);
	char buffer[64 * 1024];
	while ((ret = sceHttpReadData(httpRequestId, buffer, sizeof(buffer))) > 0) {
		if (ret < 0) {
			Errorf("Failed to read data ret %x", ret);
			break;
		}
		totalDownloaded += ret;
		progress = (float)totalDownloaded / (float)contentLength;
		fwrite(buffer, 1, ret, outputFile);
	}
	sceKernelChmod(outputPath, 777);
	fclose(outputFile);
	sceHttpDeleteRequest(httpRequestId);
	sceHttpDeleteConnection(httpConnectionId);
	sceHttpDeleteTemplate(httpTemplateId);
	sceHttpTerm(httpCtxId);
	sceSslTerm(sslCtxId);
	sceNetPoolDestroy(netPoolId);
	sceNetTerm();
	DownloadInProgress = 0;
	pthread_join(progressThread, NULL);
	
	return (ret >= 0) ? SCE_OK : ret;
}

// Loads a user selected (and/or downloaded) executable (has to be signed or else it will be seen as invalid by Orbis)
int LoadExecutable(const char *path) {
	int32_t ret;
	Displayf("in LoadExecutable");
	const char* execpath = nullptr;
	ret = ShowYesNoDialog("Would you like to download an executable from a link?");
	if (ret == 1) {
		Warningf("Work in progress");
		char url[MAX_LENGTH] = { 0 };
		char outputPath[MAX_LENGTH] = { 0 };
		ret = InputString(url, MAX_LENGTH, "Enter URL", L"https://example.com/eboot.bin", SCE_IME_TYPE_URL);
		if (ret == SCE_OK) {
			Successf("User typed url: %s", url);
			ret = InputString(outputPath, MAX_LENGTH, "Enter Output Path", L"/data/eboot.bin", SCE_IME_TYPE_DEFAULT);
			if (ret == SCE_OK) {
				Successf("User typed path: %s", outputPath);
				ret = DownloadExec(url, outputPath);
				char str[512];
				if (ret < SCE_OK) {
					FormatString(str, sizeof(str),"Couldn't download file %s", url);
					ShowDialog(str);
				}
				FormatString(str, sizeof(str), "Successfully downloaded %s to %s", url, outputPath);
				ShowDialog(str);

			}
			else {
				ShowDialog("Invalid output path, no file will be downloaded");
			}
		}
		else {
			ShowDialog("Invalid URL, no file will be downloaded");
		}

			
	}
	else if (ret == -1) {
		ShowDialog("Invalid selection, assuming No");
	}

	ret = ShowTwoButtonDialog("Would you like to input a custom path or use presets?", "Custom path", "Presets");
	if (ret == 0) {
		ret = ShowTwoButtonDialog("Select the path you would like to launch from", path, FALLBACK_PATH);
		execpath = (ret == 0 || ret < 0) ? FALLBACK_PATH : path;
	}
	else {
		Warningf("Work in progress");
		char userPath[MAX_LENGTH] = { 0 };
		ret = InputString(userPath, MAX_LENGTH, "Enter Executable Path", L"e.g. /data/eboot.bin", SCE_IME_TYPE_DEFAULT);
		if (ret == SCE_OK && strlen(userPath)) {
			execpath = userPath;
			Displayf("User entered path: %s", execpath);
		}
		else {
			Warningf("Failed to get input or user canceled, defaulting to %s", FALLBACK_PATH);
			ShowDialog("Failed to get input or user canceled, using fallback path");
			execpath = FALLBACK_PATH;
		}

	}
	ret = sceKernelCheckReachability(execpath);
	if (ret != SCE_OK) {
		Warningf("Selected path is not reachable, using fallback path");
		ShowDialog("Selected path is not reachable, using fallback path");
		execpath = FALLBACK_PATH;
	}
	sceKernelChmod(execpath, 777);
	ShowDialog("Launching Game eboot");
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

int main(void) {
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
		return ret;
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