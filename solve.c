#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_BLOCK	8

typedef struct one_st {
	uint8_t wall:1;
	uint8_t pos:3;
	uint8_t block:3;
} one_st;

typedef struct pos_st {
	uint8_t x:4;
	uint8_t y:4;
	uint8_t type;
} pos_st;

#define MAP_WIDTH	9
#define MAP_HEIGHT	7

static one_st g_map[MAP_HEIGHT][MAP_WIDTH];
static pos_st g_pos[MAX_BLOCK];
static int g_nblock;

typedef struct state_st {
	struct state_st *hnext;
	struct state_st *parent;
	struct state_st *next;
	uint32_t hkey;
	pos_st block[MAX_BLOCK];
	char opr;
	char step;
} state_st;

#define HSIZE	(1<<24)
static struct state_st *hash_head[HSIZE];
static state_st *state_head, *state_tail;

static inline unsigned int hash_key(const void *key, int klen)
{
	unsigned int h = 5831;
	const unsigned char *p = (const unsigned char *)key;
	
	while (klen > 0) {
		h = h * 33 + (*p);
		p++;
		klen--;
	}
	return h;
}

static void hash_insert(state_st *s)
{
	uint32_t key = hash_key(s->block, g_nblock * sizeof(pos_st));
	uint32_t idx = key & (HSIZE - 1);
	s->hkey = key;
	s->hnext = hash_head[idx];
	hash_head[idx] = s;
}

static int hash_search(state_st *s)
{
	uint32_t key = hash_key(s->block, g_nblock * sizeof(pos_st));
	uint32_t idx = key & (HSIZE - 1);
	state_st *p = hash_head[idx];
	
	while (p) {
		if (p->hkey == key && memcmp(p->block, s->block, g_nblock * sizeof(pos_st)) == 0) {
			return 0;
		}
		p = p->hnext;
	}
	return 1;
}

#define OPR_LEFT	1
#define OPR_RIGHT	2
#define OPR_UP		3
#define OPR_DOWN	4

static state_st *load_state(const char *file)
{
	char line[64];
	int h = 0;
	state_st *s = calloc(sizeof(state_st), 1);
	FILE *fp = fopen(file, "rb");
	if (!fp)
		return NULL;
	
	while (fgets(line, sizeof(line), fp)) {
		char *p;
		int len;
		int i;
		
		if ((p = strchr(line, '\r')))
			*p = 0;
		if ((p = strchr(line, '\n')))
			*p = 0;
		
		len = strlen(line);
		if (len == 0)
			continue;
		
		if (len != MAP_WIDTH)
			return NULL;
		
		for (i = 0; i < len; ++i) {
			if (line[i] == '#') {
				g_map[h][i].wall = 1;
			} else if (line[i] == ' ') {
				g_map[h][i].wall = 0;
			} else if (isupper((int)line[i]) && line[i] <= 'G') {
				g_pos[line[i] - 'A' + 1].x = i;
				g_pos[line[i] - 'A' + 1].y = h;
			} else if (islower((int)line[i]) && line[i] <= 'g') {
				s->block[g_nblock].x = i;
				s->block[g_nblock].y = h;
				s->block[g_nblock].type = line[i] - 'a' + 1;
				g_nblock++;
			} else {
				return NULL;
			}
		}
		h++;
	}

	return s;
}


static state_st *copy_state(state_st *s, char opr)
{
	state_st *ret = malloc(sizeof(state_st));
	
	memcpy(ret, s, sizeof(state_st));
	ret->parent = s;
	ret->next = NULL;
	ret->opr = opr;
	ret->step = s->step + 1;
	return ret;
}

static int xy_cmp_up(const void *a, const void *b)
{
	const pos_st *pa = a, *pb = b;
	int n = (int)pa->y - (int)pb->y;
	
	if (n != 0) 
		return n;
	
	return (int)pa->x - (int)pb->x;
}

static int check_state(state_st *d)
{
	int i;
	
	for (i = 0; i < g_nblock; ++i) {
		pos_st *p = &d->block[i];
		pos_st *cmp = &g_pos[p->type];
		if (cmp->x && cmp->y) {
			if (p->x != cmp->x || p->y != cmp->y) {
				return 0;
			}
		}
	}
	return 1;
}

