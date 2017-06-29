#include "dagviz.h"

/*---------PIDAG Reader's functions---------------*/

#if 0
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
  if (CS->nP >= DM_MAX_DAG_FILE)
    return NULL;
  dm_pidag_t * P = &CS->P[CS->nP++];
  P->fn = filename;
  P->filename = (char *) dv_malloc( sizeof(char) * (strlen(filename) + 1) );
  strcpy(P->filename, filename);
  P->filename[strlen(filename)] = '\0';
  P->short_filename = dv_filename_get_short_name(P->filename);
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

dm_pidag_t *
dm_pidag_read_new_file_bk(char * filename) {
  /* Get new PIDAG */
  dv_check(CS->nP < DM_MAX_DAG_FILE);
  dm_pidag_t * P = &CS->P[CS->nP++];
  P->fn = filename;
  dm_llist_init(P->itl);
  
  /* Read file */
  int err;
  int fd;
  struct stat statbuf[1];
  void *dp;
  
  fd = open(filename, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "open: %d\n", errno);
    exit(1);
  }
  err = fstat(fd, statbuf);
  if (err < 0) {
    fprintf(stderr, "fstat: %d\n", errno);
    exit(1);
  }
  printf("File status:\n"
         "  st_size = %ld bytes (%0.0lfMB)\n",
         (long) statbuf->st_size,
         ((double) statbuf->st_size) / (1024.0 * 1024.0));
  
  dp = mmap(0, statbuf->st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (!dp) {
    fprintf(stderr, "mmap: error\n");
    exit(1);
  }

  char dr_header[DAG_RECORDER_HEADER_LEN+1];
  strncpy(dr_header, dp, DAG_RECORDER_HEADER_LEN);
  dr_header[DAG_RECORDER_HEADER_LEN] = '\0';
  dr_header[DAG_RECORDER_HEADER_LEN] = '\0';
  long n, m, sc, nw;
  long * ldp = (long *) (dp + DAG_RECORDER_HEADER_LEN);
  n = *ldp;
  ldp++;
  m = *ldp;
  ldp++;
  sc = *ldp;
  ldp++;
  nw = *ldp;
  ldp++;
  printf("PI DAG: %s\n"
         "  n = %ld\n"
         "  m = %ld\n"
         "  sc = %ld\n"
         "  nw = %ld\n", dr_header, n, m, sc, nw);
  P->n = n;
  P->m = m;
  P->start_clock = sc;
  P->num_workers = nw;

  P->T = (dr_pi_dag_node *) ldp;
  P->E = (dr_pi_dag_edge *) (P->T + n);
  dr_pi_string_table * stp = (dr_pi_string_table *) (P->E + m);
  dr_pi_string_table * S = P->S;
  *S = *stp;
  S->I = (long *) (stp + 1);
  S->C = (const char *) (S->I + S->n);
  
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
  dv_check(id >=0 && id < P->n);
  dr_pi_dag_node * ret = P->T + id;
  return ret;
}

dr_pi_dag_node *
dm_pidag_get_node_by_dag_node(dm_pidag_t * P, dv_dag_node_t * node) {
  if (!node) return NULL;
  return dm_pidag_get_node_by_id(P, node->pii);
}

static dr_pi_dag_node *
dm_pidag_get_node_by_offset(dm_pidag_t * P, dr_pi_dag_node * pi, long offset) {
  if (!pi) return NULL;
  long id = dm_pidag_get_node_id_with_offset(P, pi, offset);
  return dm_pidag_get_node_by_id(P, id);
}
#endif
/*---------end of PIDAG Reader's functions---------------*/


/*---------DAG's functions---------------*/

#if 0
static void
dv_node_coordinate_init(dv_node_coordinate_t * c) {
  memset(c, 0, sizeof(dv_node_coordinate_t));
  /*
  c->x = 0.0;
  c->y = 0.0;
  c->xp = 0.0;
  c->xpre = 0.0;
  c->lw = 0.0;
  c->rw = 0.0;
  c->dw = 0.0;
  c->link_lw = 0.0;
  c->link_rw = 0.0;
  c->link_dw = 0.0;
  */
}

void
dv_dag_node_init(dv_dag_node_t * node, dv_dag_node_t * parent, long pii) {
  /* node->next: do not initialize next which is used already for linking in node pool */
  //dv_dag_node_t * t = node->next;

  //memset(node, 0, sizeof(dv_dag_node_t));
  
  node->pii = pii;
  dv_node_flag_init(node->f);
  node->d = (parent)?(parent->d + 1):0;
  //node->next = t;

  node->parent = parent;
  node->pre = NULL;
  /* node->next: do not initialize next which is used already for linking in node pool */
  node->spawn = NULL;
  node->head = NULL;
  node->tail = NULL;

  int i;
  for (i = 0; i < DV_NUM_LAYOUT_TYPES; i++)
    dv_node_coordinate_init(&node->c[i]);

  node->started = 0.0;

  node->r = 0;
  node->link_r = 0;

  node->highlight = 0;

  for (i = 0; i < DV_NUM_CRITICAL_PATHS; i++) {
    node->cpss[i].work = 0.0;
    node->cpss[i].delay = 0.0;
    node->cpss[i].sched_delay = 0.0;
    int ek;
    for (ek = 0; ek < dr_dag_edge_kind_max; ek++) {
      node->cpss[i].sched_delays[ek] = 0.0;
    }
    node->cpss[i].sched_delay_nowork = 0.0;
    node->cpss[i].sched_delay_delay = 0.0;
  }    
}

