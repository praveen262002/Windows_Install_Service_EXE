#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE

#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "install_service.h"
#include "file.h"

#define MAX_BLOCK_LINES 64

bool remove_service_registry_entry(const char* registry_file, const char* service_name) {

	if (!registry_file || !service_name || registry_file[0] == '\0' || service_name[0] == '\0') {
		return false;
	}

	FILE* src;
	FILE* tmp;
	char tmp_path[PATH_BUF_SIZE];
	char line[PATH_BUF_SIZE];
	char block[MAX_BLOCK_LINES][PATH_BUF_SIZE];
	int block_lines;
	bool block_matches;

	src = fopen(registry_file, "r");
	if (src == NULL) {
		return true; /* Nothing to clean up yet */
	}

	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", registry_file);
	tmp = fopen(tmp_path, "w");
	if (tmp == NULL) {
		printf("[ERROR] Unable to create temp registry file:\n%s\n", tmp_path);
		fclose(src);
		return false;
	}

	block_lines = 0;
	block_matches = false;

	while (fgets(line, sizeof(line), src) != NULL) {

		if (block_lines < MAX_BLOCK_LINES) {
			snprintf(block[block_lines], sizeof(block[block_lines]), "%s", line);
			block_lines++;
		}

		if (strncmp(line, "Service Name", 12) == 0) {

			char* colon = strchr(line, ':');
			if (colon != NULL) {

				char value[256];
				char* v = colon + 1;
				size_t len = 0;

				/* skip leading spaces after the colon */
				while (*v == ' ') {
					v++;
				}

				/* copy until end of line (strip \r, \n, trailing spaces) */
				while (v[len] != '\0' && v[len] != '\r' && v[len] != '\n' && len < sizeof(value) - 1) {
					value[len] = v[len];
					len++;
				}
				value[len] = '\0';

				/* trim any trailing spaces */
				while (len > 0 && value[len - 1] == ' ') {
					len--;
					value[len] = '\0';
				}

				if (strcmp(value, service_name) == 0) {
					block_matches = true;
				}
			}
		}

		/* End of one entry = dashed line followed by blank line or start of next dashed section.
		 * We detect it when a dashed line is seen after block has already started. */
		if (block_lines >= 2 &&
			strncmp(block[0], "====", 4) == 0 &&
			strncmp(line, "====", 4) == 0) {

			/* Peek the following blank line, if present */
			char blank[PATH_BUF_SIZE];
			long pos = ftell(src);
			if (fgets(blank, sizeof(blank), src) != NULL) {
				if ((blank[0] == '\n' || blank[0] == '\r') && block_lines < MAX_BLOCK_LINES) {
					snprintf(block[block_lines], sizeof(block[block_lines]), "%s", blank);
					block_lines++;
				}
				else {
					fseek(src, pos, SEEK_SET);
				}
			}

			if (!block_matches) {
				for (int i = 0; i < block_lines; i++) {
					fputs(block[i], tmp);
				}
			}

			block_lines = 0;
			block_matches = false;
		}
	}

	/* Flush any trailing partial/malformed block as-is, unmatched */
	if (block_lines > 0 && !block_matches) {
		for (int i = 0; i < block_lines; i++) {
			fputs(block[i], tmp);
		}
	}

	fclose(src);
	fclose(tmp);

	remove(registry_file);
	if (rename(tmp_path, registry_file) != 0) {
		if (!CopyFileA(tmp_path, registry_file, FALSE)) {
			printf("[ERROR] Failed to replace registry file with updated temp file (error %lu)\n", GetLastError());
			DeleteFileA(tmp_path);
			return false;
		}
		DeleteFileA(tmp_path);
	}

	printf("Cleaned old registry entry for service '%s'\n", service_name);
	return true;
}

