CC = gcc
CFLAGS = -Wall -g -I/opt/homebrew/include -I/opt/homebrew/include/json-c -I.
LIBS = -L/opt/homebrew/lib -lcurl -ljson-c -lm

all: shell2_complete_ai test_ollama test_ollama_direct

shell2_complete_ai: shell2_complete.c ollama_integration.c ollama_integration.h
	$(CC) $(CFLAGS) -o shell2_complete_ai shell2_complete.c ollama_integration.c $(LIBS)

test_ollama: test_ollama.c ollama_integration.h
	$(CC) $(CFLAGS) -o test_ollama test_ollama.c $(LIBS)

test_ollama_direct: test_ollama_direct.c ollama_integration.h
	$(CC) $(CFLAGS) -o test_ollama_direct test_ollama_direct.c $(LIBS)

clean:
	rm -f shell2_complete_ai test_ollama test_ollama_direct

.PHONY: all clean 