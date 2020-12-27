#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "icsmap.h"

#define log(fmt, ...) (printf(fmt "\n", ##__VA_ARGS__))

#define printkey(k) (printf("Key: %p, ", (*((char **)k)) ))
#define printval(v) (printf("Val: %d",   (*((int *)v))  ))

#define logentry(m, e, fmt, ...) \
	printkey(map_entry_key(m, e)); printval(map_entry_val(m, e)); log(" " fmt, ##__VA_ARGS__);

#define logkey(k, fmt, ...) \
	printkey(k); log(" " fmt, ##__VA_ARGS__);

#define logval(v, fmt, ...) \
	printval(v); log(" " fmt, ##__VA_ARGS__);

typedef uint8_t *map_entry;  // map entry is just a byte array composed of two halves
typedef uint8_t *map_key;    // map key is the first half of the byte array
typedef uint8_t *map_val;    // map val is the second half of the byte array

// size th map is initially created with
#define INITIAL_SIZE 13

// percentage the map needs to be filled to before triggering a resize
#define LOAD_FACTOR  33

static const map_entry tombstone = (map_entry)0xffffffff;

typedef enum ics_bool {
	false = 0,
	true = 1
} ics_bool;

typedef struct icsmap {
	uint32_t size;      // number of elements in the map
	uint32_t capacity;  // size of the underlying array

	uint32_t keysize;   // size of the key
	uint32_t valsize;   // size of the value

	get_key_fn get_key; // how to retrieve key from given key or null to just use the given one

	map_entry *arr;     // underlying array
} icsmap;

/** Begin general function definition */
const char *
ics_status_str(ics_status status)
{
#define DEFINE_ICS_ERR(_err, _str) _str,
	static const char *status_str[] = {
		ICS_ERROR_CODES
	};
#undef DEFINE_ICS_ERR
	return status_str[status];
}

static inline uint32_t
ics_percent(uint32_t x, uint32_t y)
{
	return x * 100 / y;
}

static void
ics_memset(void *target, char val, uint32_t len)
{
	uint32_t i;
	char *target_ = target;
	for (i = 0; i < len; ++i) {
		target_[i] = val;
	}
}

static void
ics_memcpy(void *dest, const void *src, uint32_t len)
{
	char *d = dest;
	const char *s = src;
	uint32_t i;
	for(i = 0; i < len; ++i) {
		d[i] = s[i];
	}
}

static ics_bool
ics_equal(const void *a, const void *b, uint32_t len)
{
	const char *aa = a, *bb = b;
	uint32_t i;
	for (i = 0; i < len; ++i) {
		if (aa[i] != bb[i]) {
			return false;
		}
	}
	return true;
}

static ics_bool
ics_is_prime(uint32_t n)
{
	if (n <= 1) {
		return false;
	} else if (n == 3) {
		return true;
	}
    // This is checked so that we can skip   
    // middle five numbers in below loop  
	if (n % 2 == 0 || n % 3 == 0) {
		return false;
	}
	uint32_t i;
	for (i = 5; i*i <= n; i += 6) {
		if (n %i == 0 || n % (i + 2) == 0) {
			return false;
		}
	}
    return true;  
}

static void
ics_next_prime(uint32_t start, uint32_t *res)
{
	if (start <= 1) {
		*res = 2;
		return;
	}

    uint32_t prime = start; 
	do {
		++prime;
	} while (!ics_is_prime(prime));

    *res = prime;
}
/** End general function definition */

static void
hash_fn(const map_key key, uint32_t len, uint32_t *res)
{
	uint32_t x = 0, i = 0, hash = 0;
	for (i = 0; i < len; ++i) {
	    hash = (hash << 4) + key[i];
	    if ((x = hash & 0xF0000000L) != 0) {
	       hash ^= (x >> 24);
	    }
	    hash &= ~x;
	}
	*res = hash;
}

static void
hash(const icsmap *map, const map_key key, uint32_t keysize, uint32_t *res)
{
	hash_fn(key, keysize, res);
	*res %= map->capacity;
}

static inline uint64_t
entry_size(const icsmap *map)
{
	return map->keysize + map->valsize;
}

static inline map_key
map_entry_key(const icsmap *map, const map_entry entry)
{
	return (map_key) entry;
}

static inline map_val
map_entry_val(const icsmap *map, const map_entry entry)
{
	return (map_val)(entry + map->keysize);
}

static inline ics_bool
is_deleted(const map_entry entry)
{
	return entry == tombstone;
}

static inline ics_bool
is_empty(const map_entry entry)
{
	return entry == NULL;
}

static const map_key
get_key(const icsmap *map, const void *key, uint32_t *size)
{
	// if the user did not specify a way to get the key, use default
	if (map->get_key == NULL) {
		*size = map->keysize;
		return (const map_key)key;
	}
	// else use the funciton they supplied us.
	return (const map_key)map->get_key(key, size);
}

static inline ics_bool
is_overloaded(const icsmap *map)
{
	return ics_percent(map->size, map->capacity) > LOAD_FACTOR;
}

static ics_status
find_key(const icsmap *map, const map_key key, uint32_t *index)
{
	uint32_t keysize;
	const map_key k = get_key(map, key, &keysize);
	/*const char *ckey = (const char *)k;*/
	/*log("Finding %s", ckey);*/

	uint32_t hash_index;
	hash(map, k, keysize, &hash_index);
	logkey(k, "finding index of key from map starting at index %d", hash_index);
	// we now have the starting point for the key search space
	uint32_t candidate_size, i = hash_index;
	while (!is_empty(map->arr[i])) {
		const map_key candidate = get_key(map, map_entry_key(map, map->arr[i]), &candidate_size);
		assert(candidate_size == keysize);
		if (ics_equal(candidate, k, keysize)) {
			*index = i;
			return ICS_OK;
		}       
		i = (i + 1) % map->capacity;
		if (i == hash_index) {
			// we looped back to hash index, can quit
			break;
		}
	}
	// if here then it means we did not find the key
	return ICS_NOT_FOUND;
}

static ics_status
find_hole(const icsmap *map, const map_key key, uint32_t *index)
{
	// find a starting position
	uint32_t keysize;
	const map_key k = get_key(map, key, &keysize);

	uint32_t hash_index;
	hash(map, k, keysize, &hash_index);
	assert(hash_index < map->capacity);

	// for each value in the hashmap starting at index and looping with mod:
	//     if this value is empty, then we have found a hole. return index/OK
	//     if this value is deleted, then we have found a hole. return index/OK
	//     if this value is the same, then compare for equality with search key.
	//         if same search key, then return index/ICS_EXISTS
	//         if not same search key, then continue to next loop
	uint32_t candidate_size, i = hash_index;

	// while we have not found an empty hole
	while (!is_empty(map->arr[i])) {
		const map_key candidate = get_key(map, map_entry_key(map, map->arr[i]), &candidate_size);
		assert(candidate_size == keysize);
		// if this value has been deleted
		if (is_deleted(map->arr[i])) {
			*index = i;
			return ICS_OK;
		// else if this is the same key we are finding a hole for
		} else if (ics_equal(candidate, k, keysize)) {
			*index = i;
			return ICS_EXISTS;
		}
		// else increment i and loop back to beginning of array at end
		i = (i + 1) % map->capacity;
	}
	// if here it means we have found an empty spot. We are guarenteed not to
	// infinite loop because we resize the array when there is not enough space
	// return index/ok
	*index = i;
	return ICS_OK;
}

ics_status
icsmap_init(icsmap_handle *handle, const icsmap_cfg *cfg)
{
	icsmap *map = malloc(sizeof(icsmap));
	if (map == NULL) {
		return ICS_NO_MEMORY;
	}

	map->size = 0;
	map->capacity = INITIAL_SIZE;
	map->keysize = cfg->keysize;
	map->valsize = cfg->valsize;
	map->get_key  = cfg->get_key;

	uint64_t arr_size = sizeof(map_entry) * INITIAL_SIZE;
	map->arr = malloc(arr_size);
	if (map->arr == NULL) {
		free(map);
		return ICS_NO_MEMORY;
	}

	ics_memset(map->arr, 0, arr_size);
	*handle = map;
	return ICS_OK;
}

void
icsmap_deinit(icsmap_handle handle)
{
	assert(handle != NULL);
	icsmap *map = handle;
	uint32_t i;
	for (i = 0; i < map->capacity; ++i) {
		if (!is_empty(map->arr[i]) && !is_deleted(map->arr[i])) {
			free(map->arr[i]);
		}
	}
	free(map->arr);
	free(map);
}

static ics_status
resize(icsmap *map)
{
	uint32_t old_cap = map->capacity;
	ics_next_prime(map->capacity * 2, &map->capacity);

	map_entry *old_arr = map->arr;
	uint32_t arr_size = sizeof(map_entry) * map->capacity;
	map->arr = malloc(arr_size);
	if (map->arr == NULL) {
		map->arr = old_arr;
		return ICS_NO_MEMORY;
	}
	ics_memset(map->arr, 0, arr_size);
	log("Resize %d -> %d", old_cap, map->capacity);

	ics_status status;
	uint32_t i, hash_index;
	for (i = 0; i < old_cap; ++i) {
		if (!is_empty(old_arr[i]) && !is_deleted(old_arr[i])) {
			status = find_hole(map, map_entry_key(map, old_arr[i]), &hash_index);
			logentry(map, old_arr[i], "Relocating from old_arr[%d] to map->arr[%d] ", i, hash_index);
			if (status != ICS_OK) {
				log("Error Relocating: %s", ics_status_str(status));
				return status;
			}
			// need to find an empty spot if collision
			map->arr[hash_index] = old_arr[i];
		}
	}

	free(old_arr);
	return ICS_OK;
}

static void
init_map_entry(const icsmap *map, map_entry entry, const void *key, const void *val)
{
	ics_memcpy(entry, key, map->keysize);
	ics_memcpy(entry + map->keysize, val, map->valsize);
}

ics_status
icsmap_put(icsmap_handle handle, const void *key, const void *val)
{
	icsmap *map = handle;
	if (is_overloaded(map)) {
		log("icsmap_put: overloaded - resizing");
		ics_status status = resize(map);
		if (status != ICS_OK) {
			return status;
		}
	}

	uint32_t index;
	ics_status status = find_hole(map, (const map_key)key, &index);
	if (status == ICS_EXISTS) {
		// value already exists in array replace the value
		init_map_entry(map, map->arr[index], key, val);
		logentry(map, map->arr[index], "icsmap_put: Key Exists, updating at index %d", index);
		return ICS_OK;
	}
	// else we have found a hole
	map_entry entry = malloc(entry_size(map));
	if (entry == NULL) {
		return ICS_NO_MEMORY;
	}
	init_map_entry(map, entry, key, val);
	logentry(map, entry, "icsmap_put: Does not exist, inserting at index %d", index);
	map->arr[index] = entry;
	map->size += 1;

	return ICS_OK;
}

ics_status
icsmap_get(const icsmap_handle handle, const void *key, void *out)
{
	icsmap *map = handle;
	uint32_t index;
	ics_status status = find_key(map, (const map_key)key, &index);
	if (status != ICS_OK) {
		return status;
	}
	logkey(key, "Key found at index %d", index);

	assert(!is_empty(map->arr[index]) && !is_deleted(map->arr[index]));

	ics_memcpy(out, map_entry_val(map, map->arr[index]), map->valsize);
	return ICS_OK;
}

ics_status
icsmap_remove(icsmap_handle handle, const void *key)
{
	icsmap *map = handle;
	uint32_t index;
	ics_status status = find_key(map, (const map_key)key, &index);
	if (status != ICS_OK) {
		return status;
	}
	assert(!is_empty(map->arr[index]) && !is_deleted(map->arr[index]));
	
	// delete by freeing the entry and setting the index to tombstone value
	free(map->arr[index]);
	map->arr[index] = tombstone;
	map->size--;

	return ICS_OK;
}

ics_status
icsmap_contains(const icsmap_handle handle, const void *key)
{
	icsmap *map = handle;
	uint32_t index;
	ics_status status = find_key(map, (const map_key)key, &index);
	return status == ICS_NOT_FOUND ? ICS_NOT_FOUND : ICS_EXISTS;
}

void
icsmap_foreach(const icsmap_handle handle, foreach_fn fn, void *data)
{
	icsmap *map = handle;
	uint32_t i;
	for (i = 0; i < map->capacity; ++i) {
		map_entry entry = map->arr[i];
		if (!is_empty(entry) && !is_deleted(entry)) {
			fn(map_entry_key(map, entry), map_entry_val(map, entry), data);
		}
	}
}

uint32_t
icsmap_count(const icsmap_handle handle)
{
	return ((icsmap *)handle)->size;
}

void
icsmap_all(const icsmap_handle handle, void *keys, void *vals)
{
	icsmap *map = handle;
	uint32_t i, index = 0;
	for (i = 0; i < map->capacity; ++i) {
		map_entry entry = map->arr[i];
		if (!is_empty(entry) && !is_deleted(entry)) {
			ics_memcpy(&keys[index * map->keysize], map_entry_key(map, entry), map->keysize);
			ics_memcpy(&vals[index * map->valsize], map_entry_val(map, entry), map->valsize);
			index++;
		}
	}
	assert(index == map->size);
}
