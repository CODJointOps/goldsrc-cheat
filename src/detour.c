/**
 * @file      detour.c
 * @brief     Detour hooking library source
 * @author    8dcc
 *
 * https://github.com/8dcc/detour-lib
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>   /* getpagesize */
#include <sys/mman.h> /* mprotect */
#include <stdio.h>    /* printf */

#include "include/detour.h"

#define PAGE_SIZE          getpagesize()
#define PAGE_MASK          (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(x)      ((x + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_ALIGN_DOWN(x) (PAGE_ALIGN(x) - PAGE_SIZE)

/* Store detour data for functions we've hooked */
#define MAX_HOOKS 32
static detour_data_t g_hooks[MAX_HOOKS];
static int g_hook_count = 0;

static bool protect_addr(void* ptr, int new_flags) {
    void* p  = (void*)PAGE_ALIGN_DOWN((detour_ptr_t)ptr);
    int pgsz = getpagesize();

    if (mprotect(p, pgsz, new_flags) == -1)
        return false;

    return true;
}

/*
 * 64 bits:
 *   0:  48 b8 45 55 46 84 45    movabs rax,0x454584465545
 *   7:  45 00 00
 *   a:  ff e0                   jmp    rax
 *
 * 32 bits:
 *   0:  b8 01 00 00 00          mov    eax,0x1
 *   5:  ff e0                   jmp    eax
 */
#ifdef __i386__
static uint8_t def_jmp_bytes[] = { 0xB8, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xE0 };
#define JMP_BYTES_PTR 1 /* Offset inside the array where the ptr should go */
#else
static uint8_t def_jmp_bytes[] = { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0xFF, 0xE0 };
#define JMP_BYTES_PTR 2 /* Offset inside the array where the ptr should go */
#endif

void detour_init(detour_data_t* data, void* orig, void* hook) {
    if (!orig || !hook)
        return;
        
    data->detoured = false;
    data->orig     = orig;
    data->hook     = hook;

    /* Store the first N bytes of the original function, where N is the size of
     * the jmp instructions */
    memcpy(data->saved_bytes, orig, sizeof(data->saved_bytes));

    /* Default jmp bytes */
    memcpy(data->jmp_bytes, &def_jmp_bytes, sizeof(def_jmp_bytes));

    /* JMP_BYTES_PTR is defined bellow def_jmp_bytes, and it changes depending
     * on the arch.
     * We use "&hook" and not "hook" because we want the address of
     * the func, not the first bytes of it like before. */
    memcpy(&data->jmp_bytes[JMP_BYTES_PTR], &hook, sizeof(detour_ptr_t));
}

bool detour_add(detour_data_t* d) {
    /* Already detoured, nothing to do */
    if (d->detoured)
        return true;

    /* See util.c */
    if (!protect_addr(d->orig, PROT_READ | PROT_WRITE | PROT_EXEC))
        return false;

    /* Copy our jmp instruction with our hook address to the orig */
    memcpy(d->orig, d->jmp_bytes, sizeof(d->jmp_bytes));

    /* Restore old protection */
    if (protect_addr(d->orig, PROT_READ | PROT_EXEC)) {
        d->detoured = true;
        return true;
    }

    return false;
}

bool detour_del(detour_data_t* d) {
    /* Not detoured, nothing to do */
    if (!d->detoured)
        return true;

    /* See util.c */
    if (!protect_addr(d->orig, PROT_READ | PROT_WRITE | PROT_EXEC))
        return false;

    /* Restore the bytes that were at the start of orig (we saved on init) */
    memcpy(d->orig, d->saved_bytes, sizeof(d->saved_bytes));

    /* Restore old protection */
    if (protect_addr(d->orig, PROT_READ | PROT_EXEC)) {
        d->detoured = false;
        return true;
    }

    return false;
}

/* Get existing hook data for an original function pointer */
static detour_data_t* find_hook_data(void* orig) {
    for (int i = 0; i < g_hook_count; i++) {
        if (g_hooks[i].orig == orig) {
            return &g_hooks[i];
        }
    }
    return NULL;
}

/* Wrapper functions for backward compatibility with code using 
 * DetourFunction and DetourRemove naming convention */

/* 
 * Wrapper for detour_add that saves and returns the original 
 * function pointer as required by the DetourFunction interface
 */
void* DetourFunction(void* orig, void* hook) {
    if (!orig || !hook) {
        printf("DetourFunction error: NULL pointer provided\n");
        return NULL;
    }

    /* Check if we already have a hook for this function */
    detour_data_t* existing = find_hook_data(orig);
    if (existing) {
        /* If the function was already hooked but with a different hook,
           restore the original first */
        if (existing->hook != hook) {
            printf("DetourFunction: Function at %p already hooked, updating\n", orig);
            detour_del(existing);
            existing->hook = hook;
            /* Recreate the jump instruction with new hook */
            memcpy(existing->jmp_bytes, &def_jmp_bytes, sizeof(def_jmp_bytes));
            memcpy(&existing->jmp_bytes[JMP_BYTES_PTR], &hook, sizeof(detour_ptr_t));
            if (!detour_add(existing)) {
                printf("DetourFunction error: Failed to update hook for %p\n", orig);
                return NULL;
            }
        }
        return existing->orig;
    }

    /* No existing hook, create a new one if we have space */
    if (g_hook_count >= MAX_HOOKS) {
        printf("DetourFunction error: Max hooks (%d) reached\n", MAX_HOOKS);
        return NULL;
    }
        
    detour_data_t* data = &g_hooks[g_hook_count++];
    memset(data, 0, sizeof(detour_data_t)); // Clear memory before use
    detour_init(data, orig, hook);
    if (detour_add(data))
        return data->orig;
        
    /* If detour failed, decrement the counter */
    printf("DetourFunction error: Failed to add hook for %p\n", orig);
    g_hook_count--;
    return NULL;
}

/*
 * Wrapper for detour_del that takes the original and hook
 * function pointers directly
 */
bool DetourRemove(void* orig, void* hook) {
    if (!orig) {
        printf("DetourRemove error: NULL original function pointer\n");
        return false;
    }
        
    /* Find the hook in our array */
    detour_data_t* data = find_hook_data(orig);
    if (!data) {
        printf("DetourRemove error: Hook for function %p not found\n", orig);
        return false;  /* Not found */
    }
        
    /* If hook is specified, make sure it matches */
    if (hook && data->hook != hook) {
        printf("DetourRemove error: Hook function mismatch for %p\n", orig);
        return false;
    }
        
    /* Remove the hook */
    bool result = detour_del(data);
    if (!result) {
        printf("DetourRemove error: Failed to remove hook for %p\n", orig);
    }
    return result;
}
