#pragma once

enum ServerType {
	GitHub,
	GitBlit,
	GitLab
};

const int max_repos = 1024;

struct Server {
	char name[max_name_length];
	char base_url[max_url_length];
	char user[max_name_length];
	char pass[max_name_length];
	char* repos[max_repos + 1];

	Server() {
		name[0] = 0;
		base_url[0] = 0;
		user[0] = 0;
		pass[0] = 0;
		for (int i = 0; i < max_repos + 1; ++i) {
			repos[i] = 0;
		}
	}

	bool has(const char* repo);
};

const int max_servers = 32;
extern Server* servers[max_servers + 1];

void parse_options(const char* data_path, Server** servers);
void parse_server(const char* data_path, Server* server);
