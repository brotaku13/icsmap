#include <stdio.h>
#include <stdlib.h>
#include "icsmap.h"

#define log(fmt, ...) (printf(fmt "\n", ##__VA_ARGS__))

typedef struct person {
	uint32_t age;
	uint32_t sibling_count;
} person;

void
print_person(const void *key, const void *val, void *data)
{
	// key and val are both pointers to the part of memory in icsmap where the
	// values reside. In order to obtain their values, you need to cast them
	// to their appropriate type and then dereference them.

	const int *id = (const int *)key;
	const person *p = *((const person **)val);

	if (data != NULL) {
		int *scoped_var = (int *)data;
		printf("I'm using a scoped variable! Which is %d. I am ", *scoped_var);
	}
	printf("person %p: ID: %d -> age: %d, siblings: %d\n", p, *id, p->age, p->sibling_count);
}

int main() {
	srand(5);
	// we will make a map from int -> person.
	icsmap_handle map;
	icsmap_cfg cfg = {
		.keysize = sizeof(int),
		.valsize = sizeof(person *),
		.get_key = NULL
	};
	ics_status status = icsmap_init(&map, &cfg);
	if (status != ICS_OK) {
		log("Could not init map: %s", ics_status_str(status));
	}
	
	// let's init some people into the map
	int i;
	for (i = 0; i < 5; ++i) {
		person *p = malloc(sizeof(person));
		p->age = rand() % 50;
		p->sibling_count = rand() % 3;
		print_person(&i, &p, NULL);
		status = icsmap_put(map, &i, &p);
		if (status != ICS_OK) {
			log("Could not insert value");
			return 2;
		}
	}

	// Now suppose we want to print out each person in the map There are a few
	// ways to iterate the map; by using a callback function which is called on
	// each map entry, or by copying the maps entries into memory we have allocated.
	//
	//
	// To print out each person in the map, we will use the first method. First,
	// we create a callback function of the of the type foreach_fn which is defined
	// in icsmap.h. Then we call icsmap_foreach using a pointer to that function.

	icsmap_foreach(map, print_person, NULL);

	// You'll notice that the api allows us to pass in some data that we may need
	// in the function call. For example, if we have a value in the current scope
	// which we would like in the scope of the function, we can pass a pointer to
	// it here
	int scoped_var = 10;
	icsmap_foreach(map, print_person, &scoped_var);

	// Sometimes it's not enough to just have a single variable in. You may want
	// to do some other work with the keys and values that can't really be
	// encapsulated by a function. In this case you can use icsmap_all to extract
	// a copy of all keys/values from the map.
	
	int count = icsmap_count(map);

	// Remember that we want an array of keys. if the keys are of type int, then
	// we need an int array, represented with int *keys.
	// The values are of type person pointer. So we need an array of person pointers
	// which is represented with person **vals.
	int *keys = malloc(sizeof(int) * count);
	person **values = malloc(sizeof(person *) * count);

	// icsmap_all iterates through it's map and copies each of it's key/val pairs
	// into the arrays pointed to by keys and vals.
	icsmap_all(map, keys, values);

	// we can then iterate through them as normal
	int id;
	person *p;
	for (i = 0; i < count; ++i) {
		id = keys[i];
		p  = values[i];
		printf("This function is happening in main's scope and I'm ");
		log("person %p: ID: %d, Age: %d, Siblings: %d", p, id, p->age, p->sibling_count);
	}
	
	// In the last example, even though we called deinit on the map, we did not
	// manage the memory that we allocated. Now that we know how to iterate
	// through key value pairs, we can use this technique to properly manage our
	// memory

	for(i = 0; i < count; ++i) {
		p = values[i];
		log("Freeing pointer %p", p);
		free(p);
	}

	// don't forget to free our memory when we are done with it!
	free(keys);
	free(values);

	icsmap_deinit(map);

	return 0;
}
