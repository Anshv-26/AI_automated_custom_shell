#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h> // For directory listing
#include <time.h>   // For date/time functions
#include <math.h>   // For calculator function
#include <fnmatch.h> // For pattern matching
#include <sys/stat.h> // For mkdir, touch
#include <curl/curl.h> // For Ollama API calls
#include <termios.h>  // For raw terminal mode
#include "ollama_integration.h"

// Handle macOS json-c include path
#ifdef __APPLE__
#include "/opt/homebrew/include/json-c/json.h"
#else
#include <json-c/json.h>
#endif

// Constants for buffer sizes and token delimiters
#define RIPPLE_RL_BUFSIZE 1024
#define RIPPLE_TOK_BUFSIZE 64
#define RIPPLE_TOK_DELIM " \t\r\n\a"
#define RIPPLE_VERSION "1.0.0"
#define OLLAMA_API_URL "http://localhost:11434/api/generate"

// Special key codes
#define KEY_TAB 9
#define KEY_BACKSPACE 127
#define KEY_ENTER 10


// Function declarations for built-in commands
int ripple_cd(char **args);
int ripple_help(char **args);
int ripple_exit(char **args);
int ripple_bg(char **args);
int ripple_history(char **args);
int ripple_clear(char **args);
int ripple_echo(char **args);
int ripple_pwd(char **args);
int ripple_ls(char **args);
int ripple_version(char **args);
int ripple_calc(char **args);
int ripple_datetime(char **args);
int ripple_count(char **args);
int ripple_find(char **args);
int ripple_cat(char **args);
int ripple_tree(char **args);
int ripple_mkdir(char **args);
int ripple_touch(char **args);
int ripple_rm(char **args);
int ripple_whoami(char **args);

// Forward declarations for functions used by builtins
char* strAppend(char* str1, char* str2);
void add_to_hist(char **args);
void print_tree(const char *basepath, const char *prefix, int is_last);

// Array of built-in command names, used to map user input to the right functions
char *builtin_str[] = {
    "cd",
    "help",
    "exit",
    "bg",
    "history",
    "clear",
    "echo",
    "pwd",
    "ls",
    "version",
    "calc",
    "datetime",
    "count",
    "find",
    "cat",
    "tree",
    "mkdir",
    "touch",
    "rm",
    "whoami"
};



// Get number of built-in commands
int ripple_num_builtins(void) {
    return sizeof(builtin_str) / sizeof(char *);
}

// Array of built-in function pointers
int (*builtin_func[])(char **) = {
    &ripple_cd,
    &ripple_help,
    &ripple_exit,
    &ripple_bg,
    &ripple_history,
    &ripple_clear,
    &ripple_echo,
    &ripple_pwd,
    &ripple_ls,
    &ripple_version,
    &ripple_calc,
    &ripple_datetime,
    &ripple_count,
    &ripple_find,
    &ripple_cat,
    &ripple_tree,
    &ripple_mkdir,
    &ripple_touch,
    &ripple_rm,
    &ripple_whoami
};

// Struct for curl response
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Function to handle memory allocation for curl response
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

// Add these terminal control functions with better error handling and verification
void enable_raw_mode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    // Disable terminal flags that might interfere
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_cc[VMIN] = 1; // Wait for at least one character
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode() {
    struct termios term;
    
    // Get current settings
    if (tcgetattr(STDIN_FILENO, &term) == -1) {
        perror("tcgetattr");
        exit(1);
    }
    
    // Restore original settings
    term.c_lflag |= (ECHO | ICANON | ISIG | IEXTEN);
    term.c_iflag |= (IXON | ICRNL);
    term.c_oflag |= (OPOST);
    
    // Apply the restored settings
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) == -1) {
        perror("tcsetattr");
        exit(1);
    }
}


// Add a function to check if we're in raw mode
int is_raw_mode() {
    struct termios term;
    if (tcgetattr(STDIN_FILENO, &term) == -1) {
        perror("tcgetattr");
        return 0;
    }
    
    // Check if canonical mode is disabled
    return !(term.c_lflag & ICANON);
}

