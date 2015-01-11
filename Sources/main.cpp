#include <git2.h>
#include <stdio.h>
#include <string.h>

const int pathLength = 200;
const int urlLength = pathLength;

const char* name;

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
char baseUrl[urlLength];

int lastIndexOf(const char* str, char value) {
	for (int i = strlen(str) - 1; i >= 0; --i) {
		if (str[i] == value) return i;
	}
	return -1;
}

int submodule(git_submodule* sub, const char* name, void*) {
	::name = name;
	char path[pathLength];
	char url[urlLength];

	strcpy(url, baseUrl);
	strcat(url, git_submodule_url(sub) + 3);
	
	strcpy(path, basePath);
	strcat(path, "/");
	strcat(path, git_submodule_path(sub));

	git_repository* repo = NULL;
	git_clone_options options = GIT_CLONE_OPTIONS_INIT;
	options.remote_callbacks.transfer_progress = transfer_progress;
	git_clone(&repo, url, path, &options);

	return 0;
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

int pull() {
	git_repository* repo;
	git_reference* current_branch;
	git_reference* upstream;
	git_buf remote_name = { 0 };
	git_remote* remote;
	
	check_lg2(git_repository_open_ext(&repo, "kraffiti", 0, NULL), "failed to open repo", NULL);
	check_lg2(git_repository_head(&current_branch, repo), "failed to lookup current branch", NULL);
	check_lg2(git_branch_upstream(&upstream, current_branch), "failed to get upstream branch", NULL);
	check_lg2(git_branch_remote_name(&remote_name, repo, git_reference_name(upstream)), "failed to get the reference's upstream", NULL);
	check_lg2(git_remote_lookup(&remote, repo, remote_name.ptr), "failed to load remote", NULL);
	git_buf_free(&remote_name);
	check_lg2(git_remote_fetch(remote, NULL, NULL, NULL), "failed to fetch from upstream", NULL);
	
	{
		git_annotated_commit *merge_heads[1];
		check_lg2(git_annotated_commit_from_ref(&merge_heads[0], repo, upstream), "failed to create merge head", NULL);

		git_merge_analysis_t analysis;
		git_merge_preference_t preference;
		git_merge_analysis(&analysis, &preference, repo, (const git_annotated_commit**)merge_heads, 1);

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
			check_lg2(git_object_lookup(&obj, repo, id, GIT_OBJ_ANY), "Failed getting new head id.", NULL);

			git_checkout_options options = GIT_CHECKOUT_OPTIONS_INIT;
			options.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;
			check_lg2(git_checkout_tree(repo, obj, &options), "Checkout failed.", NULL);

			git_reference* newhead;
			check_lg2(git_reference_set_target(&newhead, current_branch, id, NULL, "Fast forwarding"), "Fast forward fail.", NULL);
		}
		else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
			check_lg2(git_merge(repo, (const git_annotated_commit**)merge_heads, 1, NULL, NULL), "failed to merge", NULL);
			{
				git_index *index;
				int has_conflicts;
				check_lg2(git_repository_index(&index, repo), "failed to load index", NULL);
				has_conflicts = git_index_has_conflicts(index);
				git_index_free(index);
				if (has_conflicts) {
					printf("There were conflicts merging. Please resolve them and commit.\n");
					git_reference_free(upstream);
					git_reference_free(current_branch);
					git_repository_free(repo);
					return 0;
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
	git_repository_free(repo);
	return 0;
}

void clone() {
	git_repository* repo = NULL;
	git_clone_options options = GIT_CLONE_OPTIONS_INIT;
	options.remote_callbacks.transfer_progress = transfer_progress;

	name = "kraffiti";
	basePath = "kraffiti";
	const char* url = "https://github.com/ktxsoftware/kraffiti.git";
	int index = lastIndexOf(url, '/');
	strncpy(baseUrl, url, lastIndexOf(url, '/') + 1);
	git_clone(&repo, url, basePath, &options);

	git_submodule_foreach(repo, submodule, NULL);
}

int main(int argc, char** argv) {
	//clone();
	pull();
}
