#pragma once

bool record_service_details(
    const char* registry_file,
    const char* service_name,
    const char* exe_path,
    const char* config_file,
    const char* log_dir,
    const char* stdout_log,
    const char* stderr_log,
    const char* timestamp);

bool file_exists(const char* path);
void list_files_in_current_directory(const char* path);
int check_folder(const char* path);
bool check_file_exists_Env(const char* filename);
bool get_pathOf_file(const char* filename, char* full_path, size_t full_path_size);
