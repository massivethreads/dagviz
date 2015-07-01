#include "dagviz.h"

#define DV_STACK_CELL_SZ 1
#define DV_LLIST_CELL_SZ 1


/* DV LINKED_LIST  */ /* (obsolete) */

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

/* end of DV LINKED_LIST  */



/* DV STACK  */

void dv_stack_init(dv_stack_t * s) {
  s->freelist = 0;
  s->top = 0;
}

void dv_stack_fini(dv_stack_t * s) {
  dv_check(!s->top);
  dv_stack_cell_t * cell;
  dv_stack_cell_t * next;
  for (cell=s->freelist; cell; cell=next) {
    int i;
    next = cell;
    for (i=0; i<DV_STACK_CELL_SZ; i++)
      next = next->next;
    dv_free(cell, sizeof(dv_stack_cell_t) * DV_STACK_CELL_SZ);    
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

/* end of DV STACK  */



/*----------LLIST's functions-------------------------------*/

void dv_llist_init(dv_llist_t *l) {
  l->sz = 0;
  l->top = 0;
}

void dv_llist_fini(dv_llist_t *l) {
  dv_llist_cell_t *p = l->top;
  dv_llist_cell_t *pp;
  while (p) {
    pp = p->next;
    p->next = CS->FL;
    CS->FL = p;
    p = pp;
  }
  dv_llist_init(l);
}

dv_llist_t * dv_llist_create() {
  dv_llist_t * l = (dv_llist_t *) dv_malloc( sizeof(dv_llist_t) );
  dv_llist_init(l);
  return l;
}

void dv_llist_destroy(dv_llist_t *l) {
  dv_llist_fini(l);
  dv_free(l, sizeof(dv_llist_t));      
}

int dv_llist_is_empty(dv_llist_t *l) {
  if (l->sz)
    return 0;
  else
    return 1;
}

static dv_llist_cell_t * dv_llist_ensure_freelist() {
  if (!CS->FL) {
    int n = DV_LLIST_CELL_SZ;
    dv_llist_cell_t *FL;
    FL = (dv_llist_cell_t *) dv_malloc(sizeof(dv_llist_cell_t) * n);
    int i;
    for (i=0; i<n-1; i++) {
      FL[i].item = 0;
      FL[i].next = &FL[i+1];
    }
    FL[n-1].item = 0;
    FL[n-1].next = 0;
    CS->FL = FL;
  }
  dv_llist_cell_t * c = CS->FL;
  CS->FL = CS->FL->next;
  c->next = 0;
  return c;
}

void dv_llist_add(dv_llist_t *l, void *x) {
  dv_llist_cell_t * c = dv_llist_ensure_freelist();
  c->item = x;
  if (!l->top) {
    l->top = c;
  } else {
    dv_llist_cell_t * h = l->top;
    while (h->next)
      h = h->next;
    h->next = c;
  }
  l->sz++;
}

void * dv_llist_pop(dv_llist_t *l) {
  dv_llist_cell_t * c = l->top;
  void * ret = NULL;
  if (c) {
    l->top = c->next;
    l->sz--;
    ret = c->item;
    c->item = 0;
    c->next = CS->FL;
    CS->FL = c;
  }
  return ret;
}

void * dv_llist_get(dv_llist_t *l, int idx) {
  void *ret = NULL;
  if (l->sz <= idx)
    return ret;
  dv_llist_cell_t * c = l->top;
  int i;
  for (i=0; i<idx; i++) {
    if (!c) break;
    c = c->next;
  }
  if (i == idx && c)
    ret = c->item;
  return ret;
}

void * dv_llist_get_top(dv_llist_t *l) {
  return dv_llist_get(l, 0);
}

void * dv_llist_remove(dv_llist_t *l, void *x) {
  void * ret = NULL;
  dv_llist_cell_t * h = l->top;
  dv_llist_cell_t * pre = NULL;
  // find item
  while (h) {
    if (h->item == x) {
      break;
    }
    pre = h;
    h = h->next;
  }
  if (h && h->item == x) {
    // remove item
    ret = h->item;
    if (pre) {
      pre->next = h->next;
    } else {
      l->top = h->next;
    }
    l->sz--;
    // recycle llist_cell
    h->item = 0;
    h->next = CS->FL;
    CS->FL = h;
  }
  return ret;
}

int dv_llist_has(dv_llist_t *l, void *x) {
  int ret = 0;
  dv_llist_cell_t * c = l->top;
  while (c) {
    if (c->item == x) {
      ret = 1;
      break;
    }
    c = c->next;
  }
  return ret;
}

void * dv_llist_iterate_next(dv_llist_t *l, void *u) {
  dv_check(l);
  void * ret = NULL;
  dv_llist_cell_t * c = l->top;
  if (!u) {
    if (c)
      ret = c->item;
  } else {
    while (c && c->item != u)
      c = c->next;
    if (c && c->item == u && c->next)
      ret = c->next->item;
  }
  return ret;
}

int dv_llist_size(dv_llist_t *l) {
  dv_check(l);
  return l->sz;
}

/*----------end of LLIST's functions-------------------------------*/



/*----------Convenient Functions-------------------------------*/

const char * dv_convert_char_to_binary(int x)
{
  static char b[9];
  b[0] = '\0';
  int z;
  for (z=1; z<=1<<3; z<<=1)
    strcat(b, ((x & z) == z) ? "1" : "0");
  return b;
}

double dv_max(double d1, double d2) {
  if (d1 > d2)
    return d1;
  else
    return d2;
}

double dv_min(double d1, double d2) {
  if (d1 < d2)
    return d1;
  else
    return d2;
}

/*----------end of Convenient Functions-----------------------*/





/*----------DAG Node Pool------------------------------*/

void
dv_dag_node_pool_init(dv_dag_node_pool_t * pool) {
  pool->head = NULL;
  pool->tail = NULL;
  pool->pages = NULL;
  pool->sz = 0;
  pool->N = 0;
  pool->n = 0;
}

static dv_dag_node_t *
dv_dag_node_pool_add_page(dv_dag_node_pool_t * pool) {
  /* allocate page */
  size_t sz_ = DV_DEFAULT_PAGE_SIZE;
  size_t sz = (sz_ >= sizeof(dv_dag_node_page_t) ? sz_ : sizeof(dv_dag_node_page_t));
  dv_dag_node_page_t * page = (dv_dag_node_page_t *) dv_malloc(sz);
  /* initialize page */
  page->sz = sz;
  long n = ( sz - sizeof(dv_dag_node_page_t) ) / sizeof(dv_dag_node_t) + DV_DEFAULT_PAGE_MIN_NODES;
  long i;
  for (i = 0; i < n - 1; i++) {
    page->nodes[i].next = &page->nodes[i+1];
  }
  page->nodes[n-1].next = NULL;
  /* append page */
  page->next = pool->pages;
  pool->pages = page;
  pool->sz += page->sz;
  /* append nodes */
  if (!pool->head)
    pool->head = &page->nodes[0];
  else
    pool->tail->next = &page->nodes[0];
  pool->tail = &page->nodes[n-1];
  pool->N += n;
  pool->n += n;
  //fprintf(stderr, "allocated %ld bytes (%ld nodes), total %ld (%ld)\n", sz, n, pool->sz, pool->N);
  return pool->head;
}

dv_dag_node_t *
dv_dag_node_pool_pop(dv_dag_node_pool_t * pool) {
  dv_dag_node_t * ret = pool->head;
  if (!ret)
    ret = dv_dag_node_pool_add_page(pool);
  dv_check(ret);
  pool->head = ret->next;
  if (!pool->head)
    pool->tail = NULL;
  ret->next = NULL;
  pool->n--;
  return ret;
}

void
dv_dag_node_pool_push(dv_dag_node_pool_t * pool, dv_dag_node_t * node) {
  node->next = pool->head;
  pool->head = node;
  if (!pool->tail)
    pool->tail = node;
  pool->n++;
}

dv_dag_node_t *
dv_dag_node_pool_pop_contiguous(dv_dag_node_pool_t * pool, long num) {
  if (num < 1)
    return NULL;
  while (pool->n < num)
    dv_dag_node_pool_add_page(pool);
  dv_dag_node_t * ret = pool->head;
  dv_check(ret);
  dv_dag_node_t * ret_ = ret;
  long i = 1;
  while (i < num) {
    dv_check(ret_);
    ret_ = ret_->next;
    i++;
  }
  pool->head = ret_->next;
  if (!pool->head)
    pool->tail = NULL;
  ret_->next = NULL;
  pool->n -= num;
  return ret;
}


/*----------end of DAG Node Pool-----------------------*/




/*----------HISTOGRAM Entry Pool------------------------------*/

void
dv_histogram_entry_pool_init(dv_histogram_entry_pool_t * pool) {
  pool->head = NULL;
  pool->tail = NULL;
  pool->pages = NULL;
  pool->sz = 0;
  pool->N = 0;
  pool->n = 0;
}

static dv_histogram_entry_t *
dv_histogram_entry_pool_add_page(dv_histogram_entry_pool_t * pool) {
  /* allocate page */
  size_t sz_ = DV_DEFAULT_PAGE_SIZE;
  size_t sz = (sz_ >= sizeof(dv_histogram_entry_page_t) ? sz_ : sizeof(dv_histogram_entry_page_t));
  dv_histogram_entry_page_t * page = (dv_histogram_entry_page_t *) dv_malloc(sz);
  /* initialize page */
  page->sz = sz;
  long n = ( sz - sizeof(dv_histogram_entry_page_t) ) / sizeof(dv_histogram_entry_t) + DV_DEFAULT_PAGE_MIN_ENTRIES;
  long i;
  for (i = 0; i < n - 1; i++) {
    page->entries[i].next = &page->entries[i+1];
  }
  page->entries[n-1].next = NULL;
  /* append page */
  page->next = pool->pages;
  pool->pages = page;
  pool->sz += page->sz;
  /* append nodes */
  if (!pool->head)
    pool->head = &page->entries[0];
  else
    pool->tail->next = &page->entries[0];
  pool->tail = &page->entries[n-1];
  pool->N += n;
  pool->n += n;
  //fprintf(stderr, "allocated %ld bytes (%ld nodes), total %ld (%ld)\n", sz, n, pool->sz, pool->N);
  return pool->head;
}

dv_histogram_entry_t *
dv_histogram_entry_pool_pop(dv_histogram_entry_pool_t * pool) {
  dv_histogram_entry_t * ret = pool->head;
  if (!ret)
    ret = dv_histogram_entry_pool_add_page(pool);
  dv_check(ret);
  pool->head = ret->next;
  if (!pool->head)
    pool->tail = NULL;
  ret->next = NULL;
  pool->n--;
  return ret;
}

void
dv_histogram_entry_pool_push(dv_histogram_entry_pool_t * pool, dv_histogram_entry_t * node) {
  node->next = pool->head;
  pool->head = node;
  if (!pool->tail)
    pool->tail = node;
  pool->n++;
}


/*----------end of HISTOGRAM Entry Pool-----------------------*/