bool remove_service_log_folder(const char* log_dir, const char* service_name) {

	char folder_path[PATH_BUF_SIZE];
	char search_path[PATH_BUF_SIZE];
	char file_path[PATH_BUF_SIZE];
	WIN32_FIND_DATAA find_data;
	HANDLE find_handle;
	bool success = true;

	snprintf(folder_path, sizeof(folder_path), "%s\\%s", log_dir, service_name);

	/* Nothing to do if the folder doesn't exist */
	DWORD attrs = GetFileAttributesA(folder_path);
	if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
		printf("Log folder does not exist, nothing to remove: %s\n", folder_path);
		return true;
	}

	printf("Removing log folder: %s\n", folder_path);

	snprintf(search_path, sizeof(search_path), "%s\\*", folder_path);
	find_handle = FindFirstFileA(search_path, &find_data);

	if (find_handle == INVALID_HANDLE_VALUE) {
		printf("[ERROR] Unable to enumerate log folder: %s\n", folder_path);
		return false;
	}

	do {
		if (strcmp(find_data.cFileName, ".") == 0 ||
			strcmp(find_data.cFileName, "..") == 0) {
			continue;
		}

		snprintf(file_path, sizeof(file_path), "%s\\%s", folder_path, find_data.cFileName);

		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			/* Recurse into subfolder (in case rotated logs are nested) */
			char sub_service_rel[PATH_BUF_SIZE];
			snprintf(sub_service_rel, sizeof(sub_service_rel), "%s\\%s", service_name, find_data.cFileName);
			if (!remove_service_log_folder(log_dir, sub_service_rel)) {
				success = false;
			}
		}
		else {
			/* Clear read-only attribute */
			SetFileAttributesA(file_path, FILE_ATTRIBUTE_NORMAL);

			/* Retry file deletion up to 3 times to allow pending file handle closures */
			bool deleted = false;
			for (int retry = 0; retry < 3; retry++) {
				if (DeleteFileA(file_path)) {
					deleted = true;
					break;
				}
				Sleep(300);
			}

			if (!deleted) {
				printf("[ERROR] Failed to delete log file: %s (error %lu)\n",
					file_path, GetLastError());
				success = false;
			}
		}

	} while (FindNextFileA(find_handle, &find_data));

	FindClose(find_handle);

	if (!RemoveDirectoryA(folder_path)) {
		printf("[ERROR] Failed to remove log folder: %s (error %lu)\n",
			folder_path, GetLastError());
		success = false;
	}
	else {
		printf("Log folder removed: %s\n", folder_path);
	}

	return success;
}

bool remove_existing_service(const char* service_name) {

	printf("Removing existing service: %s\n", service_name);
	char command[2048];
	bool success = true;

	/* Stop service */
	snprintf(command, sizeof(command), "\"\"%s\" stop \"%s\" >nul 2>&1\"", path_details_t.nssm_file_path, service_name);
	system(command);

	/* Wait 2 seconds for service process to shutdown */
	Sleep(2000);

	/* Remove service */
	snprintf(command, sizeof(command), "\"\"%s\" remove \"%s\" confirm >nul 2>&1\"", path_details_t.nssm_file_path, service_name);
	if (system(command) != 0) {
		printf("[WARN] NSSM remove returned non-zero for service '%s'\n", service_name);
	}

	// Remove service registry entry
	if (!remove_service_registry_entry(path_details_t.registry_file_path, service_name)) {
		success = false;
	}

	// Remove service log folder
	if (!remove_service_log_folder(path_details_t.log_folder_path, service_name)) {
		success = false;
	}

	printf("Existing service removal step completed.\n");
	return success;
}

bool service_exists(const char* service_name) {
	if (!service_name || service_name[0] == '\0') return false;

	SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
	if (scm == NULL) {
		return false;
	}

	SC_HANDLE service = OpenServiceA(scm, service_name, SERVICE_QUERY_STATUS);
	if (service == NULL) {
		CloseServiceHandle(scm);
		return false;
	}

	CloseServiceHandle(service);
	CloseServiceHandle(scm);
	return true;
}

