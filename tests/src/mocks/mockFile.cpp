#include "mockFile.h"
#include "file.h"
#include "dirent.h"

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
        if (isFileSystemCaseInsensitive())
            return _strnicmp(name.c_str(), otherName, len) == 0;
        else
            return strncmp(name.c_str(), otherName, len) == 0;
    }

    std::string name;
    NodeType type;
    dirent ent;

    // union with vector seems unsafe, so let's just waste a bit of memory
    const char *data;
    size_t size;
    std::vector<size_t> children;   // use indices instead of pointers as vector might reallocate and invalidate them
    bool isBad = false;
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

static const char *ignoreVolume(const char *path)
{
    if (path && isalpha(*path) && path[1] == ':')
        path += 3;

    return path;
}

static void addNode(const char *path, NodeType nodeType, const char *data = nullptr, size_t size = 0)
{
    assert(nodeType == kFile || nodeType == kDirectory);
    assert(!m_nodes.empty() && m_nodes[0].isDirectory());

    path = ignoreVolume(path);

    size_t currentDirIndex = 0;

    visitEachPathComponent(path, [&](const char *component, size_t len, bool isLastComponent) {
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
    path = ignoreVolume(path);
    auto result = findNodeAndParent(path);
    return std::get<0>(result);
}

bool deleteFakeFile(const char *path)
{
    auto [nodeIndex, parentIndex, childIndex] = findNodeAndParent(path);
    if (nodeIndex < 0)
        return false;

    auto& parent = m_nodes[parentIndex];
    parent.children.erase(parent.children.begin() + childIndex);
    m_nodes[nodeIndex].name.clear();

    return true;
}

bool setFileAsCorrupted(const char *path, bool corrupted /* = true */)
{
    auto nodeIndex = findNode(path);
    if (nodeIndex < 0)
        return false;

    return m_nodes[nodeIndex].isBad = true;
}

DIR *opendir(const char *dirName)
{
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
    size_t parentNodeIndex = dirp->ent.d_ino;
    if (parentNodeIndex >= m_nodes.size())
        return nullptr;

    const auto& parentNode = m_nodes[parentNodeIndex];
    auto& childIndex = dirp->currentNodeIndex;

    if (dirp->currentNodeIndex < parentNode.children.size()) {
        auto nodeIndex = parentNode.children[childIndex++];
        auto& node = m_nodes[nodeIndex];
        node.ent.d_ino = nodeIndex;
        strcpy(node.ent.d_name, node.name.c_str());
        node.ent.d_namlen = node.name.size();
        return &node.ent;
    }

    return nullptr;
}


#define dirExists dirExists_REAL
std::string pathInRootDir_REAL(const char *filename);
#define pathInRootDir pathInRootDir_REAL
int loadFile_REAL(const char *path, void *buffer, bool required = true);
#define loadFile loadFile_REAL
FILE *openFile_REAL(const char *path, const char *mode = "rb");
#define openFile openFile_REAL
#include "file.cpp"
#include "mockFile.h"
#undef dirExists
#undef loadFile
#undef openFile
#undef pathInRootDir

bool dirExists(const char *path)
{
    if (m_enableMocking) {
        auto nodeIndex = findNode(path);
        return nodeIndex >= 0 && m_nodes[nodeIndex].isDirectory();
    } else {
        return dirExists_REAL(path);
    }
}

FILE *openFile(const char *, const char *)
{
    assert(false);
    return nullptr;
}

std::string pathInRootDir(const char *filename)
{
    return filename;
}

int loadFile(const char *path, void *buffer, bool required /* = true */)
{
    if (!m_enableMocking)
        return loadFile_REAL(path, buffer, required);

    auto nodeIndex = findNode(path);
    if (nodeIndex < 0)
        return -1;

    const auto& node = m_nodes[nodeIndex];
    if (!node.isFile() || node.isBad)
        return -1;

    memcpy(buffer, node.data, node.size);

    return node.size;
}

std::pair<char *, size_t> loadFile(const char *path, size_t bufferOffset /* = 0 */)
{
    if (!m_enableMocking)
        return loadFile_REAL(path, bufferOffset);

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
