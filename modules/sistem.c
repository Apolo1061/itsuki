#include "itsuki_ext.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <process.h>
#include <windows.h>
typedef BOOLEAN(WINAPI* CreateSymbolicLinkAFn)(LPCSTR, LPCSTR, DWORD);
#define SISTEM_STAT_T struct _stat
#define sistem_stat _stat
#define SISTEM_PATH_SEP ';'
#define SISTEM_DIR_SEP '\\'
#define SISTEM_EXE_EXT ".exe"
#define SISTEM_ACCESS _access
#define SISTEM_CHMOD _chmod
#else
#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define SISTEM_STAT_T struct stat
#define sistem_stat stat
#define SISTEM_PATH_SEP ':'
#define SISTEM_DIR_SEP '/'
#define SISTEM_EXE_EXT ""
#define SISTEM_ACCESS access
#define SISTEM_CHMOD chmod
#endif

#ifndef SISTEM_PATH_MAX
#define SISTEM_PATH_MAX 4096
#endif

#define SISTEM_MAX_PROCS 256
#define SISTEM_MAX_SEGMENTS 512

static const ItsukiApi* g_api = NULL;

typedef struct {
    int used;
    int id;
    int finished;
    int exit_code;
#ifdef _WIN32
    DWORD pid;
    HANDLE handle;
#else
    pid_t pid;
#endif
} ProcEntry;

static ProcEntry g_procs[SISTEM_MAX_PROCS];
static int g_next_proc_id = 1;

static Result fail(TipoError tipo, const char* msg) {
    if (g_api && g_api->raise) g_api->raise(tipo, msg ? msg : "Error");
    return g_api ? g_api->make_null() : (Result){0};
}

static int is_string(Result r) {
    return r.tipo == TIPO_CADENA && r.s != NULL;
}

static int is_number(Result r) {
    return r.tipo == TIPO_NUMERO || r.tipo == TIPO_BOOL;
}

static int path_is_sep(char c) {
    return c == '/' || c == '\\';
}

static int path_is_abs(const char* p) {
    if (!p || !*p) return 0;
#ifdef _WIN32
    if (path_is_sep(p[0])) return 1;
    if (isalpha((unsigned char)p[0]) && p[1] == ':' && path_is_sep(p[2])) return 1;
    return 0;
#else
    return p[0] == '/';
#endif
}

static int path_exists(const char* path) {
    SISTEM_STAT_T st;
    if (!path || !*path) return 0;
    return sistem_stat(path, &st) == 0;
}

static int path_is_dir(const char* path) {
    SISTEM_STAT_T st;
    if (!path || !*path) return 0;
    if (sistem_stat(path, &st) != 0) return 0;
#ifdef _WIN32
    return (st.st_mode & _S_IFDIR) != 0;
#else
    return S_ISDIR(st.st_mode);
#endif
}

static int path_is_file(const char* path) {
    SISTEM_STAT_T st;
    if (!path || !*path) return 0;
    if (sistem_stat(path, &st) != 0) return 0;
#ifdef _WIN32
    return (st.st_mode & _S_IFREG) != 0;
#else
    return S_ISREG(st.st_mode);
#endif
}

static int make_one_dir(const char* path) {
    int rc;
    if (!path || !*path) return -1;
#ifdef _WIN32
    rc = _mkdir(path);
#else
    rc = mkdir(path, 0755);
#endif
    if (rc == 0) return 0;
    if (errno == EEXIST && path_is_dir(path)) return 0;
    return -1;
}

static int make_dirs_recursive(const char* path) {
    char* tmp;
    size_t len;
    size_t i;
    if (!path || !*path) return -1;

    len = strlen(path);
    tmp = (char*)malloc(len + 1);
    if (!tmp) return -1;
    strcpy(tmp, path);

    while (len > 1 && path_is_sep(tmp[len - 1])) {
        tmp[len - 1] = '\0';
        len--;
    }

    for (i = 1; i < len; i++) {
        if (!path_is_sep(tmp[i])) continue;
#ifdef _WIN32
        if (i == 2 && tmp[1] == ':') continue;
#endif
        tmp[i] = '\0';
        if (*tmp && !path_exists(tmp)) {
            if (make_one_dir(tmp) != 0) {
                free(tmp);
                return -1;
            }
        }
        tmp[i] = path[i];
    }

    if (*tmp && !path_exists(tmp)) {
        if (make_one_dir(tmp) != 0) {
            free(tmp);
            return -1;
        }
    }
    free(tmp);
    return 0;
}

