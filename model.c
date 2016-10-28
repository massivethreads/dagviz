#include "model.h"

dv_global_state_t CS[1];

/***** PIDAG Reader's functions *****/

dv_pidag_t *
dv_pidag_read_new_file(char * filename) {
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
  if (CS->nP >= DV_MAX_DAG_FILE)
    return NULL;
  dv_pidag_t * P = &CS->P[CS->nP++];
  P->fn = filename;
  P->filename = (char *) dv_malloc( sizeof(char) * (strlen(filename) + 1) );
  strcpy(P->filename, filename);
  P->filename[strlen(filename)] = '\0';
  P->short_filename = dv_filename_get_short_name(P->filename);
  dv_llist_init(P->itl);

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
dv_pidag_get_node_id(dv_pidag_t * P, dr_pi_dag_node * pi) {
  return pi - P->T;
}

static long
dv_pidag_get_node_id_with_offset(dv_pidag_t * P, dr_pi_dag_node * pi, long offset) {
  return pi - P->T + offset;
}
  
dr_pi_dag_node *
dv_pidag_get_node_by_id(dv_pidag_t * P, long id) {
  dv_check(id >=0 && id < P->n);
  dr_pi_dag_node * ret = P->T + id;
  return ret;
}

dr_pi_dag_node *
dv_pidag_get_node_by_dag_node(dv_pidag_t * P, dv_dag_node_t * node) {
  if (!node) return NULL;
  return dv_pidag_get_node_by_id(P, node->pii);
}

static dr_pi_dag_node *
dv_pidag_get_node_by_offset(dv_pidag_t * P, dr_pi_dag_node * pi, long offset) {
  if (!pi) return NULL;
  long id = dv_pidag_get_node_id_with_offset(P, pi, offset);
  return dv_pidag_get_node_by_id(P, id);
}

/***** end of PIDAG Reader's functions *****/



/***** DAG's functions *****/

static void
dv_node_coordinate_init(dv_node_coordinate_t * c) {
  memset(c, 0, sizeof(dv_node_coordinate_t));
}

void
dv_dag_node_init(dv_dag_node_t * node, dv_dag_node_t * parent, long pii) {
  /* Reset */
  /* Keep node->next intact because it is already used for linking in node pool */
  dv_dag_node_t * t = node->next;
  memset(node, 0, sizeof(dv_dag_node_t));
  node->next = t;

  /* Initialize */
  node->pii = pii;
  dv_node_flag_init(node->f);
  node->d = (parent) ? (parent->d + 1) : 0;
  node->parent = parent;
  int i;
  for (i = 0; i < DV_NUM_LAYOUT_TYPES; i++)
    dv_node_coordinate_init(&node->c[i]);
}

int
dv_dag_node_set(dv_dag_t * D, dv_dag_node_t * node) {
  dv_pidag_t * P = D->P;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(P, node);

  if (pi->info.kind == dr_dag_node_kind_section
      || pi->info.kind == dr_dag_node_kind_task) {

    if (pi->subgraphs_begin_offset < pi->subgraphs_end_offset)
      dv_node_flag_set(node->f, DV_NODE_FLAG_UNION);
    else
      dv_node_flag_remove(node->f, DV_NODE_FLAG_UNION);
    
  } else {

    dv_node_flag_remove(node->f, DV_NODE_FLAG_UNION);
    
  }
  
  dv_node_flag_set(node->f, DV_NODE_FLAG_SET);
  dv_node_flag_remove(node->f, DV_NODE_FLAG_INNER_LOADED);

  return DV_OK;
}

int
dv_dag_build_node_inner(dv_dag_t * D, dv_dag_node_t * node) {
  dv_pidag_t * P = D->P;

  if (!dv_is_set(node))
    dv_dag_node_set(D, node);
  
  if (dv_is_union(node)) {
    
    dr_pi_dag_node * pi, * pi_a, * pi_b, * pi_x;
    dv_dag_node_t * node_a, * node_b, * node_x, * node_t;

    pi = dv_pidag_get_node_by_dag_node(P, node);
    pi_a = dv_pidag_get_node_by_offset(P, pi, pi->subgraphs_begin_offset);
    pi_b = dv_pidag_get_node_by_offset(P, pi, pi->subgraphs_end_offset - 1);
    dv_check(pi_a <= pi_b);
    
    // Check node pool for all child nodes
    long num_children;
    num_children = pi_b - pi_a + 1;
    node_a = dv_dag_node_pool_pop_contiguous(CS->pool, num_children);
    if (!node_a)
      return dv_log_set_error(DV_ERROR_OONP);
    D->n += num_children;

    // Get last node, btw check number of children
    node_b = node_a;
    long i;
    for (i = 1; i < num_children; i++) {
      dv_check(node_b);
      node_b = node_b->next;
    }
    dv_check(node_b && !node_b->next);
    
    // Initialize, set children's parent
    pi_x = pi_a;
    node_x = node_a;
    while (node_x) {
      dv_dag_node_init(node_x, node, dv_pidag_get_node_id(P, pi_x));
      dv_dag_node_set(D, node_x);
      if (node_x->d > D->dmax)
        D->dmax = node_x->d;
      pi_x++;
      node_x = dv_dag_node_get_next(node_x);
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
      pi_x = dv_pidag_get_node_by_dag_node(P, node_x);
      if (pi_x->info.kind == dr_dag_node_kind_create_task) {
        node_t = dv_dag_node_pool_pop(CS->pool);
        if (!node_t)
          return dv_log_set_error(DV_ERROR_OONP);
        dv_dag_node_init(node_t, node, dv_pidag_get_node_id_with_offset(P, pi_x, pi_x->child_offset));
        dv_dag_node_set(D, node_t);
        node_x->spawn = node_t;
        node_t->pre = node_x;
        D->n++;
      }
      node_x = node_x->next;
    }
    
    // Set node->f
    dv_node_flag_set(node->f, DV_NODE_FLAG_INNER_LOADED);
    dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
    
  }    

  return DV_OK;
}

int
dv_dag_collapse_node_inner(dv_dag_t * D, dv_dag_node_t * node) {
  (void) D;
  // Check flags node->f
  if (dv_is_set(node) && dv_is_union(node)
      && dv_is_inner_loaded(node)) {

    dv_dag_node_t * x = node->head;
    dv_dag_node_t * next;
    while (x) {
      next = x->next;
      if (x->spawn) {
        dv_check(!dv_is_union(x->spawn) || !dv_is_inner_loaded(x->spawn));
        dv_dag_node_pool_push(CS->pool, x->spawn);
      }
      dv_check(!dv_is_union(x) || !dv_is_inner_loaded(x));
      dv_dag_node_pool_push(CS->pool, x);
      x = next;
    }

    // Unset node->f
    dv_node_flag_remove(node->f, DV_NODE_FLAG_INNER_LOADED);

  }  
  return DV_OK;
}

static void
dv_dag_clear_shrinked_nodes_1(dv_dag_t * D, dv_dag_node_t * node) {
  dv_dag_collapse_node_inner(D, node);
}

static void
dv_dag_clear_shrinked_nodes_r(dv_dag_t * D, dv_dag_node_t * node) {
  if (!dv_is_set(node)) return;
  
  if (dv_is_union(node) && dv_is_inner_loaded(node)) {

    if (dv_is_shrinked(node)
        && !dv_is_shrinking(node)
        && !dv_is_expanding(node)) {
      // check if node has inner_loaded node
      int has_no_innerloaded_node = 1;
      dv_dag_node_t * x = NULL;
      while ( (x = dv_dag_node_traverse_children(node, x)) ) {
        if (dv_is_set(x) && dv_is_union(x) && dv_is_inner_loaded(x)) {
          has_no_innerloaded_node = 0;
          break;
        }
      }
      if (has_no_innerloaded_node) {
        // collapse inner
        dv_dag_clear_shrinked_nodes_1(D, node);
      } else {
        /* Call inward */
        dv_dag_clear_shrinked_nodes_r(D, node->head);
      }
      
    } else {
      /* Call inward */
      dv_dag_clear_shrinked_nodes_r(D, node->head);      
    }
    
  }
  
  /* Call link-along */
  dv_dag_node_t * x = NULL;
  while ( (x = dv_dag_node_traverse_nexts(node, x)) ) {
    dv_dag_clear_shrinked_nodes_r(D, x);
  }
}

void
dv_dag_clear_shrinked_nodes(dv_dag_t * D) {
  dv_dag_clear_shrinked_nodes_r(D, D->rt);
}

static char *
dv_get_component_from_string(char * s, int n) {
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
    char * ret = (char *) dv_malloc( sizeof(char) * (ii - i + 1) );
    unsigned int k;
    for (k = 0; k < ii - i; k++)
      ret[k] = s[k + i];
    //strncpy(ret, s + i, ii - i);
    ret[ii-i] = '\0';
    return ret;
  }
  return NULL;
}

