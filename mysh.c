#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <glob.h>
#include <errno.h>

#define MAX_INPUT_LENGTH 1024
#define MAX_ARGS 64
#define MAX_DIRS 6

char *dirs[MAX_DIRS] = {"/usr/local/sbin", "/usr/local/bin", "/usr/sbin", "/usr/bin", "/sbin", "/bin"};

int execute_builtin_command(char *command, char **args);
int search_file(char *filename, char *path);
int expand_wildcards(char **args);
int execute_command(char **args, int *status);


int main(int argc, char *argv[])
{
    char input[MAX_INPUT_LENGTH];
    int batch_mode = 0;
    FILE *input_file = NULL;
    int status = 0;

    // Parse command-line arguments
    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [batch_file]\n", argv[0]);
        exit(1);
    }
    if (argc == 2)
    {
        batch_mode = 1;
        input_file = fopen(argv[1], "r");
        if (input_file == NULL)
        {
            fprintf(stderr, "Error: Cannot open file '%s'\n", argv[1]);
            exit(1);
        }
    }

    // Print welcome message if running in interactive mode
    if (!batch_mode)
    {
        printf("Welcome to my shell!\n");
    }
    // Set the default search path
    char *default_path = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    if (setenv("PATH", default_path, 1) != 0)
    {
        fprintf(stderr, "Error: failed to set default search path\n");
        return 1;
    }

    // Input loop
    while (1)
    {
        // Print prompt
        if (!batch_mode)
        {
            if (batch_mode || !status)
            {
                printf("mysh> ");
            }
            else
            {
                printf("!mysh> ");
            }
        }

        // Read input
        if (batch_mode)
        {
            if (fgets(input, MAX_INPUT_LENGTH, input_file) == NULL)
            {
                // End of file
                break;
            }
            // Remove newline character
            input[strcspn(input, "\n")] = '\0';
            // printf("%s\n", input); // Echo input in batch mode
            // break;
        }
        else
        {
            // Read a line of input from the user
            if (fgets(input, MAX_INPUT_LENGTH, stdin) == NULL)
            {
                break;
            }
            // Remove newline character
            input[strcspn(input, "\n")] = '\0';
        }

        // Check for exit command
        if (strcmp(input, "exit") == 0)
        {
            break;
        }

        // Parse input into arguments
        char *args[MAX_ARGS];
        char *token = strtok(input, " ");
        int i = 0;
        while (token != NULL)
        {
            args[i] = token;
            token = strtok(NULL, " ");
            i++;
        }
        args[i] = NULL; // Null-terminate the argument list

        // Expand wildcards in arguments
        if (expand_wildcards(args) != 0)
        {
            status = 1;
            continue;
        }

        // Check for built-in commands
        if (args[0] != NULL && (strcmp(args[0], "cd") == 0 || strcmp(args[0], "pwd") == 0 || strcmp(args[0], "echo") == 0 || strcmp(args[0], "cat") == 0 || strcmp(args[0], "ls") == 0))
        {
            status = execute_builtin_command(args[0], args);
            continue;
        }

        // Execute command
        status = execute_command(args, &status);
    }

    // Print goodbye message if running in interactive mode
    if (!batch_mode)
    {
        printf("mysh: exiting\n");
    }

    return 0;
}

