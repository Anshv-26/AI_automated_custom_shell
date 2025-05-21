#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>

// Structure to hold response data
struct MemoryStruct {
    char *memory;
    size_t size;
};

// Callback function for curl
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

int main() {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    
    if(curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        // Simple test payload
        const char *json_data = "{"
            "\"model\": \"tinyllama\","
            "\"prompt\": \"Is Ollama working correctly?\","
            "\"stream\": false"
        "}";
        
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/api/generate");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        
        printf("Testing Ollama connection...\n");
        res = curl_easy_perform(curl);
        
        if(res != CURLE_OK) {
            printf("\033[31mConnection failed: %s\033[0m\n", curl_easy_strerror(res));
            printf("Make sure Ollama is installed and running:\n");
            printf("1. Install Ollama: curl -fsSL https://ollama.com/install.sh | sh\n");
            printf("2. Start Ollama: ollama serve\n");
            printf("3. Pull TinyLlama model: ollama pull tinyllama\n");
        } else {
            // Parse response to check if it's valid
            struct json_object *parsed_json = json_tokener_parse(chunk.memory);
            
            if (parsed_json == NULL) {
                printf("\033[31mFailed to parse response. Ollama may be running but returned invalid data.\033[0m\n");
            } else {
                // Check for response field
                struct json_object *response_obj;
                if (json_object_object_get_ex(parsed_json, "response", &response_obj)) {
                    printf("\033[32mSuccess! Ollama is properly configured and responding.\033[0m\n");
                    printf("\nResponse preview: %s\n", json_object_get_string(response_obj));
                } else {
                    printf("\033[31mOllama returned a response but it doesn't contain the expected 'response' field.\033[0m\n");
                    printf("Raw response: %s\n", chunk.memory);
                }
                
                json_object_put(parsed_json);
            }
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        free(chunk.memory);
    }
    
    curl_global_cleanup();
    return 0;
}

// Compile with:
// gcc -o test_ollama test_ollama.c -lcurl -ljson-c 