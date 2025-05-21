#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#ifdef __APPLE__
#include "/opt/homebrew/include/json-c/json.h"
#else
#include <json-c/json.h>
#endif

struct MemoryStruct {
    char *memory;
    size_t size;
};

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
        
        // Create the JSON data to send
        char json_data[1024];
        snprintf(json_data, sizeof(json_data), 
                 "{\"model\": \"tinyllama\", \"prompt\": \"Give me a command to list files\", \"stream\": false}");
        
        printf("Sending request to Ollama API...\n");
        printf("JSON payload: %s\n", json_data);
        
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/api/generate");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        
        res = curl_easy_perform(curl);
        
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            printf("\nRaw response from Ollama API:\n%s\n", chunk.memory);
            
            // Parse the JSON response
            struct json_object *parsed_json;
            struct json_object *response_obj;
            
            parsed_json = json_tokener_parse(chunk.memory);
            
            if (parsed_json == NULL) {
                fprintf(stderr, "Failed to parse JSON response\n");
            } else {
                // Get the 'response' field
                if (json_object_object_get_ex(parsed_json, "response", &response_obj)) {
                    const char* response_text = json_object_get_string(response_obj);
                    printf("\nExtracted response text:\n%s\n", response_text);
                } else {
                    fprintf(stderr, "No 'response' field in JSON response\n");
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