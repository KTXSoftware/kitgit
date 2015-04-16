#include <git2.h>
#include <stdio.h>
#include <string.h>
#include "constants.h"
#include "basic_git.h"
#include "options.h"

const char* name;
const char* basePath;
char baseUrl[max_url_length];

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

void pull_recursive(const char* repo_name, bool specials);

int pull_submodule(git_submodule* sub, const char* name, void*) {
	::name = name;
	char path[max_path_length];

	strcpy(path, basePath);
	strcat(path, "/");
	strcat(path, git_submodule_path(sub));

	git_repository* repo = NULL;
	pull(&repo, path);
	git_submodule_foreach(repo, pull_submodule, NULL);
	git_repository_free(repo);

	return 0;
}

void pull_recursive(const char* repo_name) {
	git_repository* repo = NULL;
	pull(&repo, repo_name);
	git_submodule_foreach(repo, pull_submodule, NULL);
	git_repository_free(repo);
}

int clone_submodule(git_submodule* sub, const char* name, void* branch) {
	::name = name;
	char path[max_path_length];
	char url[max_url_length];

	strcpy(url, baseUrl);
	strcat(url, git_submodule_url(sub) + 3);

	strcpy(path, basePath);
	strcat(path, "/");
	strcat(path, git_submodule_path(sub));

	git_repository* repo = NULL;
	clone(&repo, url, path, (const char*)branch);
	git_repository_free(repo);

	return 0;
}

struct Remote {
	const char* name;
	const char* url;
};

struct Repository {
	git_repository* repo;
	Server* server;
	Server** others;
	const char* base_url;
	const char* name;
};

void add_all_remotes(Repository* repo, Remote* remotes, int remote_count, const char* dir) {
	for (int i = 0; i < remote_count; ++i) {
		git_remote* remote;
		git_remote_create(&remote, repo->repo, remotes[i].name, remotes[i].url);
	}
}

void add_remotes(Repository* repo, const char* dir) {
	if (repo->server != NULL) {
		int remote_count = 1;
		Remote remotes[10];
		remotes[0].name = repo->server->name;
		remotes[0].url = repo->base_url;// +repo->name;

		/*for (var o in repo.others) {
			var other = repo.others[o];
			remotes.push({ name: other.server.name, url : other.baseurl + other.name });
		}*/
		add_all_remotes(repo, remotes, remote_count, dir);
	}
}

//repo, repos, branch, baseurl, dir, subdir, projectsDir, specials
void clone_recursive(const char* url, const char* path, const char* branch) {
	basePath = path;
	int index = last_index_of(url, '/');
	strncpy(baseUrl, url, last_index_of(url, '/') + 1);

	git_repository* repo = NULL;
	clone(&repo, url, path, branch);
	git_submodule_foreach(repo, clone_submodule, (void*)branch);
	git_repository_free(repo);
}

bool is_dir(const char* dir) {
	return false;
}

void update(const char* repo_name) {
	if (is_dir(repo_name)) {
		pull_recursive(repo_name);
	}
	else {
		//clone_recursive(repo, repos, "master", NULL, projects_dir, reponame, projects_dir, !is_special(reponame) && index_of(reponame, '/') == -1);
	}
}

int main(int argc, char** argv) {
	const char* data_path = "C:\\Users\\Robert\\AppData\\Local\\Kit\\";
	
	Server* servers[10];

	const char* path = "kraffiti";
	const char* url = "https://github.com/ktxsoftware/kraffiti.git";
	//clone_recursive(url, path, "master");
	//pull_recursive(path);
	parse_options(data_path, servers);
	parse_server(data_path, "ktx-github");
}