static char *
dv_get_distinct_components_name_string(char * name) {
  char str[100] = "";
  int i = 1;
  while (1) {
    int to_get_this_component = 0;
    char * com1 = dv_get_component_from_string(name, i);
    if (!com1) break;
    int j;
    for (j = 0; j < CS->nP; j++) {
      char * com2 = dv_get_component_from_string(CS->P[j].short_filename, i);
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
        dv_free(com2, sizeof(char) * (strlen(com2) + 1));
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
      dv_free(com1, sizeof(char) * (strlen(com1) + 1));
    i++;
  }
  char * ret = (char *) dv_malloc( sizeof(char) * (strlen(str) + 1) );
  strcpy(ret, str);
  ret[strlen(str)] = '\0';
  return ret;
}

void
dv_dag_init(dv_dag_t * D, dv_pidag_t * P) {
  memset(D, 0, sizeof(dv_dag_t));
  char str[10];
  sprintf(str, "DAG %ld", D - CS->D);
  D->name = malloc( sizeof(char) * (strlen(str) + 1) );
  strcpy(D->name, str);
  D->name[strlen(str)] = '\0';
  D->name_on_graph = dv_get_distinct_components_name_string(P->short_filename);
  D->P = P;
  D->rt = dv_dag_node_pool_pop(CS->pool);
  dv_dag_node_init(D->rt, 0, 0);
  if (P)
    dv_dag_node_set(D, D->rt);
  D->dmax = 0;
  D->bt = 0.0;
  D->et = 0.0;
  D->n = 1;
  D->cur_d = 0;
  D->cur_d_ex = 0;
  D->collapsing_d = 0;
  D->sdt = DV_SCALE_TYPE_INIT;
  D->log_radix = DV_RADIX_LOG;
  D->power_radix = DV_RADIX_POWER;
  D->linear_radix = DV_RADIX_LINEAR;
  D->frombt = DV_FROMBT_INIT;
  D->radius = CS->opts.radius;

  /* D <-> Vs */
  int i;
  for (i = 0; i < DV_MAX_VIEW; i++)
    D->mV[i] = 0;
  
  dv_llist_init(D->itl);
  D->H = NULL;
  for (i=0; i<DV_NUM_LAYOUT_TYPES; i++)
    D->nviews[i] = 0;
  D->nr = 0;

  D->draw_with_current_time = 0;
  D->current_time = 0.0;
  D->time_step = 1000;
  for (i = 0; i < DV_NUM_CRITICAL_PATHS; i++) {
    D->show_critical_paths[i] = 0;
  }
  D->critical_paths_computed = 0;
}

dv_dag_t *
dv_dag_create_new_with_pidag(dv_pidag_t * P) {
  /* Get new DAG */
  dv_check(CS->nD < DV_MAX_DAG);
  dv_dag_t * D = &CS->D[CS->nD++];
  dv_dag_init(D, P);

  /* Set values */
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(P, D->rt);
  D->bt = pi->info.first_ready_t;
  D->et = pi->info.end.t;
  
  return D;
}

double
dv_dag_get_radix(dv_dag_t * D) {
  double radix = 0.0;
  switch (D->sdt) {
  case 0:
    radix = D->log_radix;
    break;
  case 1:
    radix = D->power_radix;
    break;
  case 2:
    radix = D->linear_radix;
    break;
  default:
    dv_check(0);
  }
  return radix;
}

void
dv_dag_set_radix(dv_dag_t * D, double radix) {
  switch (D->sdt) {
  case 0:
    D->log_radix = radix;
    break;
  case 1:
    D->power_radix = radix;
    break;
  case 2:
    D->linear_radix = radix;
    break;
  default:
    dv_check(0);
  }
}

int
dv_pidag_node_lookup_value(dr_pi_dag_node * pi, int nc) {
  int v = 0;
  dv_check(nc < DV_NUM_COLOR_POOLS);
  switch (nc) {
  case 0:
    v = pi->info.worker;
    break;
  case 1:
    v = pi->info.cpu;
    break;
  case 2:
    v = (int) pi->info.kind;
    v += 10;
    break;
  case 3:
    v = dv_get_color_pool_index(nc, 0, 0, pi->info.start.pos.file_idx, pi->info.start.pos.line);
    break;
  case 4:
    v = dv_get_color_pool_index(nc, 0, 0, pi->info.end.pos.file_idx, pi->info.end.pos.line);
    break;
  case 5:
    v = dv_get_color_pool_index(nc, pi->info.start.pos.file_idx, pi->info.start.pos.line, pi->info.end.pos.file_idx, pi->info.end.pos.line);
    break;
  default:
    dv_check(0);
    break;
  }
  return v;
}

int
dv_dag_node_lookup_value(dv_dag_t * D, dv_dag_node_t * node, int nc) {
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  return dv_pidag_node_lookup_value(pi, nc);
}

dv_dag_node_t *
dv_dag_node_traverse_children(dv_dag_node_t * node, dv_dag_node_t * cur) {
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

dv_dag_node_t *
dv_dag_node_traverse_children_inorder(dv_dag_node_t * node, dv_dag_node_t * cur) {
  if (!node) return NULL;
  if (!cur) {
    dv_dag_node_t * x = node->head;
    if (x->spawn)
      return x->spawn;
    else
      return x;
  } else if (cur == node->tail) {
    return NULL;
  } else if (!cur->next) {
    return cur->pre;
  } else {
    dv_dag_node_t * x = cur->next;
    if (x->spawn)
      return x->spawn;
    else
      return x;
  }
}

dv_dag_node_t *
dv_dag_node_traverse_tails(dv_dag_node_t * node, dv_dag_node_t * cur) {
  if (!node) return NULL;
  if (cur == node->tail) return NULL;
  
  dv_dag_node_t * x;
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

dv_dag_node_t *
dv_dag_node_traverse_nexts(dv_dag_node_t * node, dv_dag_node_t * cur) {
  if (!node) return NULL;
  if (!cur)
    return node->next;
  else if (cur == node->next)
    return node->spawn;
  return NULL;
}

int
dv_dag_node_count_nexts(dv_dag_node_t * node) {
  if (!node) return 0;
  if (node->next && node->spawn)
    return 2;
  else if (node->next)
    return 1;
  return 0;
}

dv_dag_node_t *
dv_dag_node_get_next(dv_dag_node_t * node) {
  if (!node) return NULL;
  return node->next;
}

dv_dag_node_t *
dv_dag_node_get_single_head(dv_dag_node_t * node) {
  if (!node) return NULL;
  while (!dv_is_single(node)) {
    dv_check(node->head);
    node = node->head;
  }
  return node;
}

dv_dag_node_t *
dv_dag_node_get_single_last(dv_dag_node_t * node) {
  if (!node) return NULL;
  while (!dv_is_single(node)) {
    dv_check(node->tail);
    node = node->tail;
  }
  return node;
}

/***** end of DAG's functions *****/


/***** DAG Node Pool *****/

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
  //dv_statusbar_update_pool_status();
  return ret;
}

void
dv_dag_node_pool_push(dv_dag_node_pool_t * pool, dv_dag_node_t * node) {
  memset(node, 0, sizeof(dv_dag_node_t));
  node->next = pool->head;
  pool->head = node;
  if (!pool->tail)
    pool->tail = node;
  pool->n++;
  //dv_statusbar_update_pool_status();
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
  //dv_statusbar_update_pool_status();
  return ret;
}

/***** end of DAG Node Pool *****/


/***** HISTOGRAM Entry Pool *****/

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
  //dv_statusbar_update_pool_status();
  return ret;
}

void
dv_histogram_entry_pool_push(dv_histogram_entry_pool_t * pool, dv_histogram_entry_t * node) {
  node->next = pool->head;
  pool->head = node;
  if (!pool->tail)
    pool->tail = node;
  pool->n++;
  //dv_statusbar_update_pool_status();
}

/***** end of HISTOGRAM Entry Pool *****/


/***** Basic data structure *****/

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
  //dv_check(top);
  if (!top)
    return NULL;
  s->top = top->next;
  top->next = s->freelist;
  s->freelist = top;
  return top->item;
}

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