static char* dup_substr(const char* s, size_t n) {
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char* path_join_c(const char* a, const char* b) {
    size_t la;
    size_t lb;
    char* out;
    if (!a) a = "";
    if (!b) b = "";
    la = strlen(a);
    lb = strlen(b);
    out = (char*)malloc(la + lb + 3);
    if (!out) return NULL;
    out[0] = '\0';
    if (la > 0) memcpy(out, a, la + 1);
    if (la > 0 && !path_is_sep(out[la - 1])) {
        out[la] = SISTEM_DIR_SEP;
        out[la + 1] = '\0';
        la++;
    }
    while (*b && path_is_sep(*b)) b++;
    lb = strlen(b);
    memcpy(out + la, b, lb + 1);
    return out;
}

static Result make_splitext_result(const char* root, const char* ext) {
    Result arr = g_api->make_array();
    g_api->array_push(arr, g_api->make_string(root ? root : ""));
    g_api->array_push(arr, g_api->make_string(ext ? ext : ""));
    return arr;
}

static Result process_wait_internal(int proc_id, int timeout_ms) {
    int i;
    ProcEntry* p = NULL;
    Result m = g_api->make_map();

    for (i = 0; i < SISTEM_MAX_PROCS; i++) {
        if (g_procs[i].used && g_procs[i].id == proc_id) {
            p = &g_procs[i];
            break;
        }
    }
    if (!p) return fail(ERROR_NOMBRE, "process id no existe");

    if (!p->finished) {
#ifdef _WIN32
        DWORD wr = WaitForSingleObject(p->handle, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms);
        if (wr == WAIT_OBJECT_0) {
            DWORD code = 0;
            GetExitCodeProcess(p->handle, &code);
            p->finished = 1;
            p->exit_code = (int)code;
        } else if (wr != WAIT_TIMEOUT) {
            return fail(ERROR_SISTEMA, "wait fallo");
        }
#else
        int status = 0;
        if (timeout_ms < 0) {
            if (waitpid(p->pid, &status, 0) == p->pid) {
                p->finished = 1;
                if (WIFEXITED(status)) p->exit_code = WEXITSTATUS(status);
                else if (WIFSIGNALED(status)) p->exit_code = 128 + WTERMSIG(status);
                else p->exit_code = -1;
            }
        } else {
            int waited = 0;
            while (waited <= timeout_ms) {
                pid_t rc = waitpid(p->pid, &status, WNOHANG);
                if (rc == p->pid) {
                    p->finished = 1;
                    if (WIFEXITED(status)) p->exit_code = WEXITSTATUS(status);
                    else if (WIFSIGNALED(status)) p->exit_code = 128 + WTERMSIG(status);
                    else p->exit_code = -1;
                    break;
                }
                if (rc < 0) break;
                if (timeout_ms == 0) break;
                usleep(1000);
                waited += 1;
            }
        }
#endif
    }

    g_api->map_set(m, "id", g_api->make_number((double)p->id));
#ifdef _WIN32
    g_api->map_set(m, "pid", g_api->make_number((double)p->pid));
#else
    g_api->map_set(m, "pid", g_api->make_number((double)p->pid));
#endif
    g_api->map_set(m, "done", g_api->make_bool(p->finished ? 1 : 0));
    if (p->finished) g_api->map_set(m, "exit_code", g_api->make_number((double)p->exit_code));
    else g_api->map_set(m, "exit_code", g_api->make_null());
    return m;
}

static Result fn_version(Result args[], int n_args) {
    (void)args;
    (void)n_args;
    return g_api->make_string("sistem/1.1");
}

static Result fn_name(Result args[], int n_args) {
    (void)args;
    (void)n_args;
#ifdef _WIN32
    return g_api->make_string("windows");
#elif __APPLE__
    return g_api->make_string("darwin");
#else
    return g_api->make_string("linux");
#endif
}

static Result fn_pid(Result args[], int n_args) {
    (void)args;
    (void)n_args;
#ifdef _WIN32
    return g_api->make_number((double)_getpid());
#else
    return g_api->make_number((double)getpid());
#endif
}

static Result fn_run(Result args[], int n_args) {
    int rc;
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.run(cmd) requiere string");
    rc = system(args[0].s);
    return g_api->make_number((double)rc);
}

static Result fn_spawn(Result args[], int n_args) {
    int i;
    ProcEntry* p = NULL;
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.spawn(cmd) requiere string");

    for (i = 0; i < SISTEM_MAX_PROCS; i++) {
        if (!g_procs[i].used) {
            p = &g_procs[i];
            memset(p, 0, sizeof(ProcEntry));
            p->used = 1;
            p->id = g_next_proc_id++;
            break;
        }
    }
    if (!p) return fail(ERROR_SISTEMA, "limite de procesos alcanzado");

#ifdef _WIN32
    {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        size_t n = strlen(args[0].s);
        char* cmdline = (char*)malloc(n + 12);
        if (!cmdline) {
            memset(p, 0, sizeof(ProcEntry));
            return fail(ERROR_SISTEMA, "sin memoria");
        }
        strcpy(cmdline, "cmd.exe /C ");
        strcat(cmdline, args[0].s);
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);
        if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            free(cmdline);
            memset(p, 0, sizeof(ProcEntry));
            return fail(ERROR_SISTEMA, "spawn fallo");
        }
        free(cmdline);
        CloseHandle(pi.hThread);
        p->handle = pi.hProcess;
        p->pid = pi.dwProcessId;
    }
#else
    {
        pid_t pid = fork();
        if (pid < 0) {
            memset(p, 0, sizeof(ProcEntry));
            return fail(ERROR_SISTEMA, "spawn fallo");
        }
        if (pid == 0) {
            execl("/bin/sh", "sh", "-c", args[0].s, (char*)NULL);
            _exit(127);
        }
        p->pid = pid;
    }
#endif
    return g_api->make_number((double)p->id);
}

static Result fn_wait(Result args[], int n_args) {
    int id;
    int timeout_ms = -1;
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "sistem.wait(id, [timeout_ms]) requiere id");
    id = (int)args[0].n;
    if (n_args > 1 && is_number(args[1])) timeout_ms = (int)args[1].n;
    return process_wait_internal(id, timeout_ms);
}

