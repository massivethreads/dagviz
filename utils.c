#include "dagviz.h"

#define DV_STACK_CELL_SZ 1


dv_linked_list_t * dv_linked_list_create() {
	dv_linked_list_t * l = (dv_linked_list_t *) dv_malloc( sizeof(dv_linked_list_t) );
	return l;
}

void dv_linked_list_destroy(dv_linked_list_t * l) {
	dv_free(l, sizeof(dv_linked_list_t));
}

void dv_linked_list_init(dv_linked_list_t *l) {
	dv_check(l);
	l->item = 0;
	l->next = 0;
}

void * dv_linked_list_remove(dv_linked_list_t *list, void *item) {
	void * ret = NULL;
	dv_linked_list_t *l = list;
	dv_linked_list_t *pre = NULL;
	while (l) {
		if (l->item == item) {
			break;
		}
		pre = l;
		l = l->next;
	}
	if (l && l->item == item) {
		ret = l->item;
		if (pre) {
			pre->next = l->next;
			free(l);
		} else {
			l->item = NULL;
		}		
	}
	return ret;
}

void dv_linked_list_add(dv_linked_list_t *list, void *item) {
	if (list->item == NULL) {
		list->item = item;
	} else {
		dv_linked_list_t * newl = (dv_linked_list_t *) dv_malloc( sizeof(dv_linked_list_t) );
		newl->item = item;
		newl->next = 0;
		dv_linked_list_t *l = list;
		while (l->next)
			l = l->next;
		l->next = newl;
	}
}


void dv_stack_init(dv_stack_t * s) {
	s->freelist = 0;
	s->top = 0;
}

void dv_stack_fini(dv_stack_t * s) {
	dv_check(!s->top);
	dv_stack_cell_t * cell;
	dv_stack_cell_t * next;
	for (cell=s->freelist; cell; cell=next) {
		next = cell->next;
		
	}
}

static dv_stack_cell_t * dv_stack_ensure_freelist(dv_stack_t * s) {
  dv_stack_cell_t * f = s->freelist;
  if (!f) {
    int n = DV_STACK_CELL_SZ;
    f = (dv_stack_cell_t *) dv_malloc(sizeof(dv_stack_cell_t) * n);
    int i;
    for (i=0; i<n-1; i++) {
      f[i].next = &f[i+1];
    }
    f[n-1].next = 0;
    s->freelist = f;
  }
  return f;
}

void dv_stack_push(dv_stack_t * s, void * node) {
	dv_stack_cell_t * f = dv_stack_ensure_freelist(s);
	dv_check(f);
	f->item = node;
	s->freelist = f->next;
	f->next = s->top;
	s->top = f;
}

void * dv_stack_pop(dv_stack_t * s) {
	dv_stack_cell_t * top = s->top;
	dv_check(top);
	s->top = top->next;
	top->next = s->freelist;
	s->freelist = top;
	return top->item;
}