/***** end of Basic data structure *****/


/****** HISTOGRAM ******/

void
dv_histogram_init(dv_histogram_t * H) {
  memset(H, 0, sizeof(dv_histogram_t));
  H->min_entry_interval = DV_PARAPROF_MIN_ENTRY_INTERVAL;
  H->unit_thick = 2.0;
}

static void
dv_histogram_entry_init(dv_histogram_entry_t * e) {
  e->t = 0.0;
  int i;
  for (i=0; i<dv_histogram_layer_max; i++) {
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
dv_histogram_compute_tallest_entry(dv_histogram_t * H) {
  H->max_h = 0.0;
  dv_histogram_entry_t * e = H->head_e;
  while (e != NULL) {
    double h = 0.0;
    int i;
    for (i = 0; i < dv_histogram_layer_max; i++) {
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
dv_histogram_get_max_height(dv_histogram_t * H) {
  if (!H->tallest_e || H->max_h == 0.0) {
    dv_histogram_compute_tallest_entry(H);
  }    
  return H->max_h * (H->unit_thick * H->D->radius);
}

dv_histogram_entry_t *
dv_histogram_insert_entry(dv_histogram_t * H, double t, dv_histogram_entry_t * e_hint) {
  dv_histogram_entry_t * e = NULL;
  dv_histogram_entry_t * ee = H->head_e;
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
  dv_histogram_entry_t * new_e = dv_histogram_entry_pool_pop(CS->epool);
  if (!new_e) {
    fprintf(stderr, "Error: cannot pop a new entry structure!\n");
    dv_check(0);
  }
  dv_histogram_entry_init(new_e);
  new_e->t = t;
  new_e->next = ee;
  if (e) {
    e->next = new_e;
    // Copy h
    int i;
    for (i=0; i<dv_histogram_layer_max; i++) {
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
dv_histogram_add_node(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_entry_t ** hint_entry) {
  if (!H)
    return;
  
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(H->D->P, node);
  double from_t, to_t, dt;
  dv_histogram_entry_t * e_from;
  dv_histogram_entry_t * e_to;
  dv_histogram_entry_t * e;

  from_t = pi->info.first_ready_t;
  to_t = pi->info.last_start_t;
  dt = to_t - from_t;
  e_from = dv_histogram_insert_entry(H, from_t, (hint_entry)?(*hint_entry):NULL);
  e_to = dv_histogram_insert_entry(H, to_t, e_from);
  dv_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    int layer;
    layer = dv_histogram_layer_ready_end;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_end] / dt);
    layer = dv_histogram_layer_ready_create;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_create] / dt);
    layer = dv_histogram_layer_ready_create_cont;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_create_cont] / dt);
    layer = dv_histogram_layer_ready_wait_cont;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_wait_cont] / dt);
    layer = dv_histogram_layer_ready_other_cont;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_other_cont] / dt);
    e = e->next;
  }

  from_t = pi->info.start.t;
  to_t = pi->info.end.t;
  dt = to_t - from_t;
  e_from = dv_histogram_insert_entry(H, from_t, e_from);
  e_to = dv_histogram_insert_entry(H, to_t, e_to);
  dv_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    e->h[dv_histogram_layer_running] += (pi->info.t_1 / dt);
    e = e->next;
  }
  if (hint_entry) *hint_entry = e_to;
  
  /* invalidate current max height */
  H->tallest_e = 0;
  H->max_h = 0.0;
}

