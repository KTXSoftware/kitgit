#include <git2.h>
#include <stdio.h>
#include <string.h>
#include "jsmn.h"

const int max_path_length = 4096;
const int max_url_length = max_path_length;
const int max_name_length = 256;

const char* name;

bool is_special(const char* name) {
	return strcmp(name, "Kha") == 0 || strcmp(name, "Kore") == 0;
}

int transfer_progress(const git_transfer_progress* stats, void* payload) {
	if (stats->received_objects < stats->total_objects) {
		printf("%s: Received %i of %i objects (%i Bytes).\n", name, stats->received_objects, stats->total_objects, stats->received_bytes);
	}
	else {
		printf("%s: Processing %i of %i deltas.\n", name, stats->indexed_deltas, stats->total_deltas);
	}
	return 0;
}

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

void check_lg2(int error, const char *message, const char *extra) {
	const git_error *lg2err;
	const char *lg2msg = "", *lg2spacer = "";

	if (!error)
		return;

	if ((lg2err = giterr_last()) != NULL && lg2err->message != NULL) {
		lg2msg = lg2err->message;
		lg2spacer = " - ";
	}

	if (extra)
		fprintf(stderr, "%s '%s' [%d]%s%s\n",
		message, extra, error, lg2spacer, lg2msg);
	else
		fprintf(stderr, "%s [%d]%s%s\n",
		message, error, lg2spacer, lg2msg);

	exit(1);
}

void pull(git_repository** repo, const char* path) {
	git_reference* current_branch;
	git_reference* upstream;
	git_buf remote_name = { 0 };
	git_remote* remote;
	
	check_lg2(git_repository_open_ext(repo, path, 0, NULL), "failed to open repo", NULL);
	check_lg2(git_repository_head(&current_branch, *repo), "failed to lookup current branch", NULL);
	check_lg2(git_branch_upstream(&upstream, current_branch), "failed to get upstream branch", NULL);
	check_lg2(git_branch_remote_name(&remote_name, *repo, git_reference_name(upstream)), "failed to get the reference's upstream", NULL);
	check_lg2(git_remote_lookup(&remote, *repo, remote_name.ptr), "failed to load remote", NULL);
	git_buf_free(&remote_name);
	check_lg2(git_remote_fetch(remote, NULL, NULL, NULL), "failed to fetch from upstream", NULL);
	
	{
		git_annotated_commit *merge_heads[1];
		check_lg2(git_annotated_commit_from_ref(&merge_heads[0], *repo, upstream), "failed to create merge head", NULL);

		git_merge_analysis_t analysis;
		git_merge_preference_t preference;
		git_merge_analysis(&analysis, &preference, *repo, (const git_annotated_commit**)merge_heads, 1);

		if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
			printf("Up to date\n");
		}
		else if (analysis & GIT_MERGE_ANALYSIS_NONE || analysis & GIT_MERGE_ANALYSIS_UNBORN) {
			printf("No merge possible\n");
		}
		else if (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) {
			printf("Fast forward\n");
			const git_oid* id = git_annotated_commit_id(merge_heads[0]);

			git_object* obj;
			check_lg2(git_object_lookup(&obj, *repo, id, GIT_OBJ_ANY), "Failed getting new head id.", NULL);

			git_checkout_options options = GIT_CHECKOUT_OPTIONS_INIT;
			options.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;
			check_lg2(git_checkout_tree(*repo, obj, &options), "Checkout failed.", NULL);

			git_reference* newhead;
			check_lg2(git_reference_set_target(&newhead, current_branch, id, NULL, "Fast forwarding"), "Fast forward fail.", NULL);
		}
		else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
			check_lg2(git_merge(*repo, (const git_annotated_commit**)merge_heads, 1, NULL, NULL), "failed to merge", NULL);
			{
				git_index *index;
				int has_conflicts;
				check_lg2(git_repository_index(&index, *repo), "failed to load index", NULL);
				has_conflicts = git_index_has_conflicts(index);
				git_index_free(index);
				if (has_conflicts) {
					printf("There were conflicts merging. Please resolve them and commit.\n");
				}
				else {
					git_index *index;
					git_buf message = { 0 };
					git_oid commit_id, tree_id;
					git_commit *parents[2];
					git_signature *user;
					git_tree *tree;
					
					check_lg2(git_repository_index(&index, *repo), "failed to load index", NULL);
					check_lg2(git_index_write_tree(&tree_id, index), "failed to write tree", NULL);
					git_index_free(index);
					
					check_lg2(git_signature_default(&user, *repo), "failed to get user's ident", NULL);
					check_lg2(git_repository_message(&message, *repo), "failed to get message", NULL);
					
					check_lg2(git_tree_lookup(&tree, *repo, &tree_id), "failed to lookup tree", NULL);
					
					check_lg2(git_commit_lookup(&parents[0], *repo, git_reference_target(current_branch)), "failed to lookup first parent", NULL);
					check_lg2(git_commit_lookup(&parents[1], *repo, git_reference_target(upstream)), "failed to lookup second parent", NULL);

					check_lg2(git_commit_create(&commit_id, *repo, "HEAD", user, user, NULL, message.ptr, tree, 2, (const git_commit **)parents), "failed to create commit", NULL);

					git_tree_free(tree);
					git_signature_free(user);
				}
			}
		}
		else {
			printf("Unknown merge state.\n");
		}
				
		git_annotated_commit_free(merge_heads[0]);
	}
	
	git_reference_free(upstream);
	git_reference_free(current_branch);
}

void pull_recursive(const char* repo_name, bool specials);

