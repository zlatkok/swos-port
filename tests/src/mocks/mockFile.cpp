#include "mockFile.h"
#include "file.h"

#define opendir opendir_REAL
#define readdir readdir_REAL
#define closedir closedir_REAL
#include <dirent.h>
#undef opendir
#undef readdir
#undef closedir

static bool m_enableMocking;

enum NodeType { kFile, kDirectory, };

struct Node
{
    Node() : type(kDirectory) {}
    Node(NodeType type, std::string name, const char *data = nullptr, size_t size = 0)
        : type(type), name(name), data(data), size(size) {}

    bool isDirectory() const { return type == kDirectory; };
    bool isFile() const { return type == kFile; };
    bool filenameEqual(const char *otherName, size_t len) const {
#ifdef _WIN32
            return _strnicmp(name.c_str(), otherName, len) == 0;
#else
            return strncmp(name.c_str(), otherName, len) == 0;
#endif
    }

    std::string name;
    NodeType type;
    dirent ent;

    // union with vector seems unsafe, so let's just waste a bit of memory
    const char *data;
    size_t size;
    std::vector<size_t> children;   // use indices instead of pointers as vector might reallocate and invalidate them
    bool isBad = false;
    size_t numWrites = 0;
};

static std::vector<Node> m_nodes = {{ kDirectory, "C:\\" }};

void enableFileMocking(bool mockingEnabled)
{
    m_enableMocking = mockingEnabled;
}

static std::tuple<const char *, size_t> extractNextPathComponent(const char *path)
{
    size_t len = 0;
    auto endPtr = strchr(path, getDirSeparator());

    if (endPtr) {
        len = endPtr - path;
        endPtr++;
    } else {
        len = strlen(path);
    }

    return { endPtr, len };
}

static int getSubnodeIndex(const Node& node, const char *filename, size_t len)
{
    assert(node.isDirectory());

    for (size_t i = 0; i < node.children.size(); i++) {
        auto nodeIndex = node.children[i];
        assert(nodeIndex < m_nodes.size());

        if (m_nodes[node.children[i]].filenameEqual(filename, len))
            return nodeIndex;
    }

    return -1;
}

static bool isFinalComponent(const char *path)
{
    return !path || !*path || (*path == '.' && (!path[1] || (path[1] == getDirSeparator() && !path[1])));
}

template<typename F>
void visitEachPathComponent(const char *path, F f)
{
    if (!path || !*path)
        return;

    auto currentComponent = path;

    while (currentComponent) {
        auto [nextComponent, len] = extractNextPathComponent(currentComponent);
        assert(len);

        bool finalComponent = isFinalComponent(nextComponent);
        auto keepIterating = f(currentComponent, len, finalComponent);

        if (!keepIterating || finalComponent)
            break;

        currentComponent = nextComponent;
    }
}

void resetFakeFiles()
{
    m_nodes.resize(1);
    m_nodes[0].children.clear();
}

static bool isAbsolutePath(const char *path)
{
    assert(path);
    return isalpha(*path) && path[1] == ':' && path[2] == getDirSeparator();
}

static std::string ignoreVolume(std::string& path)
{
    if (isAbsolutePath(path.c_str()))
        path.erase(0, 3);

    return path;
}

static std::string normalizePath(const char *path)
{
    std::string normalPath(path);

    if (!isAbsolutePath(path))
        normalPath = joinPaths(rootDir().c_str(), path);

    return ignoreVolume(normalPath);
}

static void addNode(const char *path, NodeType nodeType, const char *data = nullptr, size_t size = 0)
{
    assert(nodeType == kFile || nodeType == kDirectory);
    assert(!m_nodes.empty() && m_nodes[0].isDirectory());

    size_t currentDirIndex = 0;
    const auto& normalPath = normalizePath(path);

    visitEachPathComponent(normalPath.c_str(), [&](const char *component, size_t len, bool isLastComponent) {
        assert(currentDirIndex < m_nodes.size() && m_nodes[currentDirIndex].isDirectory());

        auto subnodeIndex = getSubnodeIndex(m_nodes[currentDirIndex], component, len);
        if (subnodeIndex >= 0) {
            auto& subNode = m_nodes[subnodeIndex];
            if (subNode.isDirectory()) {
                currentDirIndex = subnodeIndex;
                return true;
            } else if (isLastComponent) {
                assert(nodeType != kFile || (subNode.isFile() && subNode.children.empty()));
                if (nodeType == kFile) {
                    subNode.data = data;
                    subNode.size = size;
                    subNode.numWrites++;
                } else {
                    subNode.data = nullptr;
                    subNode.size = 0;
                }
                return false;
            }
        }

        if (isLastComponent)
            m_nodes.emplace_back(nodeType, std::string(component, len), data, size);
        else
            m_nodes.emplace_back(kDirectory, std::string(component, len));

        auto nodeIndex = m_nodes.size() - 1;
        m_nodes[currentDirIndex].children.push_back(nodeIndex);
        currentDirIndex = nodeIndex;

        return !isLastComponent;
    });
}

