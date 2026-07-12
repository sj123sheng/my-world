#pragma once
#include <string>
#include <vector>
struct ManifestEntry { std::string id; size_t size; std::string hash; std::vector<std::string> deps; };
struct ResourceLoader { bool loadManifest(const char* path); bool verify(const ManifestEntry& e); };
