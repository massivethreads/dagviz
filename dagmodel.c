#include "dagmodel.h"

dm_global_state_t DMG[1];

/***** PIDAG Reader's functions *****/

dm_pidag_t *
dm_pidag_read_new_file(char * filename) {
  /* Read file size */
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "cannot open file: %d\n", errno);
    return NULL;
  }
  struct stat statbuf[1];
  int err = fstat(fd, statbuf);
  if (err < 0) {
    fprintf(stderr, "cannot run fstat: %d\n", errno);
    return NULL;
  }
  
  /* Read DR's PI_DAG */
  dr_pi_dag * G = dr_read_dag(filename);
  if (!G)
    return NULL;
  
  /* Get new PIDAG */
  if (DMG->nP >= DM_MAX_DAG_FILE)
    return NULL;
  dm_pidag_t * P = &DMG->P[DMG->nP++];
  memset(P, 0, sizeof(dm_pidag_t));
  P->fn = filename;
  P->filename = (char *) dm_malloc( sizeof(char) * (strlen(filename) + 1) );
  strcpy(P->filename, filename);
  P->filename[strlen(filename)] = '\0';
  P->short_filename = dm_filename_get_short_name(P->filename);
  dm_llist_init(P->itl);

  P->sz = statbuf->st_size;
  P->n = G->n;
  P->m = G->m;
  P->start_clock = G->start_clock;
  P->num_workers = G->num_workers;
  printf("DAG file:\n"
         "  file name = %s\n"
         "  header = %s"
         "  nodes = %ld\n"
         "  edges = %ld\n"
         "  start clock = %llu\n"
         "  workers = %ld\n",
         P->fn, DAG_RECORDER_HEADER, P->n, P->m, P->start_clock, P->num_workers);
  
  P->T = G->T;
  P->E = G->E;
  P->S = G->S;
  P->G = G;

  close(fd);
  return P;
}

static long
dm_pidag_get_node_id(dm_pidag_t * P, dr_pi_dag_node * pi) {
  return pi - P->T;
}

static long
dm_pidag_get_node_id_with_offset(dm_pidag_t * P, dr_pi_dag_node * pi, long offset) {
  return pi - P->T + offset;
}
  
dr_pi_dag_node *
dm_pidag_get_node_by_id(dm_pidag_t * P, long id) {
  dm_check(id >=0 && id < P->n);
  dr_pi_dag_node * ret = P->T + id;
  return ret;
}

dr_pi_dag_node *
dm_pidag_get_node_by_dag_node(dm_pidag_t * P, dm_dag_node_t * node) {
  if (!node) return NULL;
  return dm_pidag_get_node_by_id(P, node->pii);
}

static dr_pi_dag_node *
dm_pidag_get_node_by_offset(dm_pidag_t * P, dr_pi_dag_node * pi, long offset) {
  if (!pi) return NULL;
  long id = dm_pidag_get_node_id_with_offset(P, pi, offset);
  return dm_pidag_get_node_by_id(P, id);
}

/***** end of PIDAG Reader's functions *****/



/***** DAG's functions *****/

void
dm_dag_node_init(dm_dag_node_t * node, dm_dag_node_t * parent, long pii) {
  /* Reset */
  /* Keep node->next intact because it is already used for linking in node pool */
  dm_dag_node_t * t = node->next;
  memset(node, 0, sizeof(dm_dag_node_t));
  node->next = t;

  /* Initialize */
  node->pii = pii;
  dm_node_flag_init(node->f);
  node->d = (parent) ? (parent->d + 1) : 0;
  node->parent = parent;
}

int
dm_dag_node_set(dm_dag_t * D, dm_dag_node_t * node) {
  dm_pidag_t * P = D->P;
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(P, node);

  if (pi->info.kind == dr_dag_node_kind_section
      || pi->info.kind == dr_dag_node_kind_task) {

    if (pi->subgraphs_begin_offset < pi->subgraphs_end_offset)
      dm_node_flag_set(node->f, DM_NODE_FLAG_UNION);
    else
      dm_node_flag_remove(node->f, DM_NODE_FLAG_UNION);
    
  } else {

    dm_node_flag_remove(node->f, DM_NODE_FLAG_UNION);
    
  }
  
  dm_node_flag_set(node->f, DM_NODE_FLAG_SET);
  dm_node_flag_remove(node->f, DM_NODE_FLAG_INNER_LOADED);

  return DM_OK;
}

int
dm_dag_build_node_inner(dm_dag_t * D, dm_dag_node_t * node) {
  dm_pidag_t * P = D->P;

  if (!dm_is_set(node))
    dm_dag_node_set(D, node);
  
  if (dm_is_union(node)) {
    
    dr_pi_dag_node * pi, * pi_a, * pi_b, * pi_x;
    dm_dag_node_t * node_a, * node_b, * node_x, * node_t;

    pi = dm_pidag_get_node_by_dag_node(P, node);
    pi_a = dm_pidag_get_node_by_offset(P, pi, pi->subgraphs_begin_offset);
    pi_b = dm_pidag_get_node_by_offset(P, pi, pi->subgraphs_end_offset - 1);
    dm_check(pi_a <= pi_b);
    
    // Check node pool for all child nodes
    long num_children;
    num_children = pi_b - pi_a + 1;
    node_a = dm_dag_node_pool_pop_contiguous(DMG->pool, num_children);
    if (!node_a)
      return dm_log_set_error(DM_ERROR_OONP);
    D->n += num_children;

    // Get last node, btw check number of children
    node_b = node_a;
    long i;
    for (i = 1; i < num_children; i++) {
      dm_check(node_b);
      node_b = node_b->next;
    }
    dm_check(node_b && !node_b->next);
    
    // Initialize, set children's parent
    pi_x = pi_a;
    node_x = node_a;
    while (node_x) {
      dm_dag_node_init(node_x, node, dm_pidag_get_node_id(P, pi_x));
      dm_dag_node_set(D, node_x);
      if (node_x->d > D->dmax)
        D->dmax = node_x->d;
      pi_x++;
      node_x = dm_dag_node_get_next(node_x);
    }
    
    // Set node->head, node->tail
    node->head = node_a;
    node->tail = node_b;
    // Set children's pre, spawn
    node_x = node_a;
    while (node_x && node_x->next) {
      // next's pre
      node_x->next->pre = node_x;
      // spawn
      pi_x = dm_pidag_get_node_by_dag_node(P, node_x);
      if (pi_x->info.kind == dr_dag_node_kind_create_task) {
        node_t = dm_dag_node_pool_pop(DMG->pool);
        if (!node_t)
          return dm_log_set_error(DM_ERROR_OONP);
        dm_dag_node_init(node_t, node, dm_pidag_get_node_id_with_offset(P, pi_x, pi_x->child_offset));
        dm_dag_node_set(D, node_t);
        node_x->spawn = node_t;
        node_t->pre = node_x;
        D->n++;
      }
      node_x = node_x->next;
    }
    
    // Set node->f
    dm_node_flag_set(node->f, DM_NODE_FLAG_INNER_LOADED);
    dm_node_flag_set(node->f, DM_NODE_FLAG_SHRINKED);
    
  }    

  return DM_OK;
}

int
dm_dag_collapse_node_inner(dm_dag_t * D, dm_dag_node_t * node) {
  (void) D;
  // Check flags node->f
  if (dm_is_set(node) && dm_is_union(node)
      && dm_is_inner_loaded(node)) {

    dm_dag_node_t * x = node->head;
    dm_dag_node_t * next;
    while (x) {
      next = x->next;
      if (x->spawn) {
        dm_check(!dm_is_union(x->spawn) || !dm_is_inner_loaded(x->spawn));
        dm_dag_node_pool_push(DMG->pool, x->spawn);
      }
      dm_check(!dm_is_union(x) || !dm_is_inner_loaded(x));
      dm_dag_node_pool_push(DMG->pool, x);
      x = next;
    }

    // Unset node->f
    dm_node_flag_remove(node->f, DM_NODE_FLAG_INNER_LOADED);

  }  
  return DM_OK;
}

static void
dm_dag_clear_shrinked_nodes_1(dm_dag_t * D, dm_dag_node_t * node) {
  dm_dag_collapse_node_inner(D, node);
}

static void
dm_dag_clear_shrinked_nodes_r(dm_dag_t * D, dm_dag_node_t * node) {
  if (!dm_is_set(node)) return;
  
  if (dm_is_union(node) && dm_is_inner_loaded(node)) {

    if (dm_is_shrinked(node)
        && !dm_is_shrinking(node)
        && !dm_is_expanding(node)) {
      // check if node has inner_loaded node
      int has_no_innerloaded_node = 1;
      dm_dag_node_t * x = NULL;
      while ( (x = dm_dag_node_traverse_children(node, x)) ) {
        if (dm_is_set(x) && dm_is_union(x) && dm_is_inner_loaded(x)) {
          has_no_innerloaded_node = 0;
          break;
        }
      }
      if (has_no_innerloaded_node) {
        // collapse inner
        dm_dag_clear_shrinked_nodes_1(D, node);
      } else {
        /* Call inward */
        dm_dag_clear_shrinked_nodes_r(D, node->head);
      }
      
    } else {
      /* Call inward */
      dm_dag_clear_shrinked_nodes_r(D, node->head);      
    }
    
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    dm_dag_clear_shrinked_nodes_r(D, x);
  }
}

void
dm_dag_clear_shrinked_nodes(dm_dag_t * D) {
  dm_dag_clear_shrinked_nodes_r(D, D->rt);
}

static char *
dm_get_component_from_string(char * s, int n) {
  unsigned int i = 0;
  while (i < strlen(s) && (s[i] == ' ' || s[i] == '_')) i++;
  int com = 1;
  while (i < strlen(s) && com < n) {
    while (i < strlen(s) && s[i] != ' ' && s[i] != '_') i++;
    while (i < strlen(s) && (s[i] == ' ' || s[i] == '_')) i++;
    if (i < strlen(s))
      com++;
  }
  if (com == n && i < strlen(s)) {
    unsigned int ii = i;
    while (ii < strlen(s) && s[ii] != ' ' && s[ii] != '_') ii++;
    char * ret = (char *) dm_malloc( sizeof(char) * (ii - i + 1) );
    unsigned int k;
    for (k = 0; k < ii - i; k++)
      ret[k] = s[k + i];
    //strncpy(ret, s + i, ii - i);
    ret[ii-i] = '\0';
    return ret;
  }
  return NULL;
}

char *
dm_get_distinct_components_name_string(char * name) {
  char str[100] = "";
  int i = 1;
  while (1) {
    int to_get_this_component = 0;
    char * com1 = dm_get_component_from_string(name, i);
    if (!com1) break;
    int j;
    for (j = 0; j < DMG->nP; j++) {
      char * com2 = dm_get_component_from_string(DMG->P[j].short_filename, i);
      int equal = 1;
      unsigned int t = 0;
      if (strlen(com1) != strlen(com2)) {
        equal = 0;
      } else {
        while (t < strlen(com1)) {
          if (com1[t] != com2[t]) {
            equal = 0;
            break;
          }
          t++;
        }
      }
      //if (com2 && strcmp(com1,com2) != 0) {
      if (!equal) {
        to_get_this_component = 1;
        //printf("com2 = %s\n", com2);
        //printf("get component %d (%s) (com2 = %s)\n", i, com1, com2);
      }
      if (com2)
        dm_free(com2, sizeof(char) * (strlen(com2) + 1));
      if (to_get_this_component)
        break;
    }
    if (to_get_this_component) {
      if (strlen(str))
        sprintf(str, "%s %s", str, com1);
      else
        sprintf(str, "%s", com1);
    }
    if (com1)
      dm_free(com1, sizeof(char) * (strlen(com1) + 1));
    i++;
  }
  char * ret = (char *) dm_malloc( sizeof(char) * (strlen(str) + 1) );
  strcpy(ret, str);
  ret[strlen(str)] = '\0';
  return ret;
}