void addFakeFiles(const MockFileList& files)
{
    for (const auto& file : files)
        addFakeFile(file);

    m_enableMocking = true;
}

void addFakeFile(const MockFile& file)
{
    addNode(file.path, kFile, file.data, file.size);
}

void addFakeDirectory(const char *path)
{
    addNode(path, kDirectory);
}

static std::tuple<int, int, int> findNodeAndParent(const char *path)
{
    assert(!m_nodes.empty() && m_nodes[0].isDirectory());

    if (path && path[0] == '.' && !path[1])
        return { 0, -1, -1 };

    int currentNodeIndex = 0;
    int parentNodeIndex = -1;
    int childIndex = -1;

    visitEachPathComponent(path, [&](const char *component, size_t len, bool isLastComponent) {
        assert(currentNodeIndex >= 0 && currentNodeIndex < static_cast<int>(m_nodes.size()));
        const auto& currentNode = m_nodes[currentNodeIndex];
        assert(currentNode.isDirectory());

        std::string name(component, len);
        auto it = std::find_if(currentNode.children.begin(), currentNode.children.end(), [&name](const auto nodeIndex) {
            assert(nodeIndex < m_nodes.size());
            const auto& node = m_nodes[nodeIndex];
            return node.filenameEqual(name.c_str(), name.size());
        });

        if (it != currentNode.children.end()) {
            parentNodeIndex = currentNodeIndex;
            childIndex = it - currentNode.children.begin();
            currentNodeIndex = *it;
            return !isLastComponent;
        } else {
            currentNodeIndex = -1;
            return false;
        }
    });

    return { currentNodeIndex, parentNodeIndex, childIndex };
}

static int findNode(const char *path)
{
    const auto& normalPath = normalizePath(path);
    auto result = findNodeAndParent(normalPath.c_str());
    return std::get<0>(result);
}

bool deleteFakeFile(const char *path)
{
    auto [nodeIndex, parentIndex, childIndex] = findNodeAndParent(path);

    if (nodeIndex < 0) {
        assert(false);
        return false;
    }

    auto& parent = m_nodes[parentIndex];
    parent.children.erase(parent.children.begin() + childIndex);
    m_nodes[nodeIndex].name.clear();

    return true;
}

size_t getNumFakeFiles()
{
    return std::count_if(m_nodes.begin(), m_nodes.end(), [](const auto& node) {
        return node.type == kFile && !node.isBad;
    });
}

bool setFileAsCorrupted(const char *path, bool corrupted /* = true */)
{
    auto nodeIndex = findNode(path);
    if (nodeIndex < 0)
        return false;

    return m_nodes[nodeIndex].isBad = true;
}

bool fakeFilesEqualByContent(const char *path1, const char *path2)
{
    auto node1Index = findNode(path1);
    if (node1Index < 0)
        return false;

    auto node2Index = findNode(path2);
    if (node2Index < 0)
        return false;

    const auto& node1 = m_nodes[node1Index];
    const auto& node2 = m_nodes[node2Index];

    if (node1.isBad || !node1.isFile() || node2.isBad || !node2.isFile())
        return false;

    if (node1.size != node2.size && memcmp(node1.data, node2.data, node1.size))
        return false;

    return true;
}

const char *getFakeFileData(const char *path, size_t& size, size_t& numWrites)
{
    auto nodeIndex = findNode(path);
    if (nodeIndex < 0)
        return nullptr;

    const auto& node = m_nodes[nodeIndex];

    if (node.type != kFile || node.isBad)
        return nullptr;

    size = node.size;
    numWrites = node.numWrites;
    return node.data;
}

size_t getFakeFileSize(const char *path)
{
    size_t size = 0, numWrites;
    getFakeFileData(path, size, numWrites);
    return size;
}

DIR *opendir(const char *dirName)
{
    if (!m_enableMocking)
        return opendir_REAL(dirName);

    auto nodeIndex = findNode(dirName);
    if (nodeIndex < 0)
        return nullptr;

    auto dir = new DIR();

    dir->ent.d_ino = nodeIndex;
    const auto& node = m_nodes[nodeIndex];

    dir->ent.d_namlen = node.name.size();
    assert(dir->ent.d_namlen < PATH_MAX);
    memcpy(dir->ent.d_name, node.name.c_str(), node.name.size() + 1);

    return dir;
}

