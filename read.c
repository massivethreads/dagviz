#include "dagviz.h"

/*---------PIDAG Reader's functions---------------*/

dv_pidag_t * dv_pidag_read_new_file(char * filename) {
  /* Get new PIDAG */
  dv_check(CS->nP < DV_MAX_DAG_FILE);
  dv_pidag_t * P = &CS->P[CS->nP++];
  P->fn = filename;
  
  /* Read file */
  int err;
  int fd;
  struct stat *statbuf = P->stat;
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

  long n, m, nw;
  long * ldp = (long *) dp;
  n = *ldp;
  ldp++;
  m = *ldp;
  ldp++;
  nw = *ldp;
  ldp++;
  printf("PI DAG:\n"
         "  n = %ld\n"
         "  m = %ld\n"
         "  nw = %ld\n", n, m, nw);
  P->n = n;
  P->m = m;
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

dr_pi_dag_node * dv_pidag_get_node(dv_pidag_t *P, dv_dag_node_t *node) {
  if (!node) return NULL;
  long idx = node->pii;
  dv_check(idx >=0 && idx < P->n);
  dr_pi_dag_node *ret = P->T + idx;
  return ret;
}

dr_pi_dag_node * dv_pidag_get_node_offset(dv_pidag_t *P, dr_pi_dag_node *pi, long offset) {
  if (!pi) return NULL;
  long idx = pi - P->T + offset;
  dv_check(idx >=0 && idx < P->n);
  dr_pi_dag_node *ret = P->T + idx;
  return ret;
}

long dv_pidag_get_idx(dv_pidag_t *P, dr_pi_dag_node *pi) {
  long ret = pi - P->T;
  return ret;
}

long dv_pidag_get_idx_with_offset(dv_pidag_t *P, dr_pi_dag_node *pi, long offset) {
  long ret = pi - P->T + offset;
  return ret;
}
  
/*---------end of PIDAG Reader's functions---------------*/


/*---------DAG's functions---------------*/

void dv_dag_node_pool_init(dv_dag_t *D) {
  if (!D) return;
  if (D->T) return;
  D->Tsz = DV_DAG_NODE_POOL_SIZE;
  D->T = (dv_dag_node_t *) dv_malloc( sizeof(dv_dag_node_t) * D->Tsz );  
  D->To = (char *) dv_malloc( sizeof(char) * D->Tsz );
  int i;
  for (i=0; i<D->Tsz; i++)
    D->To[i] = 0;
  D->Tn = 0;
}

int dv_dag_node_pool_is_empty(dv_dag_t *D) {
  return (D->Tn >= D->Tsz);
}

dv_dag_node_t * dv_dag_node_pool_pop(dv_dag_t *D) {
  if (dv_dag_node_pool_is_empty(D)) {
    dv_dag_clear_shrinked_nodes(D);
    if (dv_dag_node_pool_is_empty(D))
      return NULL;
  }
  int i;
  for (i=0; i<D->Tsz; i++)
    if (!D->To[i]) {
      D->To[i] = 1;
      D->Tn++;
      return D->T + i;
    }
  return NULL;
}

void dv_dag_node_pool_push(dv_dag_t *D, dv_dag_node_t *node) {
  int i = node - D->T;
  dv_check(i >=0 && i < D->Tsz);
  dv_check(D->To[i]);
  D->To[i] = 0;
  D->Tn--;
}

int dv_dag_node_pool_avail(dv_dag_t *D) {
  return D->Tsz - D->Tn;
}

dv_dag_node_t * dv_dag_node_pool_pop_contiguous(dv_dag_t *D, long num) {
  // Try searching
  long i, j;
  int ok;
  for (i=0; i<D->Tsz-num+1; i++) {
    ok = 1;
    for (j=i; j<i+num; j++)
      if (D->To[j]) {
        ok = 0;
        break;
      }
    if (ok) {
      for (j=i; j<i+num; j++)
        D->To[j] = 1;
      D->Tn += num;
      return D->T + i;
    }
  }
  // If there is not, clear shrinked nodes
  long avail, new_avail;
  avail = -1;
  new_avail = dv_dag_node_pool_avail(D);
  while (new_avail > avail) {
    avail = new_avail;
    dv_dag_clear_shrinked_nodes(D);
    new_avail = dv_dag_node_pool_avail(D);
  }
  if (dv_dag_node_pool_avail(D) < num)
    return NULL;
  // Try searching again
  for (i=0; i<D->Tsz-num+1; i++) {
    ok = 1;
    for (j=i; j<i+num; j++)
      if (D->To[j]) {
        ok = 0;
        break;
      }
    if (ok) {
      for (j=i; j<i+num; j++)
        D->To[j] = 1;
      D->Tn += num;
      return D->T + i;
    }
  }
  return NULL;
}

void dv_node_coordinate_init(dv_node_coordinate_t *c) {
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
}

void dv_dag_node_init(dv_dag_node_t *node, dv_dag_node_t *parent, long pii) {
  node->pii = pii;
  dv_node_flag_init(node->f);
  node->d = (parent)?(parent->d + 1):0;

  node->parent = parent;
  node->pre = 0;
  dv_llist_init(node->links);
  node->head = 0;
  dv_llist_init(node->tails);

  int i;
  for (i=0; i<DV_NUM_LAYOUT_TYPES; i++)
    dv_node_coordinate_init(&node->c[i]);

  node->started = 0.0;
}

int dv_dag_node_set(dv_dag_t *D, dv_dag_node_t *node) {
  dv_pidag_t *P = D->P;
  dr_pi_dag_node *pi = dv_pidag_get_node(P, node);

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

int dv_dag_build_node_inner(dv_dag_t *D, dv_dag_node_t *node) {
  dv_pidag_t *P = D->P;

  if (!dv_is_set(node))
    dv_dag_node_set(D, node);
  
  if (dv_is_union(node)) {
    
    dr_pi_dag_node *pi, *pi_a, *pi_b, *pi_x;
    dv_dag_node_t *node_a, *node_b, *node_x, *node_t;

    pi = dv_pidag_get_node(P, node);    
    pi_a = dv_pidag_get_node_offset(P, pi, pi->subgraphs_begin_offset);
    pi_b = dv_pidag_get_node_offset(P, pi, pi->subgraphs_end_offset - 1);
    dv_check(pi_a < pi_b);
    
    // Check node pool for all child nodes
    long num_children;
    num_children = pi_b - pi_a + 1;
    node_a = dv_dag_node_pool_pop_contiguous(D, num_children);
    if (!node_a)
      return dv_log_set_error(DV_ERROR_OONP);
    node_b = node_a + num_children - 1;
    
      // Initialize children
    pi_x = pi_a;
    for (node_x = node_a; node_x <= node_b; node_x++) {
      dv_dag_node_init(node_x, node, dv_pidag_get_idx(P, pi_x));
      dv_dag_node_set(D, node_x);
      pi_x++;
      if (node_x->d > D->dmax)
        D->dmax = node_x->d;
    }
    
    // Set node->head, node->tails
    node->head = node_a;
    dv_llist_add(node->tails, (void *) (node_b));
    // Set children's links, node->tails
    for (node_x = node_a; node_x < node_b; node_x++) {
      // x -> x+1
      dv_llist_add(node_x->links, (void *) (node_x + 1));
      (node_x + 1)->pre = node_x;
      // x = create
      pi_x = dv_pidag_get_node(P, node_x);
      if (pi_x->info.kind == dr_dag_node_kind_create_task) {
        node_t = dv_dag_node_pool_pop(D);
        if (!node_t)
          return dv_log_set_error(DV_ERROR_OONP);
        dv_dag_node_init(node_t, node, dv_pidag_get_idx_with_offset(P, pi_x, pi_x->child_offset));
        dv_dag_node_set(D, node_t);
        // x -> Child
        dv_llist_add(node_x->links, (void *) node_t);
        node_t->pre = node_x;
        // Set node->tails
        dv_llist_add(node->tails, (void *) node_t);
      }
    }
    
    // Set node->f
    dv_node_flag_set(node->f, DV_NODE_FLAG_INNER_LOADED);
    dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
    
  }    

  return DV_OK;
}

int dv_dag_destroy_node_innner(dv_dag_t *D, dv_dag_node_t *node) {
  // Check flags node->f
  if (dv_is_set(node) && dv_is_union(node)
      && dv_is_inner_loaded(node)) {

    dv_dag_node_t *node_a, *node_b, *node_x, *node_t;
    
    node_a = node->head;
    node_b = (dv_dag_node_t *) dv_llist_pop(node->tails);
    dv_check(node_b >= node_a);

    // Unset node->head, node->tails
    for (node_x = node_a; node_x <= node_b; node_x++) {
      dv_check(!dv_is_union(node_x) || !dv_is_inner_loaded(node_x));
      dv_dag_node_pool_push(D, node_x);
    }
    while (!dv_llist_is_empty(node->tails)) {
      node_t = (dv_dag_node_t *) dv_llist_pop(node->tails);
      dv_check(!dv_is_union(node_t) || !dv_is_inner_loaded(node_t));
      dv_dag_node_pool_push(D, node_t);
    }

    // Unset node->f
    dv_node_flag_remove(node->f, DV_NODE_FLAG_INNER_LOADED);

  }  
  return DV_OK;
}

static void dv_dag_clear_shrinked_nodes_1(dv_dag_t *D, dv_dag_node_t *node) {
  dv_dag_destroy_node_innner(D, node);
}

static void dv_dag_clear_shrinked_nodes_r(dv_dag_t *D, dv_dag_node_t *node) {
  if (!dv_is_set(node))
    return;
  if (dv_is_union(node) && dv_is_inner_loaded(node)) {

    if (dv_is_shrinked(node)
        && !dv_is_shrinking(node)
        && !dv_is_expanding(node)) {
      // check if node has inner_loaded node
      int has_no_innerloaded_node = 1;
      dv_stack_t s[1];
      dv_stack_init(s);
      dv_check(node->head);
      dv_stack_push(s, (void *) node->head);
      while (s->top) {
        dv_dag_node_t * x = (dv_dag_node_t *) dv_stack_pop(s);
        if (dv_is_set(node) && dv_is_union(x) && dv_is_inner_loaded(x))
          has_no_innerloaded_node = 0;
        dv_dag_node_t *xx = NULL;
        while (xx = (dv_dag_node_t *) dv_llist_iterate_next(x->links, xx)) {
          dv_stack_push(s, (void *) xx);
        }      
      }
      if (has_no_innerloaded_node) {
        // destroy inner
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
  dv_dag_node_t *u = NULL;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links, u)) {
    dv_dag_clear_shrinked_nodes_r(D, u);
  }
}

void dv_dag_clear_shrinked_nodes(dv_dag_t *D) {
  dv_dag_clear_shrinked_nodes_r(D, D->rt);
}

void dv_dag_init(dv_dag_t *D, dv_pidag_t *P) {
  D->P = P;
  dv_dag_node_pool_init(D);
  dv_check(!dv_dag_node_pool_is_empty(D));
  D->rt = dv_dag_node_pool_pop(D);
  dv_dag_node_init(D->rt, 0, 0);
  if (P)
    dv_dag_node_set(D, D->rt);
  D->dmax = 0;
  D->bt = 0.0;
  D->et = 0.0;
  dv_llist_init(D->itl);
  D->cur_d = 0;
  D->cur_d_ex = 0;
}

dv_dag_t * dv_dag_create_new_with_pidag(dv_pidag_t *P) {
  /* Get new DAG */
  dv_check(CS->nD < DV_MAX_DAG);
  dv_dag_t * D = &CS->D[CS->nD++];
  dv_dag_init(D, P);

  /* Set values */
  dr_pi_dag_node *pi = dv_pidag_get_node(P, D->rt);
  D->bt = pi->info.start.t - 1;
  D->et = pi->info.end.t + 1;
  
  // Traverse pidag's nodes
  /*
  dv_stack_t s[1];
  dv_stack_init(s);
  dv_stack_push(s, (void *) G->rt);
  while (s->top) {
    dv_dag_node_t * x = (dv_dag_node_t *) dv_stack_pop(s);
    pi = dv_pidag_get_node(x);
    p = dv_traverse_node(pi, x, p, plim, s, G);
  }
  */

  return D;
}

/*-----------------end of DAG's functions-----------*/