void
dm_dag_init(dm_dag_t * D, dm_pidag_t * P) {
  memset(D, 0, sizeof(dm_dag_t));
  char str[10];
  sprintf(str, "DAG %ld", D - DMG->D);
  D->name = malloc( sizeof(char) * (strlen(str) + 1) );
  strcpy(D->name, str);
  D->name[strlen(str)] = '\0';
  D->name_on_graph = dm_get_distinct_components_name_string(P->short_filename);
  D->P = P;
  D->rt = dm_dag_node_pool_pop(DMG->pool);
  dm_dag_node_init(D->rt, 0, 0);
  if (P)
    dm_dag_node_set(D, D->rt);
  D->dmax = 0;
  D->bt = 0.0;
  D->et = 0.0;
  D->n = 1;
  D->cur_d = 0;
  D->cur_d_ex = 0;
  D->collapsing_d = 0;
  D->log_radix = DM_RADIX_LOG;
  D->power_radix = DM_RADIX_POWER;
  D->linear_radix = DM_RADIX_LINEAR;
  D->radius = DMG->opts.radius;

  dm_llist_init(D->itl);
  D->H = NULL;

  D->time_step = 1000;

  dm_animation_init(D->anim, D);
  dm_motion_init(D->move, D);
}

dm_dag_t *
dm_dag_create_new_with_pidag(dm_pidag_t * P) {
  /* Get new DAG */
  dm_check(DMG->nD < DM_MAX_DAG);
  dm_dag_t * D = &DMG->D[DMG->nD++];
  dm_dag_init(D, P);

  /* Set values */
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(P, D->rt);
  D->bt = pi->info.first_ready_t;
  D->et = pi->info.end.t;
  
  return D;
}

dm_dag_t *
dm_add_dag(char * filename) {
  /* check existing D */
  dm_dag_t * D = NULL;
  int i;
  for (i = 0; i < DMG->nD; i++) {
    if (strcmp(DMG->D[i].P->filename, filename) == 0) {
      D = &DMG->D[i];
      break;
    }
  }
  if (!D) {
    /* read DAG file */
    dm_pidag_t * P = dm_pidag_read_new_file(filename);
    if (!P) {
      dm_perror("could not read DAG from %s.", filename);
      return NULL;
    }
    /* create a new D */
    D = dm_dag_create_new_with_pidag(P);
    if (!D) {
      dm_perror("could not create a new DAG.");
      return NULL;
    }
  }
  return D;
}

int
dm_get_dag_id(dm_dag_t * D) {
  int dag_id = -1;
  if (D) {
    dag_id = D - DMG->D;
  }
  return dag_id;
}

dm_dag_t *
dm_get_dag(int dag_id) {
  dm_dag_t * D = NULL;
  if (dag_id >= 0) {
    D = &DMG->D[dag_id];
  }
  return D;
}

dm_dag_node_t *
dm_dag_node_traverse_children(dm_dag_node_t * node, dm_dag_node_t * cur) {
  if (!node) return NULL;
  if (!cur) {
    return node->head;
  } else if (cur == node->tail) {
    return NULL;
  } else if (!cur->next) {
    return cur->pre->next;
  } else if (cur->spawn) {
    return cur->spawn;
  } else {
    return cur->next;
  }
}

dm_dag_node_t *
dm_dag_node_traverse_children_inorder(dm_dag_node_t * node, dm_dag_node_t * cur) {
  if (!node) return NULL;
  if (!cur) {
    dm_dag_node_t * x = node->head;
    if (x->spawn)
      return x->spawn;
    else
      return x;
  } else if (cur == node->tail) {
    return NULL;
  } else if (!cur->next) {
    return cur->pre;
  } else {
    dm_dag_node_t * x = cur->next;
    if (x->spawn)
      return x->spawn;
    else
      return x;
  }
}

dm_dag_node_t *
dm_dag_node_traverse_tails(dm_dag_node_t * node, dm_dag_node_t * cur) {
  if (!node) return NULL;
  if (cur == node->tail) return NULL;
  
  dm_dag_node_t * x;
  if (!cur)
    x = node->head;
  else
    x = cur->pre->next;

  while (x && !x->spawn && x->next) {
    x = x->next;
  }

  if (!x)
    return NULL;
  if (x->spawn)
    return x->spawn;
  return x;
}

dm_dag_node_t *
dm_dag_node_traverse_nexts(dm_dag_node_t * node, dm_dag_node_t * cur) {
  if (!node) return NULL;
  if (!cur)
    return node->next;
  else if (cur == node->next)
    return node->spawn;
  return NULL;
}

int
dm_dag_node_count_nexts(dm_dag_node_t * node) {
  if (!node) return 0;
  if (node->next && node->spawn)
    return 2;
  else if (node->next)
    return 1;
  return 0;
}

dm_dag_node_t *
dm_dag_node_get_next(dm_dag_node_t * node) {
  if (!node) return NULL;
  return node->next;
}

dm_dag_node_t *
dm_dag_node_get_single_head(dm_dag_node_t * node) {
  if (!node) return NULL;
  while (!dm_is_single(node)) {
    dm_check(node->head);
    node = node->head;
  }
  return node;
}

dm_dag_node_t *
dm_dag_node_get_single_last(dm_dag_node_t * node) {
  if (!node) return NULL;
  while (!dm_is_single(node)) {
    dm_check(node->tail);
    node = node->tail;
  }
  return node;
}

/***** end of DAG's functions *****/


/***** DAG Node Pool *****/

void
dm_dag_node_pool_init(dm_dag_node_pool_t * pool) {
  memset(pool, 0, sizeof(dm_dag_node_pool_t));
  /*
  pool->head = NULL;
  pool->tail = NULL;
  pool->pages = NULL;
  pool->sz = 0;
  pool->N = 0;
  pool->n = 0;
  */
}

