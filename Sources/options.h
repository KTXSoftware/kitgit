#pragma once

enum ServerType {
	GitHub,
	GitBlit
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

void parse_options(const char* data_path, Server** servers);
void parse_server(const char* data_path, const char* server_name);
