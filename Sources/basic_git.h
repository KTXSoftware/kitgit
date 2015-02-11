#pragma once

struct git_repository;

void pull(git_repository** repo, const char* path);
void clone(git_repository** repo, const char* url, const char* path, const char* branch);
