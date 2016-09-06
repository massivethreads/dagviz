#include "dagviz.h"

/*---------PIDAG Reader's functions---------------*/

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

dv_pidag_t *
dv_pidag_read_new_file_bk(char * filename) {
  /* Get new PIDAG */
  dv_check(CS->nP < DV_MAX_DAG_FILE);
  dv_pidag_t * P = &CS->P[CS->nP++];
  P->fn = filename;
  dv_llist_init(P->itl);
  
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

/*---------end of PIDAG Reader's functions---------------*/


/*---------DAG's functions---------------*/

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
  dv_pidag_t * P = D->P;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(P, node);

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
  dv_pidag_t * P = D->P;

  if (!dv_is_set(node))
    dv_dag_node_set(D, node);
  
  if (dv_is_union(node)) {
    
    dr_pi_dag_node * pi, * pi_a, * pi_b, * pi_x;
    dv_dag_node_t * node_a, * node_b, * node_x, * node_t;

    pi = dv_pidag_get_node_by_dag_node(P, node);
    pi_a = dv_pidag_get_node_by_offset(P, pi, pi->subgraphs_begin_offset);
    pi_b = dv_pidag_get_node_by_offset(P, pi, pi->subgraphs_end_offset - 1);
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
  char str[10];
  sprintf(str, "DAG %ld", D - CS->D);
  D->name = malloc( sizeof(char) * (strlen(str) + 1) );
  strcpy(D->name, str);
  D->name[strlen(str)] = '\0';
  //D->name_on_graph = (char *) malloc( sizeof(char) * (strlen(str) + 1) );
  //strcpy(D->name_on_graph, str);
  //D->name_on_graph[strlen(str)] = '\0';
  D->name_on_graph = dv_get_distinct_components_name_string(P->short_filename);
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
  //D->bt = pi->info.start.t - 1;
  //D->et = pi->info.end.t + 1;
  D->bt = pi->info.first_ready_t;
  D->et = pi->info.end.t;
  
  // Traverse pidag's nodes
  /*
  dv_stack_t s[1];
  dv_stack_init(s);
  dv_stack_push(s, (void *) G->rt);
  while (s->top) {
    dv_dag_node_t * x = (dv_dag_node_t *) dv_stack_pop(s);
    pi = dv_pidag_get_node_by_dag_node(x);
    p = dv_traverse_node(pi, x, p, plim, s, G);
  }
  */

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
    //v = dv_get_color_pool_index(nc, 0, 0, 0, pi->info.worker);
    v = pi->info.worker;
    break;
  case 1:
    //v = dv_get_color_pool_index(nc, 0, 0, 0, pi->info.cpu);
    v = pi->info.cpu;
    break;
  case 2:
    //v = dv_get_color_pool_index(nc, 0, 0, 0, pi->info.kind);
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


/*-----------------end of DAG's functions-----------*/


/*-----------------BTSAMPLE_VIEWER's functions-----------*/

void dv_btsample_viewer_init(dv_btsample_viewer_t * btviewer) {
  btviewer->label_dag_file_name = gtk_label_new(0);
  btviewer->entry_bt_file_name = gtk_entry_new();
  btviewer->entry_binary_file_name = gtk_entry_new();
  btviewer->combobox_worker = gtk_combo_box_text_new();
  btviewer->entry_time_from = gtk_entry_new();
  btviewer->entry_time_to = gtk_entry_new();
  btviewer->text_view = gtk_text_view_new();
  btviewer->entry_node_id = gtk_entry_new();
  g_object_ref(btviewer->label_dag_file_name);
  g_object_ref(btviewer->entry_bt_file_name);
  g_object_ref(btviewer->entry_binary_file_name);
  g_object_ref(btviewer->combobox_worker);
  g_object_ref(btviewer->entry_time_from);
  g_object_ref(btviewer->entry_time_to);
  g_object_ref(btviewer->text_view);
  g_object_ref(btviewer->entry_node_id);
}

#ifdef DV_ENABLE_BFD
static asymbol **syms;           /* Symbol table.  */
static bfd_vma pc;
static const char *filename;
static const char *functionname;
static unsigned int line;
static bfd_boolean found;

static void slurp_symtab(bfd *);
static void find_address_in_section(bfd *, asection *, void *);
static void translate_addresses(int, bfd *, bt_sample_t *, dv_btsample_viewer_t *);
static void process_one_sample(int, const char *, const char *, bt_sample_t *, dv_btsample_viewer_t *);

/* Read in the symbol table.  */
static void
slurp_symtab(bfd *abfd) {
  long symcount;
  unsigned int size;

  if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
    return;

  symcount = bfd_read_minisymbols(abfd, FALSE, (void *) &syms, &size);
  if (symcount == 0)
    symcount = bfd_read_minisymbols(abfd, TRUE /* dynamic */, (void *) &syms, &size);

  if (symcount < 0) {
    //bfd_fatal(bfd_get_filename(abfd));
    perror(bfd_get_filename(abfd));
    exit(1);
  }
}

/* Look for an address in a section.  This is called via
   bfd_map_over_sections.  */
static void
find_address_in_section(bfd *abfd, asection *section, void *data ATTRIBUTE_UNUSED) {
  bfd_vma vma;
  bfd_size_type size;

  if (found)
    return;

  if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0)
    return;

  vma = bfd_get_section_vma (abfd, section);
  if (pc < vma)
    return;

  size = bfd_get_section_size (section);
  if (pc >= vma + size)
    return;

  found = bfd_find_nearest_line (abfd, section, syms, pc - vma,
                                 &filename, &functionname, &line);
}

/* Read hexadecimal addresses from stdin, translate into
   file_name:line_number and optionally function name.  */
static void
translate_addresses(int c, bfd *abfd, bt_sample_t *s, dv_btsample_viewer_t *btviewer) {
  char str_frame[200];
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(btviewer->text_view));
  
  char addr[30];
  int i;
  for (i=0; i<s->n; i++) {
    sprintf(addr, "%p", s->frames[i]);
    pc = bfd_scan_vma(addr, NULL, 16);
    
    found = FALSE;
    bfd_map_over_sections(abfd, find_address_in_section, NULL);
    
    if (!found) {
      sprintf(str_frame, "[%d] : Worker %d : Clock %llu : Frame %d : Addr %s : ?? : ?? : ??\n",
              c,
              s->worker,
              s->tsc - btviewer->P->start_clock,
              i,
              addr);
    } else {
      sprintf(str_frame, "[%d] : Worker %d : Clock %llu : Frame %d : Addr %s : %s : %s : %d\n",
              c,
              s->worker,
              s->tsc - btviewer->P->start_clock,
              i,
              addr,
              functionname ? functionname : "??",
              filename ? filename : "??",
              line);
    }
    gtk_text_buffer_insert_at_cursor(buffer, str_frame, -1);
  }
  sprintf(str_frame, "\n");
  gtk_text_buffer_insert_at_cursor(buffer, str_frame, -1);
}

