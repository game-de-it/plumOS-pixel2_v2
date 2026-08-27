#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <spawn.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

static const char *env_value(char *const envp[], const char *name) {
    size_t length = strlen(name);
    size_t index;

    if (!envp)
        envp = environ;
    for (index = 0; envp && envp[index]; index++) {
        if (strncmp(envp[index], name, length) == 0 && envp[index][length] == '=')
            return envp[index] + length + 1;
    }
    return NULL;
}

static int token_present(const char *value, const char *token, size_t token_length) {
    const char *cursor;

    if (!value || !token_length)
        return 0;
    cursor = value;
    while (*cursor) {
        const char *end = strchr(cursor, ':');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        if (length == token_length && memcmp(cursor, token, length) == 0)
            return 1;
        if (!end)
            break;
        cursor = end + 1;
    }
    return 0;
}

static int contains_required_tokens(const char *value, const char *required) {
    const char *cursor;

    if (!required || !*required)
        return 1;
    cursor = required;
    while (*cursor) {
        const char *end = strchr(cursor, ':');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        if (length && !token_present(value, cursor, length))
            return 0;
        if (!end)
            break;
        cursor = end + 1;
    }
    return 1;
}

static char *join_assignment(const char *name, const char *current, const char *required) {
    size_t name_length = strlen(name);
    size_t current_length = current ? strlen(current) : 0;
    size_t required_length = required ? strlen(required) : 0;
    size_t separator = current_length && required_length ? 1 : 0;
    char *result = malloc(name_length + 1 + current_length + separator + required_length + 1);
    char *cursor;

    if (!result)
        return NULL;
    cursor = result;
    memcpy(cursor, name, name_length);
    cursor += name_length;
    *cursor++ = '=';
    if (current_length) {
        memcpy(cursor, current, current_length);
        cursor += current_length;
    }
    if (separator)
        *cursor++ = ':';
    if (required_length) {
        memcpy(cursor, required, required_length);
        cursor += required_length;
    }
    *cursor = '\0';
    return result;
}

static char *force_assignment(const char *name, const char *value) {
    return join_assignment(name, NULL, value ? value : "");
}

struct guarded_environment {
    char **entries;
    char *library_path;
    char *preload;
    char *session;
};

static void free_guarded_environment(struct guarded_environment *guarded) {
    if (!guarded)
        return;
    free(guarded->entries);
    free(guarded->library_path);
    free(guarded->preload);
    free(guarded->session);
    memset(guarded, 0, sizeof(*guarded));
}

static int build_guarded_environment(char *const source_envp[],
                                     struct guarded_environment *guarded) {
    const char *required_library_path;
    const char *required_preload;
    const char *required_session;
    const char *current_library_path;
    const char *current_preload;
    char *const *envp = source_envp ? source_envp : environ;
    size_t count = 0;
    size_t output = 0;
    size_t index;
    int replace_library_path;
    int replace_preload;
    int replace_session;

    memset(guarded, 0, sizeof(*guarded));
    required_library_path = env_value(environ, "PLUMOS_PORTMASTER_REQUIRED_LD_LIBRARY_PATH");
    required_preload = env_value(environ, "PLUMOS_PORTMASTER_REQUIRED_LD_PRELOAD");
    required_session = env_value(environ, "PLUMOS_PORTMASTER_SESSION_ID");
    if ((!required_library_path || !*required_library_path) &&
        (!required_preload || !*required_preload) &&
        (!required_session || !*required_session))
        return 0;

    current_library_path = env_value(envp, "LD_LIBRARY_PATH");
    current_preload = env_value(envp, "LD_PRELOAD");
    replace_library_path = required_library_path && *required_library_path &&
        !contains_required_tokens(current_library_path, required_library_path);
    replace_preload = required_preload && *required_preload &&
        !contains_required_tokens(current_preload, required_preload);
    replace_session = required_session && *required_session &&
        (!env_value(envp, "PLUMOS_PORTMASTER_SESSION_ID") ||
         strcmp(env_value(envp, "PLUMOS_PORTMASTER_SESSION_ID"), required_session) != 0);
    if (!replace_library_path && !replace_preload && !replace_session)
        return 0;

    while (envp && envp[count])
        count++;
    guarded->entries = calloc(count + 4, sizeof(*guarded->entries));
    if (!guarded->entries)
        return -1;
    if (replace_library_path) {
        guarded->library_path = join_assignment(
            "LD_LIBRARY_PATH", current_library_path, required_library_path);
        if (!guarded->library_path)
            goto allocation_failed;
    }
    if (replace_preload) {
        guarded->preload = join_assignment("LD_PRELOAD", current_preload, required_preload);
        if (!guarded->preload)
            goto allocation_failed;
    }
    if (replace_session) {
        guarded->session = force_assignment("PLUMOS_PORTMASTER_SESSION_ID", required_session);
        if (!guarded->session)
            goto allocation_failed;
    }

    for (index = 0; index < count; index++) {
        if (replace_library_path && strncmp(envp[index], "LD_LIBRARY_PATH=", 16) == 0)
            continue;
        if (replace_preload && strncmp(envp[index], "LD_PRELOAD=", 11) == 0)
            continue;
        if (replace_session && strncmp(envp[index], "PLUMOS_PORTMASTER_SESSION_ID=", 29) == 0)
            continue;
        guarded->entries[output++] = envp[index];
    }
    if (guarded->library_path)
        guarded->entries[output++] = guarded->library_path;
    if (guarded->preload)
        guarded->entries[output++] = guarded->preload;
    if (guarded->session)
        guarded->entries[output++] = guarded->session;
    guarded->entries[output] = NULL;
    return 1;

allocation_failed:
    free_guarded_environment(guarded);
    return -1;
}