static dm_dag_node_t *
dm_dag_node_pool_add_page(dm_dag_node_pool_t * pool) {
  /* allocate page */
  size_t sz_ = DM_DEFAULT_PAGE_SIZE;
  size_t sz = (sz_ >= sizeof(dm_dag_node_page_t) ? sz_ : sizeof(dm_dag_node_page_t));
  dm_dag_node_page_t * page = (dm_dag_node_page_t *) dm_malloc(sz);
  /* initialize page */
  page->sz = sz;
  long n = ( sz - sizeof(dm_dag_node_page_t) ) / sizeof(dm_dag_node_t) + DM_DEFAULT_PAGE_MIN_NODES;
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

dm_dag_node_t *
dm_dag_node_pool_pop(dm_dag_node_pool_t * pool) {
  dm_dag_node_t * ret = pool->head;
  if (!ret)
    ret = dm_dag_node_pool_add_page(pool);
  dm_check(ret);
  pool->head = ret->next;
  if (!pool->head)
    pool->tail = NULL;
  ret->next = NULL;
  pool->n--;
  return ret;
}

void
dm_dag_node_pool_push(dm_dag_node_pool_t * pool, dm_dag_node_t * node) {
  memset(node, 0, sizeof(dm_dag_node_t));
  node->next = pool->head;
  pool->head = node;
  if (!pool->tail)
    pool->tail = node;
  pool->n++;
}

dm_dag_node_t *
dm_dag_node_pool_pop_contiguous(dm_dag_node_pool_t * pool, long num) {
  if (num < 1)
    return NULL;
  while (pool->n < num)
    dm_dag_node_pool_add_page(pool);
  dm_dag_node_t * ret = pool->head;
  dm_check(ret);
  dm_dag_node_t * ret_ = ret;
  long i = 1;
  while (i < num) {
    dm_check(ret_);
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

/***** end of DAG Node Pool *****/


/***** HISTOGRAM Entry Pool *****/

void
dm_histogram_entry_pool_init(dm_histogram_entry_pool_t * pool) {
  pool->head = NULL;
  pool->tail = NULL;
  pool->pages = NULL;
  pool->sz = 0;
  pool->N = 0;
  pool->n = 0;
}

static dm_histogram_entry_t *
dm_histogram_entry_pool_add_page(dm_histogram_entry_pool_t * pool) {
  /* allocate page */
  size_t sz_ = DM_DEFAULT_PAGE_SIZE;
  size_t sz = (sz_ >= sizeof(dm_histogram_entry_page_t) ? sz_ : sizeof(dm_histogram_entry_page_t));
  dm_histogram_entry_page_t * page = (dm_histogram_entry_page_t *) dm_malloc(sz);
  /* initialize page */
  page->sz = sz;
  long n = ( sz - sizeof(dm_histogram_entry_page_t) ) / sizeof(dm_histogram_entry_t) + DM_DEFAULT_PAGE_MIN_ENTRIES;
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

dm_histogram_entry_t *
dm_histogram_entry_pool_pop(dm_histogram_entry_pool_t * pool) {
  dm_histogram_entry_t * ret = pool->head;
  if (!ret)
    ret = dm_histogram_entry_pool_add_page(pool);
  dm_check(ret);
  pool->head = ret->next;
  if (!pool->head)
    pool->tail = NULL;
  ret->next = NULL;
  pool->n--;
  return ret;
}

void
dm_histogram_entry_pool_push(dm_histogram_entry_pool_t * pool, dm_histogram_entry_t * node) {
  node->next = pool->head;
  pool->head = node;
  if (!pool->tail)
    pool->tail = node;
  pool->n++;
}

/***** end of HISTOGRAM Entry Pool *****/


/***** Basic data structure *****/

void dm_stack_init(dm_stack_t * s) {
  s->freelist = 0;
  s->top = 0;
}

void dm_stack_fini(dm_stack_t * s) {
  dm_check(!s->top);
  dm_stack_cell_t * cell;
  dm_stack_cell_t * next;
  for (cell=s->freelist; cell; cell=next) {
    int i;
    next = cell;
    for (i=0; i<DM_STACK_CELL_SZ; i++)
      next = next->next;
    dm_free(cell, sizeof(dm_stack_cell_t) * DM_STACK_CELL_SZ);    
  }
}

static dm_stack_cell_t * dm_stack_ensure_freelist(dm_stack_t * s) {
  dm_stack_cell_t * f = s->freelist;
  if (!f) {
    int n = DM_STACK_CELL_SZ;
    f = (dm_stack_cell_t *) dm_malloc(sizeof(dm_stack_cell_t) * n);
    int i;
    for (i=0; i<n-1; i++) {
      f[i].next = &f[i+1];
    }
    f[n-1].next = 0;
    s->freelist = f;
  }
  return f;
}

void dm_stack_push(dm_stack_t * s, void * node) {
  dm_stack_cell_t * f = dm_stack_ensure_freelist(s);
  dm_check(f);
  f->item = node;
  s->freelist = f->next;
  f->next = s->top;
  s->top = f;
}

void * dm_stack_pop(dm_stack_t * s) {
  dm_stack_cell_t * top = s->top;
  //dm_check(top);
  if (!top)
    return NULL;
  s->top = top->next;
  top->next = s->freelist;
  s->freelist = top;
  return top->item;
}

void dm_llist_init(dm_llist_t *l) {
  l->sz = 0;
  l->top = 0;
}

void dm_llist_fini(dm_llist_t *l) {
  dm_llist_cell_t *p = l->top;
  dm_llist_cell_t *pp;
  while (p) {
    pp = p->next;
    p->next = DMG->FL;
    DMG->FL = p;
    p = pp;
  }
  dm_llist_init(l);
}

dm_llist_t * dm_llist_create() {
  dm_llist_t * l = (dm_llist_t *) dm_malloc( sizeof(dm_llist_t) );
  dm_llist_init(l);
  return l;
}

void dm_llist_destroy(dm_llist_t *l) {
  dm_llist_fini(l);
  dm_free(l, sizeof(dm_llist_t));      
}

int dm_llist_is_empty(dm_llist_t *l) {
  if (l->sz)
    return 0;
  else
    return 1;
}

static dm_llist_cell_t * dm_llist_ensure_freelist() {
  if (!DMG->FL) {
    int n = DM_LLIST_CELL_SZ;
    dm_llist_cell_t *FL;
    FL = (dm_llist_cell_t *) dm_malloc(sizeof(dm_llist_cell_t) * n);
    int i;
    for (i=0; i<n-1; i++) {
      FL[i].item = 0;
      FL[i].next = &FL[i+1];
    }
    FL[n-1].item = 0;
    FL[n-1].next = 0;
    DMG->FL = FL;
  }
  dm_llist_cell_t * c = DMG->FL;
  DMG->FL = DMG->FL->next;
  c->next = 0;
  return c;
}

void dm_llist_add(dm_llist_t *l, void *x) {
  dm_llist_cell_t * c = dm_llist_ensure_freelist();
  c->item = x;
  if (!l->top) {
    l->top = c;
  } else {
    dm_llist_cell_t * h = l->top;
    while (h->next)
      h = h->next;
    h->next = c;
  }
  l->sz++;
}

void * dm_llist_pop(dm_llist_t *l) {
  dm_llist_cell_t * c = l->top;
  void * ret = NULL;
  if (c) {
    l->top = c->next;
    l->sz--;
    ret = c->item;
    c->item = 0;
    c->next = DMG->FL;
    DMG->FL = c;
  }
  return ret;
}

void * dm_llist_get(dm_llist_t *l, int idx) {
  void *ret = NULL;
  if (l->sz <= idx)
    return ret;
  dm_llist_cell_t * c = l->top;
  int i;
  for (i=0; i<idx; i++) {
    if (!c) break;
    c = c->next;
  }
  if (i == idx && c)
    ret = c->item;
  return ret;
}

void * dm_llist_get_top(dm_llist_t *l) {
  return dm_llist_get(l, 0);
}

void * dm_llist_remove(dm_llist_t *l, void *x) {
  void * ret = NULL;
  dm_llist_cell_t * h = l->top;
  dm_llist_cell_t * pre = NULL;
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
    h->next = DMG->FL;
    DMG->FL = h;
  }
  return ret;
}

int dm_llist_has(dm_llist_t *l, void *x) {
  int ret = 0;
  dm_llist_cell_t * c = l->top;
  while (c) {
    if (c->item == x) {
      ret = 1;
      break;
    }
    c = c->next;
  }
  return ret;
}

void * dm_llist_iterate_next(dm_llist_t *l, void *u) {
  dm_check(l);
  void * ret = NULL;
  dm_llist_cell_t * c = l->top;
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

int dm_llist_size(dm_llist_t *l) {
  dm_check(l);
  return l->sz;
}

/***** end of Basic data structure *****/


/****** HISTOGRAM ******/

void
dm_histogram_init(dm_histogram_t * H) {
  memset(H, 0, sizeof(dm_histogram_t));
  H->min_entry_interval = DM_PARAPROF_MIN_ENTRY_INTERVAL;
  H->unit_thick = 2.0;
}

static void
dm_histogram_entry_init(dm_histogram_entry_t * e) {
  e->t = 0.0;
  int i;
  for (i=0; i<dm_histogram_layer_max; i++) {
    e->h[i] = 0.0;
  }
  e->next = NULL;
  e->value_1 = 0.0;
  e->value_2 = 0.0;
  e->value_3 = 0.0;
  e->cumul_value_1 = 0.0;
  e->cumul_value_2 = 0.0;
  e->cumul_value_3 = 0.0;
}

static void
dm_histogram_compute_tallest_entry(dm_histogram_t * H) {
  H->max_h = 0.0;
  dm_histogram_entry_t * e = H->head_e;
  while (e != NULL) {
    double h = 0.0;
    int i;
    for (i = 0; i < dm_histogram_layer_max; i++) {
      h += e->h[i];
    }
    if (h > H->max_h) {
      H->max_h = h;
      H->tallest_e = e;
    }
    e = e->next;
  }
}

double
dm_histogram_get_max_height(dm_histogram_t * H) {
  if (!H->tallest_e || H->max_h == 0.0) {
    dm_histogram_compute_tallest_entry(H);
  }
  return H->max_h * (H->unit_thick * H->D->radius);
}

dm_histogram_entry_t *
dm_histogram_insert_entry(dm_histogram_t * H, double t, dm_histogram_entry_t * e_hint) {
  dm_histogram_entry_t * e = NULL;
  dm_histogram_entry_t * ee = H->head_e;
  if (H->tail_e && (t >= H->tail_e->t)) {
    e = H->tail_e;
    ee = NULL;
  } else {
    if (e_hint && (t >= e_hint->t)) {
      e = e_hint;
      ee = e->next;
    }
    while (ee != NULL && ee->t <= t) {
      e = ee;
      ee = ee->next;
    }
  }
  if (e && (e->t == t))
    return e;
  if (e && ee && (ee->t - e->t) < H->min_entry_interval)
    return e;
  dm_histogram_entry_t * new_e = dm_histogram_entry_pool_pop(DMG->epool);
  if (!new_e) {
    fprintf(stderr, "Error: cannot pop a new entry structure!\n");
    dm_check(0);
  }
  dm_histogram_entry_init(new_e);
  new_e->t = t;
  new_e->next = ee;
  if (e) {
    e->next = new_e;
    // Copy h
    int i;
    for (i=0; i<dm_histogram_layer_max; i++) {
      new_e->h[i] = e->h[i];
    }
  } else {
    H->head_e = new_e;
  }
  if (ee) {
    new_e->next = ee;
  } else {
    H->tail_e = new_e;
  }
  H->n_e++;
  return new_e;
}

void
dm_histogram_add_node(dm_histogram_t * H, dm_dag_node_t * node, dm_histogram_entry_t ** hint_entry) {
  if (!H)
    return;
  
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(H->D->P, node);
  double from_t, to_t, dt;
  dm_histogram_entry_t * e_from;
  dm_histogram_entry_t * e_to;
  dm_histogram_entry_t * e;

  from_t = pi->info.first_ready_t;
  to_t = pi->info.last_start_t;
  dt = to_t - from_t;
  e_from = dm_histogram_insert_entry(H, from_t, (hint_entry)?(*hint_entry):NULL);
  e_to = dm_histogram_insert_entry(H, to_t, e_from);
  dm_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    int layer;
    layer = dm_histogram_layer_ready_end;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_end] / dt);
    layer = dm_histogram_layer_ready_create;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_create] / dt);
    layer = dm_histogram_layer_ready_create_cont;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_create_cont] / dt);
    layer = dm_histogram_layer_ready_wait_cont;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_wait_cont] / dt);
    layer = dm_histogram_layer_ready_other_cont;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_other_cont] / dt);
    e = e->next;
  }

  from_t = pi->info.start.t;
  to_t = pi->info.end.t;
  dt = to_t - from_t;
  e_from = dm_histogram_insert_entry(H, from_t, e_from);
  e_to = dm_histogram_insert_entry(H, to_t, e_to);
  dm_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    e->h[dm_histogram_layer_running] += (pi->info.t_1 / dt);
    e = e->next;
  }
  if (hint_entry) *hint_entry = e_to;
  
  /* invalidate current max height */
  H->tallest_e = 0;
  H->max_h = 0.0;
}

void
dm_histogram_remove_node(dm_histogram_t * H, dm_dag_node_t * node, dm_histogram_entry_t ** hint_entry) {
  if (!H)
    return;
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(H->D->P, node);
  double from_t, to_t, dt;
  dm_histogram_entry_t * e_from;
  dm_histogram_entry_t * e_to;
  dm_histogram_entry_t * e;
  
  from_t = pi->info.first_ready_t;
  to_t = pi->info.last_start_t;
  dt = to_t - from_t;
  e_from = dm_histogram_insert_entry(H, from_t, (hint_entry)?(*hint_entry):NULL);
  e_to = dm_histogram_insert_entry(H, to_t, e_from);
  dm_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    int layer;
    layer = dm_histogram_layer_ready_end;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_end] / dt);
    layer = dm_histogram_layer_ready_create;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_create] / dt);
    layer = dm_histogram_layer_ready_create_cont;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_create_cont] / dt);
    layer = dm_histogram_layer_ready_wait_cont;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_wait_cont] / dt);
    layer = dm_histogram_layer_ready_other_cont;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_other_cont] / dt);
    e = e->next;
  }
  
  from_t = pi->info.start.t;
  to_t = pi->info.end.t;
  dt = to_t - from_t;
  e_from = dm_histogram_insert_entry(H, from_t, e_from);
  e_to = dm_histogram_insert_entry(H, to_t, e_to);
  dm_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    e->h[dm_histogram_layer_running] -= (pi->info.t_1 / dt);
    e = e->next;
  }
  if (hint_entry) *hint_entry = e_to;
  
  /* invalidate current max height */
  H->tallest_e = 0;
  H->max_h = 0.0;
}

void
dm_histogram_clean(dm_histogram_t * H) {
  dm_histogram_entry_t * e = H->head_e;
  dm_histogram_entry_t * ee;
  long n = 0;
  while (e != NULL) {
    ee = e->next;
    dm_histogram_entry_pool_push(DMG->epool, e);
    n++;
    e = ee;
  }
  dm_check(n == H->n_e);
  H->head_e = NULL;
  H->tail_e = NULL;
  H->n_e = 0;
}

void
dm_histogram_fini(dm_histogram_t * H) {
  dm_histogram_clean(H);
  H->D = NULL;
}

static void
dm_histogram_reset_node(dm_histogram_t * H, dm_dag_node_t * node, dm_histogram_entry_t * e_hint_) {
  /* Calculate inward */
  dm_histogram_entry_t * e_hint = e_hint_;
  if (dm_is_union(node) && dm_is_inner_loaded(node)) {
    // Recursive call
    if (dm_is_expanded(node) || dm_is_expanding(node)) {
      dm_histogram_reset_node(H, node->head, e_hint);
    } else {
      dm_histogram_add_node(H, node, &e_hint);
    }
  } else {
    dm_histogram_add_node(H, node, &e_hint);
  }
    
  /* Calculate link-along */
  dm_dag_node_t * u, * v; // linked nodes
  switch ( dm_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    dm_histogram_reset_node(H, u, e_hint);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    dm_histogram_reset_node(H, u, e_hint);
    dm_histogram_reset_node(H, v, e_hint);
    break;
  default:
    dm_check(0);
    break;
  }  
}

void
dm_histogram_reset(dm_histogram_t * H) {
  double time = dm_get_time();
  if (DMG->verbose_level >= 1) {
    fprintf(stderr, "dm_histogram_reset()\n");
  }
  dm_histogram_clean(H);
  if (H->D) 
    dm_histogram_reset_node(H, H->D->rt, NULL);
  if (DMG->verbose_level >= 1) {
    fprintf(stderr, "... done dm_histogram_reset(): %lf ms\n", dm_get_time() - time);
  }
}

static void
dm_histogram_build_all_r(dm_histogram_t * H, dm_dag_node_t * node, dm_histogram_entry_t * e_hint_) {
  /* Calculate inward */
  dm_histogram_entry_t * e_hint = e_hint_;
  if (dm_is_union(node) && dm_is_inner_loaded(node)) {
    dm_histogram_build_all_r(H, node->head, e_hint);
  } else {
    dm_histogram_add_node(H, node, &e_hint);
  }
    
  /* Calculate link-along */
  dm_dag_node_t * u, * v; // linked nodes
  switch ( dm_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    dm_histogram_build_all_r(H, u, e_hint);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    dm_histogram_build_all_r(H, u, e_hint);
    dm_histogram_build_all_r(H, v, e_hint);
    break;
  default:
    dm_check(0);
    break;
  }  
}

