#pragma once

extern const char* name;
extern const char* basePath;
const int max_path_length = 4096;
const int max_url_length = max_path_length;
const int max_name_length = 256;

int index_of(const char* str, char value);
int last_index_of(const char* str, char value);
