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

void pull_recursive(const char* repo_name, const char* path);

int pull_submodule(git_submodule* sub, const char* name, void*) {
	git_repository* parent = git_submodule_owner(sub);
	char path[max_path_length];
	strcpy(path, git_repository_workdir(parent));
	strcat(path, git_submodule_path(sub));

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

int clone_submodule(git_submodule* sub, const char* name, void*) {
	git_repository* parent = git_submodule_owner(sub);
	char path[max_path_length];
	strcpy(path, git_repository_workdir(parent));
	strcat(path, git_submodule_path(sub));
	
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
	projects_dir = "C:\\Users\\Robert\\Projekte\\KitTest\\";
	const char* data_path = "C:\\Users\\Robert\\AppData\\Local\\Kit\\";
	
	for (int i = 0; i < max_servers + 1; ++i) {
		servers[i] = 0;
	}
	parse_options(data_path, servers);
	for (int i = 0; servers[i] != 0; ++i) {
		parse_server(data_path, servers[i]);
	}

	update("kraffiti");
}

#ifdef SYS_WINDOWS
#include <Windows.h>
#endif

bool is_dir(const char* dir) {
#ifdef SYS_WINDOWS
	DWORD attribs = GetFileAttributesA(dir);
	if (attribs == INVALID_FILE_ATTRIBUTES) return false;
	return (attribs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	struct stat st;
	return stat(dir, &st) == 0 && (st.st_mode & S_IFDIR) != 0;
#endif
}