void
dv_histogram_remove_node(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_entry_t ** hint_entry) {
  if (!H)
    return;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(H->D->P, node);
  double from_t, to_t, dt;
  dv_histogram_entry_t * e_from;
  dv_histogram_entry_t * e_to;
  dv_histogram_entry_t * e;
  
  from_t = pi->info.first_ready_t;
  to_t = pi->info.last_start_t;
  dt = to_t - from_t;
  e_from = dv_histogram_insert_entry(H, from_t, (hint_entry)?(*hint_entry):NULL);
  e_to = dv_histogram_insert_entry(H, to_t, e_from);
  dv_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    int layer;
    layer = dv_histogram_layer_ready_end;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_end] / dt);
    layer = dv_histogram_layer_ready_create;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_create] / dt);
    layer = dv_histogram_layer_ready_create_cont;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_create_cont] / dt);
    layer = dv_histogram_layer_ready_wait_cont;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_wait_cont] / dt);
    layer = dv_histogram_layer_ready_other_cont;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_other_cont] / dt);
    e = e->next;
  }
  
  from_t = pi->info.start.t;
  to_t = pi->info.end.t;
  dt = to_t - from_t;
  e_from = dv_histogram_insert_entry(H, from_t, e_from);
  e_to = dv_histogram_insert_entry(H, to_t, e_to);
  dv_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    e->h[dv_histogram_layer_running] -= (pi->info.t_1 / dt);
    e = e->next;
  }
  if (hint_entry) *hint_entry = e_to;
  
  /* invalidate current max height */
  H->tallest_e = 0;
  H->max_h = 0.0;
}

