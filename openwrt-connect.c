/*
 * openwrt-connect.exe - Remote Setup Tool (Modular)
 *
 * Core features (built-in):
 *   - IPv4 gateway auto-detection
 *   - SSH key authentication management
 *   - .conf file driven command execution
 *
 * Usage:
 *   openwrt-connect.exe                  Interactive SSH connection
 *   openwrt-connect.exe <command>        Execute command defined in .conf
 *   openwrt-connect.exe --list           List available commands from .conf
 *   openwrt-connect.exe --help           Show usage
 *
 * Configuration:
 *   Reads openwrt-connect.conf from the same directory as the executable.
 *   Commands are defined as [command.<name>] sections.
 */
#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

/* ================================================== */
/* Configuration constants                            */
/* ================================================== */
#define MAX_COMMANDS        16
#define MAX_VALUE_LEN       512
#define MAX_LINE_LEN        1024
#define MAX_CMD_BUF         8192
/* .conf自動検出: exeと同ディレクトリの最初の.confファイルを使用 */
static int find_conf_file(const char *exe_dir, char *conf_path, size_t size)
{
    WIN32_FIND_DATAA fd;
    HANDLE hFind;
    char pattern[MAX_VALUE_LEN];

    snprintf(pattern, sizeof(pattern), "%s*.conf", exe_dir);
    hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    snprintf(conf_path, size, "%s%s", exe_dir, fd.cFileName);
    FindClose(hFind);
    return 1;
}

#define SSH_OPTS \
    " -o StrictHostKeyChecking=no" \
    " -o UserKnownHostsFile=NUL" \
    " -o GlobalKnownHostsFile=NUL" \
    " -o HostKeyAlgorithms=+ssh-rsa" \
    " -o PubkeyAcceptedKeyTypes=+ssh-rsa" \
    " -o LogLevel=ERROR"

/* ================================================== */
/* Data structures                                    */
/* ================================================== */
typedef struct {
    char name[64];           /* command name (section key) */
    char label[MAX_VALUE_LEN];
    char icon[MAX_VALUE_LEN];
    char url[MAX_VALUE_LEN];
    char dir[MAX_VALUE_LEN];
    char bin[MAX_VALUE_LEN];
} CommandDef;

typedef struct {
    char product_name[MAX_VALUE_LEN];
    char default_ip[64];
    char ssh_user[64];
    char ssh_key_prefix[64];
    CommandDef commands[MAX_COMMANDS];
    int command_count;
} Config;

/* ================================================== */
/* Forward declarations                               */
/* ================================================== */
/* Network */
int is_private_ip(const char *ip);
int get_default_gateway(char *ip, size_t size);
int detect_router_ip(char *ip, size_t size);

/* SSH key */
int file_exists(const char *path);
void get_key_paths(const Config *cfg, const char *ip, char *priv, char *pub, char *ssh_dir, size_t size);
int ensure_ssh_key(const char *sysroot, const char *key_path, const char *ssh_dir);
int test_key_auth(const char *sysroot, const char *key_path, const char *ip, const char *user);
int send_public_key(const char *sysroot, const char *pub_path, const char *ip, const char *user);

/* Config */
int load_config(const char *exe_path, Config *cfg);
CommandDef* find_command(Config *cfg, const char *name);
void build_install_script(const CommandDef *cmd, char *buf, size_t size);

