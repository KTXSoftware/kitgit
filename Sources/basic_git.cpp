#include "constants.h"
#include "basic_git.h"
#include "options.h"
#include <git2.h>
#include <stdio.h>

void check_lg2(int error, const char *message, const char *extra) {
	const git_error *lg2err;
	const char *lg2msg = "", *lg2spacer = "";

	if (!error)
		return;

	if ((lg2err = giterr_last()) != NULL && lg2err->message != NULL) {
		lg2msg = lg2err->message;
		lg2spacer = " - ";
	}

	if (extra) fprintf(stderr, "%s '%s' [%d]%s%s\n", message, extra, error, lg2spacer, lg2msg);
	else fprintf(stderr, "%s [%d]%s%s\n", message, error, lg2spacer, lg2msg);

	exit(1);
}

int transfer_progress(const git_transfer_progress* stats, void* payload) {
	if (stats->received_objects < stats->total_objects) {
		printf("#%s: Received %i of %i objects (%i Bytes).\n", name, stats->received_objects, stats->total_objects, stats->received_bytes);
	}
	else {
		printf("#%s: Processing %i of %i deltas.\n", name, stats->indexed_deltas, stats->total_deltas);
	}
	return 0;
}

int get_credentials(git_cred** cred, const char* url, const char* username_from_url, unsigned int allowed_types, void* payload) {
	Server* server = 0;
	for (int i = 0; servers[i] != 0; ++i) {
		if (starts_with(url, servers[i]->base_url)) {
			server = servers[i];
			break;
		}
	}
	if (server == 0) return 1;
	git_cred_userpass_plaintext_new(cred, server->user, server->pass);
	return 0;
}

int check_certificate(git_cert* cert, int valid, const char* host, void* payload) {
	return 1;
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

	git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;
	callbacks.credentials = get_credentials;
	callbacks.transfer_progress = transfer_progress;
	callbacks.certificate_check = check_certificate;
	git_remote_connect(remote, GIT_DIRECTION_FETCH, &callbacks, NULL);
	
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
			options.checkout_strategy = GIT_CHECKOUT_SAFE;
			check_lg2(git_checkout_tree(*repo, obj, &options), "Checkout failed.", NULL);

			git_reference* newhead;
			check_lg2(git_reference_set_target(&newhead, current_branch, id, "Fast forwarding"), "Fast forward fail.", NULL);
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

void clone(git_repository** repo, const char* url, const char* path, const char* branch) {
	git_clone_options options = GIT_CLONE_OPTIONS_INIT;
	options.fetch_opts.callbacks.transfer_progress = transfer_progress;
	options.fetch_opts.callbacks.credentials = get_credentials;
	options.fetch_opts.callbacks.certificate_check = check_certificate;
	options.checkout_branch = branch;
	git_clone(repo, url, path, &options);
}