int
dv_dag_node_set(dv_dag_t * D, dv_dag_node_t * node) {
  dm_pidag_t * P = D->P;
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(P, node);

  if (pi->info.kind == dr_dag_node_kind_section
      || pi->info.kind == dr_dag_node_kind_task) {

    if (pi->subgraphs_begin_offset < pi->subgraphs_end_offset)
      dv_node_flag_set(node->f, DV_NODE_FLAG_UNION);
    else
      dv_node_flag_remove(node->f, DV_NODE_FLAG_UNION);
    
  } else { // kind == (section || task)

    dv_node_flag_remove(node->f, DV_NODE_FLAG_UNION);
    
  }
  
  dv_node_flag_set(node->f, DV_NODE_FLAG_SET);
  dv_node_flag_remove(node->f, DV_NODE_FLAG_INNER_LOADED);

  return DV_OK;
}

int
dv_dag_build_node_inner(dv_dag_t * D, dv_dag_node_t * node) {
  dm_pidag_t * P = D->P;

  if (!dv_is_set(node))
    dv_dag_node_set(D, node);
  
  if (dv_is_union(node)) {
    
    dr_pi_dag_node * pi, * pi_a, * pi_b, * pi_x;
    dv_dag_node_t * node_a, * node_b, * node_x, * node_t;

    pi = dm_pidag_get_node_by_dag_node(P, node);
    pi_a = dm_pidag_get_node_by_offset(P, pi, pi->subgraphs_begin_offset);
    pi_b = dm_pidag_get_node_by_offset(P, pi, pi->subgraphs_end_offset - 1);
#if 0
    /* tau: I think this condition does not hold when P has 
       exactly one child. the condition should be pi_a <= pi_b;
       also, did you consider the case a section/task is
       collapsed so does not have any children? */
    dv_check(pi_a < pi_b);
#else
    dv_check(pi_a <= pi_b);
#endif
    
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
      dv_dag_node_init(node_x, node, dm_pidag_get_node_id(P, pi_x));
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
      pi_x = dm_pidag_get_node_by_dag_node(P, node_x);
      if (pi_x->info.kind == dr_dag_node_kind_create_task) {
        node_t = dv_dag_node_pool_pop(CS->pool);
        if (!node_t)
          return dv_log_set_error(DV_ERROR_OONP);
        dv_dag_node_init(node_t, node, dm_pidag_get_node_id_with_offset(P, pi_x, pi_x->child_offset));
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
  (void)D;
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
      while ( (x = dm_dag_node_traverse_children(node, x)) ) {
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
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
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

_unused_ static char *
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
dv_dag_init(dv_dag_t * D, dm_pidag_t * P) {
  char str[10];
  sprintf(str, "DAG %ld", D - CS->D);
  D->name = malloc( sizeof(char) * (strlen(str) + 1) );
  strcpy(D->name, str);
  D->name[strlen(str)] = '\0';

  /* make a short name for use on plots */
  D->name_on_graph = (char *) malloc( sizeof(char) * (strlen(str) + 1) );
  strcpy(D->name_on_graph, str);
  //D->name_on_graph[strlen(str)] = '\0';
  //D->name_on_graph = dv_get_distinct_components_name_string(P->short_filename);
  //printf("%s: %s\n", D->name, D->name_on_graph);
  
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
  //D->sdt = DV_SCALE_TYPE_INIT;
  D->log_radix = DV_RADIX_LOG;
  D->power_radix = DV_RADIX_POWER;
  D->linear_radix = DV_RADIX_LINEAR;
  D->frombt = DV_FROMBT_INIT;
  D->radius = CS->opts.radius;

  /* D <-> Vs */
  int i;
  for (i = 0; i < DV_MAX_VIEW; i++)
    D->mV[i] = 0;
  
  dm_llist_init(D->itl);
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
dv_dag_create_new_with_pidag(dm_pidag_t * P) {
  /* Get new DAG */
  dv_check(CS->nD < DM_MAX_DAG);
  dv_dag_t * D = &CS->D[CS->nD++];
  dv_dag_init(D, P);

  /* Set values */
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(P, D->rt);
  //D->bt = pi->info.start.t - 1;
  //D->et = pi->info.end.t + 1;
  D->bt = pi->info.first_ready_t;
  D->et = pi->info.end.t;
  
  // Traverse pidag's nodes
  /*
  dm_stack_t s[1];
  dm_stack_init(s);
  dm_stack_push(s, (void *) G->rt);
  while (s->top) {
    dv_dag_node_t * x = (dv_dag_node_t *) dm_stack_pop(s);
    pi = dm_pidag_get_node_by_dag_node(x);
    p = dv_traverse_node(pi, x, p, plim, s, G);
  }
  */

  return D;
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

#endif





/*-----------------end of DAG's functions-----------*/