int execute_builtin_command(char *command, char *args[])
{
    // Check if the command is "cd"
    if (strcmp(command, "cd") == 0)
    {
        // Check if an argument was passed in or not
        if (args[1] == NULL)
        {
            // If no argument was passed in, change to home directory
            char *home_dir = getenv("HOME"); // Get home directory using getenv
            if (home_dir == NULL)
            {
                home_dir = "/"; // If getenv fails, set home directory to root directory
            }
            if (chdir(home_dir) == -1) // Change to home directory using chdir
            {
                perror("cd"); // If there was an error, print error message using perror
                return 1;     // Return 1 to indicate failure
            }
        }
        else
        {
            // If an argument was passed in, change to specified directory
            char *dir_path = args[1]; // Get directory path from arguments
            if (dir_path[0] == '~')
            {
                // If the argument starts with a tilde, replace with home directory path
                char *home_dir = getenv("HOME"); // Get home directory using getenv
                if (home_dir == NULL)
                {
                    home_dir = "/"; // If getenv fails, set home directory to root directory
                }
                char *new_path = malloc(strlen(home_dir) + strlen(dir_path)); // Allocate memory for new path
                sprintf(new_path, "%s%s", home_dir, dir_path + 1);            // Combine home directory and path
                dir_path = new_path;                                          // Update directory path variable
            }
            if (chdir(dir_path) == -1) // Change to specified directory using chdir
            {
                perror("cd"); // If there was an error, print error message using perror
                return 1;     // Return 1 to indicate failure
            }
        }
        return 0; // Return 0 to indicate success
    }
    // Check if the command is "pwd"
    else if (strcmp(command, "pwd") == 0)
    {
        char cwd[1024];                       // Declare buffer to hold current working directory
        if (getcwd(cwd, sizeof(cwd)) == NULL) // Get current working directory using getcwd
        {
            perror("pwd"); // If there was an error, print error message using perror
            return 1;      // Return 1 to indicate failure
        }
        printf("%s\n", cwd); // Print current working directory to console
        return 0;            // Return 0 to indicate success
    }
    else if (strcmp(command, "cat") == 0)
    {
        if (args[1] == NULL)
        {
            fprintf(stderr, "Error: no input file specified\n");
            return 1;
        }

        for (int i = 1; args[i] != NULL; i++)
        {
            char *filename = args[i];
            FILE *input_file = fopen(filename, "r");
            if (input_file == NULL)
            {
                fprintf(stderr, "Error: failed to open file '%s'\n", filename);
                return 1;
            }

            char line[MAX_INPUT_LENGTH];
            while (fgets(line, sizeof(line), input_file))
            {
                printf("%s", line);
            }

            fclose(input_file);
        }

        return 0;
    }
    // Check if the command is "echo"
    else if (strcmp(command, "echo") == 0)
    {
        int redirect_output = 0;      // Flag to indicate output redirection
        char *output_filename = NULL; // Output filename for redirection

        // Check for output redirection
        int i;
        for (i = 1; args[i] != NULL; i++)
        {
            if (strcmp(args[i], ">") == 0)
            {
                redirect_output = 1;
                if (args[i + 1] == NULL)
                {
                    fprintf(stderr, "Error: no output file specified\n");
                    return 1;
                }
                output_filename = args[i + 1];
                args[i] = NULL;
                break;
            }
        }

        // If output redirection was specified, open the output file and redirect stdout to it
        if (redirect_output)
        {
            FILE *output_file = fopen(output_filename, "w");
            if (output_file == NULL)
            {
                fprintf(stderr, "Error: failed to open output file\n");
                return 1;
            }
            stdout = output_file;
        }

        // Print the arguments to the console or to the output file if redirection is enabled
        for (int j = 1; args[j] != NULL; j++)
        {
            if (args[j][0] == '$')
            {
                char *env_var = getenv(args[j] + 1); // Get the value of the environment variable
                if (env_var != NULL)
                {
                    printf("%s ", env_var); // Substitute the variable's value
                }
            }
            else
            {
                printf("%s ", args[j]);
            }
        }
        printf("\n");

        // Flush the output buffer and close the output file if redirection was enabled
        if (redirect_output)
        {
            fflush(stdout);
            fclose(stdout);
            stdout = fopen("/dev/tty", "w");
            if (stdout == NULL)
            {
                fprintf(stderr, "Error: failed to restore stdout\n");
                return 1;
            }
        }

        return 0; // Return 0 to indicate success
    }

    else if (strcmp(command, "ls") == 0)
    {
        // Check for arguments
        if (args[1] != NULL)
        {
            fprintf(stderr, "Error: ls command does not accept arguments\n");
            return 1;
        }

        // Execute the ls command
        if (system("ls") == -1) // Use system() to run the ls command with the -1 option
        {
            perror("ls");
            return 0;
        }
    }

    else
    {
        fprintf(stderr, "Unknown command: %s\n", command);
        return 1;
    }
    return 0;
}

int search_file(char *filename, char *full_path)
{
    struct stat sb;
    // Iterate over each directory in the search path
    for (int i = 0; i < MAX_DIRS; i++)
    {
        // Construct the full path to the file
        size_t path_len = strlen(dirs[i]) + 1 + strlen(filename) + 1;
        char path[path_len];
        snprintf(path, path_len, "%s/%s", dirs[i], filename);
        // Check if the file exists and is executable
        if (stat(path, &sb) != -1 && S_ISREG(sb.st_mode) && (sb.st_mode & S_IXUSR))
        {
            // If the file exists and is executable, store its full path and return success
            strcpy(full_path, path);
            return 1;
        }
        else if (errno == ENOENT)
        {
            // If the file was not found, skip to the next directory
            continue;
        }
        else
        {
            // If an error occurred (other than "file not found"), print an error message and return failure
            fprintf(stderr, "Error: failed to access file '%s' in directory '%s'\n", filename, dirs[i]);
            return 0;
        }
    }
    // If the file was not found in any directory, print an error message and return failure
    fprintf(stderr, "Error: file '%s' not found\n", filename);
    return 0;
}

int expand_wildcards(char **args)
{
    for (int i = 0; args[i] != NULL; i++)
    {
        if (strchr(args[i], '*') != NULL)
        {
            glob_t results;
            if (glob(args[i], GLOB_NOCHECK | GLOB_TILDE, NULL, &results) == 0)
            {
                size_t j;
                for (j = 0; j < results.gl_pathc; j++)
                {
                    // Add each expanded path to the argument list
                    if (i < MAX_ARGS - 1)
                    {
                        args[++i] = strdup(results.gl_pathv[j]);
                    }
                    else
                    {
                        fprintf(stderr, "Error: too many arguments\n");
                        return 1;
                    }
                }
                globfree(&results);
                // Shift remaining arguments to make room for expanded paths
                for (int k = i + 1; args[k] != NULL; k++)
                {
                    args[k - j] = args[k];
                }
                args[i - j + 1] = NULL;
            }
            else
            {
                fprintf(stderr, "Error: failed to expand wildcard %s\n", args[i]);
                return 1;
            }
        }
    }
    return 0;
}