static void
process_one_sample(int c, const char *bin_file, const char *target, bt_sample_t *s, dv_btsample_viewer_t *btviewer) {
  bfd *abfd;
  char **matching;

  abfd = bfd_openr(bin_file, target);
  if (abfd == NULL) {
    //bfd_fatal(bin_file);
    perror(bin_file);
    exit(1);
  }

  if (bfd_check_format(abfd, bfd_archive)) {
    //fatal (_("%s: can not get addresses from archive"), bin_file);
    perror("can not get addresses from archive");
    exit(1);
  }

  if (!bfd_check_format_matches(abfd, bfd_object, &matching)) {
    //bfd_nonfatal(bfd_get_filename(abfd));
    perror(bfd_get_filename(abfd));
    if (bfd_get_error() == bfd_error_file_ambiguously_recognized) {
      //list_matching_formats(matching);
      free(matching);
    }
    exit(1);
  }

  slurp_symtab(abfd);

  translate_addresses(c, abfd, s, btviewer);

  if (syms != NULL) {
    free (syms);
    syms = NULL;
  }

  bfd_close(abfd);
}
#endif /* DV_ENABLE_BFD */

int
dv_btsample_viewer_extract_interval(dv_btsample_viewer_t * btviewer, _unused_ int worker, _unused_ unsigned long long from, _unused_ unsigned long long to) {
#ifdef DV_ENABLE_BFD  
  const char * binary_file = gtk_entry_get_text(GTK_ENTRY(btviewer->entry_binary_file_name));
  const char * btsample_file = gtk_entry_get_text(GTK_ENTRY(btviewer->entry_bt_file_name));

  bfd_init();
  //set_default_bfd_target();

  FILE * fi = fopen(btsample_file, "r");
  if (!fi) {
    perror("fopen btsample_file");
    return 0;
  }

  int c = 0;
  bt_sample_t s[1];
  while (fread(s, sizeof(bt_sample_t), 1, fi))
    if (s->worker == worker && s->tsc >= from && s->tsc <= to)
      process_one_sample(c++, binary_file, NULL, s, btviewer);

  fclose(fi);
#else  
  GtkTextBuffer * buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(btviewer->text_view));
  gtk_text_buffer_insert_at_cursor(buffer, "Error: BFD is not enabled.", -1);
#endif /* DV_ENABLE_BFD */
  return 0;
}

/*-----------------end of BTSAMPLE_VIEWER's functions-----------*/