// the above 3 functions are directly taken from terminal shell documentations,less understandable but working



// Built-in: Change directory
int ripple_cd(char **args) {
    if (args[1] == NULL) {
        // If no argument is provided, change to the user's home directory
        char *home_dir = getenv("HOME");
        if (home_dir != NULL) {
            if (chdir(home_dir) != 0) {
                perror("ripple");
            } else {
                char cwd[1024];
                if (getcwd(cwd, sizeof(cwd)) != NULL) {
                    printf("Current directory: %s\n", cwd);
                }
            }
        } else {
            fprintf(stderr, "ripple: HOME environment variable not set\n");
        }
    } else {
        if (chdir(args[1]) != 0) {
            perror("ripple");
        } else {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("Current directory: %s\n", cwd);
            }
        }
    }
    return 1; // Continue shell loop
}

// Built-in: help
int ripple_help(char **args)
{
  int i;
  printf("ACM's very own shell\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < ripple_num_builtins(); i++)
  {
    printf("  %s\n", builtin_str[i]);
  }

  return 1;
}

// Built-in: Exit the shell
int ripple_exit(char **args) {
    return 0; // Exit shell loop
}

// Built-in: Clear the screen
int ripple_clear(char **args) {
    printf("\033[H\033[J"); // ANSI escape sequence to clear screen
    return 1;
}

// Built-in: Echo text
int ripple_echo(char **args) {
    int i = 1;
    
    // If no arguments, just print a newline
    if (args[1] == NULL) {
        printf("\n");
        return 1;
    }
    
    // Print all arguments with spaces in between
    while (args[i] != NULL) {
        printf("%s", args[i]);
        if (args[i+1] != NULL) {
            printf(" ");
        }
        i++;
    }
    printf("\n");
    return 1;
}

// Built-in: Print working directory
int ripple_pwd(char **args) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("ripple: pwd");
    }
    return 1;
}


// Built-in: List directory contents
int ripple_ls(char **args) {
    DIR *d;
    struct dirent *dir;
    char *path = "."; // Default to current directory
    
    if (args[1] != NULL) {
        path = args[1];
    }
    
    d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            // Skip hidden files (starting with .)
            if (dir->d_name[0] != '.') {
                printf("%s\n", dir->d_name);
            }
        }
        closedir(d);
    } else {
        perror("ripple: ls");
    }
    return 1;
}

// Built-in: Show version
int ripple_version(char **args) {
    printf("Ripple Shell version %s\n", RIPPLE_VERSION);
    return 1;
}

// Built-in: Simple calculator
int ripple_calc(char **args) {
    if (args[1] == NULL || args[2] == NULL || args[3] == NULL) {
        printf("Usage: calc <number> <operator> <number>\n");
        printf("Operators: + - * / %% ^\n");
        return 1;
    }
    
    double a = atof(args[1]);
    double b = atof(args[3]);
    char op = args[2][0];
    double result = 0;
    
    switch (op) {
        case '+':
            result = a + b;
            break;
        case '-':
            result = a - b;
            break;
        case '*':
            result = a * b;
            break;
        case '/':
            if (b == 0) {
                printf("Error: Division by zero\n");
                return 1;
            }
            result = a / b;
            break;
        case '%':
            if (b == 0) {
                printf("Error: Division by zero\n");
                return 1;
            }
            result = fmod(a, b);
            break;
        case '^':
            result = pow(a, b);
            break;
        default:
            printf("Error: Unknown operator %c\n", op);
            return 1;
    }
    
    printf("%.6g\n", result);
    return 1;
}

// Built-in: Display formatted date and time
// struct_tm is a part c standard library
int ripple_datetime(char **args) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char date_str[64];
    
    strftime(date_str, sizeof(date_str), "%A, %B %d, %Y - %I:%M:%S %p", tm);
    printf("%s\n", date_str);
    
    return 1;
}