/* ================================================== */
/* Utility functions                                  */
/* ================================================== */
static void trim(char *s)
{
    char *start = s;
    char *end;

    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    end = s + strlen(s) - 1;
    while (end >= s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        *end-- = '\0';
}

static void get_exe_dir(char *buf, size_t size)
{
    GetModuleFileNameA(NULL, buf, (DWORD)size);
    char *last_sep = strrchr(buf, '\\');
    if (last_sep) *(last_sep + 1) = '\0';
}

/* ================================================== */
/* Network functions                                  */
/* ================================================== */
int is_private_ip(const char *ip)
{
    unsigned int b1, b2, b3, b4;
    if (sscanf(ip, "%u.%u.%u.%u", &b1, &b2, &b3, &b4) != 4) return 0;
    if (b1 == 10) return 1;
    if (b1 == 172 && b2 >= 16 && b2 <= 31) return 1;
    if (b1 == 192 && b2 == 168) return 1;
    return 0;
}

int get_default_gateway(char *ip, size_t size)
{
    PMIB_IPFORWARDTABLE pIpForwardTable = NULL;
    ULONG dwSize = 0;
    DWORD dwRetVal = 0;
    int found = 0;

    dwRetVal = GetIpForwardTable(NULL, &dwSize, 0);
    if (dwRetVal == ERROR_INSUFFICIENT_BUFFER) {
        pIpForwardTable = (MIB_IPFORWARDTABLE *)malloc(dwSize);
        if (pIpForwardTable == NULL) return 0;
    } else {
        return 0;
    }

    if (GetIpForwardTable(pIpForwardTable, &dwSize, 0) == NO_ERROR) {
        for (DWORD i = 0; i < pIpForwardTable->dwNumEntries; i++) {
            if (pIpForwardTable->table[i].dwForwardDest == 0) {
                struct in_addr addr;
                addr.S_un.S_addr = pIpForwardTable->table[i].dwForwardNextHop;
                char temp[64];
                snprintf(temp, sizeof(temp), "%d.%d.%d.%d",
                    addr.S_un.S_un_b.s_b1, addr.S_un.S_un_b.s_b2,
                    addr.S_un.S_un_b.s_b3, addr.S_un.S_un_b.s_b4);
                if (is_private_ip(temp)) {
                    strncpy(ip, temp, size - 1);
                    ip[size - 1] = '\0';
                    found = 1;
                    break;
                }
            }
        }
    }

    free(pIpForwardTable);
    return found;
}

int detect_router_ip(char *ip, size_t size)
{
    char temp[64];

    if (get_default_gateway(temp, sizeof(temp))) {
        strncpy(ip, temp, size - 1);
        ip[size - 1] = '\0';
        return 1;
    }

    return 0;
}

/* ================================================== */
/* SSH key authentication                             */
/* ================================================== */
int file_exists(const char *path)
{
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

void get_key_paths(const Config *cfg, const char *ip, char *priv, char *pub, char *ssh_dir, size_t size)
{
    char userprofile[512] = {0};
    char key_name[256] = {0};
    char ip_safe[256] = {0};
    const char *p;
    char *q;

    /* IPアドレスのドットをアンダースコアに置き換え */
    for (p = ip, q = ip_safe; *p && (q - ip_safe < (int)sizeof(ip_safe) - 1); p++) {
        *q++ = (*p == '.') ? '_' : *p;
    }
    *q = '\0';

    /* 鍵名を生成: <prefix>_<IP>_rsa */
    snprintf(key_name, sizeof(key_name), "%s_%s_rsa", cfg->ssh_key_prefix, ip_safe);

    GetEnvironmentVariableA("USERPROFILE", userprofile, sizeof(userprofile));
    snprintf(ssh_dir, size, "%s\\.ssh", userprofile);
    snprintf(priv, size, "%s\\%s", ssh_dir, key_name);
    snprintf(pub, size, "%s\\%s.pub", ssh_dir, key_name);
}

int ensure_ssh_key(const char *sysroot, const char *key_path, const char *ssh_dir)
{
    char cmd[2048];

    if (file_exists(key_path)) return 1;

    CreateDirectoryA(ssh_dir, NULL);

    printf("Generating SSH key...\n");
    snprintf(cmd, sizeof(cmd),
        "%s\\System32\\OpenSSH\\ssh-keygen.exe -t rsa -N \"\" -f \"%s\"",
        sysroot, key_path);

    return (system(cmd) == 0);
}

int test_key_auth(const char *sysroot, const char *key_path, const char *ip, const char *user)
{
    char cmd[2048];

    snprintf(cmd, sizeof(cmd),
        "%s\\System32\\OpenSSH\\ssh.exe"
        SSH_OPTS
        " -o BatchMode=yes"
        " -o ConnectTimeout=5"
        " -i \"%s\""
        " %s@%s exit",
        sysroot, key_path, user, ip);

    return (system(cmd) == 0);
}

int send_public_key(const char *sysroot, const char *pub_path, const char *ip, const char *user)
{
    char cmd[4096];

    printf("Registering public key (password required once)...\n\n");
    snprintf(cmd, sizeof(cmd),
        "type \"%s\" | %s\\System32\\OpenSSH\\ssh.exe"
        SSH_OPTS
        " %s@%s"
        " \""
        "[ -f /etc/openwrt_release ] || { echo 'ERROR: Not an OpenWrt device. Aborting.'; exit 1; };"
        "cat > /tmp/.pubkey_tmp;"
        "  [ -d /etc/dropbear ] && cat /tmp/.pubkey_tmp >> /etc/dropbear/authorized_keys && chmod 600 /etc/dropbear/authorized_keys;"
        "  [ -d /root/.ssh ]    && cat /tmp/.pubkey_tmp >> /root/.ssh/authorized_keys    && chmod 600 /root/.ssh/authorized_keys;"
        "  rm -f /tmp/.pubkey_tmp"
        "\"",
        pub_path, sysroot, user, ip);

    return (system(cmd) == 0);
}

/* ================================================== */
/* Configuration file parser                          */
/* ================================================== */
int load_config(const char *exe_path, Config *cfg)
{
    char conf_path[MAX_VALUE_LEN];
    char line[MAX_LINE_LEN];
    char current_section[128] = {0};
    char current_command[64] = {0};
    FILE *fp;

    /* defaults */
    strcpy(cfg->product_name, "OpenWrt Connect");
    strcpy(cfg->default_ip, "192.168.1.1");
    strcpy(cfg->ssh_user, "root");
    strcpy(cfg->ssh_key_prefix, "owrt-connect");
    cfg->command_count = 0;

    /* .confのパスを構築 (EXEと同じディレクトリの.confを自動検出) */
    if (!find_conf_file(exe_path, conf_path, sizeof(conf_path))) {
        printf("[WARN] No .conf file found in: %s\n", exe_path);
        printf("[WARN] Using built-in defaults.\n\n");
        return 0;
    }

    fp = fopen(conf_path, "r");
    if (!fp) {
        printf("[WARN] Config file not found: %s\n", conf_path);
        printf("[WARN] Using built-in defaults.\n\n");
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        trim(line);

        /* 空行・コメント行をスキップ */
        if (line[0] == '\0' || line[0] == '#') continue;

        /* セクションヘッダ */
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, line + 1, sizeof(current_section) - 1);
                current_section[sizeof(current_section) - 1] = '\0';

                /* [command.<name>] の場合、コマンドを登録 */
                if (strncmp(current_section, "command.", 8) == 0) {
                    const char *cmd_name = current_section + 8;
                    if (cfg->command_count < MAX_COMMANDS && strlen(cmd_name) > 0) {
                        CommandDef *c = &cfg->commands[cfg->command_count];
                        memset(c, 0, sizeof(CommandDef));
                        strncpy(c->name, cmd_name, sizeof(c->name) - 1);
                        strncpy(current_command, cmd_name, sizeof(current_command) - 1);
                        cfg->command_count++;
                    }
                } else {
                    current_command[0] = '\0';
                }
            }
            continue;
        }

        /* key = value の解析 */
        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char key[128], val[MAX_VALUE_LEN];
        strncpy(key, line, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';
        strncpy(val, eq + 1, sizeof(val) - 1);
        val[sizeof(val) - 1] = '\0';
        trim(key);
        trim(val);

        /* [general] セクション */
        if (strcmp(current_section, "general") == 0) {
            if (strcmp(key, "product_name") == 0)
                strncpy(cfg->product_name, val, sizeof(cfg->product_name) - 1);
            else if (strcmp(key, "default_ip") == 0)
                strncpy(cfg->default_ip, val, sizeof(cfg->default_ip) - 1);
            else if (strcmp(key, "ssh_user") == 0)
                strncpy(cfg->ssh_user, val, sizeof(cfg->ssh_user) - 1);
            else if (strcmp(key, "ssh_key_prefix") == 0)
                strncpy(cfg->ssh_key_prefix, val, sizeof(cfg->ssh_key_prefix) - 1);
        }

        /* [command.*] セクション */
        if (current_command[0] != '\0' && cfg->command_count > 0) {
            CommandDef *c = &cfg->commands[cfg->command_count - 1];
            if (strcmp(key, "label") == 0)
                strncpy(c->label, val, sizeof(c->label) - 1);
            else if (strcmp(key, "icon") == 0)
                strncpy(c->icon, val, sizeof(c->icon) - 1);
            else if (strcmp(key, "url") == 0)
                strncpy(c->url, val, sizeof(c->url) - 1);
            else if (strcmp(key, "dir") == 0)
                strncpy(c->dir, val, sizeof(c->dir) - 1);
            else if (strcmp(key, "bin") == 0)
                strncpy(c->bin, val, sizeof(c->bin) - 1);
        }
    }

    fclose(fp);
    return 1;
}

