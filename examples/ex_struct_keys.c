#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "icsmap.h"

#define log(fmt, ...) (printf(fmt "\n", ##__VA_ARGS__))

typedef struct name {
	int len;
	char *name;
} name;

const void *
get_key(const void *icsmap_key, uint32_t *size)
{
	const name *n = (const name *)icsmap_key;
	*size = n->len;
	return (const void *)n->name;
}

int main() {
	srand(5);

	// Remember, icsmap is essentially just a glorified bit array manager.
	// It sees keys as byte arrays of a given size. For primitive types this is
	// fine. Or if we have structs which are all primitive types then this is
	// also fine. But what if we want the key to be a pointer to something?
	// Let's see what happens

	icsmap_handle map;
	// say we want a map of string -> int
	icsmap_cfg cfg = {
		.keysize = sizeof(char *),
		.valsize = sizeof(int),
		.get_key = NULL
	};
	ics_status status = icsmap_init(&map, &cfg);
	if (status != ICS_OK) {
		log("Could not init map: %s", ics_status_str(status));
	}
	// let's go ahead an insert a value
	int val = 42;
	status = icsmap_put(map, "ics53", &val);
	if (status != ICS_OK) {
		log("Could not insert into map");
		return 2;
	}

	char *key = malloc(sizeof(char) * 6);
	memcpy(key, "ics53", 6);

	status = icsmap_get(map, key, &val);
	if (status == ICS_NOT_FOUND) {
		log("Aha! we couldn't retrieve the value");
	} else if (status != ICS_OK) {
		log("Could not retrieve value");
		return 2;
	}
	free(key);
	log("");

	// What's happening here is that icsmap stores a pointer instead of an actual
	// value. So in order to access a value where the key is a pointer, you need
	// to know that exact pointer.
	// We can fix this by telling icsmap how to locate the key from the given key.
	// We do so my adding a get_key fn to icsmap.
	
	cfg.keysize = sizeof(name);
	cfg.valsize = sizeof(int);
	cfg.get_key = get_key;
	icsmap_deinit(map);
	icsmap_init(&map, &cfg);

	// The get_key_fn API requies 2 things.
	// 1. return a const void * to the address of the key.
	// 2. insert the length of the key (in bytes) into the size variable
	//
	// In order to make this easier, we created a struct name, which encapsulates
	// both the pointer and the length.
	char *name1 = malloc(sizeof(char) * 6);
	char *name2 = malloc(sizeof(char) * 6);
	memcpy(name1, "Brian", 6);
	memcpy(name2, "Brian", 6);
	assert(name1 != name2);

	// have two names, both pointing to different pointers, but containing the same
	// key
	name n1 = {
		.name = name1,
		.len = 6
	};
	name n2 = {
		.name = name2,
		.len = 6
	};

	// let's insert the key/value into the map.
	val = 42;
	status = icsmap_put(map, &n1, &val);
	if (status != ICS_OK) {
		log("Could not insert into map");
		return 2;
	}
	val = 0;

	// and attempt to retrieve the value, this time using the key funtion
	// that we supplied
	status = icsmap_get(map, &n2, &val);
	if (status != ICS_OK) {
		log("Could not retrieve value: %s", ics_status_str(status));
		return 2;
	}
	log("Retrieved the value: %d", val);

	free(name1);
	free(name2);
	icsmap_deinit(map);
	return 0;
}

