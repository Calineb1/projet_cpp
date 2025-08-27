#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <ctime>
#include <iomanip>
#include <algorithm>

#include "json.hpp"

using namespace std;
using json = nlohmann::json;

// Utility functions
string currentTimestamp() {
    time_t now = time(nullptr);
    tm t{};
    localtime_s(&t, &now); // Windows-safe
    ostringstream oss;
    oss << put_time(&t, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

string hashContent(const string& str) {
    return to_string(hash<string>{}(str));
}

string generateDiff(const string& oldStr, const string& newStr) {
    ostringstream oss;
    size_t maxLen = max(oldStr.size(), newStr.size());
    for (size_t i = 0; i < maxLen; ++i) {
        char o = i < oldStr.size() ? oldStr[i] : '\0';
        char n = i < newStr.size() ? newStr[i] : '\0';
        if (o != n) {
            if (o != '\0') oss << "[-" << o << "]";
            if (n != '\0') oss << "[+" << n << "]";
        }
        else {
            oss << o;
        }
    }
    return oss.str();
}

string applyDiff(const string& base, const string& diff) {
    string result;
    for (size_t i = 0; i < diff.size();) {
        if (i + 3 < diff.size() && diff[i] == '[' && (diff[i + 1] == '-' || diff[i + 1] == '+')) {
            char type = diff[i + 1];
            char val = diff[i + 2];
            i += 4; // Skip [-X] or [+X]
            if (type == '+') result += val;
        }
        else {
            result += diff[i++];
        }
    }
    return result;
}

string renderDiffWithColor(const string& diff) {
    ostringstream oss;
    for (size_t i = 0; i < diff.size();) {
        if (i + 3 < diff.size() && diff[i] == '[' && (diff[i + 1] == '-' || diff[i + 1] == '+')) {
            char type = diff[i + 1];
            char val = diff[i + 2];
            if (type == '-') oss << "\033[31m" << val << "\033[0m";
            else if (type == '+') oss << "\033[32m" << val << "\033[0m";
            i += 4;
        }
        else {
            oss << diff[i++];
        }
    }
    return oss.str();
}

// Templated document class
template <typename T>
class Document {
public:
    T content;
    Document(const T& content = T()) : content(content) {}
};

// Templated version class
template <typename T>
struct Version {
    int id = 0;
    string timestamp;
    string message;
    T content{};
    string hash;
    int parentId = 0;
    bool isCompressed = false;
    string diffWithParent;

    json toJson() const {
        return {
          {"id", id},
          {"timestamp", timestamp},
          {"message", message},
          {"content", content},
          {"hash", hash},
          {"parentId", parentId},
          {"isCompressed", isCompressed},
          {"diffWithParent", diffWithParent}
        };
    }

    static Version fromJson(const json& j) {
        return {
          j["id"].get<int>(),
          j["timestamp"].get<string>(),
          j["message"].get<string>(),
          j["content"].get<T>(),
          j["hash"].get<string>(),
          j["parentId"].get<int>(),
          j.value("isCompressed", false),
          j.value("diffWithParent", "")
        };
    }

    bool isDiffBased() const {
        return isCompressed && !diffWithParent.empty();
    }
};

// DocumentStore managing versioned documents
template <typename T>
class DocumentStore {
private:
    unordered_map<int, Version<T>> versions;
    unordered_map<string, int> branches{ {"main", 0} };
    string currentBranch = "main";

    optional<T> lastUncommitted;
    int headId = 0;
    int nextId = 1;
    T workingContent;
    const int compressionThreshold = 5;

public:
    void create(const T& content) {
        workingContent = content;
        lastUncommitted.reset();
        cout << "[Created] New document initialized.\n";
    }

    void append(const T& text) {
        lastUncommitted = workingContent;
        workingContent += text;
    }

    void removeLast(int chars) {
        lastUncommitted = workingContent;
        if ((int)workingContent.size() >= chars)
            workingContent.erase(workingContent.size() - chars);
    }

    void filter(const string& word) {
        string lowerWord = word;
        transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), ::tolower);
        for (auto& [id, v] : versions) {
            string lowerMsg = v.message;
            transform(lowerMsg.begin(), lowerMsg.end(), lowerMsg.begin(), ::tolower);
            if (lowerMsg.find(lowerWord) != string::npos)
                cout << "#" << v.id << " | " << v.timestamp << " | " << v.message << "\n";
        }
    }

    void avg() const {
        if (versions.empty()) {
            cout << "[Avg] No versions available.\n";
            return;
        }
        double totalChars = 0.0;
        for (auto& [id, v] : versions)
            totalChars += reconstructContent(id).size();
        cout << "[Avg] Average characters per version: " << fixed << setprecision(2) << totalChars / versions.size() << "\n";
    }

    void undo() {
        if (lastUncommitted) {
            workingContent = *lastUncommitted;
            lastUncommitted.reset();
            cout << "[Undo] Reverted to last uncommitted state.\n";
        }
        else {
            cout << "[Undo] Nothing to undo.\n";
        }
    }

    void loadFromFile(const string& filename) {
        ifstream in(filename);
        if (!in) {
            cerr << "[Error] Could not open file for reading.\n";
            return;
        }
        json j;
        in >> j;
        versions.clear();
        branches.clear();
        nextId = 1;

        if (j.contains("versions"))
            for (auto& [idStr, val] : j["versions"].items()) {
                Version<T> v = Version<T>::fromJson(val);
                versions[v.id] = v;
                if (v.id >= nextId) nextId = v.id + 1;
            }

        if (j.contains("branches"))
            for (auto& [name, id] : j["branches"].items())
                branches[name] = id.get<int>();

        currentBranch = j.value("currentBranch", "main");
        headId = branches[currentBranch];
        workingContent = reconstructContent(headId);
        cout << "[Load] Loaded history from " << filename << "\n";
    }

    void saveToFile(const string& filename) const {
        json j;
        j["versions"] = json::object();
        for (auto& [id, v] : versions)
            j["versions"][to_string(id)] = v.toJson();

        j["branches"] = json::object();
        for (auto& [name, id] : branches)
            j["branches"][name] = id;

        j["currentBranch"] = currentBranch;
        ofstream out(filename);
        if (!out) {
            cerr << "[Error] Could not open file for writing.\n";
            return;
        }
        out << setw(4) << j;
        cout << "[Save] History saved to " << filename << "\n";
    }

    void commit(const string& message = "") {
        string hash = hashContent(workingContent);
        Version<T> v{ nextId, currentTimestamp(), message, workingContent, hash, headId };
        if (nextId > compressionThreshold) {
            string diff = generateDiff(versions[headId].content, workingContent);
            v.diffWithParent = diff;
            v.content = "";
            v.isCompressed = true;
        }
        versions[nextId] = v;
        headId = nextId;
        branches[currentBranch] = headId;
        nextId++;
        lastUncommitted.reset();
        cout << "[Commit] Version " << headId << " saved.\n";
    }

    void log() const {
        int id = headId;
        while (id != 0) {
            auto& v = versions.at(id);
            cout << "#" << v.id << " | " << v.timestamp << " | " << v.message << "\n";
            id = v.parentId;
        }
    }

    void show(int id = -1) {
        if (id == -1) id = headId;
        if (versions.count(id))
            cout << "[Show] Version " << id << ":\n" << reconstructContent(id) << "\n";
        else
            cout << "[Error] Version not found.\n";
    }

    T reconstructContent(int id) const {
        auto& v = versions.at(id);
        return v.isDiffBased() ? applyDiff(reconstructContent(v.parentId), v.diffWithParent) : v.content;
    }

    void rollback(int id) {
        if (versions.count(id)) {
            workingContent = reconstructContent(id);
            headId = id;
            branches[currentBranch] = headId;
            cout << "[Rollback] Switched to version " << id << "\n";
        }
        else cout << "[Error] Invalid version ID.\n";
    }

    void branch(const string& name) {
        branches[name] = headId;
        cout << "[Branch] Created branch '" << name << "' at version " << headId << "\n";
    }

    void checkout(const string& name) {
        if (branches.count(name)) {
            currentBranch = name;
            headId = branches[name];
            workingContent = reconstructContent(headId);
            cout << "[Checkout] Switched to branch '" << name << "'\n";
        }
        else cout << "[Error] Branch not found.\n";
    }

    void rebase(const string& ontoBranch) {
        if (!branches.count(ontoBranch)) {
            cout << "[Error] Branch not found.\n";
            return;
        }
        int base = branches[ontoBranch];
        unordered_set<int> ancestors;
        for (int id = headId; id != 0; id = versions[id].parentId)
            ancestors.insert(id);

        int commonAncestor = 0, b = base;
        while (b != 0) {
            if (ancestors.count(b)) {
                commonAncestor = b;
                break;
            }
            b = versions[b].parentId;
        }

        if (commonAncestor == 0) {
            cout << "[Error] Cannot rebase: no common ancestor.\n";
            return;
        }

        vector<Version<T>> toReplay;
        for (int id = headId; id != commonAncestor; id = versions[id].parentId)
            toReplay.push_back(versions[id]);
        reverse(toReplay.begin(), toReplay.end());

        headId = base;
        for (auto& v : toReplay) {
            string diff = generateDiff(reconstructContent(headId), v.isDiffBased() ? reconstructContent(v.id) : v.content);
            Version<T> rebased{ nextId++, currentTimestamp(), "[rebased] " + v.message, "", hashContent(v.content), headId, true, diff };
            versions[rebased.id] = rebased;
            headId = rebased.id;
        }

        branches[currentBranch] = headId;
        workingContent = reconstructContent(headId);
        cout << "[Rebase] Rebased onto " << ontoBranch << "\n";
    }

    void diff(int v1, int v2) {
        if (!versions.count(v1) || !versions.count(v2)) {
            cout << "[Error] Invalid version ID.\n";
            return;
        }
        string rawDiff = generateDiff(reconstructContent(v1), reconstructContent(v2));
        cout << renderDiffWithColor(rawDiff) << "\n";
    }
};