CommandDef* find_command(Config *cfg, const char *name)
{
    for (int i = 0; i < cfg->command_count; i++) {
        if (strcmp(cfg->commands[i].name, name) == 0) {
            return &cfg->commands[i];
        }
    }
    return NULL;
}

/* ================================================== */
/* Install script generator (template-based)          */
/* ================================================== */
/*
 * テンプレートからインストールスクリプトを生成
 * .confの url, dir, bin から組み立てる
 *
 * 生成されるスクリプト:
 *   #!/bin/sh
 *   DIR="<dir>"
 *   mkdir -p "$DIR"
 *   CACHE="?t=$(date +%s)"
 *   wget --no-check-certificate -O "$DIR/<script>.sh" "<url>${CACHE}"
 *   chmod +x "$DIR/<script>.sh"
 *   sh "$DIR/<script>.sh" "$@"
 */
void build_install_script(const CommandDef *cmd, char *buf, size_t size)
{
    /* <name>.sh をURLの末尾から抽出、なければコマンド名を使う */
    const char *url_file = strrchr(cmd->url, '/');
    char script_name[128];
    if (url_file) {
        strncpy(script_name, url_file + 1, sizeof(script_name) - 1);
        script_name[sizeof(script_name) - 1] = '\0';
        /* クエリパラメータがあれば除去 */
        char *qmark = strchr(script_name, '?');
        if (qmark) *qmark = '\0';
    } else {
        snprintf(script_name, sizeof(script_name), "%s.sh", cmd->name);
    }

    snprintf(buf, size,
        "printf '#!/bin/sh\\n"
        "CONFIG_DIR=\\\"%s\\\"\\n"
        "mkdir -p \\\"$CONFIG_DIR\\\"\\n"
        "CACHE_BUSTER=\\\"?t=$(date +%%%%s)\\\"\\n"
        "wget --no-check-certificate -O \\\"$CONFIG_DIR/%s\\\" \\\"%s${CACHE_BUSTER}\\\"\\n"
        "chmod +x \\\"$CONFIG_DIR/%s\\\"\\n"
        "sh \\\"$CONFIG_DIR/%s\\\" \\\"$@\\\"\\n' > %s && "
        "chmod +x %s",
        cmd->dir, script_name, cmd->url,
        script_name, script_name,
        cmd->bin, cmd->bin);
}

