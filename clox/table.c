#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

static void adjustCapacity(Table* table, int capacity);
static Entry* findEntry(Entry* entries, int capacity, ObjString* key);

void initTable(Table* table) {
    table->capacity = 0;
    table->count = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) {
        return false;
    }

    *value = entry->value;
    return true;
}

bool tableSet(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = (entry->key == NULL);
    if (isNewKey && IS_NIL(entry->value)) {
        // increment the count only if the new entry goes into an entirely empty bucket (i.e. not a tombstone)
        table->count++;
    }

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, ObjString* key)  {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) {
        return false;
    }

    // place a tombstone in the entry
    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

static void adjustCapacity(Table* table, int capacity) {
    Entry* newEntries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        newEntries[i].key = NULL;
        newEntries[i].value = NIL_VAL;
    }

    // rebuild the table when resizing
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* oldEntry = &table->entries[i];
        if (oldEntry->key == NULL) {
            continue;
        }

        Entry* newEntry = findEntry(newEntries, capacity, oldEntry->key);
        newEntry->key = oldEntry->key;
        newEntry->value = oldEntry->value;
        table->count++;
    }

    // free old entries
    FREE_ARRAY(Entry, table->entries, table->capacity);

    table->entries = newEntries;
    table->capacity = capacity;
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;

    for (;;) {
        // linear probing
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // when we find an empty bucket, it means the key isn't present.
                // we check if we have encountered a tombstone.
                // if there has been a tomestone, we use the tombstone instead of the empty entry.
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) {
                    tombstone = entry;
                }
            }
        } else if (entry->key == key) {
            return entry;
        }

        index = (index + 1) % capacity;
    }
}
