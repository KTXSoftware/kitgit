#include <git2.h>
#include <stdio.h>
#include <string.h>
#include "constants.h"
#include "basic_git.h"
#include "options.h"

const char* name;
char baseUrl[max_url_length];
Server* servers[max_servers + 1];
const char* projects_dir;
#ifdef SYS_WINDOWS
const char dir_sep = '\\';
#else
const char dir_sep = '/';
#endif

int index_of(const char* str, char value) {
	size_t length = strlen(str);
	for (size_t i = 0; i < length; ++i) {
		if (str[i] == value) return i;
	}
	return -1;
}

int last_index_of(const char* str, char value) {
	for (int i = strlen(str) - 1; i >= 0; --i) {
		if (str[i] == value) return i;
	}
	return -1;
}

bool starts_with(const char* string, const char* substring) {
	for (int i = 0; substring[i] != 0; ++i) {
		if (string[i] == 0) return false;
		if (string[i] != substring[i]) return false;
	}
	return true;
}

bool ends_with(const char* string, const char* substring) {
	int length = strlen(string);
	int sublength = strlen(substring);
	for (int i = length - sublength; string[i] != 0; ++i) {
		if (string[i] != substring[i - length + sublength]) return false;
	}
	return true;
}

void extract_name(const char* url, char* name) {
	int start = last_index_of(url, '/') + 1;
	int end = strlen(url);
	if (ends_with(url, ".git")) end -= 4;
	for (int i = start; i != end; ++i) {
		name[i - start] = url[i];
	}
	name[end - start] = 0;
}

void pull_recursive(const char* repo_name, const char* path);

int pull_submodule(git_submodule* sub, const char* name_, void*) {
	git_repository* parent = git_submodule_owner(sub);
	char path[max_path_length];
	strcpy(path, git_repository_workdir(parent));
	strcat(path, git_submodule_path(sub));

	char name[max_name_length];
	extract_name(git_submodule_url(sub), name);

	pull_recursive(name, path);

	return 0;
}

void pull_recursive(const char* repo_name, const char* path) {
	name = repo_name;

	git_repository* repo = NULL;
	pull(&repo, path);
	git_submodule_foreach(repo, pull_submodule, NULL);
	git_repository_free(repo);
}

void add_remotes(git_repository* repo, const char* repo_name) {
	for (int i = 0; servers[i] != 0; ++i) {
		if (servers[i]->has(repo_name)) {
			char url[max_url_length];
			strcpy(url, servers[i]->base_url);
			strcat(url, "/");
			strcat(url, repo_name);
			git_remote* remote;
			git_remote_create(&remote, repo, servers[i]->name, url);
		}
	}
}

Server* find_server(const char* repo_name) {
	for (int i = max_servers; i >= 0; --i) {
		if (servers[i] != 0 && servers[i]->has(repo_name)) return servers[i];
	}
	return 0;
}

void clone_recursive(const char* repo_name, const char* path, const char* branch);

int clone_submodule(git_submodule* sub, const char* name_, void*) {
	git_repository* parent = git_submodule_owner(sub);
	char path[max_path_length];
	strcpy(path, git_repository_workdir(parent));
	strcat(path, git_submodule_path(sub));

	char name[max_name_length];
	extract_name(git_submodule_url(sub), name);
	
	clone_recursive(name, path, git_submodule_branch(sub));

	return 0;
}

void clone_recursive(const char* repo_name, const char* path, const char* branch) {
	name = repo_name;

	Server* server = find_server(repo_name);

	char url[max_url_length];
	strcpy(url, server->base_url);
	strcat(url, "/");
	strcat(url, repo_name);

	git_repository* repo = NULL;
	clone(&repo, url, path, branch);
	add_remotes(repo, repo_name);
	git_submodule_foreach(repo, clone_submodule, 0);
	git_repository_free(repo);
}

bool is_dir(const char* dir);

void update(const char* repo_name) {
	char path[max_path_length];
	strcpy(path, projects_dir);
	strcat(path, repo_name);

	if (is_dir(path)) {
		pull_recursive(repo_name, path);
	}
	else {
		clone_recursive(repo_name, path, "master");
	}
}

int main(int argc, char** argv) {
	const char* data_path = argv[1]; //"C:\\Users\\Robert\\AppData\\Local\\Kit\\"; 
	projects_dir = argv[2]; //"C:\\Users\\Robert\\Projekte\\KitTest\\";
	
	for (int i = 0; i < max_servers + 1; ++i) {
		servers[i] = 0;
	}
	parse_options(data_path, servers);
	for (int i = 0; servers[i] != 0; ++i) {
		parse_server(data_path, servers[i]);
	}

	git_libgit2_init();
	update(argv[3]);
	//update("kraffiti");
	git_libgit2_shutdown();
}

#ifdef SYS_WINDOWS

#include <Windows.h>

bool is_dir(const char* dir) {
	DWORD attribs = GetFileAttributesA(dir);
	if (attribs == INVALID_FILE_ATTRIBUTES) return false;
	return (attribs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

#else

#include <sys/stat.h>

bool is_dir(const char* dir) {
	struct stat st;
	return stat(dir, &st) == 0 && (st.st_mode & S_IFDIR) != 0;
}

#endif