void
dv_histogram_clean(dv_histogram_t * H) {
  dv_histogram_entry_t * e = H->head_e;
  dv_histogram_entry_t * ee;
  long n = 0;
  while (e != NULL) {
    ee = e->next;
    dv_histogram_entry_pool_push(CS->epool, e);
    n++;
    e = ee;
  }
  dv_check(n == H->n_e);
  H->head_e = NULL;
  H->tail_e = NULL;
  H->n_e = 0;
}

void
dv_histogram_fini(dv_histogram_t * H) {
  dv_histogram_clean(H);
  H->D = NULL;
}

static void
dv_histogram_reset_node(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_entry_t * e_hint_) {
  /* Calculate inward */
  dv_histogram_entry_t * e_hint = e_hint_;
  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    // Recursive call
    if (dv_is_expanded(node) || dv_is_expanding(node)) {
      dv_histogram_reset_node(H, node->head, e_hint);
    } else {
      dv_histogram_add_node(H, node, &e_hint);
    }
  } else {
    dv_histogram_add_node(H, node, &e_hint);
  }
    
  /* Calculate link-along */
  dv_dag_node_t * u, * v; // linked nodes
  switch ( dv_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    dv_histogram_reset_node(H, u, e_hint);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    dv_histogram_reset_node(H, u, e_hint);
    dv_histogram_reset_node(H, v, e_hint);
    break;
  default:
    dv_check(0);
    break;
  }  
}

void
dv_histogram_reset(dv_histogram_t * H) {
  double time = dv_get_time();
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "dv_histogram_reset()\n");
  }
  dv_histogram_clean(H);
  if (H->D) 
    dv_histogram_reset_node(H, H->D->rt, NULL);
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "... done dv_histogram_reset(): %lf ms\n", dv_get_time() - time);
  }
}

static void
dv_histogram_build_all_r(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_entry_t * e_hint_) {
  /* Calculate inward */
  dv_histogram_entry_t * e_hint = e_hint_;
  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    dv_histogram_build_all_r(H, node->head, e_hint);
  } else {
    dv_histogram_add_node(H, node, &e_hint);
  }
    
  /* Calculate link-along */
  dv_dag_node_t * u, * v; // linked nodes
  switch ( dv_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    dv_histogram_build_all_r(H, u, e_hint);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    dv_histogram_build_all_r(H, u, e_hint);
    dv_histogram_build_all_r(H, v, e_hint);
    break;
  default:
    dv_check(0);
    break;
  }  
}

void
dv_histogram_build_all(dv_histogram_t * H) {
  double time = dv_get_time();
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "dv_histogram_build_all()\n");
  }
  dv_histogram_clean(H);
  if (H->D) 
    dv_histogram_build_all_r(H, H->D->rt, NULL);
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "... done dv_histogram_build_all(): %lf ms\n", dv_get_time() - time);
  }
}

void
dv_histogram_compute_significant_intervals(dv_histogram_t * H) {
  double time = dv_get_time();
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "dv_histogram_compute_significant_intervals()\n");
  }
  int num_workers = H->D->P->num_workers;
  double num_running_workers = 0.0;
  double cumul_1 = 0.0;
  double cumul_2 = 0.0;
  double cumul_3 = 0.0;
  dv_histogram_entry_t * e;
  dv_histogram_entry_t * ee;
  e = H->head_e;
  while (e != H->tail_e) {
    ee = e->next;
    e->cumul_value_1 = cumul_1;
    e->cumul_value_2 = cumul_2;
    e->cumul_value_3 = cumul_3;
    num_running_workers = e->h[dv_histogram_layer_running];
    /* sched. delay or not */
    if (num_running_workers >= (1.0 * num_workers))
      e->value_1 = 0.0;
    else
      e->value_1 = ee->t - e->t;
    cumul_1 += e->value_1;
    /* delay & nowork associated with sched. delay */
    double num_ready_tasks = 0.0;
    int layer = dv_histogram_layer_running;
    while (++layer < dv_histogram_layer_max) {
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
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "... done dv_histogram_compute_significant_intervals(): %lf ms\n", dv_get_time() - time);
  }
}

/****** end of HISTOGRAM ******/


/***** Compute *****/

char *
dv_filename_get_short_name(char * fn) {
  char * p_from = strrchr(fn, '/');
  char * p_to = strrchr(fn, '.');
  if (!p_from)
    p_from = fn;
  else
    p_from++;
  if (!p_to) p_to = fn + strlen(fn);
  int n = p_to - p_from;
  char * ret = (char *) dv_malloc( sizeof(char) * (n + 1) );
  strncpy(ret, p_from, n);
  //int k;
  //  for (k = 0; k < n; k++)
  //      ret[k] = p_from[k];  
  ret[n] = '\0';
  return ret;
}

