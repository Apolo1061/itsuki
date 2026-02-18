#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif
#include "itsuki.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Obj* lista_objetos = NULL;
int contador_objetos = 0;
int umbral_gc = 1024;

void* gc_alloc(size_t size, ObjType type) {
    if (contador_objetos >= umbral_gc && !vm.manual_memory_mode) {
        gc_recolectar();
    }

    struct Obj* obj = (struct Obj*)malloc(size);
    if (!obj) {
        fprintf(stderr, "Out of memory (gc_alloc %lu bytes)\n", (unsigned long)size);
        exit(1);
    }
    obj->type = type;
    obj->is_marked = false;
    obj->is_freed = false;
    obj->next = lista_objetos;
    lista_objetos = obj;
    contador_objetos++;
    return obj;
}

static void marcar_objeto(struct Obj* obj);

static bool valor_tiene_obj(Result r) {
    switch (r.tipo) {
        case TIPO_CADENA:
        case TIPO_ARRAY:
        case TIPO_MAP:
        case TIPO_INSTANCIA:
        case TIPO_CLAUSURA:
        case TIPO_ENUM:
        case TIPO_MODULO:
        case TIPO_SOCKET:
            return true;
        default:
            return false;
    }
}

static bool obj_en_lista(struct Obj* obj) {
    for (struct Obj* cur = lista_objetos; cur; cur = cur->next) {
        if (cur == obj) return true;
    }
    return false;
}

static void marcar_valor(Result r) {
    if (valor_tiene_obj(r) && r.obj && obj_en_lista(r.obj)) {
        marcar_objeto(r.obj);
    }
}

static void marcar_objeto(struct Obj* obj) {
    if (obj == NULL || obj->is_marked) return;
    
    obj->is_marked = true;

    switch (obj->type) {
        case OBJ_STRING: break;
        case OBJ_ARRAY: {
            Array* a = (Array*)obj;
            for (int i = 0; i < a->tamano; i++) {
                marcar_valor(a->elementos[i]);
            }
            break;
        }
        case OBJ_MAP: {
            Map* m = (Map*)obj;
            for (int i = 0; i < 64; i++) {
                MapEntry* e = m->buckets[i];
                while (e) {
                    marcar_valor(*(e->valor));
                    e = e->next;
                }
            }
            break;
        }
        case OBJ_INSTANCIA: {
            Instancia* inst = (Instancia*)obj;
            if (inst->valores_propiedades && inst->clase) {
                for (int i = 0; i < inst->clase->n_propiedades; i++) {
                    marcar_valor(inst->valores_propiedades[i]);
                }
            }
            break;
        }
        case OBJ_CLAUSURA: {
            Closure* cl = (Closure*)obj;
            for (int i = 0; i < cl->n_upvalues; i++) {
                if (cl->upvalues[i]) marcar_objeto((Obj*)cl->upvalues[i]);
            }
            break;
        }
        case OBJ_UPVALUE: {
            Upvalue* uv = (Upvalue*)obj;
            marcar_valor(uv->closed);
            break;
        }
        case OBJ_ENUM_DEF: break;
        case OBJ_ENUM_VAL: {
            EnumValor* ev = (EnumValor*)obj;
            if (ev->valores) {
                for (int i = 0; i < ev->definicion->variantes[ev->variante_index].n_campos; i++) {
                    marcar_valor(ev->valores[i]);
                }
            }
            break;
        }
        case OBJ_MODULO: {
            ObjModulo* mod = (ObjModulo*)obj;
            if (mod->exports) marcar_objeto((Obj*)mod->exports);
            break;
        }
        case OBJ_SOCKET: break;
    }
}

void gc_marcar() {
    for (Result* p = vm.pila; p < vm.pila_tope; p++) {
        marcar_valor(*p);
    }

    for (int i = 0; i < n_v; i++) {
        Result r = {0};
        r.tipo = vars[i].tipo;
        r.obj = vars[i].obj;
        if (valor_tiene_obj(r) && r.obj && obj_en_lista(r.obj)) {
            marcar_objeto(r.obj);
        }
    }

    if (vm.chunk && vm.chunk->constantes) {
        for (int i = 0; i < vm.chunk->contador_constantes; i++) {
            marcar_valor(vm.chunk->constantes[i]);
        }
    }

    if (vm.modulos_cargados) marcar_objeto((Obj*)vm.modulos_cargados);
    if (vm.clausura_actual) marcar_objeto((Obj*)vm.clausura_actual);
    marcar_valor(vm.iterator_state.iterable);
    if (vm.hay_error_pendiente) marcar_valor(vm.error_pendiente);
    if (vm.cli_args) marcar_objeto((Obj*)vm.cli_args);
    marcar_valor(last_res);

    for (int i = 0; i < n_clases; i++) {
        if (!clases[i].valores_estaticos) continue;
        for (int j = 0; j < clases[i].n_estaticos; j++) {
            marcar_valor(clases[i].valores_estaticos[j]);
        }
    }
}

