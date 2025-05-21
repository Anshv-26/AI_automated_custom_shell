#include "ollama_integration.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <math.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <json-c/json.h>

// Constants
#define RIPPLE_RL_BUFSIZE 1024
#define RIPPLE_TOK_BUFSIZE 64
#define RIPPLE_TOK_DELIM " \t\r\n\a"
#define RIPPLE_VERSION "1.0.0"
#define OLLAMA_API_URL "http://localhost:11434/api/generate"

// Struct for curl response
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Function to escape JSON string
static char* escape_json_string(const char* input) {
    if (!input) return NULL;
    
    size_t len = strlen(input);
    char* escaped = malloc(len * 2 + 1); // Worst case: every char needs escaping
    if (!escaped) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (input[i]) {
            case '\n':
                escaped[j++] = '\\';
                escaped[j++] = 'n';
                break;
            case '\r':
                escaped[j++] = '\\';
                escaped[j++] = 'r';
                break;
            case '\t':
                escaped[j++] = '\\';
                escaped[j++] = 't';
                break;
            case '\\':
                escaped[j++] = '\\';
                escaped[j++] = '\\';
                break;
            case '"':
                escaped[j++] = '\\';
                escaped[j++] = '"';
                break;
            default:
                escaped[j++] = input[i];
        }
    }
    escaped[j] = '\0';
    return escaped;
}

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

// Function to get AI-based command completion using Ollama API
char* get_ollama_completion(const char* prompt) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    if (!chunk.memory) {
        return NULL;
    }
    chunk.memory[0] = '\0';
    
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    
    if(curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        // Using tinyllama for quick responses
        char json_data[4096]; // Increased buffer size for larger prompts
        const char* prompt_template;
        
        // Special handling for cd command
        if (strncmp(prompt, "cd", 2) == 0) {
            prompt_template = "You are a Unix/Linux shell expert. The user typed '%s'. Suggest exactly 3 most useful directory paths they might want to navigate to. Format each suggestion EXACTLY like this:\n\n"
                            "1. /usr/bin - System executables and commands directory\n"
                            "2. /etc - System configuration files directory\n"
                            "3. /var/log - System and application logs directory\n\n"
                            "Keep descriptions to a single line, starting with the path followed by a brief description.";
        } else {
            prompt_template = "You are a Unix/Linux shell expert. The user typed '%s'. Suggest exactly 3 most useful command completions. Format each suggestion EXACTLY like this:\n\n"
                            "1. ls -la - List all files with detailed permissions and ownership info\n"
                            "2. grep -r 'pattern' . - Search for text recursively in all files\n"
                            "3. find . -type f -name '*.txt' - Find all .txt files in current directory and subdirectories\n\n"
                            "Keep descriptions to a single line, starting with the command followed by a brief description.";
        }
        
        // Create the full prompt
        char full_prompt[2048];
        snprintf(full_prompt, sizeof(full_prompt), prompt_template, prompt);
        
        // Escape the prompt for JSON
        char* escaped_prompt = escape_json_string(full_prompt);
        if (!escaped_prompt) {
            fprintf(stderr, "Failed to escape prompt\n");
            free(chunk.memory);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return NULL;
        }
        
        // Create the JSON request
        snprintf(json_data, sizeof(json_data), 
                "{\"model\": \"tinyllama\", \"prompt\": \"%s\", \"stream\": false, \"temperature\": 0.2, \"top_p\": 0.9, \"top_k\": 40, \"num_predict\": 300}", 
                escaped_prompt);
        
        free(escaped_prompt);
        
        curl_easy_setopt(curl, CURLOPT_URL, OLLAMA_API_URL);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        
        res = curl_easy_perform(curl);
        
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(chunk.memory);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return NULL;
        }
        
        // Parse the response
        struct json_object *parsed_json = json_tokener_parse(chunk.memory);
        if (!parsed_json) {
            fprintf(stderr, "Failed to parse JSON response\n");
            free(chunk.memory);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return NULL;
        }
        
        struct json_object *response_obj;
        if (json_object_object_get_ex(parsed_json, "response", &response_obj)) {
            const char* response_text = json_object_get_string(response_obj);
            char* result = strdup(response_text);
            
            json_object_put(parsed_json);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            free(chunk.memory);
            curl_global_cleanup();
            return result;
        }
        
        json_object_put(parsed_json);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(chunk.memory);
        curl_global_cleanup();
        return NULL;
    }
    
    free(chunk.memory);
    curl_global_cleanup();
    return NULL;
}

// Function to suggest next command based on prompt
void suggest_command(const char* partial_cmd) {
    printf("\nOllama Suggestions for '%s':\n", partial_cmd);
    
    char* ai_suggestion = get_ollama_completion(partial_cmd);
    
    if (ai_suggestion) {
        printf("%s\n", ai_suggestion);
        free(ai_suggestion);
    } else {
        printf("Unable to get AI suggestions. Is Ollama running?\n");
        printf("Try running: ollama serve\n");
        printf("Make sure you have a model: ollama pull tinyllama\n");
    }
    
    printf("\n");
}

// Split a line into tokens
char **ripple_split_line(char *line) {
    int bufsize = RIPPLE_TOK_BUFSIZE;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char *));
    char *token;

    if (!tokens) {
        fprintf(stderr, "ripple: allocation error\n");
        return NULL;
    }

    token = strtok(line, RIPPLE_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += RIPPLE_TOK_BUFSIZE;
            char **temp = realloc(tokens, bufsize * sizeof(char *));
            if (!temp) {
                fprintf(stderr, "ripple: allocation error\n");
                free(tokens);
                return NULL;
            }
            tokens = temp;
        }

        token = strtok(NULL, RIPPLE_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
} 