void
dv_global_state_init(dv_global_state_t * CS) {
  memset(CS, 0, sizeof(dv_global_state_t));
  dv_dag_node_pool_init(CS->pool);
  dv_histogram_entry_pool_init(CS->epool);
  int cp;
  for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
    CS->SBG->checked_cp[cp] = 0;
    if (cp == DV_CRITICAL_PATH_1)
      CS->SBG->checked_cp[cp] = 1;
  }
  CS->oncp_flags[DV_CRITICAL_PATH_0] = DV_NODE_FLAG_CRITICAL_PATH_0;
  CS->oncp_flags[DV_CRITICAL_PATH_1] = DV_NODE_FLAG_CRITICAL_PATH_1;
  CS->oncp_flags[DV_CRITICAL_PATH_2] = DV_NODE_FLAG_CRITICAL_PATH_2;
  CS->cp_colors[DV_CRITICAL_PATH_0] = DV_CRITICAL_PATH_0_COLOR;
  CS->cp_colors[DV_CRITICAL_PATH_1] = DV_CRITICAL_PATH_1_COLOR;
  CS->cp_colors[DV_CRITICAL_PATH_2] = DV_CRITICAL_PATH_2_COLOR;
}

static void
dv_dag_build_inner_all_r(dv_dag_t * D, dv_dag_node_t * node) {
  if (!dv_is_set(node))
    dv_dag_node_set(D, node);
  if (dv_is_union(node)) {
    if (!dv_is_inner_loaded(node)) {
      dv_dag_build_node_inner(D, node);
    }
    /* Call inward */
    dv_check(node->head);
    dv_dag_build_inner_all_r(D, node->head);
  }
  
  /* Call link-along */
  dv_dag_node_t * x = NULL;
  while ( (x = dv_dag_node_traverse_nexts(node, x)) ) {
    dv_dag_build_inner_all_r(D, x);
  }
}

void
dv_dag_build_inner_all(dv_dag_t * D) {
  dv_dag_build_inner_all_r(D, D->rt);
}

