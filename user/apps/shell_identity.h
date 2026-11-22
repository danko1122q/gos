#ifndef USER_SHELL_IDENTITY_H
#define USER_SHELL_IDENTITY_H

static inline const char *shell_home_for_uid(int uid) {
    return uid == 0 ? "/root" : "/home/user";
}

static inline const char *shell_user_for_uid(int uid) {
    return uid == 0 ? "root" : "user";
}

static inline char shell_prompt_mark_for_uid(int uid) {
    return uid == 0 ? '#' : '$';
}

#endif
