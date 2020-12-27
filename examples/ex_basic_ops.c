#include <stdio.h>
#include <stdlib.h>
#include "icsmap.h"

#define log(fmt, ...) (printf(fmt "\n", ##__VA_ARGS__))

int main() {
	// Below we init a map. A hashmap can have many different config options.
	// make sure that you set options you don't need to null.
	icsmap_handle map;
	icsmap_cfg cfg = {
		// each key we will insert into the map is a char.
		.keysize = sizeof(char),
		// each value we insert is an int
		.valsize = sizeof(int),
		// we will not need a custom get_key fn (discussed in later examples)
		.get_key = NULL
	};

	// we want a hashmap of char -> int so we set the config options and init the map
	// the map takes in a handle, which is a publicly exposed pointer to the actual
	// struct. This is one way to deal with module privacy in C. We define only the
	// client facing api in the .h file and then everything else, including the struct
	// internals in the .c file.
	ics_status status = icsmap_init(&map, &cfg);
	if (status != ICS_OK) {
		log("Could not init map: %s", ics_status_str(status));
	}

	// One of the cool things about handles, is that it prevents users from
	// accidentally altering the struct interior. Try uncommenting the line below
	// to see this in action. It won't even compile!
	// map->size += 1;
	
	// you will also notice the use of ics_status. Returning specific status designators
	// instead of simply -1 or 0 is a way to clearly communicate to the user what
	// went wrong. Golang has taken a similar approach in error handling.
	
	// Let's insert some values into the map. Lets use it for counting the occurrences
	// of letters in a string
	const char *countme = "There are many letters in this string. We will count them" \
						  " using a map, even though it would be much simpler to use" \
						  " an array!";

	int val, pos = 0;
	char key;
	while (countme[pos] != '\0') {
		key = countme[pos];
		status = icsmap_get(map, &key, &val);
		switch (status) {
		case ICS_NOT_FOUND:
			// this is the first time we encountered this character
			val = 1;
			break;
		case ICS_OK:
			// it exists in the map, let's update it
			val++;
			break;
		default:
			log("Unexpected status: %s", ics_status_str(status));
			return 2;
		}
		status = icsmap_put(map, &key, &val);
		if (status != ICS_OK) {
			log("Could not insert into map: %s", ics_status_str(status));
			return 2;
		}
		pos++;
	}

	// Lets discuss how icsmap works internally. When you put a key/value pair into
	// the map you are actually copying them by value into a byte array. icsmap will
	// allocate some memory with enough space for the configured key/val sizes. It then
	// treats this like a byte array which it copies the key/val you passed in to.
	// So when you place a value in, think of it like a pass by value. Similarly
	// with retrieving a value in the map. In icsmap_get(map, &key, &val), icsmap
	// will copy it's interal representation of the value into the memory location
	// &val is pointed at. 
	//
	// This is also why we cannot simply increment the value returned from icsmap_get.
	// since any changes to the val would have no effect on the internal representation.
	//
	// There is a much simpler way though. We could store a pointer to an int instead
	// of an actual int. That way whatever we get back from the map would be a pointer
	// and we could dereference the pointer to get to the actual value. Changes in this
	// way would be reflected in the map, since the map is only storing a pointer to that
	// location. Lets try it out.
	
	icsmap_handle refmap;
	icsmap_cfg refcfg = {
		.keysize = sizeof(char),
		.valsize = sizeof(int *),
		.get_key = NULL
	};
	status = icsmap_init(&refmap, &refcfg);
	if (status != ICS_OK) {
		log("Could not init map: %s", ics_status_str(status));
	}

	pos = 0;
	int *refval;
	while (countme[pos] != '\0') {
		key = countme[pos];
		status = icsmap_get(refmap, &key, &refval);
		switch (status) {
		case ICS_NOT_FOUND:
			refval = malloc(sizeof(int));
			*refval = 1;
			status = icsmap_put(refmap, &key, &refval);
			if (status != ICS_OK) {
				log("Could not insert into map: %s", ics_status_str(status));
				return 2;
			}
			break;
		case ICS_OK:
			(*refval)++;
			break;
		default:
			log("Unexpected status: %s", ics_status_str(status));
			return 2;
		}
		pos++;
	}

	// OK maybe it didn't simplify the logic here, but say we were storing a
	// struct as a value. We wouldn't want to create whole copies of the value
	// every time we want to update it. Storing a pointer to a struct would be
	// much more efficient.


	// when we are done with the maps, we need to free the memory that the map
	// allocated. We can do this by calling deinit on the maps
	
	icsmap_deinit(map);
	icsmap_deinit(refmap);

	//  I'm sure you've probably seen the problem here by now. When using refmap,
	//  we allocated ints for each value in the map. However we never freed them!
	//  icsmap cannot free them for us, it has no knowledge about what it is storing.
	//  We need to free the memory we allocated. We can do this by iterating through
	//  all the map key/value pairs and is discussed in examples/ex_iterating.c.

	return 0;
}
