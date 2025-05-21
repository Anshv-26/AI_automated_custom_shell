#ifndef OLLAMA_INTEGRATION_H
#define OLLAMA_INTEGRATION_H

// Function declarations
char* get_ollama_completion(const char* prompt);
void suggest_command(const char* partial_cmd);
char* ripple_read_line(void);
char** ripple_split_line(char* line);

// Constants
#define RIPPLE_RL_BUFSIZE 1024
#define RIPPLE_TOK_BUFSIZE 64
#define RIPPLE_TOK_DELIM " \t\r\n\a"
#define RIPPLE_VERSION "1.0.0"
#define OLLAMA_API_URL "http://localhost:11434/api/generate"

#endif // OLLAMA_INTEGRATION_H 