static Result fn_try_wait(Result args[], int n_args) {
    int id;
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "sistem.try_wait(id) requiere id");
    id = (int)args[0].n;
    return process_wait_internal(id, 0);
}

static Result fn_kill(Result args[], int n_args) {
    int id;
    int i;
    ProcEntry* p = NULL;
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "sistem.kill(id, [signal]) requiere id");
    id = (int)args[0].n;
    for (i = 0; i < SISTEM_MAX_PROCS; i++) {
        if (g_procs[i].used && g_procs[i].id == id) {
            p = &g_procs[i];
            break;
        }
    }
    if (!p) return fail(ERROR_NOMBRE, "process id no existe");
#ifdef _WIN32
    if (!TerminateProcess(p->handle, 1)) return fail(ERROR_SISTEMA, "kill fallo");
    p->finished = 1;
    p->exit_code = 1;
#else
    {
        int sig = SIGTERM;
        if (n_args > 1 && is_number(args[1])) sig = (int)args[1].n;
        if (kill(p->pid, sig) != 0) return fail(ERROR_SISTEMA, "kill fallo");
    }
#endif
    return g_api->make_number(0);
}

static Result fn_process_destroy(Result args[], int n_args) {
    int id;
    int i;
    ProcEntry* p = NULL;
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "sistem.process_destroy(id) requiere id");
    id = (int)args[0].n;
    for (i = 0; i < SISTEM_MAX_PROCS; i++) {
        if (g_procs[i].used && g_procs[i].id == id) {
            p = &g_procs[i];
            break;
        }
    }
    if (!p) return g_api->make_bool(0);
#ifdef _WIN32
    if (p->handle) CloseHandle(p->handle);
#endif
    memset(p, 0, sizeof(ProcEntry));
    return g_api->make_bool(1);
}

static Result fn_cwd(Result args[], int n_args) {
    char buf[SISTEM_PATH_MAX];
    (void)args;
    (void)n_args;
#ifdef _WIN32
    if (!_getcwd(buf, (int)sizeof(buf))) return fail(ERROR_SISTEMA, "getcwd fallo");
#else
    if (!getcwd(buf, sizeof(buf))) return fail(ERROR_SISTEMA, "getcwd fallo");
#endif
    return g_api->make_string(buf);
}

static Result fn_chdir(Result args[], int n_args) {
    int rc;
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.chdir(path) requiere string");
#ifdef _WIN32
    rc = _chdir(args[0].s);
#else
    rc = chdir(args[0].s);
#endif
    if (rc != 0) return fail(ERROR_SISTEMA, "chdir fallo");
    return g_api->make_number(0);
}

static Result fn_listdir(Result args[], int n_args) {
    const char* path = ".";
    Result out = g_api->make_array();
    if (n_args >= 1) {
        if (!is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.listdir([path]) requiere string");
        path = args[0].s;
    }

#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char pattern[SISTEM_PATH_MAX];
    size_t len = strlen(path);
    if (len + 3 >= sizeof(pattern)) return fail(ERROR_SISTEMA, "path demasiado largo");
    strcpy(pattern, path);
    if (len > 0 && !path_is_sep(pattern[len - 1])) {
        pattern[len] = '\\';
        pattern[len + 1] = '*';
        pattern[len + 2] = '\0';
    } else {
        pattern[len] = '*';
        pattern[len + 1] = '\0';
    }
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return fail(ERROR_SISTEMA, "listdir fallo");
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        g_api->array_push(out, g_api->make_string(fd.cFileName));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(path);
    struct dirent* de;
    if (!d) return fail(ERROR_SISTEMA, "listdir fallo");
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        g_api->array_push(out, g_api->make_string(de->d_name));
    }
    closedir(d);
#endif
    return out;
}

