/*
 * openwrt-connect.exe - Remote Setup Tool v2.0.0
 *
 * Core features (built-in):
 *   - IPv4 gateway auto-detection
 *   - SSH key authentication management
 *   - .conf file driven command execution
 *     - script: inline multi-line or ./file.sh reference
 *     - url:    wget remote script and execute
 *     - cmd:    direct command execution
 *
 * Usage:
 *   openwrt-connect.exe                  Interactive SSH connection
 *   openwrt-connect.exe <command>        Execute command defined in .conf
 *   openwrt-connect.exe --list           List available commands from .conf
 *   openwrt-connect.exe --help           Show usage
 *
 * Configuration:
 *   Reads .conf from the same directory as the executable.
 *   Commands are defined as [command.<name>] sections.
 *   Build settings (.ini) are separate and used only by generate-wxs.ps1.
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
#define MAX_SCRIPT_LEN      8192
#define MAX_LINE_LEN        1024
#define MAX_CMD_BUF         16384

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
typedef enum {
    CMD_TYPE_SSH,       /* No script/url/cmd = interactive SSH */
    CMD_TYPE_SCRIPT,    /* script field (inline or ./file.sh) */
    CMD_TYPE_URL,       /* url field (wget & execute) */
    CMD_TYPE_CMD        /* cmd field (direct command) */
} CmdType;