static state_st *extern_state(state_st *s)
{
	state_st *d;
	one_st tmp[MAP_HEIGHT][MAP_WIDTH];
	one_st block_map[MAP_HEIGHT][MAP_WIDTH];
	pos_st tmpblock[MAX_BLOCK];
	int i;
	
	memcpy(block_map, g_map, sizeof(g_map));
	for (i = 0; i < g_nblock; ++i) {
		pos_st *p = &s->block[i];
		block_map[p->y][p->x].block = p->type;
	}
	
	memcpy(tmpblock, s->block, g_nblock * sizeof(pos_st));
	qsort(tmpblock, g_nblock, sizeof(pos_st), xy_cmp_up);
	
	// left
	d = copy_state(s, OPR_LEFT);
	memcpy(tmp, block_map, sizeof(block_map));
	memcpy(d->block, tmpblock, g_nblock * sizeof(pos_st));
	
	for (i = 0; i < g_nblock; ++i) {
		pos_st *p = &d->block[i];
		one_st *left = &tmp[p->y][p->x - 1];
		if (left->wall == 0 && left->block == 0) {
			left->block = p->type;
			tmp[p->y][p->x].block = 0;
			p->x--;
		}
	}
	
	qsort(d->block, g_nblock, sizeof(pos_st), xy_cmp_up);
	if (hash_search(d) != 0) {
		hash_insert(d);
		state_tail->next = d;
		state_tail = d;
		if (check_state(d))
			return d;
	} else {
		free(d);
	}
	
	// right
	d = copy_state(s, OPR_RIGHT);
	memcpy(tmp, block_map, sizeof(block_map));
	memcpy(d->block, tmpblock, g_nblock * sizeof(pos_st));
	
	for (i = g_nblock - 1; i >= 0 ; --i) {
		pos_st *p = &d->block[i];
		one_st *right = &tmp[p->y][p->x + 1];
		if (right->wall == 0 && right->block == 0) {
			right->block = p->type;
			tmp[p->y][p->x].block = 0;
			p->x++;
		}
	}
	
	qsort(d->block, g_nblock, sizeof(pos_st), xy_cmp_up);
	if (hash_search(d) != 0) {
		hash_insert(d);
		state_tail->next = d;
		state_tail = d;
		if (check_state(d))
			return d;
	} else {
		free(d);
	}
	
	// up
	d = copy_state(s, OPR_UP);
	memcpy(tmp, block_map, sizeof(block_map));
	memcpy(d->block, tmpblock, g_nblock * sizeof(pos_st));
	
	for (i = 0; i < g_nblock; ++i) {
		pos_st *p = &d->block[i];
		tmp[p->y][p->x].block = p->type;
	}
	
	for (i = 0; i < g_nblock; ++i) {
		pos_st *p = &d->block[i];
		one_st *up = &tmp[p->y - 1][p->x];
		if (up->wall == 0 && up->block == 0) {
			up->block = p->type;
			tmp[p->y][p->x].block = 0;
			p->y--;
		}
	}
	
	qsort(d->block, g_nblock, sizeof(pos_st), xy_cmp_up);
	if (hash_search(d) != 0) {
		hash_insert(d);
		state_tail->next = d;
		state_tail = d;
		if (check_state(d))
			return d;
	} else {
		free(d);
	}
	
	// down
	d = copy_state(s, OPR_DOWN);
	memcpy(tmp, block_map, sizeof(block_map));
	memcpy(d->block, tmpblock, g_nblock * sizeof(pos_st));
	
	for (i = 0; i < g_nblock; ++i) {
		pos_st *p = &d->block[i];
		tmp[p->y][p->x].block = p->type;
	}
	
	for (i = g_nblock - 1; i >= 0; --i) {
		pos_st *p = &d->block[i];
		one_st *down = &tmp[p->y + 1][p->x];
		if (down->wall == 0 && down->block == 0) {
			down->block = p->type;
			tmp[p->y][p->x].block = 0;
			p->y++;
		}
	}
	
	qsort(d->block, g_nblock, sizeof(pos_st), xy_cmp_up);
	if (hash_search(d) != 0) {
		hash_insert(d);
		state_tail->next = d;
		state_tail = d;
		if (check_state(d))
			return d;
	} else {
		free(d);
	}
	
	return NULL;
}

static void debug_state(state_st *d)
{
	int x, y;
	int i;
	
	one_st tmp[MAP_HEIGHT][MAP_WIDTH];
	memcpy(tmp, g_map, sizeof(g_map));
	
	for (i = 0; i < g_nblock; ++i) {
		pos_st *p = &d->block[i];
		tmp[p->y][p->x].block = p->type;
	}
	
	for (y = 0; y < MAP_HEIGHT; ++y) {
		for (x = 0; x < MAP_WIDTH; ++x) {
			one_st *cur = &tmp[y][x];
			if (cur->wall) {
				putchar('#');
			} else if (cur->block) {
				putchar(cur->block + 'a' - 1);
			} else if (cur->pos) {
				putchar(cur->pos + 'A' - 1);
			} else {
				putchar(' ');
			}
		}
		putchar('\n');
	}
}


static void print_result(state_st *s)
{
	const char *res = " LRUD";
	
	if (s->parent)
		print_result(s->parent);
	
	putchar(res[(int)s->opr]);
	putchar(' ');
}

int main(int argc, char **argv)
{
	state_st *s = load_state(argv[1]);
	debug_state(s);
	state_head = s;
	state_tail = s;
	hash_insert(s);
	
	int n = 0;
	int nstate = 0;
	time_t last_time = 0;
	state_st *p = state_head;
	
	while (p) {
		if (n > 9999) {
			time_t now = time(NULL);
			if (now != last_time) {
				fprintf(stderr, "checking state: %d, current step: %d\n", nstate, p->step);
				last_time = now;
			}
			n = 0;
		}
		
		if ((s = extern_state(p))) {
			print_result(s);
			fprintf(stderr, "\nfound.\n");
			break;
		}
		p = p->next;
		nstate++;
		n++;
	}
	
	return 0;
}
