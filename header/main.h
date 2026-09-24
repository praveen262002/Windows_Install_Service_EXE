#ifndef MAIN_H
#define MAIN_H

#define PATH_BUF_SIZE 1024

struct argument_details
{
	char *installer_exe_name;
	char *exe_name;
	char *config_filename;
	char *install_all;	
	char windows_service_name[256];
};
extern struct argument_details argument_details_t;

struct path_details
{
	char cwd[PATH_BUF_SIZE];
	char exe_file_path[PATH_BUF_SIZE];
	char config_file_path[PATH_BUF_SIZE];
	char nssm_file_path[PATH_BUF_SIZE];
	char log_folder_path[PATH_BUF_SIZE];
	char registry_file_path[PATH_BUF_SIZE];
	char service_log_folder_path[PATH_BUF_SIZE];
	char stdout_log_path[PATH_BUF_SIZE];
	char stderr_log_path[PATH_BUF_SIZE];
	char install_timestamp[64];
};
extern struct path_details path_details_t;
bool is_admin_user(void);

#endif // MAIN_H