static Result fn_scandir(Result args[], int n_args) {
    const char* path = ".";
    Result out = g_api->make_array();
    if (n_args >= 1) {
        if (!is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.scandir([path]) requiere string");
        path = args[0].s;
    }

#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h;
    char pattern[SISTEM_PATH_MAX];
    size_t len = strlen(path);
    if (len + 3 >= sizeof(pattern)) return fail(ERROR_SISTEMA, "path demasiado largo");
    strcpy(pattern, path);
    if (len > 0 && !path_is_sep(pattern[len - 1])) {
        pattern[len] = '\\';
        pattern[len + 1] = '*';
        pattern[len + 2] = '\0';
    } else {
        pattern[len] = '*';
        pattern[len + 1] = '\0';
    }
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return fail(ERROR_SISTEMA, "scandir fallo");
    do {
        Result m;
        char* full;
        int isdir;
        double sz;
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        full = path_join_c(path, fd.cFileName);
        if (!full) {
            FindClose(h);
            return fail(ERROR_SISTEMA, "sin memoria");
        }
        isdir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
        sz = (double)((((unsigned long long)fd.nFileSizeHigh) << 32) | (unsigned long long)fd.nFileSizeLow);
        m = g_api->make_map();
        g_api->map_set(m, "name", g_api->make_string(fd.cFileName));
        g_api->map_set(m, "path", g_api->make_string(full));
        g_api->map_set(m, "is_dir", g_api->make_bool(isdir));
        g_api->map_set(m, "is_file", g_api->make_bool(isdir ? 0 : 1));
        g_api->map_set(m, "size", g_api->make_number(sz));
        g_api->array_push(out, m);
        free(full);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR* d = opendir(path);
    struct dirent* de;
    if (!d) return fail(ERROR_SISTEMA, "scandir fallo");
    while ((de = readdir(d)) != NULL) {
        Result m;
        char* full;
        SISTEM_STAT_T st;
        int isdir;
        int isfile;
        double sz = 0.0;
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        full = path_join_c(path, de->d_name);
        if (!full) {
            closedir(d);
            return fail(ERROR_SISTEMA, "sin memoria");
        }
        if (sistem_stat(full, &st) == 0) {
            isdir = S_ISDIR(st.st_mode) ? 1 : 0;
            isfile = S_ISREG(st.st_mode) ? 1 : 0;
            sz = (double)st.st_size;
        } else {
            isdir = 0;
            isfile = 0;
        }
        m = g_api->make_map();
        g_api->map_set(m, "name", g_api->make_string(de->d_name));
        g_api->map_set(m, "path", g_api->make_string(full));
        g_api->map_set(m, "is_dir", g_api->make_bool(isdir));
        g_api->map_set(m, "is_file", g_api->make_bool(isfile));
        g_api->map_set(m, "size", g_api->make_number(sz));
        g_api->array_push(out, m);
        free(full);
    }
    closedir(d);
#endif
    return out;
}

static int walk_collect(const char* base, Result out, int depth) {
    if (depth > 128) return 0;
#ifdef _WIN32
    {
        WIN32_FIND_DATAA fd;
        HANDLE h;
        char pattern[SISTEM_PATH_MAX];
        size_t len = strlen(base);
        if (len + 3 >= sizeof(pattern)) return 0;
        strcpy(pattern, base);
        if (len > 0 && !path_is_sep(pattern[len - 1])) {
            pattern[len] = '\\';
            pattern[len + 1] = '*';
            pattern[len + 2] = '\0';
        } else {
            pattern[len] = '*';
            pattern[len + 1] = '\0';
        }
        h = FindFirstFileA(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE) return 0;
        do {
            char* full;
            int isdir;
            if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
            full = path_join_c(base, fd.cFileName);
            if (!full) {
                FindClose(h);
                return 0;
            }
            g_api->array_push(out, g_api->make_string(full));
            isdir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
            if (isdir) walk_collect(full, out, depth + 1);
            free(full);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
#else
    {
        DIR* d = opendir(base);
        struct dirent* de;
        if (!d) return 0;
        while ((de = readdir(d)) != NULL) {
            char* full;
            SISTEM_STAT_T st;
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            full = path_join_c(base, de->d_name);
            if (!full) {
                closedir(d);
                return 0;
            }
            g_api->array_push(out, g_api->make_string(full));
            if (sistem_stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
                walk_collect(full, out, depth + 1);
            }
            free(full);
        }
        closedir(d);
    }
#endif
    return 1;
}

static Result fn_walk(Result args[], int n_args) {
    const char* path = ".";
    Result out = g_api->make_array();
    if (n_args >= 1) {
        if (!is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.walk([path]) requiere string");
        path = args[0].s;
    }
    if (!walk_collect(path, out, 0)) return fail(ERROR_SISTEMA, "walk fallo");
    return out;
}

static Result fn_mkdir(Result args[], int n_args) {
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.mkdir(path) requiere string");
    if (make_one_dir(args[0].s) != 0) return fail(ERROR_SISTEMA, "mkdir fallo");
    return g_api->make_number(0);
}

static Result fn_makedirs(Result args[], int n_args) {
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.makedirs(path) requiere string");
    if (make_dirs_recursive(args[0].s) != 0) return fail(ERROR_SISTEMA, "makedirs fallo");
    return g_api->make_number(0);
}

static Result fn_rmdir(Result args[], int n_args) {
    int rc;
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.rmdir(path) requiere string");
#ifdef _WIN32
    rc = _rmdir(args[0].s);
#else
    rc = rmdir(args[0].s);
#endif
    if (rc != 0) return fail(ERROR_SISTEMA, "rmdir fallo");
    return g_api->make_number(0);
}

static Result fn_remove(Result args[], int n_args) {
    int rc;
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.remove(path) requiere string");
    rc = remove(args[0].s);
    if (rc != 0) return fail(ERROR_SISTEMA, "remove fallo");
    return g_api->make_number(0);
}

static Result fn_rename(Result args[], int n_args) {
    int rc;
    if (n_args < 2 || !is_string(args[0]) || !is_string(args[1])) {
        return fail(ERROR_ARGUMENTO, "sistem.rename(src, dst) requiere strings");
    }
    rc = rename(args[0].s, args[1].s);
    if (rc != 0) return fail(ERROR_SISTEMA, "rename fallo");
    return g_api->make_number(0);
}

static int copy_file_raw(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    FILE* out;
    char buf[65536];
    size_t n;
    if (!in) return -1;
    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static Result fn_copy(Result args[], int n_args) {
    if (n_args < 2 || !is_string(args[0]) || !is_string(args[1])) {
        return fail(ERROR_ARGUMENTO, "sistem.copy(src, dst) requiere strings");
    }
    if (copy_file_raw(args[0].s, args[1].s) != 0) return fail(ERROR_SISTEMA, "copy fallo");
    return g_api->make_number(0);
}

static Result fn_move(Result args[], int n_args) {
    if (n_args < 2 || !is_string(args[0]) || !is_string(args[1])) {
        return fail(ERROR_ARGUMENTO, "sistem.move(src, dst) requiere strings");
    }
    if (rename(args[0].s, args[1].s) == 0) return g_api->make_number(0);
    if (copy_file_raw(args[0].s, args[1].s) != 0) return fail(ERROR_SISTEMA, "move fallo");
    if (remove(args[0].s) != 0) return fail(ERROR_SISTEMA, "move fallo");
    return g_api->make_number(0);
}

static Result fn_exists(Result args[], int n_args) {
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.exists(path) requiere string");
    return g_api->make_bool(path_exists(args[0].s) ? 1 : 0);
}

static Result fn_is_file(Result args[], int n_args) {
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.is_file(path) requiere string");
    return g_api->make_bool(path_is_file(args[0].s) ? 1 : 0);
}

static Result fn_is_dir(Result args[], int n_args) {
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.is_dir(path) requiere string");
    return g_api->make_bool(path_is_dir(args[0].s) ? 1 : 0);
}

static Result fn_stat_size(Result args[], int n_args) {
    SISTEM_STAT_T st;
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.stat_size(path) requiere string");
    if (sistem_stat(args[0].s, &st) != 0) return fail(ERROR_SISTEMA, "stat fallo");
    return g_api->make_number((double)st.st_size);
}

static Result fn_chmod(Result args[], int n_args) {
    int rc;
    int mode;
    if (n_args < 2 || !is_string(args[0]) || !is_number(args[1])) return fail(ERROR_ARGUMENTO, "sistem.chmod(path, mode)");
    mode = (int)args[1].n;
    rc = SISTEM_CHMOD(args[0].s, mode);
    if (rc != 0) return fail(ERROR_SISTEMA, "chmod fallo");
    return g_api->make_number(0);
}

static Result fn_chown(Result args[], int n_args) {
#ifdef _WIN32
    (void)args;
    (void)n_args;
    return fail(ERROR_EJECUCION, "chown no disponible en windows");
#else
    uid_t uid;
    gid_t gid;
    if (n_args < 3 || !is_string(args[0]) || !is_number(args[1]) || !is_number(args[2])) {
        return fail(ERROR_ARGUMENTO, "sistem.chown(path, uid, gid)");
    }
    uid = (uid_t)args[1].n;
    gid = (gid_t)args[2].n;
    if (chown(args[0].s, uid, gid) != 0) return fail(ERROR_SISTEMA, "chown fallo");
    return g_api->make_number(0);
#endif
}

static Result fn_utime(Result args[], int n_args) {
    if (n_args < 3 || !is_string(args[0]) || !is_number(args[1]) || !is_number(args[2])) {
        return fail(ERROR_ARGUMENTO, "sistem.utime(path, atime, mtime)");
    }
#ifdef _WIN32
    {
        struct _utimbuf tb;
        tb.actime = (time_t)args[1].n;
        tb.modtime = (time_t)args[2].n;
        if (_utime(args[0].s, &tb) != 0) return fail(ERROR_SISTEMA, "utime fallo");
    }
#else
    {
        struct utimbuf tb;
        tb.actime = (time_t)args[1].n;
        tb.modtime = (time_t)args[2].n;
        if (utime(args[0].s, &tb) != 0) return fail(ERROR_SISTEMA, "utime fallo");
    }
#endif
    return g_api->make_number(0);
}

static Result fn_symlink(Result args[], int n_args) {
    if (n_args < 2 || !is_string(args[0]) || !is_string(args[1])) {
        return fail(ERROR_ARGUMENTO, "sistem.symlink(src, dst)");
    }
#ifdef _WIN32
    {
        HMODULE k32;
        CreateSymbolicLinkAFn create_symlink;
        DWORD flags = path_is_dir(args[0].s) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
#ifdef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
        flags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
#endif
        k32 = GetModuleHandleA("kernel32.dll");
        if (!k32) k32 = LoadLibraryA("kernel32.dll");
        if (!k32) return fail(ERROR_SISTEMA, "symlink fallo");
        create_symlink = (CreateSymbolicLinkAFn)GetProcAddress(k32, "CreateSymbolicLinkA");
        if (!create_symlink) return fail(ERROR_EJECUCION, "symlink no disponible en windows");
        if (!create_symlink(args[1].s, args[0].s, flags)) return fail(ERROR_SISTEMA, "symlink fallo");
    }
#else
    if (symlink(args[0].s, args[1].s) != 0) return fail(ERROR_SISTEMA, "symlink fallo");
#endif
    return g_api->make_number(0);
}

static Result fn_readlink(Result args[], int n_args) {
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.readlink(path)");
#ifdef _WIN32
    (void)args;
    return fail(ERROR_EJECUCION, "readlink no disponible en windows");
#else
    {
        char buf[SISTEM_PATH_MAX];
        ssize_t n = readlink(args[0].s, buf, sizeof(buf) - 1);
        if (n < 0) return fail(ERROR_SISTEMA, "readlink fallo");
        buf[n] = '\0';
        return g_api->make_string(buf);
    }
#endif
}

static Result fn_access(Result args[], int n_args) {
    int mode = 0;
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.access(path, [mode])");
    if (n_args > 1 && is_number(args[1])) mode = (int)args[1].n;
    return g_api->make_bool(SISTEM_ACCESS(args[0].s, mode) == 0 ? 1 : 0);
}

static Result fn_getenv(Result args[], int n_args) {
    const char* v;
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.getenv(key, [default]) requiere string");
    v = getenv(args[0].s);
    if (v) return g_api->make_string(v);
    if (n_args > 1 && is_string(args[1])) return g_api->make_string(args[1].s);
    return g_api->make_null();
}

static Result fn_setenv(Result args[], int n_args) {
    if (n_args < 2 || !is_string(args[0]) || !is_string(args[1])) {
        return fail(ERROR_ARGUMENTO, "sistem.setenv(key, value, [overwrite]) requiere strings");
    }
#ifdef _WIN32
    {
        int overwrite = 1;
        size_t lk;
        size_t lv;
        char* kv;
        int rc;
        if (n_args > 2 && is_number(args[2])) overwrite = ((int)args[2].n != 0);
        if (!overwrite && getenv(args[0].s) != NULL) return g_api->make_number(0);
        lk = strlen(args[0].s);
        lv = strlen(args[1].s);
        kv = (char*)malloc(lk + 1 + lv + 1);
        if (!kv) return fail(ERROR_SISTEMA, "sin memoria");
        memcpy(kv, args[0].s, lk);
        kv[lk] = '=';
        memcpy(kv + lk + 1, args[1].s, lv);
        kv[lk + 1 + lv] = '\0';
        rc = _putenv(kv);
        free(kv);
        if (rc != 0) return fail(ERROR_SISTEMA, "setenv fallo");
    }
#else
    {
        int overwrite = 1;
        if (n_args > 2 && is_number(args[2])) overwrite = ((int)args[2].n != 0);
        if (setenv(args[0].s, args[1].s, overwrite) != 0) return fail(ERROR_SISTEMA, "setenv fallo");
    }
#endif
    return g_api->make_number(0);
}

static Result fn_unsetenv(Result args[], int n_args) {
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.unsetenv(key) requiere string");
#ifdef _WIN32
    {
        size_t lk = strlen(args[0].s);
        char* kv = (char*)malloc(lk + 2);
        int rc;
        if (!kv) return fail(ERROR_SISTEMA, "sin memoria");
        memcpy(kv, args[0].s, lk);
        kv[lk] = '=';
        kv[lk + 1] = '\0';
        rc = _putenv(kv);
        free(kv);
        if (rc != 0) return fail(ERROR_SISTEMA, "unsetenv fallo");
    }
#else
    if (unsetenv(args[0].s) != 0) return fail(ERROR_SISTEMA, "unsetenv fallo");
#endif
    return g_api->make_number(0);
}

static Result fn_path_join(Result args[], int n_args) {
    size_t total = 1;
    int i;
    char* out;
    size_t out_len = 0;

    if (n_args < 1) return fail(ERROR_ARGUMENTO, "sistem.path_join(part1, part2, ...) requiere strings");
    for (i = 0; i < n_args; i++) {
        if (!is_string(args[i])) return fail(ERROR_ARGUMENTO, "sistem.path_join requiere strings");
        total += strlen(args[i].s) + 2;
    }

    out = (char*)malloc(total);
    if (!out) return fail(ERROR_SISTEMA, "sin memoria");
    out[0] = '\0';

    for (i = 0; i < n_args; i++) {
        const char* part = args[i].s;
        size_t part_len = strlen(part);
        if (part_len == 0) continue;
        if (out_len == 0) {
            memcpy(out, part, part_len + 1);
            out_len = part_len;
            continue;
        }
        if (!path_is_sep(out[out_len - 1])) {
            out[out_len++] = SISTEM_DIR_SEP;
            out[out_len] = '\0';
        }
        while (*part && path_is_sep(*part)) part++;
        part_len = strlen(part);
        memcpy(out + out_len, part, part_len + 1);
        out_len += part_len;
    }

    {
        Result r = g_api->make_string(out);
        free(out);
        return r;
    }
}

static Result fn_basename(Result args[], int n_args) {
    const char* p;
    size_t len;
    size_t i;
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.basename(path)");
    p = args[0].s;
    len = strlen(p);
    if (len == 0) return g_api->make_string("");
    while (len > 0 && path_is_sep(p[len - 1])) len--;
    if (len == 0) return g_api->make_string("");
    i = len;
    while (i > 0 && !path_is_sep(p[i - 1])) i--;
    return g_api->make_string(dup_substr(p + i, len - i));
}

static Result fn_dirname(Result args[], int n_args) {
    const char* p;
    size_t len;
    size_t i;
    char* out;
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.dirname(path)");
    p = args[0].s;
    len = strlen(p);
    if (len == 0) return g_api->make_string(".");
    while (len > 0 && path_is_sep(p[len - 1])) len--;
    if (len == 0) return g_api->make_string(".");
    i = len;
    while (i > 0 && !path_is_sep(p[i - 1])) i--;
    if (i == 0) return g_api->make_string(".");
    while (i > 1 && path_is_sep(p[i - 1])) i--;
    out = dup_substr(p, i);
    if (!out) return fail(ERROR_SISTEMA, "sin memoria");
    {
        Result r = g_api->make_string(out);
        free(out);
        return r;
    }
}

static Result fn_splitext(Result args[], int n_args) {
    const char* p;
    size_t len;
    size_t i;
    size_t last_sep = 0;
    size_t last_dot = (size_t)-1;
    char* root;
    char* ext;
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.splitext(path)");
    p = args[0].s;
    len = strlen(p);
    for (i = 0; i < len; i++) {
        if (path_is_sep(p[i])) last_sep = i + 1;
        else if (p[i] == '.') last_dot = i;
    }
    if (last_dot == (size_t)-1 || last_dot < last_sep || last_dot == last_sep) {
        return make_splitext_result(p, "");
    }
    root = dup_substr(p, last_dot);
    ext = dup_substr(p + last_dot, len - last_dot);
    if (!root || !ext) {
        free(root);
        free(ext);
        return fail(ERROR_SISTEMA, "sin memoria");
    }
    {
        Result r = make_splitext_result(root, ext);
        free(root);
        free(ext);
        return r;
    }
}

static Result fn_normpath(Result args[], int n_args) {
    const char* path;
    char prefix[16];
    int abs = 0;
    const char* s;
    char* segs[SISTEM_MAX_SEGMENTS];
    int nseg = 0;
    char* out;
    size_t out_cap = SISTEM_PATH_MAX;
    size_t out_len = 0;
    int i;

    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.normpath(path)");
    path = args[0].s;
    prefix[0] = '\0';

#ifdef _WIN32
    if (isalpha((unsigned char)path[0]) && path[1] == ':') {
        prefix[0] = path[0];
        prefix[1] = ':';
        prefix[2] = '\0';
        s = path + 2;
        if (path_is_sep(*s)) {
            abs = 1;
            while (path_is_sep(*s)) s++;
        }
    } else
#endif
    {
        s = path;
        if (path_is_sep(*s)) {
            abs = 1;
            while (path_is_sep(*s)) s++;
        }
    }

    while (*s) {
        const char* st = s;
        size_t ln;
        while (*s && !path_is_sep(*s)) s++;
        ln = (size_t)(s - st);
        if (ln > 0) {
            if (ln == 1 && st[0] == '.') {
                /* skip */
            } else if (ln == 2 && st[0] == '.' && st[1] == '.') {
                if (nseg > 0 && strcmp(segs[nseg - 1], "..") != 0) {
                    free(segs[nseg - 1]);
                    nseg--;
                } else if (!abs) {
                    segs[nseg] = dup_substr(st, ln);
                    if (!segs[nseg]) goto norm_fail;
                    nseg++;
                }
            } else {
                segs[nseg] = dup_substr(st, ln);
                if (!segs[nseg]) goto norm_fail;
                nseg++;
                if (nseg >= SISTEM_MAX_SEGMENTS) goto norm_fail;
            }
        }
        while (*s && path_is_sep(*s)) s++;
    }

    out = (char*)malloc(out_cap);
    if (!out) goto norm_fail;
    out[0] = '\0';

    if (prefix[0]) {
        strcpy(out, prefix);
        out_len = strlen(out);
    }
    if (abs) {
        out[out_len++] = SISTEM_DIR_SEP;
        out[out_len] = '\0';
    }

    for (i = 0; i < nseg; i++) {
        size_t ln = strlen(segs[i]);
        if (out_len > 0 && !path_is_sep(out[out_len - 1]) && !(i == 0 && prefix[0] && !abs && out_len == 2)) {
            out[out_len++] = SISTEM_DIR_SEP;
            out[out_len] = '\0';
        }
        if (out_len + ln + 2 >= out_cap) {
            free(out);
            goto norm_fail;
        }
        memcpy(out + out_len, segs[i], ln + 1);
        out_len += ln;
    }

    if (out_len == 0) {
        strcpy(out, ".");
    } else if (prefix[0] && !abs && nseg == 0) {
        strcat(out, ".");
    }

    for (i = 0; i < nseg; i++) free(segs[i]);
    {
        Result r = g_api->make_string(out);
        free(out);
        return r;
    }

norm_fail:
    for (i = 0; i < nseg; i++) free(segs[i]);
    return fail(ERROR_SISTEMA, "normpath fallo");
}

static Result fn_abspath(Result args[], int n_args) {
    char out[SISTEM_PATH_MAX];
    if (n_args < 1 || !is_string(args[0])) return fail(ERROR_ARGUMENTO, "sistem.abspath(path) requiere string");

#ifdef _WIN32
    if (_fullpath(out, args[0].s, sizeof(out)) != NULL) return g_api->make_string(out);
#else
    if (realpath(args[0].s, out) != NULL) return g_api->make_string(out);
#endif

    if (path_is_abs(args[0].s)) return fn_normpath(args, n_args);

    {
        Result cwd = fn_cwd(NULL, 0);
        Result joined;
        if (cwd.tipo != TIPO_CADENA || !cwd.s) return fail(ERROR_SISTEMA, "abspath fallo");
        joined = fn_path_join((Result[]){cwd, args[0]}, 2);
        if (joined.tipo != TIPO_CADENA || !joined.s) return fail(ERROR_SISTEMA, "abspath fallo");
        return fn_normpath((Result[]){joined}, 1);
    }
}

static Result fn_temp_dir(Result args[], int n_args) {
    const char* t = NULL;
    (void)args;
    (void)n_args;
#ifdef _WIN32
    t = getenv("TEMP");
    if (!t || !*t) t = getenv("TMP");
    if (!t || !*t) t = "C:\\Temp";
#else
    t = getenv("TMPDIR");
    if (!t || !*t) t = "/tmp";
#endif
    return g_api->make_string(t);
}

static Result fn_home_dir(Result args[], int n_args) {
    const char* h = NULL;
    (void)args;
    (void)n_args;
#ifdef _WIN32
    h = getenv("USERPROFILE");
    if (!h || !*h) {
        const char* hd = getenv("HOMEDRIVE");
        const char* hp = getenv("HOMEPATH");
        if (hd && hp) {
            char buf[SISTEM_PATH_MAX];
            snprintf(buf, sizeof(buf), "%s%s", hd, hp);
            return g_api->make_string(buf);
        }
        h = "C:\\";
    }
#else
    h = getenv("HOME");
    if (!h || !*h) h = "/";
#endif
    return g_api->make_string(h);
}

static Result fn_getuid(Result args[], int n_args) {
    (void)args;
    (void)n_args;
#ifdef _WIN32
    return fail(ERROR_EJECUCION, "getuid no disponible en windows");
#else
    return g_api->make_number((double)getuid());
#endif
}

static Result fn_getgid(Result args[], int n_args) {
    (void)args;
    (void)n_args;
#ifdef _WIN32
    return fail(ERROR_EJECUCION, "getgid no disponible en windows");
#else
    return g_api->make_number((double)getgid());
#endif
}

static Result fn_umask(Result args[], int n_args) {
    if (n_args < 1 || !is_number(args[0])) return fail(ERROR_ARGUMENTO, "sistem.umask(mask)");
#ifdef _WIN32
    (void)args;
    return fail(ERROR_EJECUCION, "umask no disponible en windows");
#else
    mode_t old = umask((mode_t)args[0].n);
    return g_api->make_number((double)old);
#endif
}

ITSUKI_EXT_EXPORT int ITSUKI_EXT_CALL itsuki_module_init(const ItsukiApi* api, ItsukiModule* module) {
    if (!api || !module) return 0;
    if (api->version != ITSUKI_EXT_API_VERSION) return 0;
    g_api = api;

    if (!api->export_const(module, "SEP", api->make_string(
#ifdef _WIN32
        "\\"
#else
        "/"
#endif
    ))) return 0;

    {
        char pathsep[2];
        pathsep[0] = SISTEM_PATH_SEP;
        pathsep[1] = '\0';
        if (!api->export_const(module, "PATHSEP", api->make_string(pathsep))) return 0;
    }

    if (!api->export_const(module, "EXE_EXT", api->make_string(SISTEM_EXE_EXT))) return 0;

    if (!api->export_native(module, "version", fn_version)) return 0;
    if (!api->export_native(module, "name", fn_name)) return 0;
    if (!api->export_native(module, "pid", fn_pid)) return 0;
    if (!api->export_native(module, "run", fn_run)) return 0;
    if (!api->export_native(module, "spawn", fn_spawn)) return 0;
    if (!api->export_native(module, "wait", fn_wait)) return 0;
    if (!api->export_native(module, "try_wait", fn_try_wait)) return 0;
    if (!api->export_native(module, "kill", fn_kill)) return 0;
    if (!api->export_native(module, "process_destroy", fn_process_destroy)) return 0;

    if (!api->export_native(module, "cwd", fn_cwd)) return 0;
    if (!api->export_native(module, "chdir", fn_chdir)) return 0;
    if (!api->export_native(module, "listdir", fn_listdir)) return 0;
    if (!api->export_native(module, "scandir", fn_scandir)) return 0;
    if (!api->export_native(module, "walk", fn_walk)) return 0;
    if (!api->export_native(module, "mkdir", fn_mkdir)) return 0;
    if (!api->export_native(module, "makedirs", fn_makedirs)) return 0;
    if (!api->export_native(module, "rmdir", fn_rmdir)) return 0;
    if (!api->export_native(module, "remove", fn_remove)) return 0;
    if (!api->export_native(module, "rename", fn_rename)) return 0;
    if (!api->export_native(module, "copy", fn_copy)) return 0;
    if (!api->export_native(module, "move", fn_move)) return 0;
    if (!api->export_native(module, "chmod", fn_chmod)) return 0;
    if (!api->export_native(module, "chown", fn_chown)) return 0;
    if (!api->export_native(module, "utime", fn_utime)) return 0;
    if (!api->export_native(module, "symlink", fn_symlink)) return 0;
    if (!api->export_native(module, "readlink", fn_readlink)) return 0;
    if (!api->export_native(module, "access", fn_access)) return 0;

    if (!api->export_native(module, "exists", fn_exists)) return 0;
    if (!api->export_native(module, "is_file", fn_is_file)) return 0;
    if (!api->export_native(module, "is_dir", fn_is_dir)) return 0;
    if (!api->export_native(module, "stat_size", fn_stat_size)) return 0;
    if (!api->export_native(module, "getenv", fn_getenv)) return 0;
    if (!api->export_native(module, "setenv", fn_setenv)) return 0;
    if (!api->export_native(module, "unsetenv", fn_unsetenv)) return 0;

    if (!api->export_native(module, "path_join", fn_path_join)) return 0;
    if (!api->export_native(module, "basename", fn_basename)) return 0;
    if (!api->export_native(module, "dirname", fn_dirname)) return 0;
    if (!api->export_native(module, "splitext", fn_splitext)) return 0;
    if (!api->export_native(module, "normpath", fn_normpath)) return 0;
    if (!api->export_native(module, "abspath", fn_abspath)) return 0;
    if (!api->export_native(module, "temp_dir", fn_temp_dir)) return 0;
    if (!api->export_native(module, "home_dir", fn_home_dir)) return 0;

    if (!api->export_native(module, "getuid", fn_getuid)) return 0;
    if (!api->export_native(module, "getgid", fn_getgid)) return 0;
    if (!api->export_native(module, "umask", fn_umask)) return 0;

    return 1;
}
