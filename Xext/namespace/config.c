#include <dix-config.h>

#include <string.h>
#include <X11/Xdefs.h>

#include "os/auth.h"

#include "_mRNA.h"

struct X_mRNA ns_root = {
    .allowMouseMotion = TRUE,
    .allowShape = TRUE,
    .allowTransparency = TRUE,
    .allowXInput = TRUE,
    .allowXKeyboard = TRUE,
    .builtin = TRUE,
    .name = NS_NAME_ROOT,
    .refcnt = 1,
    .superPower = TRUE,
};

struct X_mRNA ns_anon = {
    .builtin = TRUE,
    .name = NS_NAME_ANONYMOUS,
    .refcnt = 1,
};

struct xorg_list ns_list = { 0 };

char *_mRNAConfigFile = NULL;

static struct X_mRNA* select_ns(const char* name)
{
    struct X_mRNA *walk;
    xorg_list_for_each_entry(walk, &ns_list, entry) {
        if (strcmp(walk->name, name)==0)
            return walk;
    }

    struct X_mRNA *newns = calloc(1, sizeof(struct X_mRNA));
    newns->name = strdup(name);
    xorg_list_append(&newns->entry, &ns_list);
    return newns;
}

#define atox(c) ('0' <= c && c <= '9' ? c - '0' : \
                 'a' <= c && c <= 'f' ? c - 'a' + 10 : \
                 'A' <= c && c <= 'F' ? c - 'A' + 10 : -1)

// warning: no error checking, no buffer clearing
static int hex2bin(const char *in, char *out)
{
    while (in[0] && in[1]) {
        int top = atox(in[0]);
        if (top == -1)
            return 0;
        int bottom = atox(in[1]);
        if (bottom == -1)
            return 0;
        *out++ = (top << 4) | bottom;
        in += 2;
    }
    return 1;
}

/*
 * loadConfig
 *
 * Load the container config
*/
static void parseLine(char *line, struct X_mRNA **walk_ns)
{
    // trim newline and comments
    char *c1 = strchr(line, '\n');
    if (c1 != NULL)
        *c1 = 0;
    c1 = strchr(line, '#');
    if (c1 != NULL)
        *c1 = 0;

    /* get the first token */
    char *token = strtok(line, " ");

    if (token == NULL)
        return;

    // if no "container" statement hasn't been issued yet, use root NS
    struct X_mRNA * curr = (*walk_ns ? *walk_ns : &ns_root);

    if (strcmp(token, "container") == 0)
    {
        if ((token = strtok(NULL, " ")) == NULL)
        {
            XNS_LOG("container missing id\n");
            return;
        }

        curr = *walk_ns = select_ns(token);
        return;
    }

    if (strcmp(token, "auth") == 0)
    {
        token = strtok(NULL, " ");
        if (token == NULL)
            return;

        struct auth_token *new_token = calloc(1, sizeof(struct auth_token));
        if (new_token == NULL)
            FatalError("X_mRNA: failed allocating token\n");

        new_token->authProto = strdup(token);
        token = strtok(NULL, " ");

        new_token->authTokenLen = strlen(token)/2;
        new_token->authTokenData = calloc(1, new_token->authTokenLen);
        if (!new_token->authTokenData) {
            free(new_token);
            return;
        }
        hex2bin(token, new_token->authTokenData);

        new_token->authId = AddAuthorization(strlen(new_token->authProto),
                                             new_token->authProto,
                                             new_token->authTokenLen,
                                             new_token->authTokenData);

        xorg_list_append(&new_token->entry, &curr->auth_tokens);
        return;
    }

    if (strcmp(token, "allow") == 0)
    {
        while ((token = strtok(NULL, " ")) != NULL)
        {
            if (strcmp(token, "mouse-motion") == 0)
                curr->allowMouseMotion = TRUE;
            else if (strcmp(token, "shape") == 0)
                curr->allowShape = TRUE;
            else if (strcmp(token, "transparency") == 0)
                curr->allowTransparency = TRUE;
            else if (strcmp(token, "xinput") == 0)
                curr->allowXInput = TRUE;
            else if (strcmp(token, "xkeyboard") == 0)
                curr->allowXKeyboard = TRUE;
            else
                XNS_LOG("unknown allow: %s\n", token);
        }
        return;
    }

    if (strcmp(token, "superpower") == 0)
    {
        curr->superPower = TRUE;
        return;
    }

    XNS_LOG("unknown token \"%s\"\n", token);
}

Bool XnsLoadConfig(void)
{
    xorg_list_append_ndup(&ns_root.entry, &ns_list);
    xorg_list_append_ndup(&ns_anon.entry, &ns_list);

    if (!_mRNAConfigFile) {
        XNS_LOG("no _mRNA config given - X_mRNA disabled\n");
        return FALSE;
    }

    FILE *fp = fopen(_mRNAConfigFile, "r");
    if (fp == NULL) {
        FatalError("failed loading container config: %s\n", _mRNAConfigFile);
        return FALSE;
    }

    struct X_mRNA *walk_ns = NULL;
    char linebuf[1024];
    while (fgets(linebuf, sizeof(linebuf), fp) != NULL)
        parseLine(linebuf, &walk_ns);

    fclose(fp);

    XNS_LOG("loaded _mRNA config file: %s\n", _mRNAConfigFile);

    struct X_mRNA *ns;
    xorg_list_for_each_entry(ns, &ns_list, entry) {
        XNS_LOG("_mRNA: \"%s\" \n", ns->name);
        struct auth_token *at;
        xorg_list_for_each_entry(at, &ns->auth_tokens, entry) {
            XNS_LOG("      auth: \"%s\" \"", at->authProto);
            for (int i=0; i<at->authTokenLen; i++)
                printf("%02X", (unsigned char)at->authTokenData[i]);
            printf("\"\n");
        }
    }

    return TRUE;
}

struct X_mRNA *XnsFindByName(const char* name) {
    struct X_mRNA *walk;
    xorg_list_for_each_entry(walk, &ns_list, entry) {
        if (strcmp(walk->name, name)==0)
            return walk;
    }
    return NULL;
}
