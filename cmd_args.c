/* OX: Open-Channel NVM Express SSD Controller
 *
 *  - OX Controller argument parser
 *
 * Copyright (C) 2016, IT University of Copenhagen. All rights reserved.
 * Written by Ivan Luiz Picoli <ivpi@itu.dk>
 *
 * Funding support provided by CAPES Foundation, Ministry of Education
 * of Brazil, Brasilia - DF 70040-020, Brazil.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <argp.h>
#include <stdint.h>
#include <string.h>

#include "include/ssd.h"

extern struct core_struct core;
const char *argp_program_version = OX_LABEL;
const char *argp_program_bug_address = "Ivan L. Picoli <ivpi@itu.dk>";

enum cmdtypes {
    CMDARG_START = 1,
    CMDARG_TEST,
    CMDARG_DEBUG,
    CMDARG_ADMIN,
    CMDARG_NULL,
    CMDARG_NULL_DEBUG
};

static char doc_global[] = "\n*** OX Controller " OX_VER " - " LABEL " ***\n"
        " \n The DFC Open-Channel SSD Controller\n\n"
        " Available commands:\n"
        "  start            Start controller as block device\n"
        "  debug            Start controller and print Admin/IO commands\n"
        "  null             Start controller with Null IOs (NVMe queue tests)\n"
        "  null-debug       Null IOs and print Admin/IO commands\n"
        "  test             Start controller, run tests and close\n"
        "  admin            Execute specific tasks within the controller\n"
        " \n Initial release developed by Ivan L. Picoli <ivpi@itu.dk>\n\n";

static char doc_start[] =
        "\nUse this command to start the controller.\n"
        "\n Examples:"
        "\n  Start controller as block device:"
        "\n    ox-ctrl start"
        "\n    ox-ctrl start -b\n"
        "\n  Start controller as open-channel SSD:"
        "\n    ox-ctrl start -o";

static char doc_debug[] =
        "\nUse this command to start the controller in debug mode.\n"
        "\n Examples:"
        "\n  Start controller as block device with global debug:"
        "\n    ox-ctrl debug"
        "\n    ox-ctrl debug -g\n"
        "\n  Start controller as block device with FTL debug:"
        "\n    ox-ctrl debug -f\n"
        "\n  Start controller as block device with global and FTL debug:"
        "\n    ox-ctrl debug -g -f\n"
        "\n  Start controller as open-channel SSD with global debug:"
        "\n    ox-ctrl debug -o";

static char doc_test[] =
        "\nUse this command to run tests, it will start the controller,"
        " run the tests and close the controller.\n"
        "\n Examples:"
        "\n  Show all available set of tests + subtests:"
        "\n    ox-ctrl test -l\n"
        "\n  Run all available tests:"
        "\n    ox-ctrl test -a\n"
        "\n  Run a specific test set:"
        "\n    ox-ctrl test -s <set_name>\n"
        "\n  Run a specific subtest:"
        "\n    ox-ctrl test -s <set_name> -t <subtest_name>";

static char doc_admin[] =
        "\nUse this command to run specific tasks within the controller.\n"
        "\n Examples:"
        "\n  Show all available Admin tasks:"
        "\n    ox-ctrl admin -l\n"
        "\n  Run a specific admin task:"
        "\n    ox-ctrl admin -t <task_name>";

static struct argp_option opt_test[] = {
    {"list", 'l', "list", OPTION_ARG_OPTIONAL,"Show available tests."},
    {"all", 'a', "run_all", OPTION_ARG_OPTIONAL, "Use to run all tests."},
    {"set", 's', "test_set", 0, "Test set name. <char>"},
    {"test", 't', "test", 0, "Subtest name. <char>"},
    {0}
};

static struct argp_option opt_admin[] = {
    {"list", 'l', "list", OPTION_ARG_OPTIONAL,"Show available admin tasks."},
    {"task", 't', "admin_task", 0, "Admin task to be executed. <char>"},
    {0}
};

static struct argp_option opt_start[] = {
    {"block", 'b', "block", OPTION_ARG_OPTIONAL,"Start as block device."},
    {"ocssd", 'o', "ocssd", OPTION_ARG_OPTIONAL, "Start as open-channel SSD."},
    {0}
};

static struct argp_option opt_debug[] = {
    {"global", 'g', "global", OPTION_ARG_OPTIONAL,"Global debug."},
    {"ftl", 'f', "ftl", OPTION_ARG_OPTIONAL, "FTL debug."},
    {"ocssd", 'o', "ocssd", OPTION_ARG_OPTIONAL, "Open-channel debug."},
    {0}
};

static error_t parse_opt_start(int key, char *arg, struct argp_state *state)
{
    struct nvm_init_arg *args = state->input;

    switch (key) {
        case 'b':
            args->arg_num++;
            args->arg_flag |= CMDARG_FLAG_B;
            break;
        case 'o':
            args->arg_num++;
            args->arg_flag |= CMDARG_FLAG_O;
            break;
        case ARGP_KEY_END:
            if (args->arg_num > 1)
                argp_usage(state);
            break;
        case ARGP_KEY_ARG:
        case ARGP_KEY_NO_ARGS:
        case ARGP_KEY_ERROR:
        case ARGP_KEY_SUCCESS:
        case ARGP_KEY_FINI:
        case ARGP_KEY_INIT:
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static error_t parse_opt_debug(int key, char *arg, struct argp_state *state)
{
    struct nvm_init_arg *args = state->input;

    switch (key) {
        case 'g':
            args->arg_num++;
            args->arg_flag |= CMDARG_FLAG_G;
            break;
        case 'f':
            args->arg_num++;
            args->arg_flag |= CMDARG_FLAG_F;
            break;
        case 'o':
            args->arg_num++;
            args->arg_flag |= CMDARG_FLAG_O;
            break;
        case ARGP_KEY_END:
            if (args->arg_num > 3)
                argp_usage(state);
            break;
        case ARGP_KEY_ARG:
        case ARGP_KEY_NO_ARGS:
        case ARGP_KEY_ERROR:
        case ARGP_KEY_SUCCESS:
        case ARGP_KEY_FINI:
        case ARGP_KEY_INIT:
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static error_t parse_opt_test(int key, char *arg, struct argp_state *state)
{
    struct nvm_init_arg *args = state->input;

    switch (key) {
        case 's':
            if (!arg || strlen(arg) == 0 || strlen(arg) > CMDARG_LEN) {
                argp_usage(state);
            }
            strcpy(args->test_setname,arg);
            args->arg_num++;
            args->arg_flag |= CMDARG_FLAG_S;
            break;
        case 't':
            if (!arg || strlen(arg) == 0 || strlen(arg) > CMDARG_LEN)
                argp_usage(state);
            strcpy(args->test_subtest,arg);
            args->arg_num++;
            args->arg_flag |= CMDARG_FLAG_T;
            break;
        case 'a':
            args->arg_num++;
            args->arg_flag |= CMDARG_FLAG_A;
            break;
        case 'l':
            args->arg_num++;
            args->arg_flag |= CMDARG_FLAG_L;
            break;
        case ARGP_KEY_END:
            if (args->arg_num > 2 || args->arg_num == 0)
                argp_usage(state);
            if (args->arg_flag & CMDARG_FLAG_A && args->arg_num > 1)
                argp_usage(state);
            if (args->arg_flag & CMDARG_FLAG_L && args->arg_num > 1)
                argp_usage(state);
            if (args->arg_flag & CMDARG_FLAG_T && args->arg_num != 2)
                argp_usage(state);
            break;
        case ARGP_KEY_ARG:
        case ARGP_KEY_NO_ARGS:
        case ARGP_KEY_ERROR:
        case ARGP_KEY_SUCCESS:
        case ARGP_KEY_FINI:
        case ARGP_KEY_INIT:
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static error_t parse_opt_admin(int key, char *arg, struct argp_state *state)
{
    struct nvm_init_arg *args = state->input;

    switch (key) {
        case 't':
            if (!arg || strlen(arg) == 0 || strlen(arg) > CMDARG_LEN)
                argp_usage(state);
            strcpy(args->admin_task,arg);
            args->arg_num++;
            args->arg_flag |= CMDARG_FLAG_T;
            break;
        case 'l':
            args->arg_num++;
            args->arg_flag |= CMDARG_FLAG_L;
            break;
        case ARGP_KEY_END:
            if (args->arg_num > 1 || args->arg_num == 0)
                argp_usage(state);
            break;
        case ARGP_KEY_ARG:
        case ARGP_KEY_NO_ARGS:
        case ARGP_KEY_ERROR:
        case ARGP_KEY_SUCCESS:
        case ARGP_KEY_FINI:
        case ARGP_KEY_INIT:
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static void cmd_prepare(struct argp_state *state, struct nvm_init_arg *args,
                                              char *cmd, struct argp *argp_cmd)
{
    /* Remove the first arg from the parser */
    int argc = state->argc - state->next + 1;
    char** argv = &state->argv[state->next - 1];
    char* argv0 = argv[0];

    argv[0] = malloc(strlen(state->name) + strlen(cmd) + 2);
    if(!argv[0])
        argp_failure(state, 1, ENOMEM, 0);

    sprintf(argv[0], "%s %s", state->name, cmd);

    argp_parse(argp_cmd, argc, argv, ARGP_IN_ORDER, &argc, args);

    free(argv[0]);
    argv[0] = argv0;
    state->next += argc - 1;
}

