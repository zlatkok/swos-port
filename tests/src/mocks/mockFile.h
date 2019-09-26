#pragma once

struct MockFile {
    MockFile(const char *path, const char *data, size_t size) : path(path), data(data), size(size) {}
    MockFile(const char *path) : path(path) {}
    const char *path;
    const char *data = nullptr;
    size_t size = 0;
};

using MockFileList = std::vector<MockFile>;

void enableFileMocking(bool mockingEnabled);
void resetFakeFiles();
void addFakeFiles(const MockFileList& files);
void addFakeFile(const MockFile& file);
void addFakeDirectory(const char *path);
bool deleteFakeFile(const char *path);
bool setFileAsCorrupted(const char *path, bool corrupted = true);