// Built-in: Count files in a directory
int ripple_count(char **args) {
    DIR *d;
    struct dirent *dir;
    char *path = "."; // Default to current directory
    int count = 0;
    int count_dirs = 0;
    int count_files = 0;
    
    if (args[1] != NULL) {
        path = args[1];
    }
    
    d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            // Skip . and ..
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }
            
            count++;
            
            // Check if it's a directory or file
            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dir->d_name);
            
            DIR *subdir = opendir(fullpath);
            if (subdir) {
                count_dirs++;
                closedir(subdir);
            } else {
                count_files++;
            }
        }
        closedir(d);
        
        printf("Total: %d items (%d directories, %d files)\n", count, count_dirs, count_files);
    } else {
        perror("ripple: count");
    }
    
    return 1;
}


// Function to recursively find files
void find_files(const char *base_path, const char *pattern, int *count) {
    DIR *d;
    struct dirent *dir;
    
    d = opendir(base_path);
    if (!d) {
        return;
    }
    
    while ((dir = readdir(d)) != NULL) {
        // Skip . and ..
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }
        
        // Create path
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", base_path, dir->d_name);
        
        // Check if matches pattern
        if (fnmatch(pattern, dir->d_name, 0) == 0) {
            printf("%s\n", path);
            (*count)++;
        }
        
        // Recurse if it's a directory
        DIR *subdir = opendir(path);
        if (subdir) {
            closedir(subdir);
            find_files(path, pattern, count);
        }
    }
    
    closedir(d);
}

// Built-in: Find files matching pattern
int ripple_find(char **args) {
    if (args[1] == NULL) {
        printf("Usage: find <pattern>\n");
        printf("Example: find \"*.c\" to find all C files\n");
        return 1;
    }
    
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("ripple: find");
        return 1;
    }
    
    printf("Searching for files matching '%s'...\n", args[1]);
    
    int count = 0;
    find_files(cwd, args[1], &count);
    
    printf("Found %d matching items\n", count);
    
    return 1;
}

// Built-in: Cat (display file contents)
int ripple_cat(char **args) {
    if (args[1] == NULL) {
        printf("Usage: cat <filename>\n");
        return 1;
    }
    
    FILE *file = fopen(args[1], "r");
    if (!file) {
        perror("ripple: cat");
        return 1;
    }
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        printf("%s", buffer);
    }
    
    fclose(file);
    return 1;
}

// Helper function to print directory tree
void print_tree(const char *basepath, const char *prefix, int is_last) {
    DIR *dir = opendir(basepath);
    if (!dir) {
        return;
    }
    
    // Print current directory
    printf("%s%s%s\n", prefix, is_last ? "└── " : "├── ", strrchr(basepath, '/') ? strrchr(basepath, '/') + 1 : basepath);
    
    // Create new prefix for children
    char new_prefix[1024];
    sprintf(new_prefix, "%s%s", prefix, is_last ? "    " : "│   ");
    
    // Count entries to determine which is the last one
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.' && strcmp(entry->d_name, "..") != 0) {
            count++;
        }
    }
    
    // Reset directory stream
    rewinddir(dir);
    
    // Process entries
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.' && strcmp(entry->d_name, "..") != 0) {
            i++;
            
            char path[1024];
            snprintf(path, sizeof(path), "%s/%s", basepath, entry->d_name);
            
            // Check if it's a directory
            DIR *subdir = opendir(path);
            if (subdir) {
                closedir(subdir);
                print_tree(path, new_prefix, i == count);
            } else {
                printf("%s%s%s\n", new_prefix, i == count ? "└── " : "├── ", entry->d_name);
            }
        }
    }
    
    closedir(dir);
}