typedef struct {
    char name[64];
    char url[MAX_VALUE_LEN];
    char cmd[MAX_VALUE_LEN];
    char script[MAX_SCRIPT_LEN];
    CmdType type;
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

/* Script file loader */
int load_script_file(const char *exe_dir, const char *filename, char *buf, size_t size);

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
/* Script file loader                                 */
/* ================================================== */
/*
 * ./file.sh 形式の場合、EXEと同ディレクトリのファイルを読み込む
 */
int load_script_file(const char *exe_dir, const char *filename, char *buf, size_t size)
{
    char filepath[MAX_VALUE_LEN];
    FILE *fp;
    size_t total = 0;
    char line[MAX_LINE_LEN];

    /* ./ を除去してパス構築 */
    const char *name = filename;
    if (name[0] == '.' && (name[1] == '/' || name[1] == '\\')) {
        name += 2;
    }

    snprintf(filepath, sizeof(filepath), "%s%s", exe_dir, name);

    fp = fopen(filepath, "r");
    if (!fp) {
        printf("[ERROR] Script file not found: %s\n", filepath);
        return 0;
    }

    buf[0] = '\0';
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (total + len >= size - 1) break;
        strcat(buf, line);
        total += len;
    }

    fclose(fp);
    return 1;
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
    int reading_script = 0;
    FILE *fp;

    /* defaults */
    strcpy(cfg->product_name, "OpenWrt Connect");
    strcpy(cfg->default_ip, "192.168.1.1");
    strcpy(cfg->ssh_user, "root");
    strcpy(cfg->ssh_key_prefix, "openwrt-connect");
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
        /* 複数行script読み込み中 */
        if (reading_script && cfg->command_count > 0) {
            char trimmed[MAX_LINE_LEN];
            strncpy(trimmed, line, sizeof(trimmed) - 1);
            trimmed[sizeof(trimmed) - 1] = '\0';
            trim(trimmed);

            /* 空行・コメント行はスクリプトの一部として含めない */
            /* 新しいセクションヘッダまたはkey=valueで終了 */
            if (trimmed[0] == '[' || (trimmed[0] != '#' && trimmed[0] != '\0' && strchr(trimmed, '=') && trimmed[0] != ' ' && trimmed[0] != '\t')) {
                reading_script = 0;
                /* このlineを再処理する必要がある → fallthrough */
            } else if (trimmed[0] == '#' || trimmed[0] == '\0') {
                /* 空行・コメントはスキップ */
                continue;
            } else {
                /* スクリプト行を追加 (先頭のインデントを除去) */
                CommandDef *c = &cfg->commands[cfg->command_count - 1];
                size_t cur_len = strlen(c->script);
                char *content = line;
                /* 先頭の2スペースインデントを除去 */
                if (content[0] == ' ' && content[1] == ' ') content += 2;
                if (cur_len + strlen(content) < MAX_SCRIPT_LEN - 1) {
                    strcat(c->script, content);
                }
                continue;
            }
        }

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
                        c->type = CMD_TYPE_SSH;
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
            if (strcmp(key, "script") == 0) {
                if (val[0] != '\0') {
                    /* script = ./file.sh または script = inline one-liner */
                    strncpy(c->script, val, sizeof(c->script) - 1);
                    c->type = CMD_TYPE_SCRIPT;
                } else {
                    /* script = (空) → 次行から複数行読み込み開始 */
                    c->script[0] = '\0';
                    c->type = CMD_TYPE_SCRIPT;
                    reading_script = 1;
                }
            }
            else if (strcmp(key, "url") == 0) {
                strncpy(c->url, val, sizeof(c->url) - 1);
                if (c->type == CMD_TYPE_SSH) c->type = CMD_TYPE_URL;
            }
            else if (strcmp(key, "cmd") == 0) {
                strncpy(c->cmd, val, sizeof(c->cmd) - 1);
                if (c->type == CMD_TYPE_SSH) c->type = CMD_TYPE_CMD;
            }
        }
    }

    fclose(fp);

    /* ./file.sh 参照の解決 */
    for (int i = 0; i < cfg->command_count; i++) {
        CommandDef *c = &cfg->commands[i];
        if (c->type == CMD_TYPE_SCRIPT && c->script[0] == '.' &&
            (c->script[1] == '/' || c->script[1] == '\\')) {
            char filename[MAX_VALUE_LEN];
            strncpy(filename, c->script, sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';
            c->script[0] = '\0';
            load_script_file(exe_path, filename, c->script, sizeof(c->script));
        }
    }

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
        printf("Available commands (.conf):\n\n");
        for (int i = 0; i < cfg.command_count; i++) {
            CommandDef *c = &cfg.commands[i];
            const char *type_str;
            switch (c->type) {
                case CMD_TYPE_SCRIPT: type_str = "script"; break;
                case CMD_TYPE_URL:    type_str = "url"; break;
                case CMD_TYPE_CMD:    type_str = "cmd"; break;
                default:              type_str = "SSH only"; break;
            }
            printf("  %-12s (%s)\n", c->name, type_str);
            if (c->type == CMD_TYPE_URL)
                printf("  %12s url: %s\n", "", c->url);
            if (c->type == CMD_TYPE_CMD)
                printf("  %12s cmd: %s\n", "", c->cmd);
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
                printf("  %s\n", cfg.commands[i].name);
            }
            printf("\nUse --help for more information.\n");
            system("pause");
            return 1;
        }
    }

    /* コマンドタイプの判定 */
    CmdType cmd_type = target_cmd ? target_cmd->type : CMD_TYPE_SSH;
    int is_ssh_only = (cmd_type == CMD_TYPE_SSH);

    /* バナー表示 */
    printf("========================================\n");
    printf("%s", cfg.product_name);
    if (target_cmd) {
        printf(" - %s", target_cmd->name);
    } else {
        printf(" - SSH Connection");
    }
    printf("\n========================================\n\n");

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
    if (is_ssh_only) {
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
    } else {
        /* コマンド実行モード */
        char remote_cmd[MAX_SCRIPT_LEN] = {0};

        printf("\nTarget: %s@%s\n", cfg.ssh_user, ip);
        printf("Command: %s\n\n", target_cmd->name);
        printf("Connecting and executing command...\n\n");

        switch (cmd_type) {
            case CMD_TYPE_SCRIPT:
                /* スクリプト本体をSSH経由で実行 */
                snprintf(remote_cmd, sizeof(remote_cmd),
                    "command -v %s >/dev/null 2>&1 || { %s }; %s",
                    target_cmd->name, target_cmd->script, target_cmd->name);
                break;

            case CMD_TYPE_URL:
                snprintf(remote_cmd, sizeof(remote_cmd),
                    "if ! command -v %s >/dev/null 2>&1; then "
                    "wget --no-check-certificate -O /tmp/%s.sh '%s' && "
                    "chmod +x /tmp/%s.sh && "
                    "sh /tmp/%s.sh; "
                    "fi; %s",
                    target_cmd->name,
                    target_cmd->name, target_cmd->url,
                    target_cmd->name,
                    target_cmd->name,
                    target_cmd->name);
                break;

            case CMD_TYPE_CMD:
                /* コマンドをそのまま実行 */
                strncpy(remote_cmd, target_cmd->cmd, sizeof(remote_cmd) - 1);
                break;

            default:
                break;
        }

        if (use_key) {
            snprintf(cmd, sizeof(cmd),
                "%s\\System32\\OpenSSH\\ssh.exe"
                SSH_OPTS
                " -i \"%s\""
                " -tt %s@%s"
                " \"%s\"",
                sysroot, key_path, cfg.ssh_user, ip,
                remote_cmd);
        } else {
            snprintf(cmd, sizeof(cmd),
                "%s\\System32\\OpenSSH\\ssh.exe"
                SSH_OPTS
                " -tt %s@%s"
                " \"%s\"",
                sysroot, cfg.ssh_user, ip,
                remote_cmd);
        }
    }

    int ret = system(cmd);

    if (!is_ssh_only) {
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