void
dm_histogram_build_all(dm_histogram_t * H) {
  double time = dm_get_time();
  if (DMG->verbose_level >= 1) {
    fprintf(stderr, "dm_histogram_build_all()\n");
  }
  dm_histogram_clean(H);
  if (H->D) 
    dm_histogram_build_all_r(H, H->D->rt, NULL);
  if (DMG->verbose_level >= 1) {
    fprintf(stderr, "... done dm_histogram_build_all(): %lf ms\n", dm_get_time() - time);
  }
}

void
dm_histogram_compute_significant_intervals(dm_histogram_t * H) {
  double time = dm_get_time();
  if (DMG->verbose_level >= 1) {
    fprintf(stderr, "dm_histogram_compute_significant_intervals()\n");
  }
  int num_workers = H->D->P->num_workers;
  double num_running_workers = 0.0;
  double cumul_1 = 0.0;
  double cumul_2 = 0.0;
  double cumul_3 = 0.0;
  dm_histogram_entry_t * e;
  dm_histogram_entry_t * ee;
  e = H->head_e;
  while (e != H->tail_e) {
    ee = e->next;
    e->cumul_value_1 = cumul_1;
    e->cumul_value_2 = cumul_2;
    e->cumul_value_3 = cumul_3;
    num_running_workers = e->h[dm_histogram_layer_running];
    /* sched. delay or not */
    if (num_running_workers >= (1.0 * num_workers))
      e->value_1 = 0.0;
    else
      e->value_1 = ee->t - e->t;
    cumul_1 += e->value_1;
    /* delay & nowork associated with sched. delay */
    double num_ready_tasks = 0.0;
    int layer = dm_histogram_layer_running;
    while (++layer < dm_histogram_layer_max) {
      num_ready_tasks += e->h[layer];
    }    
    if (num_running_workers + num_ready_tasks <= num_workers) {
      e->value_2 = (ee->t - e->t) * (num_ready_tasks);
      e->value_3 = (ee->t - e->t) * (num_workers - num_running_workers - num_ready_tasks);
    } else {
      e->value_2 = (ee->t - e->t) * (num_workers - num_running_workers);
      e->value_3 = 0.0;
    }
    cumul_2 += e->value_2;
    cumul_3 += e->value_3;
    e = ee;
  }
  H->tail_e->cumul_value_1 = cumul_1;
  H->tail_e->cumul_value_2 = cumul_2;
  H->tail_e->cumul_value_3 = cumul_3;
  if (DMG->verbose_level >= 1) {
    fprintf(stderr, "... done dm_histogram_compute_significant_intervals(): %lf ms\n", dm_get_time() - time);
  }
}

/****** end of HISTOGRAM ******/


/***** Compute *****/

char *
dm_filename_get_short_name(char * fn) {
  char * p_from = strrchr(fn, '/');
  char * p_to = strrchr(fn, '.');
  if (!p_from)
    p_from = fn;
  else
    p_from++;
  if (!p_to) p_to = fn + strlen(fn);
  int n = p_to - p_from;
  char * ret = (char *) dm_malloc( sizeof(char) * (n + 1) );
  strncpy(ret, p_from, n);
  //int k;
  //  for (k = 0; k < n; k++)
  //      ret[k] = p_from[k];  
  ret[n] = '\0';
  return ret;
}

void
dm_global_state_init() {
  memset(DMG, 0, sizeof(dm_global_state_t));
  dm_dag_node_pool_init(DMG->pool);
  dm_histogram_entry_pool_init(DMG->epool);
  int cp;
  for (cp = 0; cp < DM_NUM_CRITICAL_PATHS; cp++) {
    DMG->SBG->checked_cp[cp] = 0;
    if (cp == DM_CRITICAL_PATH_1)
      DMG->SBG->checked_cp[cp] = 1;
  }
  DMG->oncp_flags[DM_CRITICAL_PATH_0] = DM_NODE_FLAG_CRITICAL_PATH_0;
  DMG->oncp_flags[DM_CRITICAL_PATH_1] = DM_NODE_FLAG_CRITICAL_PATH_1;
  DMG->oncp_flags[DM_CRITICAL_PATH_2] = DM_NODE_FLAG_CRITICAL_PATH_2;

  DMG->opts = dm_options_default_values;
  DMG->initialized = 1;
}

int
dm_global_state_initialized() {
  return DMG->initialized;
}

static void
dm_dag_build_inner_all_r(dm_dag_t * D, dm_dag_node_t * node) {
  if (!dm_is_set(node))
    dm_dag_node_set(D, node);
  if (dm_is_union(node)) {
    if (!dm_is_inner_loaded(node)) {
      dm_dag_build_node_inner(D, node);
    }
    /* Call inward */
    dm_check(node->head);
    dm_dag_build_inner_all_r(D, node->head);
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    dm_dag_build_inner_all_r(D, x);
  }
}

void
dm_dag_build_inner_all(dm_dag_t * D) {
  dm_dag_build_inner_all_r(D, D->rt);
}

static void
dm_dag_compute_critical_paths_r(dm_dag_t * D, dm_dag_node_t * node, dm_histogram_entry_t * e_hint) {
  /* cal. leaf nodes */
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
  //printf("compute node %ld, time: %.4lf%%\n", node->pii, (pi->info.start.t - D->bt) / (D->et - D->bt));
  if (!dm_is_union(node)) {
    /* cal. work & weighted work */
    double work = pi->info.end.t - pi->info.start.t;
    //dm_histogram_entry_t * e1 = dm_histogram_insert_entry(D->H, pi->info.start.t, e_hint);
    //dm_histogram_entry_t * e2 = dm_histogram_insert_entry(D->H, pi->info.end.t, e1);
    int cp;
    for (cp = 0; cp < DM_NUM_CRITICAL_PATHS; cp++) {
      node->cpss[cp].work = work;
      node->cpss[cp].delay = 0.0;
      node->cpss[cp].sched_delay = 0.0;
      //memset(node->cpss[cp].sched_delays, 0, sizeof(double) * dr_dag_edge_kind_max);
      int ek;
      for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
        node->cpss[cp].sched_delays[ek] = 0.0;
      node->cpss[cp].sched_delay_nowork = 0.0;
      node->cpss[cp].sched_delay_delay = 0.0;
    }
    return;
  }
  
  /* cal. children's inner subgraphs first */
  dm_histogram_entry_t * first_e = dm_histogram_insert_entry(D->H, pi->info.start.t, e_hint);
  {
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_children(node, x)) ) {
      dm_dag_compute_critical_paths_r(D, x, first_e);
    }
  }

  /* cal. this node's subgraph */
  dm_dag_node_t * mostwork_tail = NULL; /* cp of work */
  dm_dag_node_t * lastfinished_tail = NULL; /* cp of work & delay */
  double lastfinished_t = 0.0;
  dm_dag_node_t * mostweighted_tail = NULL; /* cp of weighted work & weighted delay */
  dm_dag_node_t * tail = NULL;

#if 0  
  while ( (tail = dm_dag_node_traverse_tails(node, tail)) ) {
    
    dm_critical_path_stat_t cpss[DM_NUM_CRITICAL_PATHS];
    memset(cpss, 0, sizeof(dm_critical_path_stat_t) * DM_NUM_CRITICAL_PATHS);
    dr_pi_dag_node * tail_pi = dm_pidag_get_node_by_dag_node(D->P, tail);

    /* stack of nodes on path */
    dm_stack_t s[1];
    dm_stack_init(s);
    dm_dag_node_t * x = tail;
    while (x) {
      int cp;
      for (cp = 0; cp < DM_NUM_CRITICAL_PATHS; cp++) {
        cpss[cp].work += x->cpss[cp].work;
        cpss[cp].delay += x->cpss[cp].delay;
        cpss[cp].sched_delay += x->cpss[cp].sched_delay;
        int ek;
        for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
          cpss[cp].sched_delays[ek] += x->cpss[cp].sched_delays[ek];
        cpss[cp].sched_delay_nowork += x->cpss[cp].sched_delay_nowork;
        cpss[cp].sched_delay_delay += x->cpss[cp].sched_delay_delay;
      }
      dm_stack_push(s, (void *) x);
      x = x->pre;
    }

    /* compute delay along path */
    dm_critical_path_stat_t cps[1];
    cps->delay = cps->sched_delay = 0.0;
    int ek;
    for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
      cps->sched_delays[ek] = 0.0;
    cps->sched_delay_nowork = 0.0;
    cps->sched_delay_delay = 0.0;
    x = dm_stack_pop(s);
    dr_pi_dag_node * x_pi = dm_pidag_get_node_by_dag_node(D->P, x);
    dm_dag_node_t * xx = NULL;
    dr_pi_dag_node * xx_pi = NULL;
    dm_histogram_entry_t * e0 = NULL;
    dm_histogram_entry_t * e1 = first_e;
    while ( (xx = (dm_dag_node_t *) dm_stack_pop(s)) ) {
      xx_pi = dm_pidag_get_node_by_dag_node(D->P, xx);
      cps->delay += xx_pi->info.start.t - x_pi->info.end.t;
      e0 = dm_histogram_insert_entry(D->H, x_pi->info.end.t, e1);
      e1 = dm_histogram_insert_entry(D->H, xx_pi->info.start.t, e0);
      cps->sched_delay += e1->cumul_value_1 - e0->cumul_value_1;
      ek = xx_pi->info.in_edge_kind;
      cps->sched_delays[ek] += e1->cumul_value_1 - e0->cumul_value_1;
      cps->sched_delay_nowork += e1->cumul_value_3 - e0->cumul_value_3;
      cps->sched_delay_delay += e1->cumul_value_2 - e0->cumul_value_2;
      x = xx;
      x_pi = xx_pi;
    }
    cps->delay += pi->info.end.t - tail_pi->info.end.t;
    e0 = dm_histogram_insert_entry(D->H, tail_pi->info.end.t, e1);
    e1 = dm_histogram_insert_entry(D->H, pi->info.end.t, e0);
    cps->sched_delay += e1->cumul_value_1 - e0->cumul_value_1;
    ek = dr_dag_edge_kind_end;
    cps->sched_delays[ek] += e1->cumul_value_1 - e0->cumul_value_1;
    cps->sched_delay_nowork += e1->cumul_value_3 - e0->cumul_value_3;
    cps->sched_delay_delay += e1->cumul_value_2 - e0->cumul_value_2;
    int cp;
    for (cp = 0; cp < DM_NUM_CRITICAL_PATHS; cp++) {
      cpss[cp].delay += cps->delay;
      cpss[cp].sched_delay += cps->sched_delay;
      double sum = 0.0;
      for (ek = 0; ek < dr_dag_edge_kind_max; ek++) {
        cpss[cp].sched_delays[ek] += cps->sched_delays[ek];
        sum += cps->sched_delays[ek];
      }
      cpss[cp].sched_delay_nowork += cps->sched_delay_nowork;
      cpss[cp].sched_delay_delay += cps->sched_delay_delay;
      if (sum != cps->sched_delay) {
        fprintf(stderr, "Warning: sum of edge-based delays is not equal to scheduler delay: %lf <> %lf\n",
                sum, cps->sched_delay);
      }
    }
    
    /* select this cpss or not */
    if (!mostwork_tail || cpss[DM_CRITICAL_PATH_0].work > node->cpss[DM_CRITICAL_PATH_0].work) {
      node->cpss[DM_CRITICAL_PATH_0] = cpss[DM_CRITICAL_PATH_0];
      mostwork_tail = tail;
    }
    if (!lastfinished_tail || tail_pi->info.end.t >= lastfinished_t) {
      node->cpss[DM_CRITICAL_PATH_1] = cpss[DM_CRITICAL_PATH_1];
      lastfinished_tail = tail;
      lastfinished_t = tail_pi->info.end.t;
    }
    if (!mostweighted_tail ||
        (cpss[DM_CRITICAL_PATH_2].sched_delay > node->cpss[DM_CRITICAL_PATH_2].sched_delay)) {
      node->cpss[DM_CRITICAL_PATH_2] = cpss[DM_CRITICAL_PATH_2];
      mostweighted_tail = tail;
    }
    
  } /* while (tail = ..) */
  
  if (!mostwork_tail)
    fprintf(stderr, "Could not find mostwork_tail.\n");
  else if (0)
    fprintf(stderr, "mostwork_tail: %.2lf %.2lf %.2lf\n",
            node->cpss[DM_CRITICAL_PATH_0].work,
            node->cpss[DM_CRITICAL_PATH_0].delay,
            node->cpss[DM_CRITICAL_PATH_0].sched_delay);
  if (!lastfinished_tail)
    fprintf(stderr, "Could not find lastfinished_tail.\n");
  else if (0)
    fprintf(stderr, "lastfinished_tail: %.2lf %.2lf %.2lf\n",
            node->cpss[DM_CRITICAL_PATH_1].work,
            node->cpss[DM_CRITICAL_PATH_1].delay,
            node->cpss[DM_CRITICAL_PATH_1].sched_delay);
  if (!mostweighted_tail)
    fprintf(stderr, "Could not find mostweighted_tail.\n");
  else if (0)
    fprintf(stderr, "mostweighted_tail: %.2lf %.2lf %.2lf\n",
            node->cpss[DM_CRITICAL_PATH_2].work,
            node->cpss[DM_CRITICAL_PATH_2].delay,
            node->cpss[DM_CRITICAL_PATH_2].sched_delay);
  