// Built-in: Tree (display directory structure)
int ripple_tree(char **args) {
    char *path = ".";
    if (args[1] != NULL) {
        path = args[1];
    }
    
    DIR *dir = opendir(path);
    if (!dir) {
        perror("ripple: tree");
        return 1;
    }
    closedir(dir);
    
    printf("%s\n", path);
    
    // Count entries to determine which is the last one
    dir = opendir(path);
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.' && strcmp(entry->d_name, "..") != 0) {
            count++;
        }
    }
    
    // Reset directory stream
    rewinddir(dir);
    
    // Process entries
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.' && strcmp(entry->d_name, "..") != 0) {
            i++;
            
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            
            // Check if it's a directory
            DIR *subdir = opendir(full_path);
            if (subdir) {
                closedir(subdir);
                print_tree(full_path, "", i == count);
            } else {
                printf("%s%s\n", i == count ? "└── " : "├── ", entry->d_name);
            }
        }
    }
    
    closedir(dir);
    return 1;
}

// Built-in: Create directory
int ripple_mkdir(char **args) {
    if (args[1] == NULL) {
        printf("Usage: mkdir <directory_name>\n");
        return 1;
    }
    
    // Create directory with permissions 0755 (rwxr-xr-x)
    if (mkdir(args[1], 0755) != 0) {
        perror("ripple: mkdir");
        return 1;
    }
    
    printf("Directory created: %s\n", args[1]);
    return 1;
}

// Built-in: Create empty file
int ripple_touch(char **args) {
    if (args[1] == NULL) {
        printf("Usage: touch <filename>\n");
        return 1;
    }
    
    FILE *file = fopen(args[1], "a");
    if (!file) {
        perror("ripple: touch");
        return 1;
    }
    
    fclose(file);
    printf("File created/updated: %s\n", args[1]);
    return 1;
}

// Built-in: Remove file
int ripple_rm(char **args) {
    if (args[1] == NULL) {
        printf("Usage: rm <filename>\n");
        return 1;
    }
    
    if (remove(args[1]) != 0) {
        perror("ripple: rm");
        return 1;
    }
    
    printf("Removed: %s\n", args[1]);
    return 1;
}

// Built-in: Show current user
int ripple_whoami(char **args) {
    char *username = getenv("USER");
    if (username) {
        printf("%s\n", username);
    } else {
        printf("Unknown user\n");
    }
    return 1;
}

struct Node {
    char *str;
    struct Node* next;
};
struct Node* head = NULL;
struct Node* cur = NULL;

char* strAppend(char* str1, char* str2)
{
	char* str3 = (char*)malloc(sizeof(char*)*(strlen(str1)+strlen(str2)));
  strcpy(str3, str1);
  strcat(str3, str2);
	return str3;
}
void add_to_hist(char **args){
  if(head==NULL){
    head = (struct Node *)malloc(sizeof(struct Node));
      head->str = (char *)malloc(0x1000);
  char *str1 = " ";
  if (args[1] == NULL) 
     strcpy(head->str,strAppend(args[0],str1));
  else
{  
  strcpy(head->str,strAppend(args[0],str1));
  strcpy(head->str, strAppend(head->str, args[1]));
  }
  head->next = NULL;
  cur = head;
  }
else{
    struct Node *ptr = (struct Node *)malloc(sizeof(struct Node));
    cur->next = ptr;
    ptr->str = (char *)malloc(0x1000);
      char *str1 = " ";
  if (args[1] == NULL) 
     strcpy(ptr->str,strAppend(args[0],str1));
  else
{  
  strcpy(ptr->str,strAppend(args[0],str1));
  strcpy(ptr->str, strAppend(ptr->str, args[1]));
  }
    ptr->next = NULL;
    cur = ptr;
}
}

//creating a function to display history:
int ripple_history(char **args){  
   struct Node* ptr = head;
    int i = 1;
    while (ptr != NULL)
    {
    printf(" %d %s\n",i++,ptr->str);
    ptr = ptr->next;
    }
  return 1; 
  }

// Background command execution
int ripple_bg(char **args)
{
  ++args;
  char *firstCmd = args[0]; // echo
  int childpid = fork();
  if (childpid >= 0)
  {
    if (childpid == 0)
    {
      if (execvp(firstCmd, args) < 0)
      {
        perror("Error on execvp\n");
        exit(0);
      }
    }
  }
  else
  {
    perror("fork() error");
  }
  return 1;
}