int execute_command(char **args, int *status)
{
    char path[1024];
    if (search_file(args[0], path))
    {
        pid_t pid;
        int pipe_fd[2];
        const char *pipe_token = strtok(args[0], "|");
        char *input_file = "";
        char *output_file = "";
        int output_append = 0;
        int command_redirect = 0;
        const char *last_command = NULL;

        while (pipe_token != NULL)
        {
            // Parse command into arguments
            char *token = strtok((char *)pipe_token, " ");
            int i = 0;
            while (token != NULL)
            {
                if (strcmp(token, "<") == 0)
                {
                    // Input file redirection
                    token = strtok(NULL, " ");
                    if (token == NULL)
                    {
                        fprintf(stderr, "Error: no input file specified\n");
                        return 1;
                    }
                    input_file = token;
                    token = strtok(NULL, " ");
                }
                else if (strcmp(token, ">") == 0)
                {
                    // Output file redirection
                    token = strtok(NULL, " ");
                    if (token == NULL)
                    {
                        fprintf(stderr, "Error: no output file specified\n");
                        return 1;
                    }
                    output_file = token;
                    token = strtok(NULL, " ");
                    if (token != NULL && strcmp(token, ">") == 0)
                    {
                        // Output append redirection
                        output_append = 1;
                        token = strtok(NULL, " ");
                    }
                    command_redirect = 1;
                }
                else if (strchr(token, '*') != NULL)
                {
                    // Wildcard expansion
                    glob_t results;
                    int flags = 0;
                    if (command_redirect)
                    {
                        flags |= GLOB_APPEND;
                    }
                    if (glob(token, flags, NULL, &results) != 0)
                    {
                        fprintf(stderr, "Error: failed to expand wildcard %s\n", token);
                        return 1;
                    }
                    for (size_t j = 0; j < results.gl_pathc; j++)
                    {
                        args[i] = results.gl_pathv[j];
                        i++;
                        if (i == MAX_ARGS - 1)
                        {
                            fprintf(stderr, "Error: too many arguments\n");
                            return 1;
                        }
                    }
                    globfree(&results);
                    token = strtok(NULL, " ");
                }
                else
                {
                    // Normal argument
                    args[i] = token;
                    i++;
                    if (i == MAX_ARGS - 1)
                    {
                        fprintf(stderr, "Error: too many arguments\n");
                        return 1;
                    }
                    token = strtok(NULL, " ");
                }
            }
            args[i] = NULL; // Null-terminate the argument list

            if (status != 0)
            {
                break;
            }

            if (pipe_token != NULL)
            {
                // Create pipe
                if (pipe(pipe_fd) == -1)
                {
                    perror("pipe");
                    return 1;
                }
            }
            pid = fork();
            if (pid == 0)
            {
                // Child process
                if (input_file[0] != '\0')
                {
                    // Input file redirection
                    int input_fd = open(input_file, O_RDONLY);
                    if (input_fd == -1)
                    {
                        perror("open");
                        exit(1);
                    }
                    if (dup2(input_fd, STDIN_FILENO) == -1)
                    {
                        perror("dup2");
                        exit(1);
                    }
                    close(input_fd);
                }
                if (output_file[0] != '\0')
                {
                    // Output file redirection
                    int output_fd;
                    if (output_append)
                    {
                        output_fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
                    }
                    else
                    {
                        output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                    }
                    if (output_fd == -1)
                    {
                        perror("open");
                        exit(1);
                    }
                    if (dup2(output_fd, STDOUT_FILENO) == -1)
                    {
                        perror("dup2");
                        exit(1);
                    }
                    close(output_fd);
                }
                if (pipe_token != NULL)
                {
                    // Output to pipe
                    if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
                    {
                        perror("dup2");
                        exit(1);
                    }
                    close(pipe_fd[0]);
                    close(pipe_fd[1]);
                }
                if (execv(path, args) == -1)
                {
                    perror("execv");
                    exit(1);
                }
            }
            else if (pid > 0)
            {
                // Parent process
                if (pipe_token != NULL)
                {
                    // Input from pipe
                    if (dup2(pipe_fd[0], STDIN_FILENO) == -1)
                    {
                        perror("dup2");
                        exit(1);
                    }
                    close(pipe_fd[0]);
                    close(pipe_fd[1]);
                }
                waitpid(pid, status, 0);
            }
            else
            {
                // Fork failed
                perror("fork");
                return 1;
            }
            pipe_token = strtok(NULL, "|");
        }

        if (last_command != NULL)
        {
            fprintf(stderr, "Error: missing command after %s\n", last_command);
            return 1;
        }

        else
        {
            fprintf(stderr, "Command not found: %s\n", args[0]);
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "Command not found: %s\n", args[0]);
        return 1;
    }
}
