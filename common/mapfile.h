#ifndef _MAPFILE_H_
#define _MAPFILE_H_

void *y_mmap_new(const char *fn);
void y_mmap_free(void *map);
int y_mmap_length(void *map);
void *y_mmap_addr(void *map);


#endif/*_MAPFILE_H_*/

