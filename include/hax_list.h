/*
 * Copyright (c) 2011 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the copyright holder nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HAX_LIST_H_
#define HAX_LIST_H_

struct hax_link_list {
    struct hax_link_list *prev;
    struct hax_link_list *next;
};
/* Represents the head node, which is special */
typedef struct hax_link_list hax_list_head;
/* Represents any other node */
typedef struct hax_link_list hax_list_node;

static inline void hax_init_list_head(hax_list_head *head)
{
    if (!head)
        return;
    head->next = head->prev = head;
}

/* Insert to the head of the queue */
static inline void hax_list_add(hax_list_node *entry, hax_list_head *head)
{
    if (!head || !entry)
        return;
    entry->next = head->next;
    head->next->prev = entry;
    entry->prev = head;
    head->next = entry;
}

/* Insert new node before the existing node in the queue. */
static inline void hax_list_insert_before(hax_list_node *new_node,
                                          hax_list_node *ref_node)
{
    if (!new_node || !ref_node)
        return;
    new_node->next = ref_node;
    ref_node->prev->next = new_node;
    new_node->prev = ref_node->prev;
    ref_node->prev = new_node;
}

/* Insert new node after the existing node in the queue. */
static inline void hax_list_insert_after(hax_list_node *new_node,
                                         hax_list_node *ref_node)
{
    hax_list_add(new_node, ref_node);
}

static inline void hax_list_del(hax_list_node *entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    /* Do we need this ? */
    entry->next = entry->prev = entry;
}

static inline int hax_list_empty(hax_list_head *head)
{
    /* NULL pointer mean empty */
    if (!head)
        return 1;

    return ((head->next == head) && (head->prev == head));
}

/* Merge list's entries to the list indexed by the head */
static inline void hax_list_join(hax_list_head *list, hax_list_head *head)
{
    hax_list_node *next, *list_head, *list_tail;

    if (!list || !head)
        return;

    if (!hax_list_empty(list)) {
        next = head->next;
        list_head = list->next;
        list_tail = list->prev;
        /* Join together */
        head->next = list_head;
        list_head->prev = head;
        next->prev = list_tail;
        list_tail->next = next;
    }
}

#define hax_list_entry(field, type, entry) \
        (type *)((char *)(entry) - (size_t)(&((type *)0)->field))

#define hax_list_entry_for_each(entry, head, type, field)              \
        for (entry = hax_list_entry(field, type, ((head)->next));      \
             &(entry)->field != head;                                  \
             entry = hax_list_entry(field, type, (entry)->field.next))

#define hax_list_for_each(entry, head) \
        for (entry = head->next;       \
             entry != head;            \
             entry = entry->next)

#define hax_list_entry_for_each_safe(entry, tmp, head, type, field)  \
        for (entry = hax_list_entry(field, type, ((head)->next)),    \
             tmp = hax_list_entry(field, type, (entry)->field.next); \
             &(entry)->field != head;                                \
             entry = tmp,                                            \
             tmp = hax_list_entry(field, type, (tmp)->field.next))

#define hax_list_for_each_safe(listitem, tmp, head)           \
        for (listitem = (head)->next, tmp = (listitem)->next; \
             (listitem) != head;                              \
             listitem = tmp, tmp = (tmp)->next)

#endif  // HAX_LIST_H_