// Launch an external command in the foreground
int ripple_launch(char **args) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) == -1) {
            if (errno == ENOENT) {
                fprintf(stderr, "ripple: command not found: %s\n", args[0]);
            } else {
                perror("ripple");
            }
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        // Fork error
        perror("ripple");
    } else {
        // Parent process
        waitpid(pid, &status, 0); // Simplified, no WUNTRACED
    }
    return 1; // Continue shell loop
}

// Execute a command (built-in or external)
int ripple_execute(char **args) {
    if (args[0] == NULL) {
        // Empty command
        return 1;
    }

    // Check for built-in commands
    for (int i = 0; i < ripple_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            add_to_hist(args); // Add command to history before executing
            return (*builtin_func[i])(args);
        }
    }

    // External command
    add_to_hist(args); // Add command to history before executing
    return ripple_launch(args);
}

// Modify the main shell loop to use raw mode
void ripple_loop(void) {
    char *line;
    char **args;
    int status;
    char cwd[1024];

    // Enable raw mode at the start
    enable_raw_mode();

    do {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("\033[1;32m%s\033[0m > ", cwd);
            fflush(stdout);  // Ensure prompt is displayed immediately
        } else {
            printf("> ");
            fflush(stdout);
        }
        
        line = ripple_read_line();
        if (!line) {
            break;
        }
        args = ripple_split_line(line);
        if (!args) {
            free(line);
            continue;
        }
        status = ripple_execute(args);

        free(line);
        free(args);
    } while (status);

    // Disable raw mode before exiting
    disable_raw_mode();
}

// Read a line of input
char *ripple_read_line(void) {
    int bufsize = RIPPLE_RL_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;
    if (!buffer) {
        fprintf(stderr, "ripple: allocation error\n");
        exit(EXIT_FAILURE);
    }
    while (1) {
        c = getchar();
        if (c == EOF) {
            // Only return NULL if nothing has been typed (Ctrl+D at empty prompt)
            if (position == 0) {
                free(buffer);
                return NULL;
            } else {
                continue; // Ignore spurious EOFs
            }
        } else if (c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else if (c == '\t') {
            buffer[position] = '\0';
            char *current_cmd = strdup(buffer);
            suggest_command(current_cmd);
            printf("\nripple> %s", buffer);
            fflush(stdout);
            free(current_cmd);
            continue;
        } else if (c == 127 || c == '\b') { // Handle backspace
            if (position > 0) {
                position--;
                printf("\b \b");
                fflush(stdout);
            }
        } else {
            if (position >= bufsize - 1) {
                bufsize += RIPPLE_RL_BUFSIZE;
                buffer = realloc(buffer, bufsize);
                if (!buffer) {
                    fprintf(stderr, "ripple: allocation error\n");
                    exit(EXIT_FAILURE);
                }
            }
            buffer[position] = c;
            position++;
            putchar(c);
            fflush(stdout);
        }
    }
}

// Main entry point
int main(void) {
    // Print welcome message
    printf("\033[1;36m========================================\033[0m\n");
    printf("\033[1;36m     ACMShell with AI Integration      \033[0m\n");
    printf("\033[1;36m========================================\033[0m\n");
    printf("\033[1;33mFor AI-powered command suggestions:\033[0m\n");
    printf("  1. \033[1;33mBegin typing a command\033[0m\n");
    printf("  2. \033[1;33mPress TAB key at any point for AI suggestions\033[0m\n");
    printf("  3. \033[1;33mContinue typing or select a suggestion\033[0m\n\n");
    printf("\033[1;33mType 'help' for a list of built-in commands\033[0m\n");
    printf("\033[1;33mMake sure Ollama is running with the tinyllama model\033[0m\n");
    printf("\033[1;36m========================================\033[0m\n\n");
    
    // Run command loop
    ripple_loop();
    
    return EXIT_SUCCESS;
}