#else
  
  while ( (tail = dm_dag_node_traverse_tails(node, tail)) ) {
    dr_pi_dag_node * tail_pi = dm_pidag_get_node_by_dag_node(D->P, tail);
    if (!lastfinished_tail || tail_pi->info.end.t >= lastfinished_t) {
      lastfinished_tail = tail;
      lastfinished_t = tail_pi->info.end.t;
    }
  }

  tail = lastfinished_tail;
  if (tail) {
    
    dm_critical_path_stat_t cpss[DM_NUM_CRITICAL_PATHS];
    memset(cpss, 0, sizeof(dm_critical_path_stat_t) * DM_NUM_CRITICAL_PATHS);
    dr_pi_dag_node * tail_pi = dm_pidag_get_node_by_dag_node(D->P, tail);

    /* stack of nodes on path */
    dm_stack_t s[1];
    dm_stack_init(s);
    dm_dag_node_t * x = tail;
    while (x) {
      int cp = DM_CRITICAL_PATH_1;
      {
        cpss[cp].work += x->cpss[cp].work;
        cpss[cp].delay += x->cpss[cp].delay;
        cpss[cp].sched_delay += x->cpss[cp].sched_delay;
        int ek;
        for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
          cpss[cp].sched_delays[ek] += x->cpss[cp].sched_delays[ek];
        cpss[cp].sched_delay_nowork += x->cpss[cp].sched_delay_nowork;
        cpss[cp].sched_delay_delay += x->cpss[cp].sched_delay_delay;
      }
      dm_stack_push(s, (void *) x);
      x = x->pre;
    }

    /* compute delay along path */
    dm_critical_path_stat_t cps[1];
    cps->delay = cps->sched_delay = 0.0;
    int ek;
    for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
      cps->sched_delays[ek] = 0.0;
    cps->sched_delay_nowork = 0.0;
    cps->sched_delay_delay = 0.0;
    x = dm_stack_pop(s);
    dr_pi_dag_node * x_pi = dm_pidag_get_node_by_dag_node(D->P, x);
    dm_dag_node_t * xx = NULL;
    dr_pi_dag_node * xx_pi = NULL;
    dm_histogram_entry_t * e0 = NULL;
    dm_histogram_entry_t * e1 = first_e;
    while ( (xx = (dm_dag_node_t *) dm_stack_pop(s)) ) {
      xx_pi = dm_pidag_get_node_by_dag_node(D->P, xx);
      cps->delay += xx_pi->info.start.t - x_pi->info.end.t;
      e0 = dm_histogram_insert_entry(D->H, x_pi->info.end.t, e1);
      e1 = dm_histogram_insert_entry(D->H, xx_pi->info.start.t, e0);
      cps->sched_delay += e1->cumul_value_1 - e0->cumul_value_1;
      ek = xx_pi->info.in_edge_kind;
      cps->sched_delays[ek] += e1->cumul_value_1 - e0->cumul_value_1;
      cps->sched_delay_nowork += e1->cumul_value_3 - e0->cumul_value_3;
      cps->sched_delay_delay += e1->cumul_value_2 - e0->cumul_value_2;
      x = xx;
      x_pi = xx_pi;
    }
    cps->delay += pi->info.end.t - tail_pi->info.end.t;
    e0 = dm_histogram_insert_entry(D->H, tail_pi->info.end.t, e1);
    e1 = dm_histogram_insert_entry(D->H, pi->info.end.t, e0);
    cps->sched_delay += e1->cumul_value_1 - e0->cumul_value_1;
    ek = dr_dag_edge_kind_end;
    cps->sched_delays[ek] += e1->cumul_value_1 - e0->cumul_value_1;
    cps->sched_delay_nowork += e1->cumul_value_3 - e0->cumul_value_3;
    cps->sched_delay_delay += e1->cumul_value_2 - e0->cumul_value_2;
    int cp = DM_CRITICAL_PATH_1;
    {
      cpss[cp].delay += cps->delay;
      cpss[cp].sched_delay += cps->sched_delay;
      double sum = 0.0;
      for (ek = 0; ek < dr_dag_edge_kind_max; ek++) {
        cpss[cp].sched_delays[ek] += cps->sched_delays[ek];
        sum += cps->sched_delays[ek];
      }
      cpss[cp].sched_delay_nowork += cps->sched_delay_nowork;
      cpss[cp].sched_delay_delay += cps->sched_delay_delay;
      if (sum != cps->sched_delay) {
        fprintf(stderr, "Warning: sum of edge-based delays is not equal to scheduler delay: %lf <> %lf\n",
                sum, cps->sched_delay);
      }
    }
    node->cpss[DM_CRITICAL_PATH_1] = cpss[DM_CRITICAL_PATH_1];
    
  }
  
#endif
  
  /* mark nodes */
  {
    dm_dag_node_t * x;
    x = mostwork_tail;
    while (x) {
      dm_node_flag_set(x->f, DMG->oncp_flags[DM_CRITICAL_PATH_0]);
      x = x->pre;
    }
    x = lastfinished_tail;
    while (x) {
      dm_node_flag_set(x->f, DMG->oncp_flags[DM_CRITICAL_PATH_1]);
      x = x->pre;
    }
    x = mostweighted_tail;
    while (x) {
      dm_node_flag_set(x->f, DMG->oncp_flags[DM_CRITICAL_PATH_2]);
      x = x->pre;
    }
  }
}

void
dm_dag_compute_critical_paths(dm_dag_t * D) {
  double time = dm_get_time();
  if (DMG->verbose_level >= 1) {
    fprintf(stderr, "dm_dag_compute_critical_paths()\n");
  }

  if (!D->critical_paths_computed) {
    /* prepare D & H */
    dm_dag_build_inner_all(D);
    dm_histogram_t * H_bk = D->H;
    D->H = dm_malloc( sizeof(dm_histogram_t) );
    dm_histogram_init(D->H);
    D->H->D = D;
    D->H->min_entry_interval = 0.0;
    dm_histogram_build_all(D->H);
    dm_histogram_compute_significant_intervals(D->H);

    /* compute recursively */
#if 0    
    dm_node_flag_set(D->rt->f, DMG->oncp_flags[DM_CRITICAL_PATH_0]);
    dm_node_flag_set(D->rt->f, DMG->oncp_flags[DM_CRITICAL_PATH_1]);
    dm_node_flag_set(D->rt->f, DMG->oncp_flags[DM_CRITICAL_PATH_2]);
#else    
    dm_node_flag_set(D->rt->f, DMG->oncp_flags[DM_CRITICAL_PATH_1]);
#endif    
    dm_dag_compute_critical_paths_r(D, D->rt, NULL);

    /* finish */
    dm_histogram_fini(D->H);
    dm_free(D->H, sizeof(dm_histogram_t));
    D->H = H_bk;
    D->critical_paths_computed = 1;
  }

  /* output */
#if 0  
  int cp;
  for (cp = 0; cp < DM_NUM_CRITICAL_PATHS; cp++) {
    printf("DAG %ld (%.0lf) (cp %d): %.2lf %.2lf %.2lf(%.1lf%%)",
           D - DMG->D,
           D->et - D->bt,
           cp,
           D->rt->cpss[cp].work,
           D->rt->cpss[cp].delay,
           D->rt->cpss[cp].sched_delay,
           D->rt->cpss[cp].sched_delay / D->rt->cpss[cp].delay * 100.0);
    printf(" (edge-kind-based:");
    int ek;
    for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
      printf(" %.1lf%%", D->rt->cpss[cp].sched_delays[ek] / D->rt->cpss[cp].sched_delay * 100);
    printf(")\n");
  }
#else
  int cp = DM_CRITICAL_PATH_1;
  {
    printf("DAG %ld (%.0lf) (cp %d): %.2lf %.2lf %.2lf(%.1lf%%)",
           D - DMG->D,
           D->et - D->bt,
           cp,
           D->rt->cpss[cp].work,
           D->rt->cpss[cp].delay,
           D->rt->cpss[cp].sched_delay,
           D->rt->cpss[cp].sched_delay / D->rt->cpss[cp].delay * 100.0);
    printf(" (edge-kind-based:");
    int ek;
    for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
      printf(" %.1lf%%", D->rt->cpss[cp].sched_delays[ek] / D->rt->cpss[cp].sched_delay * 100);
    printf(")\n");
  }
#endif  

  if (DMG->verbose_level >= 1) {
    fprintf(stderr, "... done dm_dag_compute_critical_paths(): %lf\n", dm_get_time() - time);
  }
}

void
dm_compute_dag(dm_dag_t * D) {
  int i = dm_get_dag_id(D);

  /* calculate */
  /* t1, delay, no-work */
  dr_pi_dag * G = D->P->G;
  dr_basic_stat bs[1];
  dr_basic_stat_init(bs, G);
  dr_calc_inner_delay(bs, G);
  dr_calc_edges(bs, G);
  dr_pi_dag_chronological_traverse(G, (chronological_traverser *)bs);
  dr_clock_t work = bs->total_t_1;
  dr_clock_t delay = bs->cum_delay + (bs->total_elapsed - bs->total_t_1);
  dr_clock_t no_work = bs->cum_no_work;
  DMG->SBG->work[i] = work;
  DMG->SBG->delay[i] = delay;
  DMG->SBG->nowork[i] = no_work;
  /* critical path */
  //dm_dag_compute_critical_paths(D);  
}

dm_dag_t *
dm_compute_dag_file(char * filename) {
  dm_dag_t * D = dm_add_dag(filename);
  dm_compute_dag(D);
  return D;
}

/***** end of Compute *****/


/***** Chronological Traverser *****/

_unused_ void
dr_calc_inner_delay(dr_basic_stat * bs, dr_pi_dag * G) {
  long n = G->n;
  long i;
  dr_clock_t total_elapsed = 0;
  dr_clock_t total_t_1 = 0;
  dr_pi_dag_node * T = G->T;
  long n_negative_inner_delays = 0;
  for (i = 0; i < n; i++) {
    dr_pi_dag_node * t = &T[i];
    dr_clock_t t_1 = t->info.t_1;
    dr_clock_t elapsed = t->info.end.t - t->info.start.t;
    if (t->info.kind < dr_dag_node_kind_section
	|| t->subgraphs_begin_offset == t->subgraphs_end_offset) {
      total_elapsed += elapsed;
      total_t_1 += t_1;
      if (elapsed < t_1 && t->info.worker != -1) {
	if (1 || (n_negative_inner_delays == 0)) {
	  fprintf(stderr,
		  "warning: node %ld has negative"
		  " inner delay (worker=%d, start=%llu, end=%llu,"
		  " t_1=%llu, end - start - t_1 = -%llu\n",
		  i, t->info.worker,
		  t->info.start.t, t->info.end.t, t->info.t_1,
		  t_1 - elapsed);
	}
	n_negative_inner_delays++;
      }
    }
  }
  if (n_negative_inner_delays > 0) {
    fprintf(stderr,
	    "warning: there are %ld nodes that have negative delays",
	    n_negative_inner_delays);
  }
  bs->total_elapsed = total_elapsed;
  bs->total_t_1 = total_t_1;
}

