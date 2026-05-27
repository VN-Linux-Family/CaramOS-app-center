// ─────────────────────────────────────────────────────────────────────────────
// App Explorer — package_manager.cpp
// ─────────────────────────────────────────────────────────────────────────────

#include "package_manager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <glib.h>
#include <gtk/gtk.h>

namespace AppExplorer {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// ─────────────────────────────────────────────────────────────────────────────
// OperationContext
// ─────────────────────────────────────────────────────────────────────────────

void OperationContext::appendLog(LogLevel level, const std::string& text) {
    std::lock_guard<std::mutex> lk(log_mutex);
    log_lines.push_back({level, text, nowMs()});
    if (on_log_line) {
        LogLine line{level, text, nowMs()};
        // Schedule callback on GTK main thread
        auto* line_copy = new LogLine{level, text, nowMs()};
        auto* cb = new std::function<void(const LogLine&)>(on_log_line);
        g_idle_add([](gpointer ud) -> gboolean {
            auto* pair = static_cast<std::pair<
                std::function<void(const LogLine&)>*, LogLine*>*>(ud);
            (*pair->first)(*pair->second);
            delete pair->first;
            delete pair->second;
            delete pair;
            return G_SOURCE_REMOVE;
        }, new std::pair<decltype(cb), decltype(line_copy)>(cb, line_copy));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// AepkgParser
// ─────────────────────────────────────────────────────────────────────────────

AepkgParser::AepkgParser() = default;
AepkgParser::~AepkgParser() = default;

bool AepkgParser::parseFile(const std::string& path, AppPackage& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        last_error_ = "Cannot open: " + path;
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    bool ok = parseString(ss.str(), out);
    if (ok) out.pkg_file_path = path;
    return ok;
}

bool AepkgParser::parseString(const std::string& content, AppPackage& out) {
    IniMap ini;
    if (!parseIni(content, ini)) return false;
    populate(ini, out);
    return true;
}

bool AepkgParser::parseIni(const std::string& content, IniMap& out) {
    std::istringstream ss(content);
    std::string line, section;
    int lineno = 0;
    while (std::getline(ss, line)) {
        lineno++;
        // Line continuation
        while (!line.empty() && line.back() == '\\') {
            line.pop_back();
            std::string next;
            if (!std::getline(ss, next)) break;
            line += trim(next);
        }
        line = trim(line);
        if (line.empty() || line[0] == ';') continue;
        // Strip comment: # setelah spasi
        for (size_t i = 0; i < line.size(); i++) {
            if (line[i] == '#') {
                if (i == 0) { line = ""; break; }
                // check if inside value after '='
                size_t eq = line.find('=');
                if (eq != std::string::npos && i > eq) {
                    // '#' in value — check if preceded by space
                    if (line[i-1]==' '||line[i-1]=='\t') {
                        line = trim(line.substr(0,i)); break;
                    }
                } else {
                    line = trim(line.substr(0,i)); break;
                }
            }
        }
        if (line.empty()) continue;
        if (line.front()=='[' && line.back()==']') {
            section = trim(line.substr(1, line.size()-2));
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq+1));
        if (!key.empty()) out[section][key] = val;
    }
    return true;
}

std::string AepkgParser::getString(const IniMap& ini, const std::string& sec,
                                    const std::string& key, const std::string& def) const {
    auto sit = ini.find(sec);
    if (sit==ini.end()) return def;
    auto kit = sit->second.find(key);
    if (kit==sit->second.end()) return def;
    return kit->second;
}

bool AepkgParser::getBool(const IniMap& ini, const std::string& sec,
                           const std::string& key, bool def) const {
    std::string v = getString(ini,sec,key,"");
    if (v.empty()) return def;
    std::string lv = toLower(v);
    if (lv=="true"||lv=="1"||lv=="yes") return true;
    if (lv=="false"||lv=="0"||lv=="no") return false;
    return def;
}

std::vector<std::string> AepkgParser::splitList(const std::string& s, char delim) const {
    std::vector<std::string> result;
    if (s.empty()) return result;
    std::istringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        item = trim(item);
        if (!item.empty()) result.push_back(item);
    }
    return result;
}

InstallMethod AepkgParser::parseMethod(const std::string& s) const {
    std::string l = toLower(trim(s));
    if (l=="apt")      return InstallMethod::APT;
    if (l=="dnf")      return InstallMethod::DNF;
    if (l=="pacman")   return InstallMethod::PACMAN;
    if (l=="flatpak")  return InstallMethod::FLATPAK;
    if (l=="snap")     return InstallMethod::SNAP;
    if (l=="script")   return InstallMethod::SCRIPT;
    if (l=="appimage") return InstallMethod::APPIMAGE;
    if (l=="deb")      return InstallMethod::DEB;
    if (l=="rpm")      return InstallMethod::RPM;
    if (l=="tar")      return InstallMethod::TAR;
    return InstallMethod::UNKNOWN;
}

AppCategory AepkgParser::parseCategory(const std::string& s) const {
    std::string l = toLower(trim(s));
    if (l=="browser")  return AppCategory::BROWSER;
    if (l=="editor")   return AppCategory::EDITOR;
    if (l=="media")    return AppCategory::MEDIA;
    if (l=="dev")      return AppCategory::DEV;
    if (l=="game")     return AppCategory::GAME;
    if (l=="util")     return AppCategory::UTIL;
    if (l=="sys")      return AppCategory::SYS;
    if (l=="net")      return AppCategory::NET;
    if (l=="graphic")  return AppCategory::GRAPHIC;
    if (l=="office")   return AppCategory::OFFICE;
    return AppCategory::OTHER;
}

void AepkgParser::populate(const IniMap& ini, AppPackage& p) {
    auto S = [&](const std::string& s, const std::string& k, const std::string& d="") {
        return getString(ini,s,k,d);
    };
    auto B = [&](const std::string& s, const std::string& k, bool d=false) {
        return getBool(ini,s,k,d);
    };

    // [meta]
    p.id             = S("meta","id");
    p.name           = S("meta","name");
    p.version        = S("meta","version");
    p.description    = S("meta","description");
    p.long_desc      = S("meta","long_desc");
    // Replace literal \n
    for (size_t i=0; (i=p.long_desc.find("\\n",i))!=std::string::npos; ) {
        p.long_desc.replace(i,2,"\n"); i++;
    }
    p.usage = S("meta","usage");
    for (size_t i=0; (i=p.usage.find("\\n",i))!=std::string::npos; ) {
        p.usage.replace(i,2,"\n"); i++;
    }
    p.category_str = S("meta","category","other");
    p.category     = parseCategory(p.category_str);
    p.tags         = splitList(S("meta","tags"), ';');
    p.author       = S("meta","author");
    p.maintainer   = S("meta","maintainer");
    p.license      = S("meta","license");
    p.homepage     = S("meta","homepage");
    p.source_url   = S("meta","source_url");
    p.arch         = splitList(S("meta","arch"), ';');
    p.os           = splitList(S("meta","os"), ';');
    p.min_os_ver   = S("meta","min_os_ver");

    // [source]
    p.icon_online   = S("source","icon_online");
    p.icon_offline  = S("source","icon_offline");
    p.icon_size     = S("source","icon_size","64x64");
    p.icon_format   = S("source","icon_format","png");
    p.screenshot_online  = splitList(S("source","screenshot_online"),';');
    p.screenshot_offline = splitList(S("source","screenshot_offline"),';');
    p.pkg_online    = S("source","pkg_online");
    p.pkg_offline   = S("source","pkg_offline");

    // [install]
    p.install_method      = parseMethod(S("install","method"));
    p.packages            = splitList(S("install","packages"), ' ');
    p.flatpak_id          = S("install","flatpak_id");
    p.snap_name           = S("install","snap_name");
    p.install_script      = S("install","script");
    p.install_script_file = S("install","script_file");
    p.deps                = splitList(S("install","deps"), ' ');
    p.sudo_required       = B("install","sudo_required", true);
    p.interactive         = B("install","interactive", false);
    p.estimated_size      = S("install","estimated_size");
    p.download_size       = S("install","download_size");
    p.checksum_type       = S("install","checksum_type");
    p.checksum            = S("install","checksum");
    p.repo_key_url        = S("install","repo_key_url");
    p.repo_entry          = S("install","repo_entry");

    // [uninstall]
    p.uninstall_method      = parseMethod(S("uninstall","method"));
    if (p.uninstall_method == InstallMethod::UNKNOWN)
        p.uninstall_method = p.install_method; // fallback
    p.uninstall_packages    = splitList(S("uninstall","packages"), ' ');
    if (p.uninstall_packages.empty())
        p.uninstall_packages = p.packages;
    p.purge                 = B("uninstall","purge",false);
    p.uninstall_script      = S("uninstall","script");
    p.uninstall_script_file = S("uninstall","script_file");
    p.remove_deps           = B("uninstall","remove_deps",false);
    p.remove_data           = B("uninstall","remove_data",false);
    p.data_paths            = splitList(S("uninstall","data_paths"),';');

    // [verify]
    p.verify_binary  = S("verify","binary");
    p.verify_cmd     = S("verify","check_cmd");
    p.verify_file    = S("verify","check_file");
    p.verify_service = S("verify","check_service");

    // [hooks]
    p.hook_pre_install     = S("hooks","pre_install");
    p.hook_post_install    = S("hooks","post_install");
    p.hook_pre_uninstall   = S("hooks","pre_uninstall");
    p.hook_post_uninstall  = S("hooks","post_uninstall");

    // [desktop]
    p.desktop_name  = S("desktop","desktop_name");
    p.desktop_exec  = S("desktop","desktop_exec");
    p.desktop_icon  = S("desktop","desktop_icon");
    p.desktop_cats  = S("desktop","desktop_cats");
    p.create_desktop= B("desktop","create_desktop", true);

    // Default status
    p.status = InstallStatus::UNKNOWN;
}

// ─────────────────────────────────────────────────────────────────────────────
// PackageRepository
// ─────────────────────────────────────────────────────────────────────────────

PackageRepository::PackageRepository() = default;
PackageRepository::~PackageRepository() = default;

bool PackageRepository::loadDirectory(const std::string& dir_path) {
    packages_.clear();
    loaded_dir_ = dir_path;
    GDir* d = g_dir_open(dir_path.c_str(), 0, nullptr);
    if (!d) {
        last_error_ = "Cannot open directory: " + dir_path;
        return false;
    }
    const gchar* fname;
    while ((fname = g_dir_read_name(d)) != nullptr) {
        std::string name(fname);
        if (name.size() > 6 && name.substr(name.size()-6) == ".aepkg") {
            loadSingleFile(dir_path + "/" + name);
        }
    }
    g_dir_close(d);
    return true;
}

bool PackageRepository::loadSingleFile(const std::string& path) {
    AepkgParser parser;
    AppPackage pkg;
    if (!parser.parseFile(path, pkg)) {
        last_error_ = parser.lastError();
        return false;
    }
    if (pkg.id.empty()) return false;
    // Remove duplicate
    packages_.erase(std::remove_if(packages_.begin(), packages_.end(),
        [&](const AppPackage& p){ return p.id == pkg.id; }), packages_.end());
    packages_.push_back(std::move(pkg));
    return true;
}

void PackageRepository::reload() {
    if (!loaded_dir_.empty()) loadDirectory(loaded_dir_);
}

const AppPackage* PackageRepository::findById(const std::string& id) const {
    for (auto& p : packages_) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

std::vector<const AppPackage*> PackageRepository::findByCategory(AppCategory cat) const {
    std::vector<const AppPackage*> result;
    for (auto& p : packages_) {
        if (p.category == cat) result.push_back(&p);
    }
    return result;
}

std::vector<const AppPackage*> PackageRepository::search(const std::string& query) const {
    std::vector<const AppPackage*> result;
    std::string q = toLower(query);
    for (auto& p : packages_) {
        if (toLower(p.name).find(q) != std::string::npos ||
            toLower(p.description).find(q) != std::string::npos ||
            toLower(p.id).find(q) != std::string::npos) {
            result.push_back(&p);
            continue;
        }
        for (auto& tag : p.tags) {
            if (toLower(tag).find(q) != std::string::npos) {
                result.push_back(&p);
                break;
            }
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// InstallManager
// ─────────────────────────────────────────────────────────────────────────────

InstallManager& InstallManager::instance() {
    static InstallManager inst;
    return inst;
}

bool InstallManager::commandExists(const std::string& cmd) {
    std::string check = "command -v " + cmd + " > /dev/null 2>&1";
    return system(check.c_str()) == 0;
}

InstallMethod InstallManager::detectSystemPM() {
    if (commandExists("apt"))    return InstallMethod::APT;
    if (commandExists("dnf"))    return InstallMethod::DNF;
    if (commandExists("pacman")) return InstallMethod::PACMAN;
    return InstallMethod::UNKNOWN;
}

std::string InstallManager::expandHomePath(const std::string& path) const {
    if (path.empty() || path[0] != '~') return path;
    const char* home = getenv("HOME");
    if (!home) return path;
    return std::string(home) + path.substr(1);
}

InstallStatus InstallManager::checkStatus(const AppPackage& pkg) {
    // 1. Binary check
    if (!pkg.verify_binary.empty()) {
        struct stat st;
        if (stat(pkg.verify_binary.c_str(), &st) == 0) return InstallStatus::INSTALLED;
        return InstallStatus::NOT_INSTALLED;
    }
    // 2. Command check
    if (!pkg.verify_cmd.empty()) {
        int ret = system((pkg.verify_cmd + " > /dev/null 2>&1").c_str());
        return (ret == 0) ? InstallStatus::INSTALLED : InstallStatus::NOT_INSTALLED;
    }
    // 3. File check
    if (!pkg.verify_file.empty()) {
        struct stat st;
        return (stat(pkg.verify_file.c_str(), &st) == 0)
            ? InstallStatus::INSTALLED : InstallStatus::NOT_INSTALLED;
    }
    // 4. Fallback: check package name via package manager
    switch (pkg.install_method) {
        case InstallMethod::APT: {
            std::string cmd = "dpkg -l " + (pkg.packages.empty() ? pkg.id : pkg.packages[0])
                            + " 2>/dev/null | grep -q '^ii'";
            return (system(cmd.c_str())==0) ? InstallStatus::INSTALLED : InstallStatus::NOT_INSTALLED;
        }
        case InstallMethod::PACMAN: {
            std::string cmd = "pacman -Qi " + (pkg.packages.empty() ? pkg.id : pkg.packages[0])
                            + " > /dev/null 2>&1";
            return (system(cmd.c_str())==0) ? InstallStatus::INSTALLED : InstallStatus::NOT_INSTALLED;
        }
        case InstallMethod::FLATPAK: {
            if (!pkg.flatpak_id.empty()) {
                std::string cmd = "flatpak info " + pkg.flatpak_id + " > /dev/null 2>&1";
                return (system(cmd.c_str())==0) ? InstallStatus::INSTALLED : InstallStatus::NOT_INSTALLED;
            }
            break;
        }
        default: break;
    }
    return InstallStatus::UNKNOWN;
}

void InstallManager::buildInstallCmd(const AppPackage& pkg, std::vector<std::string>& out) {
    const std::string sudo = pkg.sudo_required ? "sudo " : "";

    // Thêm repo nếu cần (apt)
    if (!pkg.repo_key_url.empty()) {
        out.push_back("curl -fsSL " + pkg.repo_key_url + " | " + sudo + "gpg --dearmor -o /etc/apt/trusted.gpg.d/" + pkg.id + ".gpg");
    }
    if (!pkg.repo_entry.empty()) {
        out.push_back("echo '" + pkg.repo_entry + "' | " + sudo + "tee /etc/apt/sources.list.d/" + pkg.id + ".list");
        out.push_back(sudo + "apt-get update -q");
    }

    // Cài deps trước
    if (!pkg.deps.empty()) {
        std::string deps_list;
        for (auto& d : pkg.deps) deps_list += d + " ";
        switch (pkg.install_method) {
            case InstallMethod::APT:
                out.push_back(sudo + "apt-get install -y " + deps_list);
                break;
            default: break;
        }
    }

    // Hook pre-install
    if (!pkg.hook_pre_install.empty())
        out.push_back(pkg.hook_pre_install);

    // Main install
    switch (pkg.install_method) {
        case InstallMethod::APT: {
            std::string pkgs;
            for (auto& p : pkg.packages) pkgs += p + " ";
            out.push_back(sudo + "apt-get install -y " + pkgs);
            break;
        }
        case InstallMethod::DNF: {
            std::string pkgs;
            for (auto& p : pkg.packages) pkgs += p + " ";
            out.push_back(sudo + "dnf install -y " + pkgs);
            break;
        }
        case InstallMethod::PACMAN: {
            std::string pkgs;
            for (auto& p : pkg.packages) pkgs += p + " ";
            out.push_back(sudo + "pacman -S --noconfirm " + pkgs);
            break;
        }
        case InstallMethod::FLATPAK: {
            out.push_back("flatpak install -y flathub " + pkg.flatpak_id);
            break;
        }
        case InstallMethod::SNAP: {
            out.push_back(sudo + "snap install " + pkg.snap_name);
            break;
        }
        case InstallMethod::DEB: {
            std::string resolved = pkg.pkg_offline.empty() ? pkg.pkg_online : pkg.pkg_offline;
            if (!pkg.pkg_online.empty() && pkg.pkg_offline.empty()) {
                out.push_back("wget -O /tmp/" + pkg.id + ".deb " + resolved);
                resolved = "/tmp/" + pkg.id + ".deb";
            }
            out.push_back(sudo + "dpkg -i " + resolved);
            out.push_back(sudo + "apt-get install -f -y");
            break;
        }
        case InstallMethod::SCRIPT: {
            if (!pkg.install_script_file.empty()) {
                out.push_back("chmod +x " + pkg.install_script_file);
                out.push_back(pkg.install_script_file);
            } else if (!pkg.install_script.empty()) {
                // Write to temp file
                std::string tmp = "/tmp/ae_install_" + pkg.id + ".sh";
                out.push_back("cat > " + tmp + " << 'AESCRIPT'\n" + pkg.install_script + "\nAESCRIPT");
                out.push_back("chmod +x " + tmp);
                out.push_back(tmp);
            }
            break;
        }
        case InstallMethod::APPIMAGE: {
            std::string dest = expandHomePath("~/.local/bin/" + pkg.id + ".AppImage");
            std::string resolved = pkg.pkg_offline.empty() ? pkg.pkg_online : pkg.pkg_offline;
            if (!pkg.pkg_online.empty() && pkg.pkg_offline.empty()) {
                out.push_back("wget -O " + dest + " " + resolved);
            } else {
                out.push_back("cp " + resolved + " " + dest);
            }
            out.push_back("chmod +x " + dest);
            break;
        }
        default:
            out.push_back("echo 'Install method not supported'");
            break;
    }

    // Hook post-install
    if (!pkg.hook_post_install.empty())
        out.push_back(pkg.hook_post_install);
}

void InstallManager::buildUninstallCmd(const AppPackage& pkg, std::vector<std::string>& out) {
    const std::string sudo = pkg.sudo_required ? "sudo " : "";

    // Hook pre-uninstall
    if (!pkg.hook_pre_uninstall.empty())
        out.push_back(pkg.hook_pre_uninstall);

    switch (pkg.uninstall_method) {
        case InstallMethod::APT: {
            std::string pkgs;
            for (auto& p : pkg.uninstall_packages) pkgs += p + " ";
            std::string cmd = sudo + "apt-get ";
            cmd += pkg.purge ? "purge" : "remove";
            cmd += " -y " + pkgs;
            out.push_back(cmd);
            if (pkg.remove_deps) out.push_back(sudo + "apt-get autoremove -y");
            break;
        }
        case InstallMethod::DNF: {
            std::string pkgs;
            for (auto& p : pkg.uninstall_packages) pkgs += p + " ";
            out.push_back(sudo + "dnf remove -y " + pkgs);
            break;
        }
        case InstallMethod::PACMAN: {
            std::string pkgs;
            for (auto& p : pkg.uninstall_packages) pkgs += p + " ";
            std::string flag = pkg.remove_deps ? "-Rns" : "-R";
            out.push_back(sudo + "pacman " + flag + " --noconfirm " + pkgs);
            break;
        }
        case InstallMethod::FLATPAK: {
            out.push_back("flatpak uninstall -y " + pkg.flatpak_id);
            if (pkg.remove_deps) out.push_back("flatpak uninstall --unused -y");
            break;
        }
        case InstallMethod::SNAP: {
            out.push_back(sudo + "snap remove " + pkg.snap_name);
            break;
        }
        case InstallMethod::SCRIPT: {
            if (!pkg.uninstall_script_file.empty()) {
                out.push_back("chmod +x " + pkg.uninstall_script_file);
                out.push_back(pkg.uninstall_script_file);
            } else if (!pkg.uninstall_script.empty()) {
                std::string tmp = "/tmp/ae_uninstall_" + pkg.id + ".sh";
                out.push_back("cat > " + tmp + " << 'AESCRIPT'\n" + pkg.uninstall_script + "\nAESCRIPT");
                out.push_back("chmod +x " + tmp);
                out.push_back(tmp);
            }
            break;
        }
        case InstallMethod::APPIMAGE: {
            std::string dest = expandHomePath("~/.local/bin/" + pkg.id + ".AppImage");
            out.push_back("rm -f " + dest);
            break;
        }
        default:
            out.push_back("echo 'Uninstall method not supported'");
            break;
    }

    // Xóa data người dùng nếu được yêu cầu
    if (pkg.remove_data) {
        for (auto& dp : pkg.data_paths) {
            std::string expanded = expandHomePath(dp);
            out.push_back("rm -rf " + expanded);
        }
    }

    // Hook post-uninstall
    if (!pkg.hook_post_uninstall.empty())
        out.push_back(pkg.hook_post_uninstall);
}

// Chạy sequence các lệnh shell, forward stdout/stderr
void InstallManager::runOperation(std::shared_ptr<OperationContext> ctx,
                                   const std::vector<std::string>& cmd_list,
                                   const std::string& /*working_dir*/) {
    ctx->running = true;
    ctx->finished = false;

    // Lưu ctx vào active_ops_ đã làm từ install/uninstall
    std::thread([ctx, cmd_list]() {
        bool all_ok = true;
        Logger& logger = Logger::instance();

        for (const auto& cmd : cmd_list) {
            if (!ctx->running) break; // cancelled

            ctx->appendLog(LogLevel::CMD, "$ " + cmd);
            logger.writeOperationLog(ctx->log_file_path, {LogLevel::CMD, "$ " + cmd, nowMs()});

            // Chạy lệnh qua popen (đơn giản nhất, không cần PTY)
            FILE* fp = popen((cmd + " 2>&1").c_str(), "r");
            if (!fp) {
                std::string err = "Failed to execute: " + cmd;
                ctx->appendLog(LogLevel::ERROR, err);
                logger.writeOperationLog(ctx->log_file_path, {LogLevel::ERROR, err, nowMs()});
                all_ok = false;
                break;
            }

            char buf[512];
            while (ctx->running && fgets(buf, sizeof(buf), fp)) {
                std::string line(buf);
                // Strip trailing newline
                while (!line.empty() && (line.back()=='\n'||line.back()=='\r'))
                    line.pop_back();
                if (line.empty()) continue;
                // Heuristic phân loại
                LogLevel level = LogLevel::STDOUT;
                std::string ll = toLower(line);
                if (ll.find("error")!=std::string::npos ||
                    ll.find("failed")!=std::string::npos ||
                    ll.find("cannot")!=std::string::npos) level = LogLevel::ERROR;
                else if (ll.find("warn")!=std::string::npos) level = LogLevel::STDERR;
                else if (ll.find("done")==0 || ll.find("installed")==0 ||
                         ll.find("complete")==0) level = LogLevel::SUCCESS;

                ctx->appendLog(level, line);
                logger.writeOperationLog(ctx->log_file_path, {level, line, nowMs()});
            }

            int ret = pclose(fp);
            if (ret != 0) {
                std::string err = "Command exited with code " + std::to_string(WEXITSTATUS(ret));
                ctx->appendLog(LogLevel::ERROR, err);
                logger.writeOperationLog(ctx->log_file_path, {LogLevel::ERROR, err, nowMs()});
                all_ok = false;
                ctx->exit_code = WEXITSTATUS(ret);
                break;
            }
            ctx->exit_code = 0;
        }

        ctx->success  = all_ok;
        ctx->running  = false;
        ctx->finished = true;

        // Log kết thúc
        LogLevel fin_level = all_ok ? LogLevel::SUCCESS : LogLevel::ERROR;
        std::string fin_msg = all_ok ? "Operation completed successfully." : "Operation failed.";
        ctx->appendLog(fin_level, fin_msg);
        logger.writeOperationLog(ctx->log_file_path, {fin_level, fin_msg, nowMs()});
        logger.finishOperationLog(ctx->log_file_path, all_ok);

        // Fire on_finished on main thread
        if (ctx->on_finished) {
            bool s = all_ok;
            int ec = ctx->exit_code.load();
            auto* cb = new std::function<void(bool,int)>(ctx->on_finished);
            g_idle_add([](gpointer ud) -> gboolean {
                auto* p = static_cast<std::pair<std::function<void(bool,int)>*, std::pair<bool,int>*>*>(ud);
                (*p->first)(p->second->first, p->second->second);
                delete p->first;
                delete p->second;
                delete p;
                return G_SOURCE_REMOVE;
            }, new std::pair<decltype(cb), std::pair<bool,int>*>(cb, new std::pair<bool,int>(s,ec)));
        }
    }).detach();
}

bool InstallManager::install(const AppPackage& pkg,
                              std::function<void(const LogLine&)> on_log,
                              std::function<void(bool, int)>      on_done) {
    std::lock_guard<std::mutex> lk(ops_mutex_);
    if (active_ops_.count(pkg.id)) return false;

    auto ctx = std::make_shared<OperationContext>();
    ctx->app_id   = pkg.id;
    ctx->type     = OperationType::INSTALL;
    ctx->on_log_line = on_log;
    ctx->on_finished = [this, id=pkg.id, on_done](bool s, int ec) {
        { std::lock_guard<std::mutex> lk(ops_mutex_); active_ops_.erase(id); }
        if (on_done) on_done(s, ec);
    };

    ctx->log_file_path = Logger::instance().beginOperationLog(pkg.id, OperationType::INSTALL);

    std::vector<std::string> cmds;
    buildInstallCmd(pkg, cmds);

    active_ops_[pkg.id] = ctx;
    runOperation(ctx, cmds);
    return true;
}

bool InstallManager::uninstall(const AppPackage& pkg,
                                std::function<void(const LogLine&)> on_log,
                                std::function<void(bool, int)>      on_done) {
    std::lock_guard<std::mutex> lk(ops_mutex_);
    if (active_ops_.count(pkg.id)) return false;

    auto ctx = std::make_shared<OperationContext>();
    ctx->app_id   = pkg.id;
    ctx->type     = OperationType::UNINSTALL;
    ctx->on_log_line = on_log;
    ctx->on_finished = [this, id=pkg.id, on_done](bool s, int ec) {
        { std::lock_guard<std::mutex> lk(ops_mutex_); active_ops_.erase(id); }
        if (on_done) on_done(s, ec);
    };

    ctx->log_file_path = Logger::instance().beginOperationLog(pkg.id, OperationType::UNINSTALL);

    std::vector<std::string> cmds;
    buildUninstallCmd(pkg, cmds);

    active_ops_[pkg.id] = ctx;
    runOperation(ctx, cmds);
    return true;
}

bool InstallManager::isRunning(const std::string& id) const {
    std::lock_guard<std::mutex> lk(ops_mutex_);
    return active_ops_.count(id) > 0;
}

void InstallManager::cancel(const std::string& id) {
    std::lock_guard<std::mutex> lk(ops_mutex_);
    auto it = active_ops_.find(id);
    if (it != active_ops_.end()) {
        it->second->running = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Logger
// ─────────────────────────────────────────────────────────────────────────────

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

std::string Logger::timestamp() const {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return std::string(buf);
}

std::string Logger::levelStr(AppLogLevel l) const {
    switch(l) {
        case AppLogLevel::DEBUG:   return "DEBUG";
        case AppLogLevel::INFO:    return "INFO ";
        case AppLogLevel::WARNING: return "WARN ";
        case AppLogLevel::ERROR:   return "ERROR";
    }
    return "?    ";
}

void Logger::init(const std::string& dir) {
    log_dir_ = dir;
    // Tạo thư mục nếu chưa có
    g_mkdir_with_parents(dir.c_str(), 0755);
    gui_log_path_ = dir + "/gui.log";
    logGUI(AppLogLevel::INFO, "App Explorer started");
}

void Logger::logGUI(AppLogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lk(write_mutex_);
    std::ofstream f(gui_log_path_, std::ios::app);
    if (f.is_open()) {
        f << "[" << timestamp() << "] [" << levelStr(level) << "] " << msg << "\n";
    }
}

std::string Logger::beginOperationLog(const std::string& app_id, OperationType type) {
    std::string type_str = (type == OperationType::INSTALL) ? "install" : "uninstall";
    // Timestamp cho tên file
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&t));
    std::string path = log_dir_ + "/" + type_str + "_" + app_id + "_" + buf + ".log";
    {
        std::lock_guard<std::mutex> lk(write_mutex_);
        std::ofstream f(path);
        if (f.is_open()) {
            f << "# App Explorer Operation Log\n";
            f << "# App: " << app_id << "\n";
            f << "# Type: " << type_str << "\n";
            f << "# Started: " << timestamp() << "\n";
            f << "# ────────────────────────────────────\n";
        }
    }
    return path;
}

void Logger::writeOperationLog(const std::string& log_path, const LogLine& line) {
    if (log_path.empty()) return;
    std::lock_guard<std::mutex> lk(write_mutex_);
    std::ofstream f(log_path, std::ios::app);
    if (f.is_open()) {
        std::string prefix;
        switch(line.level) {
            case LogLevel::CMD:     prefix = "CMD    "; break;
            case LogLevel::STDOUT:  prefix = "OUT    "; break;
            case LogLevel::STDERR:  prefix = "STDERR "; break;
            case LogLevel::ERROR:   prefix = "ERROR  "; break;
            case LogLevel::SUCCESS: prefix = "OK     "; break;
            case LogLevel::INFO:    prefix = "INFO   "; break;
        }
        f << "[" << timestamp() << "] " << prefix << line.text << "\n";
    }
}

void Logger::finishOperationLog(const std::string& log_path, bool success) {
    if (log_path.empty()) return;
    if (success) {
        // Xóa file log nếu thành công
        std::remove(log_path.c_str());
        logGUI(AppLogLevel::INFO, "Operation completed, log removed: " + log_path);
    } else {
        // Giữ lại file log
        std::lock_guard<std::mutex> lk(write_mutex_);
        std::ofstream f(log_path, std::ios::app);
        if (f.is_open()) {
            f << "# ────────────────────────────────────\n";
            f << "# Finished: " << timestamp() << " | Result: FAILED\n";
            f << "# Log kept for debugging.\n";
        }
        logGUI(AppLogLevel::ERROR, "Operation FAILED, log saved: " + log_path);
    }
}

} // namespace AppExplorer