// --- Main Loop ---
int main() {
    DocumentStore<string> ds;
    string cmd;
    cout << "Commands: create, append, remove, commit, undo, log, show, rollback, branch, checkout, rebase, diff, help, exit\n";

    while (true) {
        cout << "> ";
        getline(cin, cmd);
        istringstream ss(cmd);
        string op;
        ss >> op;

        if (op == "create") {
            string line;
            getline(ss, line);
            ds.create(line);
        }
        else if (op == "append") {
            string line;
            getline(ss, line);
            ds.append(line);
        }
        else if (op == "remove") {
            int n; ss >> n;
            ds.removeLast(n);
        }
        else if (op == "commit") {
            string line; getline(ss, line);
            ds.commit(line);
        }
        else if (op == "undo") ds.undo();
        else if (op == "log") ds.log();
        else if (op == "show") { int id = -1; ss >> id; ds.show(id); }
        else if (op == "rollback") { int id; ss >> id; ds.rollback(id); }
        else if (op == "branch") { string name; ss >> name; ds.branch(name); }
        else if (op == "checkout") { string name; ss >> name; ds.checkout(name); }
        else if (op == "rebase") { string name; ss >> name; ds.rebase(name); }
        else if (op == "save") ds.saveToFile("history.json");
        else if (op == "load") ds.loadFromFile("history.json");
        else if (op == "diff") { int v1, v2; ss >> v1 >> v2; ds.diff(v1, v2); }
        else if (op == "filter") { string word; ss >> word; ds.filter(word); }
        else if (op == "avg") ds.avg();
        else if (op == "exit") break;
        else if (op == "help") cout <<
            R"(
[Help] Available commands:
create <text>       - Create a new document with initial content
append <text>       - Append text to the current document
remove <n>          - Remove last n characters
commit <message>    - Commit the current document with a message
undo                - Undo last uncommitted change
log                 - Show commit history for current branch
show [id]           - Show content of version (or latest if no id)
rollback <id>       - Set head to previous version by ID
branch <name>       - Create a new branch at current head
checkout <name>     - Switch to another branch
rebase <branch>     - Rebase current branch onto another
diff <v1> <v2>      - Show diff between two versions
filter <keyword>    - Show versions with message containing keyword
avg                 - Show average number of characters per version
save                - Save history to history.json
load                - Load history from history.json
help                - Show this help message
exit                - Exit the program
)";
        else cout << "[Error] Unknown command.\n";
    }
    return 0;
}