static char *const *guard_envp(char *const envp[], struct guarded_environment *guarded,
                               int *allocated) {
    int result = build_guarded_environment(envp, guarded);
    if (result < 0) {
        errno = ENOMEM;
        return NULL;
    }
    *allocated = result;
    return result ? guarded->entries : (envp ? envp : environ);
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    static int (*real_execve)(const char *, char *const[], char *const[]);
    struct guarded_environment guarded;
    char *const *guarded_envp;
    int allocated = 0;
    int result;
    int saved_errno;

    if (!real_execve)
        real_execve = dlsym(RTLD_NEXT, "execve");
    if (!real_execve) {
        errno = ENOSYS;
        return -1;
    }
    guarded_envp = guard_envp(envp, &guarded, &allocated);
    if (!guarded_envp)
        return -1;
    result = real_execve(pathname, argv, (char *const *)guarded_envp);
    saved_errno = errno;
    if (allocated)
        free_guarded_environment(&guarded);
    errno = saved_errno;
    return result;
}

int execveat(int dirfd, const char *pathname, char *const argv[], char *const envp[], int flags) {
    static int (*real_execveat)(int, const char *, char *const[], char *const[], int);
    struct guarded_environment guarded;
    char *const *guarded_envp;
    int allocated = 0;
    int result;
    int saved_errno;

    if (!real_execveat)
        real_execveat = dlsym(RTLD_NEXT, "execveat");
    if (!real_execveat) {
        errno = ENOSYS;
        return -1;
    }
    guarded_envp = guard_envp(envp, &guarded, &allocated);
    if (!guarded_envp)
        return -1;
    result = real_execveat(dirfd, pathname, argv, (char *const *)guarded_envp, flags);
    saved_errno = errno;
    if (allocated)
        free_guarded_environment(&guarded);
    errno = saved_errno;
    return result;
}

int posix_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *actions,
                const posix_spawnattr_t *attributes, char *const argv[], char *const envp[]) {
    static int (*real_posix_spawn)(pid_t *, const char *, const posix_spawn_file_actions_t *,
                                   const posix_spawnattr_t *, char *const[], char *const[]);
    struct guarded_environment guarded;
    char *const *guarded_envp;
    int allocated = 0;
    int result;

    if (!real_posix_spawn)
        real_posix_spawn = dlsym(RTLD_NEXT, "posix_spawn");
    if (!real_posix_spawn)
        return ENOSYS;
    guarded_envp = guard_envp(envp, &guarded, &allocated);
    if (!guarded_envp)
        return ENOMEM;
    result = real_posix_spawn(pid, path, actions, attributes, argv,
                              (char *const *)guarded_envp);
    if (allocated)
        free_guarded_environment(&guarded);
    return result;
}

int posix_spawnp(pid_t *pid, const char *file, const posix_spawn_file_actions_t *actions,
                 const posix_spawnattr_t *attributes, char *const argv[], char *const envp[]) {
    static int (*real_posix_spawnp)(pid_t *, const char *, const posix_spawn_file_actions_t *,
                                    const posix_spawnattr_t *, char *const[], char *const[]);
    struct guarded_environment guarded;
    char *const *guarded_envp;
    int allocated = 0;
    int result;

    if (!real_posix_spawnp)
        real_posix_spawnp = dlsym(RTLD_NEXT, "posix_spawnp");
    if (!real_posix_spawnp)
        return ENOSYS;
    guarded_envp = guard_envp(envp, &guarded, &allocated);
    if (!guarded_envp)
        return ENOMEM;
    result = real_posix_spawnp(pid, file, actions, attributes, argv,
                               (char *const *)guarded_envp);
    if (allocated)
        free_guarded_environment(&guarded);
    return result;
}