_unused_ void
dr_calc_edges(dr_basic_stat * bs, dr_pi_dag * G) {
  long n = G->n;
  long m = G->m;
  long nw = G->num_workers;
  /* C : a three dimensional array
     C(kind,i,j) is the number of type k edges from 
     worker i to worker j.
     we may counter nodes with worker id = -1
     (executed by more than one workers);
     we use worker id = nw for such entries
  */
  long * C_ = (long *)dr_malloc(sizeof(long) * dr_dag_edge_kind_max * (nw + 1) * (nw + 1));
#define EDGE_COUNTS(k,i,j) C_[k*(nw+1)*(nw+1)+i*(nw+1)+j]
  dr_dag_edge_kind_t k;
  long i, j;
  for (k = 0; k < dr_dag_edge_kind_max; k++) {
    for (i = 0; i < nw + 1; i++) {
      for (j = 0; j < nw + 1; j++) {
	EDGE_COUNTS(k,i,j) = 0;
      }
    }
  }
  for (i = 0; i < n; i++) {
    dr_pi_dag_node * t = &G->T[i];
    if (t->info.kind >= dr_dag_node_kind_section
	&& t->subgraphs_begin_offset == t->subgraphs_end_offset) {
      for (k = 0; k < dr_dag_edge_kind_max; k++) {
	int w = t->info.worker;
	if (w == -1) {
#if 0
	  fprintf(stderr, 
		  "warning: node %ld (kind=%s) has worker = %d)\n",
		  i, dr_dag_node_kind_to_str(t->info.kind), w);
#endif
	  EDGE_COUNTS(k, nw, nw) += t->info.logical_edge_counts[k];
	} else {
	  (void)dr_check(w >= 0);
	  (void)dr_check(w < nw);
	  EDGE_COUNTS(k, w, w) += t->info.logical_edge_counts[k];
	}
      }
    }    
  }
  for (i = 0; i < m; i++) {
    dr_pi_dag_edge * e = &G->E[i];
    int uw = G->T[e->u].info.worker;
    int vw = G->T[e->v].info.worker;
    if (uw == -1) {
#if 0
      fprintf(stderr, "warning: source node (%ld) of edge %ld %ld (kind=%s) -> %ld (kind=%s) has worker = %d\n",
	      e->u,
	      i, 
	      e->u, dr_dag_node_kind_to_str(G->T[e->u].info.kind), 
	      e->v, dr_dag_node_kind_to_str(G->T[e->v].info.kind), uw);
#endif
      uw = nw;
    }
    if (vw == -1) {
#if 0
      fprintf(stderr, "warning: dest node (%ld) of edge %ld %ld (kind=%s) -> %ld (kind=%s) has worker = %d\n",
	      e->v,
	      i, 
	      e->u, dr_dag_node_kind_to_str(G->T[e->u].info.kind), 
	      e->v, dr_dag_node_kind_to_str(G->T[e->v].info.kind), vw);
#endif
      vw = nw;
    }
    (void)dr_check(uw >= 0);
    (void)dr_check(uw <= nw);
    (void)dr_check(vw >= 0);
    (void)dr_check(vw <= nw);
    EDGE_COUNTS(e->kind, uw, vw)++;
  }
#undef EDGE_COUNTS
  bs->edge_counts = C_;
}

_unused_ void 
dr_basic_stat_init(dr_basic_stat * bs, dr_pi_dag * G) {
  bs->process_event = dr_basic_stat_process_event;
  bs->G = G;
  bs->n_running = 0;
  bs->n_ready = 0;
  bs->n_workers = G->num_workers;
  bs->cum_running = 0;		/* cumulative running cpu time */
  bs->cum_delay = 0;		/* cumulative delay cpu time */
  bs->cum_no_work = 0;		/* cumulative no_work cpu time */
  bs->t = 0;			/* time of the last event */
}

_unused_ void 
dr_basic_stat_process_event(chronological_traverser * ct, 
			    dr_event evt) {
  dr_basic_stat * bs = (dr_basic_stat *)ct;
  dr_clock_t dt = evt.t - bs->t;

  int n_running = bs->n_running;
  int n_delay, n_no_work;
  if (bs->n_running >= bs->n_workers) {
    /* great, all workers are running */
    n_delay = 0;
    n_no_work = 0;
    if (bs->n_running > bs->n_workers) {
      fprintf(stderr, 
	      "warning: n_running = %ld"
	      " > n_workers = %ld (clock skew?)\n",
	      bs->n_running, bs->n_workers);
    }
    n_delay = 0;
    n_no_work = 0;
  } else if (bs->n_running + bs->n_ready <= bs->n_workers) {
    /* there were enough workers to run ALL ready tasks */
    n_delay = bs->n_ready;
    n_no_work = bs->n_workers - (bs->n_running + bs->n_ready);
  } else {
    n_delay = bs->n_workers - bs->n_running;
    n_no_work = 0;
  }
  bs->cum_running += n_running * dt;
  bs->cum_delay   += n_delay * dt;
  bs->cum_no_work += n_no_work * dt;

  switch (evt.kind) {
  case dr_event_kind_ready: {
    bs->n_ready++;
    break;
  }
  case dr_event_kind_start: {
    bs->n_running++;
    break;
  }
  case dr_event_kind_last_start: {
    bs->n_ready--;
    break;
  }
  case dr_event_kind_end: {
    bs->n_running--;
    break;
  }
  default:
    assert(0);
    break;
  }

  bs->t = evt.t;
}

/***** end of Chronological Traverser *****/


/***** Layout DAG *****/

static void
dm_do_expanding_one_1(dm_dag_t * D, dm_dag_node_t * node) {
  if (!dm_is_inner_loaded(node))
    if (dm_dag_build_node_inner(D, node) != DM_OK) return;
  /* no animation */
  if (dm_is_shrinking(node)) {
    dm_node_flag_remove(node->f, DM_NODE_FLAG_SHRINKING);
  } else if (dm_is_expanding(node)) {
    dm_node_flag_remove(node->f, DM_NODE_FLAG_EXPANDING);
  }
  dm_node_flag_remove(node->f, DM_NODE_FLAG_SHRINKED);
  /* Histogram */
  if (D->H && D->H->head_e) {
    dm_histogram_remove_node(D->H, node, NULL);
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_children(node, x)) ) {
      dm_histogram_add_node(D->H, x, NULL);
    }
  }
}

static void
dm_do_expanding_one_r(dm_dag_t * D, dm_dag_node_t * node) {
  if (!dm_is_set(node))
    dm_dag_node_set(D, node);
  if (dm_is_union(node)) {
    if ((!dm_is_inner_loaded(node)
         || dm_is_shrinked(node)
         || dm_is_shrinking(node))
        && !dm_is_expanding(node)) {
      // expand node
      dm_do_expanding_one_1(D, node);
    } else {
      /* Call inward */
      dm_check(node->head);
      dm_do_expanding_one_r(D, node->head);
    }
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    dm_do_expanding_one_r(D, x);
  }
}

void
dm_do_expanding_one(dm_dag_t * D) {
  dm_do_expanding_one_r(D, D->rt);
}

static void
dm_do_collapsing_one_1(dm_dag_t * D, dm_dag_node_t * node) {
  /* no animation */
  if (dm_is_expanding(node)) {
    dm_node_flag_remove(node->f, DM_NODE_FLAG_EXPANDING);
  } else if (dm_is_shrinking(node)) {
    dm_node_flag_remove(node->f, DM_NODE_FLAG_SHRINKING);
  }
  dm_node_flag_set(node->f, DM_NODE_FLAG_SHRINKED);
  /* Histogram */
  if (D->H && D->H->head_e) {
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_children(node, x)) ) {
      dm_histogram_remove_node(D->H, x, NULL);
    }
    dm_histogram_add_node(D->H, node, NULL);
  }
}

_static_unused_ void
dm_do_collapsing_one_r(dm_dag_t * D, dm_dag_node_t * node) {
  if (!dm_is_set(node))
    return;
  if (dm_is_union(node) && dm_is_inner_loaded(node)
      && !dm_is_shrinking(node)
      && (dm_is_expanded(node) || dm_is_expanding(node))) {
    // check if node has expanded node, excluding shrinking nodes
    int has_expanded_node = 0;
    /* Traverse all children */
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_children(node, x)) ) {
      if (dm_is_union(x) && dm_is_inner_loaded(x)
          && (dm_is_expanded(x) || dm_is_expanding(x))
          && !dm_is_shrinking(x)) {
        has_expanded_node = 1;
        break;
      }
    }
    if (!has_expanded_node) {
      // collapsing node's parent
      dm_do_collapsing_one_1(D, node);
    } else {
      /* Call inward */
      dm_do_collapsing_one_r(D, node->head);
    }
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    dm_do_collapsing_one_r(D, x);
  }
}

static void
dm_do_collapsing_one_depth_r(dm_dag_t * D, dm_dag_node_t * node, int depth) {
  if (!dm_is_set(node))
    return;
  if (dm_is_union(node) && dm_is_inner_loaded(node)
      && !dm_is_shrinking(node)
      && (dm_is_expanded(node) || dm_is_expanding(node))) {
    // check if node has expanded node, excluding shrinking nodes
    int has_expanded_node = 0;
    /* Traverse all children */
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_children(node, x)) ) {
      if (dm_is_union(x) && dm_is_inner_loaded(x)
          && (dm_is_expanded(x) || dm_is_expanding(x))
          && !dm_is_shrinking(x)) {
        has_expanded_node = 1;
        break;
      }
    }
    if (!has_expanded_node && node->d >= depth) {
      // collapsing node's parent
      dm_do_collapsing_one_1(D, node);
    } else {
      /* Call inward */
      dm_do_collapsing_one_depth_r(D, node->head, depth);
    }
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    dm_do_collapsing_one_depth_r(D, x, depth);
  }
}

void
dm_do_collapsing_one(dm_dag_t * D) {
  dm_do_collapsing_one_depth_r(D, D->rt, D->collapsing_d - 1);
}

double
dm_calculate_animation_rate(dm_dag_t * D, dm_dag_node_t * node) {
  double rate = 1.0;
  if (!node) return rate;
  double ratio = (dm_get_time() - node->started) / D->anim->duration;
  if (ratio > 1.0)
    ratio = 1.0;
  if (dm_is_shrinking(node)) {
    //rate = 1.0 - ratio;
    rate = (1.0 - ratio) * (1.0 - ratio);
  } else if (dm_is_expanding(node)) {
    //rate = ratio;
    rate = 1.0 - (1.0 - ratio) * (1.0 - ratio);
  }
  return rate;
}

double
dm_calculate_animation_reverse_rate(dm_dag_t * D, dm_dag_node_t * node) {
  double ret = 0.0;
  if (!node) return ret;
  double rate = dm_calculate_animation_rate(D, node);
  if (dm_is_shrinking(node)) {
    ret = 1.0 - sqrt(1.0 - rate);
  } else if (dm_is_expanding(node)) {
    ret = 1.0 - sqrt(rate);
  }
  return ret;
}

void
dm_animation_init(dm_animation_t * a, dm_dag_t * D) {
  memset(a, 0, sizeof(dm_animation_t));
  a->on = 0;
  a->duration = DM_ANIMATION_DURATION;
  a->step = DM_ANIMATION_STEP;
  dm_llist_init(a->movings);
  a->D = D;
}

