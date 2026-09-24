#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE

#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <direct.h>

#include "main.h"
#include "install_service.h"
#include "file.h"

#define MAX_CONFIG_FILES 64

/* Storage definitions for global variables declared extern in main.h */
struct argument_details argument_details_t;
struct path_details path_details_t;

bool is_admin_user(void) {

	BOOL isAdmin = FALSE;
	HANDLE hToken = NULL;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		TOKEN_ELEVATION elevation;
		DWORD cbSize = sizeof(TOKEN_ELEVATION);

		if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
			isAdmin = elevation.TokenIsElevated;
		}
		CloseHandle(hToken);
	}
	return isAdmin ? true : false;
}

int main(int argc, char* argv[]) {

	argument_details_t.installer_exe_name = NULL;
	argument_details_t.exe_name = NULL;
	argument_details_t.config_filename = NULL;
	argument_details_t.windows_service_name[0] = '\0';

	path_details_t.config_file_path[0] = '\0';
	path_details_t.cwd[0] = '\0';
	path_details_t.exe_file_path[0] = '\0';
	path_details_t.nssm_file_path[0] = '\0';

	/* 1. Must run as Administrator */
	if (!is_admin_user()) {
		printf("Please run this application as Administrator.\n");
		return -1;
	}else {
		printf("Running as Administrator.\n");
	}

	printf("Argument count\t: %d\n", argc);

	/* Need at least: <exe_name> <one config file> */
	if (argc < 3) {
		printf("Usage: %s <exe_name> <config1> [config2] [config3] ...\n", argv[0]);
		printf("  <exe_name>   : name of the exe in the current folder to run as a service\n");
		printf("  <configN>    : one or more config file names (each becomes a service)\n");
		return -1;
	}

	argument_details_t.installer_exe_name = argv[0];
	argument_details_t.exe_name = argv[1];

	char config_filenames[MAX_CONFIG_FILES][256];
	int config_count = 0;

	/* Everything from argv[2] onward is a config filename */
	for (int i = 2; i < argc; i++) {

		if (config_count >= MAX_CONFIG_FILES) {
			printf("WARNING More than %d config files given, ignoring the rest.\n", MAX_CONFIG_FILES);
			break;
		}

		snprintf(config_filenames[config_count], sizeof(config_filenames[config_count]), "%s", argv[i]);
		config_count++;
	}

	printf("Installer exe name \t: %s\n", argument_details_t.installer_exe_name);
	printf("Exe name \t\t: %s\n", argument_details_t.exe_name);
	printf("Config file count \t: %d\n", config_count);

	for (int i = 0; i < config_count; i++) {
		printf("Config[%d] \t\t: %s\n", i + 1, config_filenames[i]);
	}

	// get current working directory
	if (_getcwd(path_details_t.cwd, sizeof(path_details_t.cwd)) != NULL) {
		printf("CURRENT WORKING DIRECTORY : %s\n", path_details_t.cwd);
	}
	else {
		perror("getcwd() error");
		return 1;
	}

	// get current working directory's bin folder name
	char* bin_folder_name = strrchr(path_details_t.cwd, '\\');
	if (bin_folder_name != NULL) {
		bin_folder_name++;
	}
	else {
		bin_folder_name = path_details_t.cwd;
	}
	printf("Bin Folder Name: %s\n", bin_folder_name);

	// check exe is avaliable in the current working directory
	snprintf(path_details_t.exe_file_path, sizeof(path_details_t.exe_file_path), "%s\\%s", path_details_t.cwd, argument_details_t.exe_name);
	printf("EXE PATH : %s\n", path_details_t.exe_file_path);

	if (file_exists(path_details_t.exe_file_path)) {
		printf("The %s exists!\n", argument_details_t.exe_name);
	}
	else if(check_file_exists_Env(argument_details_t.exe_name)){

		if (get_pathOf_file(argument_details_t.exe_name,path_details_t.exe_file_path,sizeof(path_details_t.exe_file_path))) {
			// Copy complete EXE path to cwd
			snprintf(path_details_t.cwd,sizeof(path_details_t.cwd),"%s",path_details_t.exe_file_path);
			// Find the last '\'
			char* last_slash = strrchr(path_details_t.cwd, '\\');

			if (last_slash != NULL) {
				// Remove "\WimDAQ_v1.0.exe"
				*last_slash = '\0';
			}
			printf("CURRENT WORKING DIRECTORY UPDATED TO : %s\n",path_details_t.cwd);
		}
		else {
			printf("The %s path is not found through environmental variable \n", argument_details_t.exe_name);
			return -1;
		}
	}
	else {
		printf("The %s does not exist.\n", argument_details_t.exe_name);
		printf("*** CONTACT WIMERA SYSTEMS ***\n");
		return -1;
	}

	// make the service registry file path
	snprintf(path_details_t.registry_file_path, sizeof(path_details_t.registry_file_path), "%s\\%s", path_details_t.cwd, "Wimera_DAQ_registry.txt");
	printf("REGISTRY FILE PAH : %s\n", path_details_t.registry_file_path);

	// check nssm.exe is avaliable
	if (file_exists(path_details_t.nssm_file_path)) {
		printf("NSSM FOUND IN CURRENT WORKING DIRECTORY : %s\n", path_details_t.nssm_file_path);
	}
	else {
		char search_buf[PATH_BUF_SIZE] = { 0 };
		DWORD len = SearchPathA(NULL, "nssm.exe", NULL, sizeof(search_buf), search_buf, NULL);
		if (len > 0 && len < sizeof(search_buf)) {
			snprintf(path_details_t.nssm_file_path, sizeof(path_details_t.nssm_file_path), "%s", search_buf);
			printf("NSSM FOUND ON PATH : %s\n", path_details_t.nssm_file_path);
		}
		else {
			printf("nssm.exe not found in CWD or system PATH. Will fallback to Win32 SCM APIs.\n");
		}
	}

	// check log folder is avaliable in the current working directory
	snprintf(path_details_t.log_folder_path, sizeof(path_details_t.log_folder_path), "%s\\%s", path_details_t.cwd, "log");
	printf("LOG FOLDER PATH : %s\n", path_details_t.log_folder_path);

	if (check_folder(path_details_t.log_folder_path)) {
		printf("The log folder exists.\n");
	}
	else {
		// create log folder if not exist
		printf("The folder does not exist.\n");
		if (_mkdir(path_details_t.log_folder_path) == 0) {
			printf("Log folder created successfully.\n");
		}
		else {
			perror("Error creating log folder");
			printf("*** CONTACT WIMERA SYSTEMS ***\n");
			return -1;
		}
	}

	int success_count = 0;
	int fail_count = 0;

	for (int i = 0; i < config_count; i++) {
		printf("\n------------------------------------------------------------\n");
		printf("Installing service %d of %d: %s\n", i + 1, config_count, config_filenames[i]);
		printf("------------------------------------------------------------\n");

		if (install_one_service(config_filenames[i])) {
			success_count++;
		}
		else {
			fail_count++;
		}
	}

	printf("\n============================================================\n");
	printf("Install summary: %d succeeded, %d failed (out of %d)\n", success_count, fail_count, config_count);
	printf("============================================================\n");
	for(int j = 0; j < config_count; j++) {
		// Remove .json or .ini extension
		char* dot = strrchr(config_filenames[j], '.');
		if (dot != NULL) {
			*dot = '\0';
		}
		get_service_details(config_filenames[j]);
	}
	printf("============================================================\n");

	return (fail_count == 0) ? 0 : -1;
}