static struct argp argp_test = {opt_test, parse_opt_test, 0, doc_test};
static struct argp argp_admin = {opt_admin, parse_opt_admin, 0, doc_admin};

error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    struct nvm_init_arg *args = state->input;

    switch(key)
    {
        case ARGP_KEY_ARG:
            if (strcmp(arg, "start") == 0)
                args->cmdtype = CMDARG_START;
            else if (strcmp(arg, "debug") == 0)
                args->cmdtype = CMDARG_DEBUG;
            else if (strcmp(arg, "null") == 0)
                args->cmdtype = CMDARG_NULL;
            else if (strcmp(arg, "null-debug") == 0)
                args->cmdtype = CMDARG_NULL_DEBUG;
            else if (strcmp(arg, "test") == 0){
                args->cmdtype = CMDARG_TEST;
                cmd_prepare(state, args, "test", &argp_test);
            } else if (strcmp(arg, "admin") == 0){
                args->cmdtype = CMDARG_ADMIN;
                cmd_prepare(state, args, "admin", &argp_admin);
            }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp_global={NULL, parse_opt,"ox-ctrl [<cmd> [cmd-options]]",
                                                                   doc_global};

int cmdarg_init (int argc, char **argv)
{
    int ret;

    core.args_global = malloc (sizeof (struct nvm_init_arg));
    memset (core.args_global, 0, sizeof(struct nvm_init_arg));

    argp_parse(&argp_global, argc, argv, ARGP_IN_ORDER, NULL, core.args_global);

    switch (core.args_global->cmdtype)
    {
        case CMDARG_START:
            core.std_ftl = FTL_ID_APPNVM;
            if (core.args_global->arg_flag & CMDARG_FLAG_O) {
                core.lnvm = 1;
                core.std_ftl = FTL_ID_LNVM;
            }
            return OX_RUN_MODE;
        case CMDARG_DEBUG:
            core.std_ftl = FTL_ID_APPNVM;
            core.debug |= 1 << 0;
            if (core.args_global->arg_flag & CMDARG_FLAG_F)
                core.ftl_debug = 1;
            if (core.args_global->arg_flag & CMDARG_FLAG_O) {
                core.lnvm = 1;
                core.std_ftl = FTL_ID_LNVM;
            }
            if ((core.args_global->arg_flag & CMDARG_FLAG_F) &&
                                !(core.args_global->arg_flag & CMDARG_FLAG_G))
                core.debug ^= 1 << 0;
            return OX_RUN_MODE;
        case CMDARG_NULL:
            core.null |= 1 << 0;
            return OX_RUN_MODE;
        case CMDARG_NULL_DEBUG:
            core.debug |= 1 << 0;
            core.null |= 1 << 0;
            return OX_RUN_MODE;
        case CMDARG_TEST:
            ret = nvm_test_unit(core.args_global);
            return (!ret) ? OX_TEST_MODE : -1;
        case CMDARG_ADMIN:
            ret = nvm_admin_unit(core.args_global);
            return (!ret) ? OX_ADMIN_MODE : -1;
        default:
            printf("Invalid command, please use --help to see more info.\n");
    }

    return -1;
}