void
dm_animation_tick(dm_animation_t * a) {
  dm_check(a->on);
  double cur_t = dm_get_time();
  dm_dag_node_t * node = NULL;
  /* iterate moving nodes */
  dm_llist_cell_t * c = a->movings->top;
  while (c) {
    node = (dm_dag_node_t *) c->item;
    c = c->next;
    if (cur_t - node->started >= a->duration) {
      dm_animation_remove_node(a, node);
      if (dm_is_shrinking(node)) {
        dm_node_flag_remove(node->f, DM_NODE_FLAG_SHRINKING);
        dm_node_flag_set(node->f, DM_NODE_FLAG_SHRINKED);
      } else if (dm_is_expanding(node)) {
        dm_node_flag_remove(node->f, DM_NODE_FLAG_EXPANDING);
        dm_node_flag_remove(node->f, DM_NODE_FLAG_SHRINKED);
      }
    }
  }
  /* stop animation when there is no moving node */
  if (dm_llist_size(a->movings) == 0) {
    a->on = 0;
  }
}

void
dm_animation_add_node(dm_animation_t * a, dm_dag_node_t * node) {
  double cur_t = dm_get_time();
  node->started = cur_t;
  dm_llist_add(a->movings, node);
  a->on = 1;
}

void
dm_animation_remove_node(dm_animation_t * a, dm_dag_node_t * node) {
  dm_llist_remove(a->movings, node);
}

void
dm_animation_reverse_node(dm_animation_t * a, dm_dag_node_t * node) {
  dm_llist_remove(a->movings, node);
  double cur_t = dm_get_time();
  //node->started = 2 * cur - a->duration - node->started;
  node->started = cur_t - a->duration * dm_calculate_animation_reverse_rate(a->D, node);
  dm_llist_add(a->movings, node);
}

double
dm_get_alpha_fading_out(dm_dag_t * D, dm_dag_node_t * node) {
  double ratio = (dm_get_time() - node->started) / D->anim->duration;
  double ret;
  //ret = (1.0 - ratio) * 0.75;
  ret = 1.0 - ratio * ratio;
  return ret;
}

double
dm_get_alpha_fading_in(dm_dag_t * D, dm_dag_node_t * node) {
  double ratio = (dm_get_time() - node->started) / D->anim->duration;
  double ret;
  //ret = ratio * 1.5;
  ret = ratio * ratio;
  return ret;
}

static void
dm_motion_reset(dm_motion_t * m) {
  memset(m, 0, sizeof(dm_motion_t));
  m->target_pii = -1;
}

void
dm_motion_init(dm_motion_t * m, dm_dag_t * D) {
  dm_motion_reset(m);
  m->on = 0;
  m->duration = DM_ANIMATION_DURATION;
  m->step = DM_ANIMATION_STEP;
  m->D = D;
}

static dm_dag_node_t *
dm_dag_find_node_r(_unused_ dm_dag_t * D, double x, double y, dm_dag_node_t * node, int cid) {
  dm_dag_node_t * ret = NULL;
  dm_node_coordinate_t * c = &node->c[cid];
  /* Call inward */
  if (dm_is_single(node)) {
    if (c->x - c->lw < x && x < c->x + c->rw && c->y < y && y < c->y + c->dw) {
      return node;
    }
  } else if (c->x - c->lw < x && x < c->x + c->rw && c->y < y && y < c->y + c->dw) {
    ret = dm_dag_find_node_r(D, x, y, node->head, cid);
    if (ret)
      return ret;
  }
  /* Call link-along */
  if (c->x - c->link_lw < x && x < c->x + c->link_rw && c->y < y && y < c->y + c->link_dw) {
    dm_dag_node_t * next = NULL;
    while ( (next = dm_dag_node_traverse_nexts(node, next)) ) {
      ret = dm_dag_find_node_r(D, x, y, next, cid);
      if (ret)
        return ret;
    }
  }
  return NULL;
}

static dm_dag_node_t *
dm_dag_find_timelines_node_r(_unused_ dm_dag_t * D, double cx, double cy, dm_dag_node_t * node, int cid) {
  dm_dag_node_t * ret = NULL;
  dm_node_coordinate_t * c = &node->c[cid];
  /* Call inward */
  double x, y, w, h;
  x = c->x;
  y = c->y;
  w = c->rw;
  h = c->dw;
  if (x <= cx && cx <= x + w && y <= cy && cy <= y + h) {
    if (dm_is_single(node)) {
      ret = node;
    } else {
      ret = dm_dag_find_timelines_node_r(D, cx, cy, node->head, cid);
    }
  } else if (cx > x + w) {
    /* Call link-along */
    dm_dag_node_t * next = NULL;
    while ( (next = dm_dag_node_traverse_nexts(node, next)) ) {
      ret = dm_dag_find_timelines_node_r(D, cx, cy, next, cid);
      if (ret) break;
    }
  }
  return ret;
}

dm_dag_node_t *
dm_dag_find_node(dm_dag_t * D, double x, double y, int cid) {
  dm_dag_node_t * ret = NULL;
  switch (cid) {
  case DM_LAYOUT_DAG_COORDINATE:
  case DM_LAYOUT_DAG_BOX_LINEAR_COORDINATE:
  case DM_LAYOUT_DAG_BOX_POWER_COORDINATE:
  case DM_LAYOUT_DAG_BOX_LOG_COORDINATE:
    ret = dm_dag_find_node_r(D, x, y, D->rt, cid);
    break;
  case DM_LAYOUT_PARAPROF_COORDINATE:
    ret = dm_dag_find_timelines_node_r(D, x, y, D->rt, cid);
    break;
  default:
    dm_perror("undefined cid=%d\n", cid);
  }
  return ret;
}

static dm_dag_node_t *
dm_dag_find_node_with_pi_index_r(dm_dag_t * D, long pii, dm_dag_node_t * node) {
  if (node->pii == pii)
    return node;
  dm_dag_node_t * ret = NULL;
  /* Call inward */
  if (dm_is_union(node) && dm_is_inner_loaded(node)
      && !dm_is_shrinking(node)
      && (dm_is_expanded(node) || dm_is_expanding(node))) {
    ret = dm_dag_find_node_with_pi_index_r(D, pii, node->head);
    if (ret)
      return ret;
  }
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    ret = dm_dag_find_node_with_pi_index_r(D, pii, x);
    if (ret)
      return ret;
  }
  return NULL;
}

dm_dag_node_t *
dm_dag_find_node_with_pi_index(dm_dag_t * D, long pii) {
  return dm_dag_find_node_with_pi_index_r(D, pii, D->rt);
}

static void
dm_dag_layout1_node_phase1(dm_dag_t * D, dm_dag_node_t * node, int cid) {
  if (dm_is_shrinking(node) && (D->collapsing_d == 0 || node->d < D->collapsing_d))
    D->collapsing_d = node->d;
  dm_node_coordinate_t * node_c = &node->c[cid];
  
  /* Calculate inward */
  if (dm_is_inward_callable(node)) {
    dm_check(node->head);
    // node's head's outward
    dm_node_coordinate_t * head_c = &node->head->c[cid];
    head_c->xpre = 0.0;
    head_c->y = node_c->y;
    // Recursive call
    dm_dag_layout1_node_phase1(D, node->head, cid);
    // node's inward
    node_c->lw = head_c->link_lw;
    node_c->rw = head_c->link_rw;
    node_c->dw = head_c->link_dw;
  } else {
    // node's inward
    node_c->lw = D->radius;
    node_c->rw = D->radius;
    node_c->dw = 2 * D->radius;
  }
    
  /* Calculate link-along */
  dm_dag_node_t * u, * v; // linked nodes
  dm_node_coordinate_t * u_c, * v_c;
  double rate, ypre, hgap;
  switch ( dm_dag_node_count_nexts(node) ) {
  case 0:
    // node's link-along
    node_c->link_lw = node_c->lw;
    node_c->link_rw = node_c->rw;
    node_c->link_dw = node_c->dw;
    break;
  case 1:
    u = node->next;
    u_c = &u->c[cid];
    // node & u's rate
    rate = dm_calculate_animation_rate(D, node->parent);
    // node's linked u's outward
    u_c->xpre = 0.0;
    ypre = (node_c->dw - 2 * D->radius + DMG->opts.vnd) * rate;
    u_c->y = node_c->y + ypre;
    // Recursive call
    dm_dag_layout1_node_phase1(D, u, cid);
    // node's link-along
    node_c->link_lw = dm_max(node_c->lw, u_c->link_lw);
    node_c->link_rw = dm_max(node_c->rw, u_c->link_rw);
    node_c->link_dw = ypre + u_c->link_dw;
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    u_c = &u->c[cid];
    v_c = &v->c[cid];
    // node & u,v's rate
    rate = dm_calculate_animation_rate(D, node->parent);
    // node's linked u,v's outward
    ypre = (node_c->dw - 2 * D->radius + DMG->opts.vnd) * rate;
    u_c->y = node_c->y + ypre;
    v_c->y = node_c->y + ypre;
    // Recursive call
    dm_dag_layout1_node_phase1(D, u, cid);
    dm_dag_layout1_node_phase1(D, v, cid);
    
    // node's linked u,v's outward
    hgap = DMG->opts.hnd * rate;
    // u
    u_c->xpre = (u_c->link_lw - D->radius) + hgap;
    if (u->spawn)
      u_c->xpre = - u->spawn->c[cid].xpre;
    // v
    v_c->xpre = (v_c->link_rw - D->radius) + hgap;
    double left_push = 0.0;
    if (u->spawn)
      left_push = (u_c->link_lw - D->radius) - u_c->xpre;
    if (left_push > 0)
      v_c->xpre += left_push;
    v_c->xpre = - v_c->xpre;
    
    // node's link-along
    node_c->link_lw = - v_c->xpre + v_c->link_lw;
    node_c->link_rw = u_c->xpre + u_c->link_rw;
    node_c->link_dw = ypre + dm_max(u_c->link_dw, v_c->link_dw);
    break;
  default:
    dm_check(0);
    break;
  }  
}

static void
dm_dag_layout1_node_phase2(dm_dag_t * D, dm_dag_node_t * node, int cid) {
  dm_node_coordinate_t * node_c = &node->c[cid];
  /* Calculate inward */
  if (dm_is_inward_callable(node)) {
    dm_check(node->head);
    // node's head's outward
    dm_node_coordinate_t * head_c = &node->head->c[cid];
    head_c->xp = 0.0;
    head_c->x = node_c->x;
    // Recursive call
    dm_dag_layout1_node_phase2(D, node->head, cid);
  }
    
  /* Calculate link-along */
  dm_dag_node_t * u, * v; // linked nodes
  dm_node_coordinate_t * u_c, * v_c;
  switch ( dm_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    u_c = &u->c[cid];
    // node's linked u's outward
    u_c->xp = u_c->xpre + node_c->xp;
    u_c->x = u_c->xp + u->parent->c[cid].x;
    // Recursive call
    dm_dag_layout1_node_phase2(D, u, cid);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    u_c = &u->c[cid];
    v_c = &v->c[cid];
    // node's linked u,v's outward
    u_c->xp = u_c->xpre + node_c->xp;
    u_c->x = u_c->xp + u->parent->c[cid].x;
    v_c->xp = v_c->xpre + node_c->xp;
    v_c->x = v_c->xp + v->parent->c[cid].x;
    // Recursive call
    dm_dag_layout1_node_phase2(D, u, cid);
    dm_dag_layout1_node_phase2(D, v, cid);
    break;
  default:
    dm_check(0);
    break;
  }
}

void
dm_dag_layout1(dm_dag_t * D, int cid) {
  dm_node_coordinate_t * rt_c = &D->rt->c[cid];

  /* phase 1: calculate relative coordinates */
  D->collapsing_d = 0;
  rt_c->xpre = 0.0; /* predecessor-based */
  rt_c->y = 0.0;
  dm_dag_layout1_node_phase1(D, D->rt, cid);

  /* phase 2: calculate absolute coordinates */
  rt_c->xp = 0.0; /* parent-based */
  rt_c->x = 0.0;
  dm_dag_layout1_node_phase2(D, D->rt, cid);
}

