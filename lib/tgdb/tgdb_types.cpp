#if HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_STDIO_H
#include <stdio.h>
#endif

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

/* Local includes */
#include "tgdb_types.h"
#include "logger.h"
#include "sys_util.h"
#include "ibuf.h"
#include "tgdb_list.h"
#include "queue.h"

static int tgdb_types_print_item(void *command)
{
    struct tgdb_response *com = (struct tgdb_response *) command;
    FILE *fd = stderr;

    if (!com) {
        logger_write_pos(logger, __FILE__, __LINE__, "item is null");
        return -1;
    }

    switch (com->header) {
        case TGDB_UPDATE_BREAKPOINTS:
        {
            int i;

            fprintf(fd, "Breakpoint start\n");

            for (i = 0; i < sbcount(com->choice.update_breakpoints.breakpoints); i++) {
                struct tgdb_breakpoint *tb = &com->choice.update_breakpoints.breakpoints[i];

                fprintf(fd,
                        "\tFILE(%s) FULLNAME(%s) LINE(%d) ENABLED(%d)\n",
                        tb->file, tb->fullname, tb->line, tb->enabled);

            }

            fprintf(fd, "Breakpoint end\n");
            break;
        }
        case TGDB_UPDATE_FILE_POSITION:
        {
            struct tgdb_file_position *tfp =
                    com->choice.update_file_position.file_position;

            fprintf(fd,
                    "TGDB_UPDATE_FILE_POSITION PATH(%s) LINE(%d)\n",
                    tfp->path, tfp->line_number);
            break;
        }
        case TGDB_UPDATE_SOURCE_FILES:
        {
            struct tgdb_list *list =
                    com->choice.update_source_files.source_files;
            tgdb_list_iterator *i;
            char *s;

            fprintf(fd, "Inferior source files start\n");
            i = tgdb_list_get_first(list);

            while (i) {
                s = (char *) tgdb_list_get_item(i);
                fprintf(fd, "TGDB_SOURCE_FILE (%s)\n", s);
                i = tgdb_list_next(i);
            }
            fprintf(fd, "Inferior source files end\n");
            break;
        }
        case TGDB_INFERIOR_EXITED:
        {
            int status = com->choice.inferior_exited.exit_status;

            fprintf(fd, "TGDB_INFERIOR_EXITED(%d)\n", status);
            break;
        }
        case TGDB_UPDATE_COMPLETIONS:
        {
            struct tgdb_list *list =
                    com->choice.update_completions.completion_list;
            tgdb_list_iterator *i;
            char *s;

            fprintf(fd, "completions start\n");
            i = tgdb_list_get_first(list);

            while (i) {
                s = (char *) tgdb_list_get_item(i);
                fprintf(fd, "TGDB_UPDATE_COMPLETION (%s)\n", s);
                i = tgdb_list_next(i);
            }
            fprintf(fd, "completions end\n");
            break;
        }
        case TGDB_DISASSEMBLE_PC:
        case TGDB_DISASSEMBLE_FUNC:
            //$ TODO
            break;
        case TGDB_UPDATE_CONSOLE_PROMPT_VALUE:
        {
            const char *value =
                    com->choice.update_console_prompt_value.prompt_value;
            fprintf(fd, "TGDB_UPDATE_CONSOLE_PROMPT_VALUE(%s)\n", value);
            break;
        }
        case TGDB_DEBUGGER_COMMAND_DELIVERED: {
            const char *value =
                    com->choice.debugger_command_delivered.command;
            fprintf(fd, "TGDB_DEBUGGER_COMMAND_DELIVERED:(%s)\n", value);
            break;
        }
        case TGDB_QUIT:
        {
            struct tgdb_debugger_exit_status *status =
                    com->choice.quit.exit_status;
            fprintf(fd, "TGDB_QUIT EXIT_STATUS(%d)RETURN_VALUE(%d)\n",
                    status->exit_status, status->return_value);
            break;
        }
    }

    return 0;
}

static int tgdb_types_breakpoint_free(void *data)
{
    struct tgdb_breakpoint *tb;

    tb = (struct tgdb_breakpoint *) data;

    /* Free the structure */
    free((char *) tb->file);
    tb->file = NULL;
    free((char *) tb->fullname);
    tb->fullname = NULL;
    free(tb);
    tb = NULL;
    return 0;
}

static int tgdb_types_source_files_free(void *data)
{
    char *s = (char *) data;

    free(s);
    s = NULL;
    return 0;
}

static int tgdb_types_delete_item(void *command)
{
    struct tgdb_response *com = (struct tgdb_response *) command;

    if (!com)
        return -1;

    switch (com->header) {
        case TGDB_UPDATE_BREAKPOINTS:
        {
            int i;
            struct tgdb_breakpoint *breakpoints =
                com->choice.update_breakpoints.breakpoints;

            for (i = 0; i < sbcount(breakpoints); i++) {

                struct tgdb_breakpoint *tb = &breakpoints[i];

                free(tb->file);
                free(tb->fullname);
            }

            sbfree(breakpoints);
            com->choice.update_breakpoints.breakpoints = NULL;
            break;
        }
        case TGDB_UPDATE_FILE_POSITION:
        {
            struct tgdb_file_position *tfp =
                    com->choice.update_file_position.file_position;

            free(tfp->path);
            free(tfp->from);
            free(tfp->func);

            free(tfp);

            com->choice.update_file_position.file_position = NULL;
            break;
        }
        case TGDB_UPDATE_SOURCE_FILES:
        {
            struct tgdb_list *list =
                    com->choice.update_source_files.source_files;

            tgdb_list_free(list, tgdb_types_source_files_free);
            tgdb_list_destroy(list);

            com->choice.update_source_files.source_files = NULL;
            break;
        }
        case TGDB_INFERIOR_EXITED:
            break;
        case TGDB_UPDATE_COMPLETIONS:
        {
            struct tgdb_list *list =
                    com->choice.update_completions.completion_list;

            tgdb_list_free(list, tgdb_types_source_files_free);
            tgdb_list_destroy(list);

            com->choice.update_completions.completion_list = NULL;
            break;
        }
        case TGDB_DISASSEMBLE_PC:
        case TGDB_DISASSEMBLE_FUNC:
        {
            int i;
            char **disasm = com->choice.disassemble_function.disasm;

            for (i = 0; i < sbcount(disasm); i++) {
                free(disasm[i]);
            }
            sbfree(disasm);
            break;
        }
        case TGDB_UPDATE_CONSOLE_PROMPT_VALUE:
        {
            const char *value =
                    com->choice.update_console_prompt_value.prompt_value;

            free((char *) value);
            com->choice.update_console_prompt_value.prompt_value = NULL;
            break;
        }
        case TGDB_DEBUGGER_COMMAND_DELIVERED: {
            const char *value =
                com->choice.debugger_command_delivered.command;
            free((char*)value);
            break;
        }
        case TGDB_QUIT:
        {
            struct tgdb_debugger_exit_status *status =
                    com->choice.quit.exit_status;

            free(status);
            com->choice.quit.exit_status = NULL;
            break;
        }
    }

    free(com);
    com = NULL;
    return 0;
}

int tgdb_types_print_command(void *command)
{
    return tgdb_types_print_item((void *) command);
}

int tgdb_types_free_command(void *command)
{
    return tgdb_types_delete_item((void *) command);
}

void
tgdb_types_append_command(struct tgdb_list *command_list,
        struct tgdb_response *response)
{
    tgdb_list_append(command_list, response);
}