int pull_submodule(git_submodule* sub, const char* name, void* specials) {
	::name = name;
	char path[max_path_length];

	strcpy(path, basePath);
	strcat(path, "/");
	strcat(path, git_submodule_path(sub));

	if (is_special(name) && *(bool*)specials) {
		pull_recursive(name, false);
	}
	
	git_repository* repo = NULL;
	pull(&repo, path);
	git_submodule_foreach(repo, pull_submodule, NULL);
	git_repository_free(repo);

	return 0;
}

void pull_recursive(const char* repo_name, bool specials = true) {
	git_repository* repo = NULL;
	pull(&repo, repo_name);
	git_submodule_foreach(repo, pull_submodule, &specials);
	git_repository_free(repo);
}

void clone(git_repository** repo, const char* url, const char* path, const char* branch) {
	git_clone_options options = GIT_CLONE_OPTIONS_INIT;
	options.remote_callbacks.transfer_progress = transfer_progress;
	options.checkout_branch = branch;
	git_clone(repo, url, basePath, &options);
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

struct Server {
	char name[max_name_length];
	char base_url[max_url_length];
	char user[max_name_length];
	char pass[max_name_length];

	Server() {
		name[0] = 0;
		base_url[0] = 0;
		user[0] = 0;
		pass[0] = 0;
	}
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
		pull_recursive(repo_name, !is_special(repo_name) && index_of(repo_name, '/') == -1);
	}
	else {
		//clone_recursive(repo, repos, "master", NULL, projects_dir, reponame, projects_dir, !is_special(reponame) && index_of(reponame, '/') == -1);
	}
}

enum ServerType {
	GitHub,
	GitBlit
};

void copy_string_token(char* to, jsmntok_t* from_token, char* from_string) {
	int size = from_token->end - from_token->start;
	for (int i = 0; i < size; ++i) {
		to[i] = from_string[from_token->start + i];
	}
	to[size] = 0;
}

int compare_string_token(char* value, jsmntok_t* token, char* string) {
	return strncmp(value, &string[token->start], token->end - token->start);
}

Server* parseServer(jsmntok_t* tokens, int token_count, char* json_string, int& index) {
	Server* server = new Server;
	ServerType type = GitHub;
	char path[max_path_length];
	char url[max_url_length];

	for (; index < token_count; ++index) {
		if (tokens[index].type == JSMN_STRING) {
			if (compare_string_token("name", &tokens[index], json_string) == 0) {
				++index;
				copy_string_token(server->name, &tokens[index], json_string);
			}
			else if (compare_string_token("path", &tokens[index], json_string) == 0) {
				++index;
				copy_string_token(path, &tokens[index], json_string);
			}
			else if (compare_string_token("url", &tokens[index], json_string) == 0) {
				++index;
				copy_string_token(url, &tokens[index], json_string);
			}
			else if (compare_string_token("user", &tokens[index], json_string) == 0) {
				++index;
				copy_string_token(server->user, &tokens[index], json_string);
			}
			else if (compare_string_token("pass", &tokens[index], json_string) == 0) {
				++index;
				copy_string_token(server->pass, &tokens[index], json_string);
			}
			else if (compare_string_token("type", &tokens[index], json_string) == 0) {
				++index;
				if (compare_string_token("gitblit", &tokens[index], json_string) == 0) {
					type = GitBlit;
				}
			}
		}
	}
	if (type == GitHub) {
		strcpy(server->base_url, "https://github.com/");
		strcat(server->base_url, path);
	}
	else {
		strcpy(server->base_url, "https://");
		strcat(server->base_url, url);
	}
	return server;
}

void parse_options(const char* data_path, Server** servers) {
	char options_path[max_path_length];
	strcpy(options_path, data_path);
	strcat(options_path, "options.json");

	char json_string[1024];
	FILE* file;
	file = fopen(options_path, "rb");
	size_t length = fread(json_string, 1, 1024, file);
	fclose(file);

	jsmn_parser parser;
	jsmntok_t tokens[100];
	jsmn_init(&parser);
	int token_count = jsmn_parse(&parser, json_string, length, tokens, 100);
	for (int i = 0; i < token_count; ++i) {
		if (tokens[i].type == JSMN_STRING && strncmp("servers", &json_string[tokens[i].start], tokens[i].end - tokens[i].start) == 0) {
			++i;
			int array_size = tokens[i].size;
			++i;
			int server_index = 0;
			for (int i2 = 0; i2 < array_size; ++i2) {
				int size = tokens[i].size;
				++i;
				servers[server_index] = parseServer(tokens, i + size * 2, json_string, i);
				++server_index;
			}
			servers[server_index] = NULL;
		}
	}
}

void parse_server(const char* data_path, const char* server_name) {
	char server_path[max_path_length];
	strcpy(server_path, data_path);
	strcat(server_path, server_name);
	strcat(server_path, ".json");

	char json_string[4096];
	FILE* file;
	file = fopen(server_path, "rb");
	size_t length = fread(json_string, 1, 4096, file);
	fclose(file);

	jsmn_parser parser;
	jsmntok_t tokens[256];
	jsmn_init(&parser);
	int token_count = jsmn_parse(&parser, json_string, length, tokens, 256);
	for (int i = 0; i < token_count; ++i) {
		if (tokens[i].type == JSMN_STRING && strncmp("repositories", &json_string[tokens[i].start], tokens[i].end - tokens[i].start) == 0) {
			++i;
			int array_size = tokens[i].size;
			++i;
			int repo_index = 0;
			for (int i2 = 0; i2 < array_size; ++i2) {
				char name[max_name_length];
				for (int i3 = tokens[i].start; i3 != tokens[i].end; ++i3) {
					name[i3 - tokens[i].start] = json_string[i3];
				}
				name[tokens[i].end - tokens[i].start] = 0;
				++i;
			}
		}
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