double
dm_dag_layout_scale_down(dm_dag_t * D, double val, int cid) {
  double ret;
  switch (cid) {
  case DM_LAYOUT_DAG_BOX_LINEAR_COORDINATE:
  case DM_LAYOUT_PARAPROF_COORDINATE:
    ret = val / D->linear_radix;
    break;
  case DM_LAYOUT_DAG_BOX_POWER_COORDINATE:
    ret = pow(val, D->power_radix);
    break;
  case DM_LAYOUT_DAG_BOX_LOG_COORDINATE:
    ret = log(val) / log(D->log_radix);
    break;
  default:
    dm_perror("undefined cid=%d\n", cid);
    ret = val;
    break;
  }
  return ret;
}

static double
dm_dag_calculate_vgap(dm_dag_t * D, dm_dag_node_t * parent, dm_dag_node_t * node1, dm_dag_node_t * node2, int cid) {
  dr_pi_dag_node * pi1 = dm_pidag_get_node_by_dag_node(D->P, node1);
  dr_pi_dag_node * pi2 = dm_pidag_get_node_by_dag_node(D->P, node2);
  double rate = dm_calculate_animation_rate(D, parent);
  double vgap;
  if (!D->frombt) {
    // begin - end
    double time_gap = dm_dag_layout_scale_down(D, pi2->info.start.t - pi1->info.end.t, cid);
    vgap = rate * time_gap;
  } else {
    // from beginning
    double time1 = dm_dag_layout_scale_down(D, pi1->info.end.t - D->bt, cid);
    double time2 = dm_dag_layout_scale_down(D, pi2->info.start.t - D->bt, cid);
    vgap = rate * (time2 - time1);
  }
  return vgap;
}

static double
dm_dag_calculate_vsize(dm_dag_t * D, dm_dag_node_t * node, int cid) {
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
  double rate = 1.0;//dv_view_calculate_rate(V, node->parent);
  double vsize;
  if (!D->frombt) {
    // begin - end
    double time_gap = dm_dag_layout_scale_down(D, pi->info.end.t - pi->info.start.t, cid);
    vsize = rate * time_gap;
  } else {
    // from beginning
    double time1 = dm_dag_layout_scale_down(D, pi->info.start.t - D->bt, cid);
    double time2 = dm_dag_layout_scale_down(D, pi->info.end.t - D->bt, cid);
    vsize = rate * (time2 - time1);
  }
  return vsize;
}

static dm_dag_node_t *
dm_dag_layout2_get_last_finished_tail(dm_dag_t * D, dm_dag_node_t * node) {
  dm_dag_node_t * ret = 0;
  dm_dag_node_t * tail = NULL;
  while ( (tail = dm_dag_node_traverse_tails(node, tail)) ) {
    dr_pi_dag_node * ret_pi = dm_pidag_get_node_by_dag_node(D->P, ret);
    dr_pi_dag_node * tail_pi = dm_pidag_get_node_by_dag_node(D->P, tail);
    if (!ret || ret_pi->info.end.t < tail_pi->info.end.t)
      ret = tail;
  }
  return ret;
}

static double
dm_dag_layout2_get_xp_by_accumulating_xpre(_unused_ dm_dag_t * D, dm_dag_node_t * node, int cid) {
  dm_dag_node_t * u = node;
  double dis = 0.0;
  while (u) {
    dis += u->c[cid].xpre;
    u = u->pre;
  }
  return dis;
}

static double
dm_dag_layout2_get_last_tail_xp_r(dm_dag_t * D, dm_dag_node_t * node, int cid) {
  dm_check(node);
  double ret = 0.0;
  if (!dm_is_single(node)) {
    dm_dag_node_t * last = dm_dag_layout2_get_last_finished_tail(D, node);
    if (last) {
      ret += dm_dag_layout2_get_xp_by_accumulating_xpre(D, last, cid);
      ret += dm_dag_layout2_get_last_tail_xp_r(D, last, cid);
    }
  }
  return ret;
}

static void
dm_dag_layout2_node_phase1(dm_dag_t * D, dm_dag_node_t * node, int cid) {
  if (dm_is_shrinking(node) && (D->collapsing_d == 0 || node->d < D->collapsing_d))
    D->collapsing_d = node->d;
  dm_node_coordinate_t * node_c = &node->c[cid];
  
  /* Calculate inward */
  if (dm_is_inward_callable(node)) {
    dm_check(node->head);
    // node's head's outward
    dm_node_coordinate_t * head_c = &node->head->c[cid];
    head_c->xpre = 0.0;
    head_c->y = node_c->y;
    // Recursive call
    dm_dag_layout2_node_phase1(D, node->head, cid);
    // node's inward
    node_c->lw = head_c->link_lw;
    node_c->rw = head_c->link_rw;
    node_c->dw = head_c->link_dw;
    // for enhancing expand/collapse animation
    if (node_c->lw < D->radius)
      node_c->lw = D->radius;
    if (node_c->rw < D->radius)
      node_c->rw = D->radius;
    double vsize = dm_dag_calculate_vsize(D, node, cid);
    if (node_c->dw < vsize)
      node_c->dw = vsize;
  } else {
    // node's inward
    node_c->lw = D->radius;
    node_c->rw = D->radius;
    node_c->dw = dm_dag_calculate_vsize(D, node, cid);
  }
    
  /* Calculate link-along */
  dm_dag_node_t * u, * v; // linked nodes
  dm_node_coordinate_t * u_c, * v_c;
  double rate, ugap, vgap, hgap;
  switch ( dm_dag_node_count_nexts(node) ) {
  case 0:
    // node's link-along
    node_c->link_lw = node_c->lw;
    node_c->link_rw = node_c->rw;
    node_c->link_dw = node_c->dw;
    break;
  case 1:
    u = node->next;
    u_c = &u->c[cid];
    // node & u's rate
    rate = dm_calculate_animation_rate(D, node->parent);
    ugap = dm_dag_calculate_vgap(D, node->parent, node, u, cid);
    // node's linked u's outward    
    u_c->xpre = dm_dag_layout2_get_last_tail_xp_r(D, node, cid);
    u_c->y = node_c->y + node_c->dw * rate + ugap;
    // Recursive call
    dm_dag_layout2_node_phase1(D, u, cid);
    // node's link-along
    node_c->link_lw = dm_max(node_c->lw, u_c->link_lw - u_c->xpre);
    node_c->link_rw = dm_max(node_c->rw, u_c->link_rw + u_c->xpre);
    node_c->link_dw = node_c->dw * rate + ugap + u_c->link_dw;
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    u_c = &u->c[cid];
    v_c = &v->c[cid];
    // node & u,v's rate
    rate = dm_calculate_animation_rate(D, node->parent);
    ugap = dm_dag_calculate_vgap(D, node->parent, node, u, cid);
    vgap = dm_dag_calculate_vgap(D, node->parent, node, v, cid);
    // node's linked u,v's outward
    u_c->y = node_c->y + node_c->dw * rate + ugap;
    v_c->y = node_c->y + node_c->dw * rate + vgap;
    // Recursive call
    dm_dag_layout2_node_phase1(D, u, cid);
    dm_dag_layout2_node_phase1(D, v, cid);
    
    // node's linked u,v's outward
    hgap = rate * DMG->opts.hnd;
    // u
    u_c->xpre = (u_c->link_lw - D->radius) + hgap;
    if (u->spawn)
      u_c->xpre = - u->spawn->c[cid].xpre;
    // v
    v_c->xpre = (v_c->link_rw - D->radius) + hgap;
    if (u->spawn)
      v_c->xpre += (u_c->link_lw - D->radius) - u_c->xpre;
    v_c->xpre = - v_c->xpre;
    
    // node's link-along
    node_c->link_lw = - v_c->xpre + v_c->link_lw;
    node_c->link_rw = u_c->xpre + u_c->link_rw;
    node_c->link_dw = node_c->dw * rate + dm_max(ugap + u_c->link_dw, vgap + v_c->link_dw);
    break;
  default:
    dm_check(0);
    break;
  }
}

static void
dm_dag_layout2_node_phase2(dm_dag_t * D, dm_dag_node_t * node, int cid) {
  dm_node_coordinate_t * node_c = &node->c[cid];
  /* Calculate inward */
  if (dm_is_inward_callable(node)) {
    dm_check(node->head);
    // node's head's outward
    dm_node_coordinate_t * head_c = &node->head->c[cid];
    head_c->xp = 0.0;
    head_c->x = node_c->x;
    // Recursive call
    dm_dag_layout2_node_phase2(D, node->head, cid);
  }
    
  /* Calculate link-along */
  dm_dag_node_t * u, * v; // linked nodes
  dm_node_coordinate_t * u_c, * v_c;
  switch ( dm_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    u_c = &u->c[cid];
    // node's linked u's outward
    u_c->xp = u_c->xpre + node_c->xp;
    u_c->x = u_c->xp + u->parent->c[cid].x;
    // Recursive call
    dm_dag_layout2_node_phase2(D, u, cid);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    u_c = &u->c[cid];
    v_c = &v->c[cid];
    // node's linked u,v's outward
    u_c->xp = u_c->xpre + node_c->xp;
    u_c->x = u_c->xp + u->parent->c[cid].x;
    v_c->xp = v_c->xpre + node_c->xp;
    v_c->x = v_c->xp + v->parent->c[cid].x;
    // Recursive call
    dm_dag_layout2_node_phase2(D, u, cid);
    dm_dag_layout2_node_phase2(D, v, cid);
    break;
  default:
    dm_check(0);
    break;
  }  
}

void
dm_dag_layout2(dm_dag_t * D, int cid) {
  dm_node_coordinate_t * rt_c = &D->rt->c[cid];

  /* phase 1: calculate relative coordinates */
  D->collapsing_d = 0;
  rt_c->xpre = 0.0; /* predecessor-based */
  rt_c->y = 0.0;
  dm_dag_layout2_node_phase1(D, D->rt, cid);

  /* phase 2: calculate absolute coordinates */
  rt_c->xp = 0.0; /* parent-based */
  rt_c->x = 0.0;
  dm_dag_layout2_node_phase2(D, D->rt, cid);
}

static void
dm_dag_layout3_node(dm_dag_t * D, dm_dag_node_t * node, int cid) {
  if (node->d > D->collapsing_d)
    D->collapsing_d = node->d;  
  dm_node_coordinate_t * node_c = &node->c[cid];
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
  
  /* Calculate inward */
  node_c->x = dm_dag_layout_scale_down(D, pi->info.start.t - D->bt, cid);
  node_c->lw = 0.0;
  node_c->rw = dm_dag_layout_scale_down(D, pi->info.end.t - D->bt, cid) - dm_dag_layout_scale_down(D, pi->info.start.t - D->bt, cid);
  int worker = pi->info.worker;
  node_c->y = worker * (2 * D->radius);
  node_c->dw = D->radius * 2;      
  if (dm_is_union(node)) {
    if (worker < 0) {
      node_c->y = 0.0;
      node_c->dw = D->P->num_workers * (2 * D->radius);
    }
    if (dm_is_inner_loaded(node) && dm_is_expanded(node))
      dm_dag_layout3_node(D, node->head, cid);
  }
    
  /* Calculate link-along */
  dm_dag_node_t * u, * v;
  switch ( dm_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    dm_dag_layout3_node(D, u, cid);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    dm_dag_layout3_node(D, u, cid);
    dm_dag_layout3_node(D, v, cid);
    break;
  default:
    dm_check(0);
    break;
  }
}

void
dm_dag_layout3(dm_dag_t * D, int cid) {
  D->collapsing_d = 0;
  dm_dag_layout3_node(D, D->rt, cid);
}

/***** end of Layout DAG *****/