bool install_service(const char* nssm_path, const char* service_name, const char* app_path, const char* app_dir,
	const char* config_file, const char* log_dir) {

	char command[2048];
	char rollback_cmd[2048];

	// Macro for automatic cleanup / rollback if configuration fails after service creation
#define ROLLBACK_AND_RETURN(msg) \
	do { \
		printf("[ERROR] %s. Rolling back service installation for '%s'...\n", (msg), service_name); \
		snprintf(rollback_cmd, sizeof(rollback_cmd), "\"\"%s\" remove \"%s\" confirm >nul 2>&1\"", nssm_path, service_name); \
		system(rollback_cmd); \
		return false; \
	} while(0)

	//1. Install service
	snprintf(command, sizeof(command), "\"\"%s\" install \"%s\" \"%s\"\"", nssm_path, service_name, app_path);
	printf("Installing service with command: %s\n", command);
	if (system(command) != 0) {
		printf("Failed to install service.\n");
		return false;
	}

	//2. Working Directory
	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" AppDirectory \"%s\"\"", nssm_path, service_name, app_dir);
	printf("Setting working directory with command: %s\n", command);
	if (system(command) != 0) {
		ROLLBACK_AND_RETURN("Failed to set AppDirectory");
	}

	//3. Arguments
	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" AppParameters \"%s 0 0\"\"", nssm_path, service_name, config_file);
	printf("Setting arguments with command: %s\n", command);
	if (system(command) != 0) {
		ROLLBACK_AND_RETURN("Failed to set AppParameters");
	}

	//4. Startup Type
	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" Start SERVICE_AUTO_START\"", nssm_path, service_name);
	printf("Setting startup type with command: %s\n", command);
	if (system(command) != 0) {
		ROLLBACK_AND_RETURN("Failed to set startup type");
	}

	//5. Restart on Exit
	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" AppExit Default Restart\"", nssm_path, service_name);
	printf("Setting restart behavior with command: %s\n", command);
	if (system(command) != 0) {
		ROLLBACK_AND_RETURN("Failed to set restart behavior");
	}

	//6. Get Current PC Timestamp (YYYYMMDD_HHMMSS)
	SYSTEMTIME st;
	GetLocalTime(&st);
	snprintf(path_details_t.install_timestamp, sizeof(path_details_t.install_timestamp), "%04d%02d%02d_%02d%02d%02d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	printf("Install timestamp: %s\n", path_details_t.install_timestamp);

	//7. Service Log Directory
	snprintf(path_details_t.service_log_folder_path, sizeof(path_details_t.service_log_folder_path), "%s\\%s", log_dir, service_name);
	printf("Service log folder path: %s\n", path_details_t.service_log_folder_path);

	// creatting the wimera service registery
	CreateDirectoryA(log_dir, NULL);
	CreateDirectoryA(path_details_t.service_log_folder_path, NULL);
	printf("Service log folder: %s\n", path_details_t.service_log_folder_path);

	//8. Log File Paths
	snprintf(path_details_t.stdout_log_path, sizeof(path_details_t.stdout_log_path), "%s\\%s_%s_out.log",
		path_details_t.service_log_folder_path, service_name, path_details_t.install_timestamp);

	snprintf(path_details_t.stderr_log_path, sizeof(path_details_t.stderr_log_path), "%s\\%s_%s_err.log",
		path_details_t.service_log_folder_path, service_name, path_details_t.install_timestamp);

	printf("Stdout log: %s\n", path_details_t.stdout_log_path);
	printf("Stderr log: %s\n", path_details_t.stderr_log_path);

	//9. Configure Logging
	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" AppStdout \"%s\"\"", nssm_path, service_name, path_details_t.stdout_log_path);
	if (system(command) != 0) {
		ROLLBACK_AND_RETURN("Failed to configure stdout log");
	}

	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" AppStderr \"%s\"\"", nssm_path, service_name, path_details_t.stderr_log_path);
	if (system(command) != 0) {
		ROLLBACK_AND_RETURN("Failed to configure stderr log");
	}

	//10. Overwrite Logs on Restart
	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" AppStdoutCreationDisposition 2\"", nssm_path, service_name);
	system(command);

	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" AppStderrCreationDisposition 2\"", nssm_path, service_name);
	system(command);

	//11. Enable Log Rotation
	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" AppRotateFiles 1\"", nssm_path, service_name);
	system(command);

	//12. Rotate While Running
	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" AppRotateOnline 1\"", nssm_path, service_name);
	system(command);

	//13. Rotate After 10 MB
	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" AppRotateBytes 10485760\"", nssm_path, service_name);
	system(command);

	//14. Rotate Every 24 Hours
	snprintf(command, sizeof(command), "\"\"%s\" set \"%s\" AppRotateSeconds 86400\"", nssm_path, service_name);
	system(command);

	//15. Windows Service Recovery
	snprintf(command, sizeof(command), "\"sc failure \"%s\" reset= 86400 actions= restart/5000/restart/5000/restart/5000 >nul 2>&1\"", service_name);
	system(command);

	//16. Start Service
	snprintf(command, sizeof(command), "\"\"%s\" start \"%s\"\"", nssm_path, service_name);
	if (system(command) != 0) {
		ROLLBACK_AND_RETURN("Service installed but failed to start");
	}

#undef ROLLBACK_AND_RETURN

	printf("Service installed and started successfully.\n");
	return true;
}

