#include "constants.h"
#include "options.h"
#include <stdio.h>
#include <string.h>
#include "jsmn.h"

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
