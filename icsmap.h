#include <stdint.h>

#ifndef ICSMAP
#define ICSMAP

/*
 * icsmap functions return ics_status. These indicate how a function performed, 
 * whether it encountered an error, etc. You can obtain the string representation
 * by calling ics_status_str(status).
 */
#define ICS_ERROR_CODES                                  \
	DEFINE_ICS_ERR(ICS_OK,        "Success"           )  \
	DEFINE_ICS_ERR(ICS_FAILURE,   "Failure"           )  \
	DEFINE_ICS_ERR(ICS_NO_MEMORY, "Out of memory"     )  \
	DEFINE_ICS_ERR(ICS_NOT_FOUND, "Not found"         )  \
	DEFINE_ICS_ERR(ICS_EXISTS,    "Already Exists"    )      

/*
 * We define ics_status as an enum here but we do some other macro magic elsewhere
 * to also define the strings.
 */
#define DEFINE_ICS_ERR(_err, _str) _err,
typedef enum ics_status {
	ICS_ERROR_CODES
} ics_status;
#undef DEFINE_ICS_ERR

/*
 * The internal representation is icsmap. Defined in the .c file and invisble
 * from module consumers. icsmap_handle is the pointer which consumers of the
 * code get.
 */
struct icsmap;
typedef struct icsmap *icsmap_handle;

// If the user needs to customize how to extract the key from the supplied void*
typedef const void * (*get_key_fn) (const void *icsmap_key, uint32_t *size);

// function called on each iteration of the foreach loop
typedef void (*foreach_fn) (const void *key, const void *val, void *data);

/*
 * icsmap_init takes in a config struct. This makes it easy later on to add new
 * features to the map. It also allows clients to be explicit with how they want
 * the map to behave.
 */
typedef struct icsmap_cfg {
	uint32_t keysize;   // size of keys passed to put/get
	uint32_t valsize;   // size of values passed through api
	get_key_fn get_key; // a custom function to extract a key, or NULL to use default
} icsmap_cfg;

/*
 * Call this to get the string representation of an ics_status code.
 * Args:
 *	status [IN]: an ics_status to transform to a string.
 *
 * Returns:
 *	a pointer to a statically allocated char array.
 */
const char *
ics_status_str(ics_status status);

/*
 * icsmap_init initializes the icsmap struct with the given configuration.
 * Args:
 *	handle [IN/OUT]: A handle to an icsmap
 *	cfg    [IN]: A configuration struct
 *
 * Returns:
 *	ICS_OK if successful, Appropriate error on failure.
 */
ics_status
icsmap_init(icsmap_handle *handle, const icsmap_cfg *cfg);

/*
 * icsmap_put stores the key and value in the map. If the key already exists,
 * the value is overwritten by the new value. Keys and values are copied by value.
 * Args:
 *	handle [IN/OUT]: A handle to an icsmap
 *	key    [IN]: A pointer to a key to search for
 *	val    [IN]: A pointer to a val to place in the map
 *
 * Returns:
 *	ICS_OK if successful, Appropriate error on failure.
 */
ics_status
icsmap_put(icsmap_handle handle, const void *key, const void *val);

/*
 * icsmap_get retrieves the value associated with the key from the map. Consumers
 * pass in a pointer to a local val with enough space and icsmap handles copying
 * the value in.
 *
 * Args:
 *	handle [IN/OUT]: A handle to an icsmap
 *	key    [IN]: A pointer to a key to search for
 *	out    [OUT]: A pointer to a place in memory with enough storage for a map
 *	              value
 *
 * Returns:
 *	ICS_OK if successful, Appropriate error on failure.
 */
ics_status
icsmap_get(const icsmap_handle handle, const void *key, void *out);

/*
 * Returns an ics_status indicating if the given key is in the map
 *
 * Args:
 *	handle [IN/OUT]: A handle to an icsmap
 *	key    [IN]: A pointer to a key to search for
 *
 * Returns:
 *	ICS_EXISTS if the key exists, else ICS_NOT_FOUND
 */
ics_status
icsmap_contains(const icsmap_handle handle, const void *key);

/*
 * Removes the key from the map
 *
 * Args:
 *	handle [IN/OUT]: A handle to an icsmap
 *	key    [IN]: A pointer to the key to remove
 *
 * Returns:
 *	ICS_OK if successful, Appropriate error on failure.
 */
ics_status
icsmap_remove(icsmap_handle handle, const void *key);

/*
 * Iterates through the map and at each key/value, it calls the specified callback
 * function. Consumers can pass in a pointer to some data they would like to use
 * inside the function.
 *
 * Args:
 *	handle [IN/OUT]: A handle to an icsmap
 *	fn     [IN]: The function to call for each key/value
 *	data   [IN/OUT]: The consumer of the code can pass in data to be used inside
 *					 the callback function.
 *
 * Returns:
 *	nothing
 */
void
icsmap_foreach(const icsmap_handle handle, foreach_fn fn, void *data);

/*
 * Retrieves the number of keys inside the map
 *
 * Args:
 *	handle [IN/OUT]: A handle to an icsmap
 *
 * Returns:
 *	the number of keys in the map
 */
uint32_t
icsmap_count(const icsmap_handle);

/*
 * Args:
 *	handle [IN/OUT]: A handle to an icsmap
 *
 * Returns:
 *	nothing
 */
void
icsmap_all(const icsmap_handle handle, void *keys, void *vals);

/*
 * Args:
 *	handle [IN/OUT]: A handle to an icsmap
 *
 * Returns:
 *	nothing
 */
void
icsmap_deinit(icsmap_handle handle);

#endif  /* ICSMAP */