bool install_one_service(const char* config_filename) {

	// check config file is avaliable in the config directory
	snprintf(path_details_t.config_file_path, sizeof(path_details_t.config_file_path), "%s\\..\\%s\\%s", path_details_t.cwd, "config", config_filename);
	printf("Config file path: %s\n", path_details_t.config_file_path);

	if (!file_exists(path_details_t.config_file_path)) {
		printf("The %s config file does not exist.\n", config_filename);
		printf("*** CONTACT WIMERA SYSTEMS ***\n");
		return false;
	}

	printf("The %s config file exists!\n", config_filename);

	// Extract basename of config_filename to derive clean Windows Service Name
	const char* base_name = strrchr(config_filename, '\\');
	if (base_name == NULL) {
		base_name = strrchr(config_filename, '/');
	}
	if (base_name != NULL) {
		base_name++;
	} else {
		base_name = config_filename;
	}

	snprintf(argument_details_t.windows_service_name, sizeof(argument_details_t.windows_service_name), "%s", base_name);

	// Remove .json or .ini extension
	char* dot = strrchr(argument_details_t.windows_service_name, '.');
	if (dot != NULL) {
		*dot = '\0';
	}

	printf("Windows Service Name: %s\n", argument_details_t.windows_service_name);

	// check the service is already installed or not
	if (service_exists(argument_details_t.windows_service_name)) {

		printf("Service '%s' exists.\n", argument_details_t.windows_service_name);

		// remove the existing service
		remove_existing_service(argument_details_t.windows_service_name);

	}
	else {
		// service does not exist
		printf("Service '%s' does not exist.\n", argument_details_t.windows_service_name);
	}

	if (install_service(path_details_t.nssm_file_path, argument_details_t.windows_service_name,
		path_details_t.exe_file_path, path_details_t.cwd, path_details_t.config_file_path,
		path_details_t.log_folder_path)) {

		printf("Service '%s' installed successfully.\n", argument_details_t.windows_service_name);

		if (!record_service_details(path_details_t.registry_file_path, argument_details_t.windows_service_name,
			path_details_t.exe_file_path, path_details_t.config_file_path,
			path_details_t.service_log_folder_path, path_details_t.stdout_log_path,
			path_details_t.stderr_log_path, path_details_t.install_timestamp)) {
			printf("[WARN] Service '%s' was installed, but recording details failed.\n", argument_details_t.windows_service_name);
		}

		return true;
	}
	else {
		printf("Failed to install service '%s'.\n", argument_details_t.windows_service_name);
		printf("*** CONTACT WIMERA SYSTEMS ***\n");
		return false;
	}
}

bool get_registry_string(HKEY hRoot,const char* subKey,const char* valueName,char* output,DWORD outputSize)
{
	HKEY hKey = NULL;
	DWORD type = 0;
	DWORD size = outputSize;

	memset(output, 0, outputSize);

	LONG result = RegOpenKeyExA(hRoot,subKey,0,KEY_READ,&hKey);

	if (result != ERROR_SUCCESS) {
		return false;
	}

	result = RegQueryValueExA(hKey,valueName,NULL,&type,(LPBYTE)output,&size);

	RegCloseKey(hKey);

	if (result != ERROR_SUCCESS) {
		return false;
	}

	if (type != REG_SZ && type != REG_EXPAND_SZ) {
		return false;
	}

	return true;
}