static void
dv_dag_compute_critical_paths_r(dv_dag_t * D, dv_dag_node_t * node, dv_histogram_entry_t * e_hint) {
  /* cal. leaf nodes */
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  //printf("compute node %ld, time: %.4lf%%\n", node->pii, (pi->info.start.t - D->bt) / (D->et - D->bt));
  if (!dv_is_union(node)) {
    /* cal. work & weighted work */
    double work = pi->info.end.t - pi->info.start.t;
    //dv_histogram_entry_t * e1 = dv_histogram_insert_entry(D->H, pi->info.start.t, e_hint);
    //dv_histogram_entry_t * e2 = dv_histogram_insert_entry(D->H, pi->info.end.t, e1);
    int cp;
    for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
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
  dv_histogram_entry_t * first_e = dv_histogram_insert_entry(D->H, pi->info.start.t, e_hint);
  {
    dv_dag_node_t * x = NULL;
    while ( (x = dv_dag_node_traverse_children(node, x)) ) {
      dv_dag_compute_critical_paths_r(D, x, first_e);
    }
  }

  /* cal. this node's subgraph */
  dv_dag_node_t * mostwork_tail = NULL; /* cp of work */
  dv_dag_node_t * lastfinished_tail = NULL; /* cp of work & delay */
  double lastfinished_t = 0.0;
  dv_dag_node_t * mostweighted_tail = NULL; /* cp of weighted work & weighted delay */
  dv_dag_node_t * tail = NULL;

#if 0  
  while ( (tail = dv_dag_node_traverse_tails(node, tail)) ) {
    
    dv_critical_path_stat_t cpss[DV_NUM_CRITICAL_PATHS];
    memset(cpss, 0, sizeof(dv_critical_path_stat_t) * DV_NUM_CRITICAL_PATHS);
    dr_pi_dag_node * tail_pi = dv_pidag_get_node_by_dag_node(D->P, tail);

    /* stack of nodes on path */
    dv_stack_t s[1];
    dv_stack_init(s);
    dv_dag_node_t * x = tail;
    while (x) {
      int cp;
      for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
        cpss[cp].work += x->cpss[cp].work;
        cpss[cp].delay += x->cpss[cp].delay;
        cpss[cp].sched_delay += x->cpss[cp].sched_delay;
        int ek;
        for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
          cpss[cp].sched_delays[ek] += x->cpss[cp].sched_delays[ek];
        cpss[cp].sched_delay_nowork += x->cpss[cp].sched_delay_nowork;
        cpss[cp].sched_delay_delay += x->cpss[cp].sched_delay_delay;
      }
      dv_stack_push(s, (void *) x);
      x = x->pre;
    }

    /* compute delay along path */
    dv_critical_path_stat_t cps[1];
    cps->delay = cps->sched_delay = 0.0;
    int ek;
    for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
      cps->sched_delays[ek] = 0.0;
    cps->sched_delay_nowork = 0.0;
    cps->sched_delay_delay = 0.0;
    x = dv_stack_pop(s);
    dr_pi_dag_node * x_pi = dv_pidag_get_node_by_dag_node(D->P, x);
    dv_dag_node_t * xx = NULL;
    dr_pi_dag_node * xx_pi = NULL;
    dv_histogram_entry_t * e0 = NULL;
    dv_histogram_entry_t * e1 = first_e;
    while ( (xx = (dv_dag_node_t *) dv_stack_pop(s)) ) {
      xx_pi = dv_pidag_get_node_by_dag_node(D->P, xx);
      cps->delay += xx_pi->info.start.t - x_pi->info.end.t;
      e0 = dv_histogram_insert_entry(D->H, x_pi->info.end.t, e1);
      e1 = dv_histogram_insert_entry(D->H, xx_pi->info.start.t, e0);
      cps->sched_delay += e1->cumul_value_1 - e0->cumul_value_1;
      ek = xx_pi->info.in_edge_kind;
      cps->sched_delays[ek] += e1->cumul_value_1 - e0->cumul_value_1;
      cps->sched_delay_nowork += e1->cumul_value_3 - e0->cumul_value_3;
      cps->sched_delay_delay += e1->cumul_value_2 - e0->cumul_value_2;
      x = xx;
      x_pi = xx_pi;
    }
    cps->delay += pi->info.end.t - tail_pi->info.end.t;
    e0 = dv_histogram_insert_entry(D->H, tail_pi->info.end.t, e1);
    e1 = dv_histogram_insert_entry(D->H, pi->info.end.t, e0);
    cps->sched_delay += e1->cumul_value_1 - e0->cumul_value_1;
    ek = dr_dag_edge_kind_end;
    cps->sched_delays[ek] += e1->cumul_value_1 - e0->cumul_value_1;
    cps->sched_delay_nowork += e1->cumul_value_3 - e0->cumul_value_3;
    cps->sched_delay_delay += e1->cumul_value_2 - e0->cumul_value_2;
    int cp;
    for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
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
    if (!mostwork_tail || cpss[DV_CRITICAL_PATH_0].work > node->cpss[DV_CRITICAL_PATH_0].work) {
      node->cpss[DV_CRITICAL_PATH_0] = cpss[DV_CRITICAL_PATH_0];
      mostwork_tail = tail;
    }
    if (!lastfinished_tail || tail_pi->info.end.t >= lastfinished_t) {
      node->cpss[DV_CRITICAL_PATH_1] = cpss[DV_CRITICAL_PATH_1];
      lastfinished_tail = tail;
      lastfinished_t = tail_pi->info.end.t;
    }
    if (!mostweighted_tail ||
        (cpss[DV_CRITICAL_PATH_2].sched_delay > node->cpss[DV_CRITICAL_PATH_2].sched_delay)) {
      node->cpss[DV_CRITICAL_PATH_2] = cpss[DV_CRITICAL_PATH_2];
      mostweighted_tail = tail;
    }
    
  } /* while (tail = ..) */
  
  if (!mostwork_tail)
    fprintf(stderr, "Could not find mostwork_tail.\n");
  else if (0)
    fprintf(stderr, "mostwork_tail: %.2lf %.2lf %.2lf\n",
            node->cpss[DV_CRITICAL_PATH_0].work,
            node->cpss[DV_CRITICAL_PATH_0].delay,
            node->cpss[DV_CRITICAL_PATH_0].sched_delay);
  if (!lastfinished_tail)
    fprintf(stderr, "Could not find lastfinished_tail.\n");
  else if (0)
    fprintf(stderr, "lastfinished_tail: %.2lf %.2lf %.2lf\n",
            node->cpss[DV_CRITICAL_PATH_1].work,
            node->cpss[DV_CRITICAL_PATH_1].delay,
            node->cpss[DV_CRITICAL_PATH_1].sched_delay);
  if (!mostweighted_tail)
    fprintf(stderr, "Could not find mostweighted_tail.\n");
  else if (0)
    fprintf(stderr, "mostweighted_tail: %.2lf %.2lf %.2lf\n",
            node->cpss[DV_CRITICAL_PATH_2].work,
            node->cpss[DV_CRITICAL_PATH_2].delay,
            node->cpss[DV_CRITICAL_PATH_2].sched_delay);
  
#else
  
  while ( (tail = dv_dag_node_traverse_tails(node, tail)) ) {
    dr_pi_dag_node * tail_pi = dv_pidag_get_node_by_dag_node(D->P, tail);
    if (!lastfinished_tail || tail_pi->info.end.t >= lastfinished_t) {
      lastfinished_tail = tail;
      lastfinished_t = tail_pi->info.end.t;
    }
  }

  tail = lastfinished_tail;
  if (tail) {
    
    dv_critical_path_stat_t cpss[DV_NUM_CRITICAL_PATHS];
    memset(cpss, 0, sizeof(dv_critical_path_stat_t) * DV_NUM_CRITICAL_PATHS);
    dr_pi_dag_node * tail_pi = dv_pidag_get_node_by_dag_node(D->P, tail);

    /* stack of nodes on path */
    dv_stack_t s[1];
    dv_stack_init(s);
    dv_dag_node_t * x = tail;
    while (x) {
      int cp = DV_CRITICAL_PATH_1;
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
      dv_stack_push(s, (void *) x);
      x = x->pre;
    }

    /* compute delay along path */
    dv_critical_path_stat_t cps[1];
    cps->delay = cps->sched_delay = 0.0;
    int ek;
    for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
      cps->sched_delays[ek] = 0.0;
    cps->sched_delay_nowork = 0.0;
    cps->sched_delay_delay = 0.0;
    x = dv_stack_pop(s);
    dr_pi_dag_node * x_pi = dv_pidag_get_node_by_dag_node(D->P, x);
    dv_dag_node_t * xx = NULL;
    dr_pi_dag_node * xx_pi = NULL;
    dv_histogram_entry_t * e0 = NULL;
    dv_histogram_entry_t * e1 = first_e;
    while ( (xx = (dv_dag_node_t *) dv_stack_pop(s)) ) {
      xx_pi = dv_pidag_get_node_by_dag_node(D->P, xx);
      cps->delay += xx_pi->info.start.t - x_pi->info.end.t;
      e0 = dv_histogram_insert_entry(D->H, x_pi->info.end.t, e1);
      e1 = dv_histogram_insert_entry(D->H, xx_pi->info.start.t, e0);
      cps->sched_delay += e1->cumul_value_1 - e0->cumul_value_1;
      ek = xx_pi->info.in_edge_kind;
      cps->sched_delays[ek] += e1->cumul_value_1 - e0->cumul_value_1;
      cps->sched_delay_nowork += e1->cumul_value_3 - e0->cumul_value_3;
      cps->sched_delay_delay += e1->cumul_value_2 - e0->cumul_value_2;
      x = xx;
      x_pi = xx_pi;
    }
    cps->delay += pi->info.end.t - tail_pi->info.end.t;
    e0 = dv_histogram_insert_entry(D->H, tail_pi->info.end.t, e1);
    e1 = dv_histogram_insert_entry(D->H, pi->info.end.t, e0);
    cps->sched_delay += e1->cumul_value_1 - e0->cumul_value_1;
    ek = dr_dag_edge_kind_end;
    cps->sched_delays[ek] += e1->cumul_value_1 - e0->cumul_value_1;
    cps->sched_delay_nowork += e1->cumul_value_3 - e0->cumul_value_3;
    cps->sched_delay_delay += e1->cumul_value_2 - e0->cumul_value_2;
    int cp = DV_CRITICAL_PATH_1;
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
    node->cpss[DV_CRITICAL_PATH_1] = cpss[DV_CRITICAL_PATH_1];
    
  }
  
#endif
  
  /* mark nodes */
  {
    dv_dag_node_t * x;
    x = mostwork_tail;
    while (x) {
      dv_node_flag_set(x->f, CS->oncp_flags[DV_CRITICAL_PATH_0]);
      x = x->pre;
    }
    x = lastfinished_tail;
    while (x) {
      dv_node_flag_set(x->f, CS->oncp_flags[DV_CRITICAL_PATH_1]);
      x = x->pre;
    }
    x = mostweighted_tail;
    while (x) {
      dv_node_flag_set(x->f, CS->oncp_flags[DV_CRITICAL_PATH_2]);
      x = x->pre;
    }
  }
}

