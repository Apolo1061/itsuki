#include "itsuki.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "itsuki.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void lsp_send(const char* content) {
    if (!content) return;
    printf("Content-Length: %d\r\n\r\n%s", (int)strlen(content), content);
    fflush(stdout);
}

char* json_extract_string(const char* json, const char* key) {
    char search_key[128];
    sprintf(search_key, "\"%s\":\"", key);
    char* found = strstr(json, search_key);
    if (!found) return NULL;
    
    found += strlen(search_key);
    char* end = found;
    while (*end) {
        if (*end == '"' && *(end-1) != '\\') break;
        end++;
    }
    
    int len = end - found;
    char* res = malloc(len + 1);
    
    int j = 0;
    for (int i=0; i<len; i++) {
        if (found[i] == '\\' && i+1 < len) {
            i++;
            if (found[i] == 'n') res[j++] = '\n';
            else if (found[i] == 't') res[j++] = '\t';
            else if (found[i] == '"') res[j++] = '"';
            else if (found[i] == '\\') res[j++] = '\\';
            else res[j++] = found[i];
        } else {
            res[j++] = found[i];
        }
    }
    res[j] = 0;
    return res;
}

void lsp_check_syntax(const char* code) {
    if (!code) return;
    
    ln = 1;
    
    lsp_mode = true;
    TokenStream* local_ts = NULL;
    
    if (setjmp(lsp_jmp) == 0) {
        local_ts = tokenize_all(code);
        ts = local_ts;
        if (!ts) {
            lsp_mode = false;
            return;
        }
        
        adv();
        while (tk.tipo != TOKEN_EOF) {
            NodoAST* n = parse_stmt();
            if (n) free_ast(n);
        }
        
        lsp_send("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":\"file:///test.suki\",\"diagnostics\":[]}}");
    } else {
        int line = 1;
        char msg[512] = "Error desconocido";
        
        sscanf(lsp_error_msg, "Linea %d: %[^\n]", &line, msg);
        if (line < 1) line = 1;
        
        char response[2048];
        snprintf(response, sizeof(response), 
            "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":\"file:///test.suki\",\"diagnostics\":[{\"range\":{\"start\":{\"line\":%d,\"character\":0},\"end\":{\"line\":%d,\"character\":100}},\"severity\":1,\"message\":\"%s\"}]}}", 
            line-1, line-1, msg);
        lsp_send(response);
    }
    
    if (local_ts) {
        free_token_stream(local_ts);
        ts = NULL;
    }
    lsp_mode = false;
}

void lsp_start() {
    char buffer[4096];
    
    while (1) {
        if (!fgets(buffer, sizeof(buffer), stdin)) break;
        if (strstr(buffer, "Content-Length:")) {
            int len = atoi(buffer + 15);
            if(!fgets(buffer, sizeof(buffer), stdin)) break;
            
            char* json = malloc(len + 1);
            int read = fread(json, 1, len, stdin);
            json[read] = 0;
            
            if (strstr(json, "\"method\":\"initialize\"")) {
                lsp_send("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"capabilities\":{\"textDocumentSync\":1,\"completionProvider\":{\"resolveProvider\":true}}}}");
            } 
            else if (strstr(json, "\"method\":\"textDocument/didOpen\"")) {
                char* text = json_extract_string(json, "text");
                lsp_check_syntax(text);
                if(text) free(text);
            }
            else if (strstr(json, "\"method\":\"textDocument/didChange\"")) {
                char* text = json_extract_string(json, "text");
                lsp_check_syntax(text);
                if(text) free(text);
            }
            
            free(json);
        }
    }
}