void get_service_details(const char* serviceName)
{
	SC_HANDLE hSCManager = NULL;
	SC_HANDLE hService = NULL;

	QUERY_SERVICE_CONFIGA* config = NULL;

	printf("\n");
	printf("============================================================\n");
	printf("  SERVICE DETAILS : %s\n", serviceName);
	printf("============================================================\n");
	printf("\n");

	// Open Service Control Manager
	hSCManager = OpenSCManagerA(NULL,NULL,SC_MANAGER_CONNECT);

	if (hSCManager == NULL) {

		printf("ERROR: Cannot open Service Control Manager.\n");
		printf("Error code: %lu\n", GetLastError());

		return;
	}

	// Open Service
	hService = OpenServiceA(hSCManager,serviceName,SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);
	if (hService == NULL) {

		if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
			printf("ERROR: Service \"%s\" does not exist.\n",serviceName);
		}
		else {
			printf("ERROR: Cannot open service \"%s\".\n",serviceName);
			printf("Error code: %lu\n", GetLastError());
		}
		CloseServiceHandle(hSCManager);
		return;
	}

	//// Executable / Paths
	//printf("--- Executable / Paths -------------------------------------\n");
	char regKey[512];
	snprintf(regKey,sizeof(regKey),"SYSTEM\\CurrentControlSet\\Services\\%s\\Parameters",serviceName);

	char value[1024];
	// Application
	if (get_registry_string(HKEY_LOCAL_MACHINE,regKey,"Application",value,sizeof(value))) {
		printf("Executable       : %s\n", value);
	}
	else {
		printf("Executable       : (not set)\n");
	}

	// AppDirectory
	if (get_registry_string(HKEY_LOCAL_MACHINE,regKey,"AppDirectory",value,sizeof(value))) {
		printf("Working Dir      : %s\n", value);
	}
	else {
		printf("Working Dir      : (not set)\n");
	}

	// AppParameters
	if (get_registry_string(HKEY_LOCAL_MACHINE,regKey,"AppParameters",value,sizeof(value))) {
		printf("Arguments        : %s\n", value);
	}
	else {
		printf("Arguments        : (not set)\n");
	}

	// Logging
	//printf("\n");
	//printf("--- Logging --------------------------------------------------\n");
	// AppStdout
	if (get_registry_string(HKEY_LOCAL_MACHINE,regKey,"AppStdout",value,sizeof(value))) {
		printf("Stdout Log       : %s\n", value);
	}
	else {
		printf("Stdout Log       : (not set)\n");
	}

	// AppStderr
	if (get_registry_string(HKEY_LOCAL_MACHINE,regKey,"AppStderr",value,sizeof(value))) {
		printf("Stderr Log       : %s\n", value);
	}
	else {
		printf("Stderr Log       : (not set)\n");
	}

	// Startup / Recovery
	//printf("\n");
	//printf("--- Startup / Recovery --------------------------------------\n");
	// Query Service Configuration
	DWORD bytesNeeded = 0;
	QueryServiceConfigA(hService,NULL,0,&bytesNeeded);

	if (bytesNeeded > 0) {
		config = (QUERY_SERVICE_CONFIGA*)malloc(bytesNeeded);

		if (config != NULL) {

			if (QueryServiceConfigA(hService,config,bytesNeeded,&bytesNeeded)) {

				printf("Startup Type     : ");
				switch (config->dwStartType) {

				case SERVICE_AUTO_START:
					printf("AUTO_START\n");
					break;

				case SERVICE_BOOT_START:
					printf("BOOT_START\n");
					break;

				case SERVICE_DEMAND_START:
					printf("DEMAND_START\n");
					break;

				case SERVICE_DISABLED:
					printf("DISABLED\n");
					break;

				case SERVICE_SYSTEM_START:
					printf("SYSTEM_START\n");
					break;

				default:
					printf("UNKNOWN\n");
					break;
				}
			}
			else {
				printf("Startup Type     : (unable to query)\n");
			}
		}
		else {
			printf("Startup Type     : (memory allocation failed)\n");
		}
	}
	else {
		printf("Startup Type     : (unable to query)\n");
	}

	// AppExit\Default
	char exitKey[512];
	snprintf(exitKey,sizeof(exitKey),"%s\\AppExit",regKey);

	if (get_registry_string(HKEY_LOCAL_MACHINE,exitKey,"Default",value,sizeof(value))) {
		printf("On Exit Action   : %s\n", value);
	}
	else {
		printf("On Exit Action   : (not set)\n");
	}

	// Current Status
	//printf("\n");
	//printf("--- Current Status -------------------------------------------\n");

	SERVICE_STATUS_PROCESS status;
	DWORD statusBytes = 0;
	if (QueryServiceStatusEx(hService,SC_STATUS_PROCESS_INFO,(LPBYTE)&status,sizeof(status),&statusBytes)) {

		printf("Service State     : ");
		switch (status.dwCurrentState) {

		case SERVICE_STOPPED:
			printf("STOPPED\n");
			break;

		case SERVICE_START_PENDING:
			printf("START_PENDING\n");
			break;

		case SERVICE_STOP_PENDING:
			printf("STOP_PENDING\n");
			break;

		case SERVICE_RUNNING:
			printf("RUNNING\n");
			break;

		case SERVICE_CONTINUE_PENDING:
			printf("CONTINUE_PENDING\n");
			break;

		case SERVICE_PAUSE_PENDING:
			printf("PAUSE_PENDING\n");
			break;

		case SERVICE_PAUSED:
			printf("PAUSED\n");
			break;

		default:
			printf("UNKNOWN\n");
			break;
		}
		printf("Process ID        : %lu\n",status.dwProcessId);
	}
	else {
		printf("Service State     : (unable to query)\n");
	}

	// Service Configuration
	//printf("\n");
	//printf("------------------------------------------------------------------\n");
	printf("Service Type      : ");

	if (config != NULL) {

		switch (config->dwServiceType) {

		case SERVICE_WIN32_OWN_PROCESS:
			printf("WIN32_OWN_PROCESS\n");
			break;

		case SERVICE_WIN32_SHARE_PROCESS:
			printf("WIN32_SHARE_PROCESS\n");
			break;

		default:
			printf("OTHER\n");
			break;
		}
	}
	else {
		printf("(unknown)\n");
	}
	printf("Service Name      : %s\n", serviceName);
	printf("============================================================\n");

	// Cleanup

	if (config != NULL) {
		free(config);
		config = NULL;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);
}