/* ================================================== */
/* Main                                               */
/* ================================================== */
int main(int argc, char *argv[])
{
    Config cfg;
    const char *arg = (argc > 1) ? argv[1] : NULL;
    char ip[256] = {0};
    char input[256] = {0};
    char cmd[MAX_CMD_BUF];
    char sysroot[512] = {0};
    char key_path[512] = {0};
    char pub_path[512] = {0};
    char ssh_dir[512] = {0};
    char exe_dir[512] = {0};
    int use_key = 0;
    CommandDef *target_cmd = NULL;

    GetEnvironmentVariableA("SYSTEMROOT", sysroot, sizeof(sysroot));
    get_exe_dir(exe_dir, sizeof(exe_dir));

    /* .confファイルを読み込み */
    load_config(exe_dir, &cfg);

    /* --help */
    if (arg && strcmp(arg, "--help") == 0) {
        printf("Usage: openwrt-connect.exe [command|--list|--help]\n\n");
        printf("  (no args)    Interactive SSH connection\n");
        printf("  <command>    Execute command defined in .conf\n");
        printf("  --list       List available commands\n");
        printf("  --help       Show this help\n");
        return 0;
    }

    /* --list */
    if (arg && strcmp(arg, "--list") == 0) {
        printf("Available commands (%s):\n\n", CONF_FILENAME);
        for (int i = 0; i < cfg.command_count; i++) {
            CommandDef *c = &cfg.commands[i];
            if (c->url[0] != '\0') {
                printf("  %-12s %s\n", c->name, c->label);
                printf("  %12s url: %s\n", "", c->url);
                printf("  %12s bin: %s\n", "", c->bin);
            } else {
                printf("  %-12s %s (SSH only)\n", c->name, c->label);
            }
            printf("\n");
        }
        return 0;
    }

    /* コマンドの検索 */
    if (arg) {
        target_cmd = find_command(&cfg, arg);
        if (!target_cmd) {
            printf("[ERROR] Unknown command: %s\n\n", arg);
            printf("Available commands:\n");
            for (int i = 0; i < cfg.command_count; i++) {
                printf("  %s - %s\n", cfg.commands[i].name, cfg.commands[i].label);
            }
            printf("\nUse --help for more information.\n");
            system("pause");
            return 1;
        }
    }

    /* SSH-onlyコマンドの判定 (urlが空 = SSHセッション) */
    int is_ssh_only = (!target_cmd || target_cmd->url[0] == '\0');
    int is_remote_cmd = (target_cmd && target_cmd->url[0] != '\0');

    /* バナー表示 */
    printf("========================================\n");
    if (is_remote_cmd) {
        printf("%s - %s\n", cfg.product_name, target_cmd->label);
    } else if (target_cmd) {
        printf("%s - %s\n", cfg.product_name, target_cmd->label);
    } else {
        printf("%s - SSH Connection\n", cfg.product_name);
    }
    printf("========================================\n\n");

    /* IPアドレス検出・入力 */
    if (!detect_router_ip(ip, sizeof(ip))) {
        strcpy(ip, cfg.default_ip);
    }

    printf("Enter OpenWrt IP address [%s]: ", ip);
    fflush(stdout);
    if (fgets(input, sizeof(input), stdin)) {
        input[strcspn(input, "\r\n")] = '\0';
        if (input[0] != '\0') {
            strncpy(ip, input, sizeof(ip) - 1);
            ip[sizeof(ip) - 1] = '\0';
        }
    }

    /* SSH鍵パスの生成 */
    get_key_paths(&cfg, ip, key_path, pub_path, ssh_dir, sizeof(key_path));

    /* SSH鍵認証のセットアップ */
    if (ensure_ssh_key(sysroot, key_path, ssh_dir)) {
        if (test_key_auth(sysroot, key_path, ip, cfg.ssh_user)) {
            use_key = 1;
        } else {
            if (send_public_key(sysroot, pub_path, ip, cfg.ssh_user)) {
                use_key = 1;
            } else {
                system("pause");
                return 1;
            }
        }
    }

    /* SSHコマンドの構築 */
    if (is_remote_cmd) {
        /* リモートコマンド実行モード */
        char install_script[4096];
        build_install_script(target_cmd, install_script, sizeof(install_script));

        printf("\nTarget: %s@%s\n", cfg.ssh_user, ip);
        printf("Command: %s\n\n", target_cmd->name);
        printf("Connecting and executing command...\n\n");

        if (use_key) {
            snprintf(cmd, sizeof(cmd),
                "%s\\System32\\OpenSSH\\ssh.exe"
                SSH_OPTS
                " -i \"%s\""
                " -tt %s@%s"
                " \"command -v %s >/dev/null 2>&1 || (%s); %s\"",
                sysroot, key_path, cfg.ssh_user, ip,
                target_cmd->name, install_script, target_cmd->name);
        } else {
            snprintf(cmd, sizeof(cmd),
                "%s\\System32\\OpenSSH\\ssh.exe"
                SSH_OPTS
                " -tt %s@%s"
                " \"command -v %s >/dev/null 2>&1 || (%s); %s\"",
                sysroot, cfg.ssh_user, ip,
                target_cmd->name, install_script, target_cmd->name);
        }
    } else {
        /* インタラクティブSSHモード */
        printf("\nTarget: %s@%s\n\n", cfg.ssh_user, ip);
        printf("Connecting...\n\n");

        if (use_key) {
            snprintf(cmd, sizeof(cmd),
                "%s\\System32\\OpenSSH\\ssh.exe"
                SSH_OPTS
                " -i \"%s\""
                " -tt %s@%s",
                sysroot, key_path, cfg.ssh_user, ip);
        } else {
            snprintf(cmd, sizeof(cmd),
                "%s\\System32\\OpenSSH\\ssh.exe"
                SSH_OPTS
                " -tt %s@%s",
                sysroot, cfg.ssh_user, ip);
        }
    }

    int ret = system(cmd);

    if (is_remote_cmd) {
        printf("\n========================================\n");
        if (ret == 0) {
            printf("Completed successfully\n");
        } else {
            printf("Failed - Please check the error messages\n");
        }
        printf("========================================\n");
    }

    printf("\n");
    system("pause");
    return 0;
}
