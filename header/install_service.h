#pragma once

bool remove_existing_service(const char* service_name);
bool service_exists(const char* service_name);
bool install_service(const char* nssm_path, const char* service_name,const char* app_path,
                     const char* app_dir,const char* config_file,const char* log_dir);
bool remove_service_registry_entry(const char* registry_file, const char* service_name);
bool remove_service_log_folder(const char* log_dir, const char* service_name);
bool install_one_service(const char* config_filename);
bool get_registry_string(HKEY hRoot, const char* subKey, const char* valueName, char* output, DWORD outputSize);
void get_service_details(const char* serviceName);
bool get_registry_string(HKEY hRoot, const char* subKey, const char* valueName, char* output, DWORD outputSize);