void
dv_dag_compute_critical_paths(dv_dag_t * D) {
  double time = dv_get_time();
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "dv_dag_compute_critical_paths()\n");
  }

  if (!D->critical_paths_computed) {
    /* prepare D & H */
    dv_dag_build_inner_all(D);
    dv_histogram_t * H_bk = D->H;
    D->H = dv_malloc( sizeof(dv_histogram_t) );
    dv_histogram_init(D->H);
    D->H->D = D;
    D->H->min_entry_interval = 0.0;
    dv_histogram_build_all(D->H);
    dv_histogram_compute_significant_intervals(D->H);

    /* compute recursively */
#if 0    
    dv_node_flag_set(D->rt->f, CS->oncp_flags[DV_CRITICAL_PATH_0]);
    dv_node_flag_set(D->rt->f, CS->oncp_flags[DV_CRITICAL_PATH_1]);
    dv_node_flag_set(D->rt->f, CS->oncp_flags[DV_CRITICAL_PATH_2]);
#else    
    dv_node_flag_set(D->rt->f, CS->oncp_flags[DV_CRITICAL_PATH_1]);
#endif    
    dv_dag_compute_critical_paths_r(D, D->rt, NULL);

    /* finish */
    dv_histogram_fini(D->H);
    dv_free(D->H, sizeof(dv_histogram_t));
    D->H = H_bk;
    D->critical_paths_computed = 1;
  }

  /* output */
#if 0  
  int cp;
  for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
    printf("DAG %ld (%.0lf) (cp %d): %.2lf %.2lf %.2lf(%.1lf%%)",
           D - CS->D,
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
  int cp = DV_CRITICAL_PATH_1;
  {
    printf("DAG %ld (%.0lf) (cp %d): %.2lf %.2lf %.2lf(%.1lf%%)",
           D - CS->D,
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

  if (CS->verbose_level >= 1) {
    fprintf(stderr, "... done dv_dag_compute_critical_paths(): %lf\n", dv_get_time() - time);
  }
}

int
dm_get_dag_id(char * filename) {
  /* check existing D */
  dv_dag_t * D = NULL;
  int i;
  for (i = 0; i < CS->nD; i++) {
    if (strcmp(CS->D[i].P->filename, filename) == 0) {
      D = &CS->D[i];
      break;
    }
  }
  if (!D) {
    /* read DAG file */
    dv_pidag_t * P = dv_pidag_read_new_file(filename);
    if (!P) {
      fprintf(stderr, "Error: cannot read DAG from %s\n", filename);
      return -1;
    }
    /* create a new D */
    dv_dag_t * D = dv_dag_create_new_with_pidag(P);
    i = D - CS->D;
  }
  return i;
}

dv_dag_t *
dm_compute_dag_file(char * filename) {
  int i = dm_get_dag_id(filename);
  if (i < 0)
    return NULL;
  dv_dag_t * D = &CS->D[i];

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
  CS->SBG->work[i] = work;
  CS->SBG->delay[i] = delay;
  CS->SBG->nowork[i] = no_work;
  /* critical path */
  //dv_dag_compute_critical_paths(D);  

  /* output */
  return D;
}

/***** end of Compute *****/

