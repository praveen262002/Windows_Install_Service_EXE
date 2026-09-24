#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE

#include <windows.h>
#include <stdio.h>
#include <stdbool.h>

#include "file.h"

bool file_exists(const char* path)
{
	if (!path || path[0] == '\0') return false;
    DWORD attrib = GetFileAttributesA(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool check_file_exists_Env(const char* filename)
{
    if (filename == NULL || filename[0] == '\0') {
        return false;
    }

    char* path_env = getenv("PATH");
        if (path_env == NULL || path_env[0] == '\0') {
        printf("Error: PATH environment variable is not set.\n");
        return false;
        }

    // Make a copy because strtok_s modifies the string
    char path_copy[8192];
    snprintf(path_copy, sizeof(path_copy), "%s", path_env);

    char* context = NULL;
    // Windows PATH entries are separated by ;
    char* directory = strtok_s(path_copy, ";", &context);

    while (directory != NULL) {

        char full_path[MAX_PATH];
        snprintf(full_path,sizeof(full_path),"%s\\%s",directory,filename);

        printf("Checking: %s\n", full_path);

        if (file_exists(full_path)) {

            printf("Found: %s\n", full_path);
            return true;
        }

        directory = strtok_s(NULL, ";", &context);
    }

    printf("'%s' was not found in any PATH directory.\n", filename);
    return false;
}

void list_files_in_current_directory(const char* path) {
    WIN32_FIND_DATAA findData;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    char searchPath[1024];

    if (path == NULL || path[0] == '\0') {
        snprintf(searchPath, sizeof(searchPath), "*");
    } else {
        snprintf(searchPath, sizeof(searchPath), "%s\\*", path);
    }

    hFind = FindFirstFileA(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        printf("Failed to open directory '%s'. Error: %lu\n", path ? path : ".", GetLastError());
        return;
    }

    printf("%-25s %s\n", "Type", "Name");
    printf("----------------------------------------\n");

    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            printf("%-25s %s\n", "[ FOLDER ]", findData.cFileName);
        }
        else {
            printf("%-25s %s\n", "[ FILE ]", findData.cFileName);
        }

    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

int check_folder(const char* path) {
    if (path == NULL) return 0;
    DWORD attrib = GetFileAttributesA(path);
    if (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return 1; // Folder exists
    }
    return 0; // Folder does not exist or error
}

bool record_service_details(const char* registry_file, const char* service_name, const char* exe_path, const char* config_file,
	const char* log_dir, const char* stdout_log, const char* stderr_log, const char* timestamp) {

	FILE* file;

	printf("\n");
	printf("============================================================\n");
	printf("        WINDOWS SERVICE INSTALLED SUCCESSFULLY\n");
	printf("============================================================\n");
	printf("Service Name : %s\n", service_name);
	printf("Executable   : %s\n", exe_path);
	printf("Config File  : %s\n", config_file);
	printf("Log Dir      : %s\n", log_dir);
	printf("Output Log   : %s\n", stdout_log);
	printf("Error Log    : %s\n", stderr_log);
	printf("============================================================\n");
	printf("\n");

	file = fopen(registry_file, "a");
	if (file == NULL) {
		printf("[ERROR] Unable to open service registry file:\n");
		printf("%s\n", registry_file);
		return false;
	}

	fprintf(file, "============================================================\n");
	fprintf(file, "Service Name     : %s\n", service_name);
	fprintf(file, "Install Timestamp: %s\n", timestamp);
	fprintf(file, "Executable       : %s\n", exe_path);
	fprintf(file, "CONFIG File      : %s\n", config_file);
	fprintf(file, "Log Folder       : %s\n", log_dir);
	fprintf(file, "Stdout Log       : %s\n", stdout_log);
	fprintf(file, "Stderr Log       : %s\n", stderr_log);
	fprintf(file, "============================================================\n");
	fprintf(file, "\n");

	fclose(file);

	printf("[OK] Service details recorded in:\n");
	printf("%s\n", registry_file);
	printf("\n");
	printf("Thank's for choosing WIMERA SYSTEMS\n");
	return true;
}

bool get_pathOf_file(const char* filename, char* full_path, size_t full_path_size)
{
    if (filename == NULL || filename[0] == '\0') {
        return false;
    }

    if (full_path == NULL || full_path_size == 0) {
        return false;
    }

    DWORD result = SearchPathA( NULL, filename, NULL, (DWORD)full_path_size, full_path, NULL );

    if (result == 0) {
        printf("'%s' was not found in PATH.\n", filename);
        return false;
    }

    if (result >= full_path_size) {
        printf("Path buffer is too small.\n");
        return false;
    }

    printf("File found through PATH VARIABLE.\n");
    printf("EXCAT FILE PATH : %s\n", full_path);

    return true;
}