void liberar_objeto(struct Obj* obj) {
    if (!obj || obj->is_freed) return;
    obj->is_freed = true;

    switch (obj->type) {
        case OBJ_STRING:
            free(((ObjString*)obj)->s);
            break;
        case OBJ_ARRAY:
            free(((Array*)obj)->elementos);
            break;
        case OBJ_MAP: {
            Map* m = (Map*)obj;
            for (int i = 0; i < 64; i++) {
                MapEntry* e = m->buckets[i];
                while (e) {
                    MapEntry* next = e->next;
                    free(e->clave);
                    free(e);
                    e = next;
                }
            }
            break;
        }
        case OBJ_INSTANCIA: {
            Instancia* inst = (Instancia*)obj;
            if (inst->valores_propiedades) free(inst->valores_propiedades);
            break;
        }
        case OBJ_CLAUSURA: {
            if (((Closure*)obj)->upvalues) free(((Closure*)obj)->upvalues);
            break;
        }
        case OBJ_UPVALUE: break;
        case OBJ_ENUM_VAL: {
            EnumValor* ev = (EnumValor*)obj;
            if (ev->valores) free(ev->valores);
            break;
        }
        case OBJ_MODULO:
        case OBJ_ENUM_DEF:
            break;
        case OBJ_SOCKET: {
            ObjSocket* sock = (ObjSocket*)obj;
            if (sock->is_open) {
                #ifdef _WIN32
                closesocket(sock->handle);
                #else
                close(sock->handle);
                #endif
                sock->is_open = false;
            }
            break;
        }
    }
    
    
    
    struct Obj** curr = &lista_objetos;
    while (*curr != NULL) {
        if (*curr == obj) {
            *curr = obj->next;
            break;
        }
        curr = &(*curr)->next;
    }

    free(obj);
    contador_objetos--;
}

void gc_limpiar() {
    struct Obj** obj = &lista_objetos;
    while (*obj != NULL) {
        if ((*obj)->is_freed) {
            *obj = (*obj)->next;
            continue;
        }
        if (!(*obj)->is_marked) {
            struct Obj* no_usado = *obj;
            *obj = no_usado->next;
            
            switch (no_usado->type) {
                case OBJ_STRING: free(((ObjString*)no_usado)->s); break;
                case OBJ_ARRAY: free(((Array*)no_usado)->elementos); break;
                case OBJ_MAP: {
                     Map* m = (Map*)no_usado;
                     for (int i=0; i<64; i++) {
                         MapEntry* e = m->buckets[i];
                         while(e) { MapEntry* n=e->next; free(e->clave); free(e); e=n; }
                     }
                     break;
                }
                case OBJ_INSTANCIA: free(((Instancia*)no_usado)->valores_propiedades); break;
                case OBJ_CLAUSURA: free(((Closure*)no_usado)->upvalues); break;
                case OBJ_UPVALUE: break;
                case OBJ_ENUM_DEF: {
                    break;
                }
                case OBJ_ENUM_VAL: if(((EnumValor*)no_usado)->valores) free(((EnumValor*)no_usado)->valores); break;
                case OBJ_MODULO: break;
                case OBJ_SOCKET: {
                    ObjSocket* s = (ObjSocket*)no_usado;
                    if (s->is_open) {
                        #ifdef _WIN32
                        closesocket(s->handle);
                        #else
                        close(s->handle);
                        #endif
                    }
                    break;
                }
            }

            free(no_usado);
            contador_objetos--;
        } else {
            (*obj)->is_marked = false;
            obj = &(*obj)->next;
        }
    }
}

void gc_recolectar() {
    int antes = contador_objetos;
    gc_marcar();
    gc_limpiar();
    
    umbral_gc = contador_objetos * 2;
    if (umbral_gc < 1024) umbral_gc = 1024;
    
    printf("[GC] Recolectados %d objetos. Quedan %d. Nuevo umbral: %d\n", 
           antes - contador_objetos, contador_objetos, umbral_gc);
}

Result gc_new_string(const char* s) {
    ObjString* os = (ObjString*)gc_alloc(sizeof(ObjString), OBJ_STRING);
    os->s = strdup(s);
    Result r = {0};
    r.tipo = TIPO_CADENA;
    r.s = os->s;
    r.obj = (Obj*)os;
    return r;
}

Result gc_new_array() {
    Array* a = (Array*)gc_alloc(sizeof(Array), OBJ_ARRAY);
    a->elementos = malloc(sizeof(Result) * 8);
    a->capacidad = 8;
    a->tamano = 0;
    Result r = {0};
    r.tipo = TIPO_ARRAY;
    r.a = a;
    r.obj = (Obj*)a;
    return r;
}