int closedir(DIR *dirp)
{
    delete dirp;
    return dirp != nullptr;
}

dirent *readdir(DIR *dirp)
{
    if (!m_enableMocking)
        return readdir_REAL(dirp);

    size_t parentNodeIndex = dirp->ent.d_ino;
    if (parentNodeIndex >= m_nodes.size())
        return nullptr;

    const auto& parentNode = m_nodes[parentNodeIndex];
    auto& childIndex = dirp->ent.d_ino;

    if (dirp->ent.d_ino < parentNode.children.size()) {
        auto nodeIndex = parentNode.children[childIndex++];
        auto& node = m_nodes[nodeIndex];
        node.ent.d_ino = nodeIndex;
        strcpy(node.ent.d_name, node.name.c_str());
        node.ent.d_namlen = node.name.size();
        return &node.ent;
    }

    return nullptr;
}


#define LoadFile LoadFile_REAL
#define WriteFile WriteFile_REAL
namespace SWOS {
    int LoadFile_REAL();
    int WriteFile_REAL();
}

#define createDir createDir_REAL
#define dirExists dirExists_REAL
int loadFile_REAL(const char *path, void *buffer, int maxSize = -1, size_t skipBytes = 0, bool required = true);
#define loadFile loadFile_REAL
SDL_RWops *openFile_REAL(const char *path, const char *mode = "rb");
#define openFile openFile_REAL
#include "file.cpp"
#include "mockFile.h"
#undef createDir
#undef dirExists
#undef loadFile
#undef openFile
#undef WriteFile
#undef LoadFile

bool createDir(const char *path)
{
    if (m_enableMocking) {
        if (!dirExists(path))
            addFakeDirectory(path);
        return true;
    } else {
        return createDir_REAL(path);
    }
}

bool dirExists(const char *path)
{
    if (m_enableMocking) {
        auto nodeIndex = findNode(path);
        return nodeIndex >= 0 && m_nodes[nodeIndex].isDirectory();
    } else {
        return dirExists_REAL(path);
    }
}

SDL_RWops *openFile(const char *path, const char *mode /* = "rb" */)
{
    if (!m_enableMocking)
        return openFile_REAL(path, mode);

    int node = findNode(path);
    return node >= 0 ? reinterpret_cast<SDL_RWops *>(node) : nullptr;
}

int loadFile(const char *path, void *buffer, int bufferSize /* = -1 */, size_t skipBytes /* = 0 */, bool required /* = true */)
{
    if (!m_enableMocking)
        return loadFile_REAL(path, buffer, bufferSize, skipBytes, required);

    auto nodeIndex = findNode(path);
    if (nodeIndex < 0)
        return -1;

    const auto& node = m_nodes[nodeIndex];
    if (!node.isFile() || node.isBad)
        return -1;

    memcpy(buffer, node.data, node.size);

    return node.size;
}

std::pair<char *, size_t> loadFile(const char *path, size_t bufferOffset /* = 0 */, size_t skipBytes /* = 0 */)
{
    if (!m_enableMocking)
        return loadFile_REAL(path, bufferOffset, skipBytes);

    auto nodeIndex = findNode(path);
    if (nodeIndex < 0)
        return {};

    const auto& node = m_nodes[nodeIndex];
    if (!node.isFile() || node.isBad)
        return {};

    auto buf = new char[node.size + bufferOffset];
    memcpy(buf + bufferOffset, node.data, node.size);

    return { buf, node.size + bufferOffset };
}

static void writeFakeFile()
{
    auto path = joinPaths(rootDir().c_str(), A0.asPtr());
    MockFile mockFile(path.c_str(), A1.asPtr(), D1);
    addFakeFile(mockFile);
    D0 = 0;
}

int __declspec(naked) SWOS::WriteFile()
{
    writeFakeFile();
    SwosVM::flags.zero = 1;
    return D0 = 0;
}

static int loadFakeFile()
{
    if (!m_enableMocking)
        return SWOS::LoadFile_REAL();

    auto buffer = A1.asPtr();
    auto filename = A0.asPtr();

    auto path = joinPaths(rootDir().c_str(), filename);
    auto nodeIndex = findNode(path.c_str());

    if (nodeIndex < 0)
        return SWOS::LoadFile_REAL();

    const auto& node = m_nodes[nodeIndex];

    volatile auto savedSelTeamsPtr = swos.selTeamsPtr;  // unreal... and also optimizer was eliminating it X_X
    memcpy(buffer, node.data, node.size);
    swos.selTeamsPtr = savedSelTeamsPtr;

    return 0;
}

int __declspec(naked) SWOS::LoadFile()
{
    loadFakeFile();
    SwosVM::flags.zero = 1;
    return D0 = 0;
}
