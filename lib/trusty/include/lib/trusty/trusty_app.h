/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All rights reserved
 * Copyright (c) 2013, Google, Inc. All rights reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIB_TRUSTY_APP_H
#define __LIB_TRUSTY_APP_H

#include <assert.h>
#include <kernel/physmem.h>
#include <kernel/thread.h>
#include <kernel/usercopy.h>
#include <kernel/vm.h>
#include <lib/trusty/uuid.h>
#include <list.h>
#include <stdbool.h>
#include <sys/types.h>

enum app_state {
    APP_NOT_RUNNING = 0,
    APP_STARTING,
    APP_RUNNING,
    APP_TERMINATING,
};

struct manifest_port_entry {
    uint32_t flags;
    uint32_t path_len;
    /* Points to a string within an application's manifest blob */
    const char* path;
    struct list_node node;
};

struct manifest_mmio_entry {
    struct list_node node;
    struct phys_mem_obj phys_mem_obj;
    struct obj_ref phys_mem_obj_self_ref;
    uint32_t id;
};

struct trusty_app_props {
    uuid_t uuid;
    uint32_t mgmt_flags;
    uint32_t min_stack_size;
    uint32_t min_heap_size;
    uint32_t map_io_mem_cnt;
    uint32_t config_entry_cnt;
    uint32_t* config_blob;
    struct list_node port_entry_list;
    struct list_node mmio_entry_list;
};

struct trusty_app_img {
    uintptr_t manifest_start;
    uintptr_t manifest_end;
    uintptr_t img_start;
    uintptr_t img_end;
};

struct trusty_app;

struct trusty_thread {
    vaddr_t stack_start;
    size_t stack_size;
    vaddr_t entry;
    thread_t* thread;
    struct trusty_app* app;
};

struct trusty_app {
    /* corresponds to the order in which the apps were started */
    u_int app_id;
    enum app_state state;
    lk_time_ns_t min_start_time;
    vmm_aspace_t* aspace;
    vaddr_t start_brk;
    vaddr_t cur_brk;
    vaddr_t end_brk;
    vaddr_t load_bias;
    struct trusty_app_props props;
    struct trusty_app_img* app_img;
    struct trusty_thread* thread;
    /* app local storage */
    void** als;
    struct list_node node;
};

void trusty_app_init(void);

/**
 * trusty_app_is_startup_port() - Query if the specified port is a startup port
 * @port_path: path of the port to check.
 *
 * Return: true if @port_path has been registered as a startup port by an
 * application, false otherwise.
 */
bool trusty_app_is_startup_port(const char* port_path);

/**
 * trusty_app_request_start_by_port() - Request that the application (if any)
 * that has registered the given port be started.
 * @port_path: path of the registered port.
 * @uuid: uuid of the application triggering the request.
 *
 * If the application is already running then this function has no effect.
 * Otherwise the application will be started.
 *
 * Return: ERR_NOT_FOUND if no application has registered @port_path or if the
 * port is not accessible to @uuid, ERR_ALREADY_STARTED
 * if the application is already running or NO_ERROR otherwise.
 */
status_t trusty_app_request_start_by_port(const char* port_path,
                                          const uuid_t* uuid);

void trusty_app_exit(int status) __NO_RETURN;
void trusty_app_crash(void) __NO_RETURN;
status_t trusty_app_setup_mmio(struct trusty_app* trusty_app,
                               uint32_t mmio_id,
                               user_addr_t* uaddr_p,
                               uint32_t size);
void trusty_app_forall(void (*fn)(struct trusty_app* ta, void* data),
                       void* data);
void trusty_thread_exit(int status);

struct trusty_app_notifier {
    struct list_node node;
    status_t (*startup)(struct trusty_app* app);
    status_t (*shutdown)(struct trusty_app* app);
};

/*
 * All app notifiers registration has to be complete before
 * libtrusty is initialized which is happening at LK_INIT_LEVEL_APPS-1
 * init level.
 */
status_t trusty_register_app_notifier(struct trusty_app_notifier* n);

/*
 * All als slots must be allocated before libtrusty is initialized
 * which is happening at LK_INIT_LEVEL_APPS-1 init level.
 */
int trusty_als_alloc_slot(void);

extern uint als_slot_cnt;

static inline void* trusty_als_get(struct trusty_app* app, int slot_id) {
    uint slot = slot_id - 1;
    ASSERT(slot < als_slot_cnt);
    return app->als[slot];
}

static inline void trusty_als_set(struct trusty_app* app,
                                  int slot_id,
                                  void* ptr) {
    uint slot = slot_id - 1;
    ASSERT(slot < als_slot_cnt);
    app->als[slot] = ptr;
}

static inline struct trusty_thread* trusty_thread_get(thread_t* t) {
    return (struct trusty_thread*)thread_tls_get(t, TLS_ENTRY_TRUSTY);
}

static inline struct trusty_thread* current_trusty_thread(void) {
    return (struct trusty_thread*)tls_get(TLS_ENTRY_TRUSTY);
}

static inline struct trusty_app* current_trusty_app(void) {
    struct trusty_thread* trusty_thread = current_trusty_thread();
    if (!trusty_thread) {
        return NULL;
    }
    return trusty_thread->app;
}

#endif
