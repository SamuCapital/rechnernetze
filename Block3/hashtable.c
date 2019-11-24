#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "uthash.h"

struct datastruct
{
    int id; /* key */
    char name[10];
    UT_hash_handle hh; /* makes this structure hashable */
};

//declare the hash
struct datastruct *data = NULL;

//adding item to hash, in this case data
void set(int data_id, char *name)
{
    struct datastruct *s;
    HASH_FIND_INT(data, &data_id, s); /* id already in the hash? */
    if (s == NULL)
    {
        s = (struct data *)malloc(sizeof *s);
        s->id = data_id;
        HASH_ADD_INT(data, id, s); /* id: name of key field */
    }
    strcpy(s->name, name);
}

//delete Methode
void delete (struct datastruct *datatodel)
{
    HASH_DEL(data, datatodel); /* sucht zunächst strcuture und löscht sie dann (1.par: hash, 2.par: zu löschender struct*/
    free(datatodel);           /* optional; it's up to you! */
}

void deletewkey(int data_id)
{
    struct datastruct *s;

    HASH_FIND_INT(data, &data_id, s); /* s: output pointer */
    HASH_DEL(data, s);
    free(s);
}

struct datastruct *get(int data_id)
{
    struct datastruct *s;

    HASH_FIND_INT(data, &data_id, s); /* s: output pointer */
    if (s == NULL)
    {
        printf("struct existiert nicht\n");
    }
    return s;
}
