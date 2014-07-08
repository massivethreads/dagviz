#include "dagviz.h"

/*---------PIDAG Reader's functions---------------*/

dr_pi_dag_node * dv_pidag_get_node(long idx) {
  dv_check(idx >=0 && idx < P->n);
  dr_pi_dag_node *pi = P->T + idx;
  return pi;
}

/*---------end of PIDAG Reader's functions---------------*/




/*-----------------Read .dag format-----------*/

void dv_read_dag_file_to_pidag(char * filename, dr_pi_dag * P) {
  int err;
  int fd;
  struct stat statbuf;
  void *dp;
  
  fd = open(filename, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "open: %d\n", errno);
    exit(1);
  }
  err = fstat(fd, &statbuf);
  if (err < 0) {
    fprintf(stderr, "fstat: %d\n", errno);
    exit(1);
  }
  printf("File status:\n"
         "  st_size = %d bytes (%0.0lfMB)\n",
         (int) statbuf.st_size,
         ((double) statbuf.st_size) / (1024.0 * 1024.0));
  
  dp = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
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
  dr_pi_string_table * S = (dr_pi_string_table *) dv_malloc( sizeof(dr_pi_string_table) );
  *S = *stp;
  S->I = (long *) (stp + 1);
  S->C = (const char *) (S->I + S->n);
  P->S = S;
  
  close(fd);
}

static void dv_dag_node_init(dv_dag_node_t *u, dv_dag_node_t *p, long pii) {
  u->pii = pii;
  dv_node_flag_init(u->f);
  u->d = (p)?(p->d + 1):0;

  u->parent = p;
  u->pre = 0;
  dv_llist_init(u->links);
  u->head = 0;
  dv_llist_init(u->tails);

  u->x = 0.0;
  u->y = 0.0;
  u->xp = 0.0;
  u->xpre = 0.0;
  u->lw = 0.0;
  u->rw = 0.0;
  u->dw = 0.0;
  u->link_lw = 0.0;
  u->link_rw = 0.0;
  u->link_dw = 0.0;
  u->avoid_inward = 0;

  u->vl = 0;
  u->vl_in = 0;
  u->lc = 0L;
  u->rc = 0L;
  u->dc = 0L;
  u->c = 0L;

  u->started = 0.0;
}

static dv_dag_node_t * dv_traverse_node(dr_pi_dag_node *pi, dv_dag_node_t *u, dv_dag_node_t *p, dv_dag_node_t *plim, dv_stack_t *s, dv_dag_t *G) {
  dr_pi_dag_node * pi_a;
  dr_pi_dag_node * pi_b;
  dr_pi_dag_node * pi_x;
  dr_pi_dag_node * pi_t;
  dv_dag_node_t * u_a;
  dv_dag_node_t * u_b;
  dv_dag_node_t * u_x;
  dv_dag_node_t * u_t;
  if (pi->info.kind == dr_dag_node_kind_section
      || pi->info.kind == dr_dag_node_kind_task) {
    pi_a = pi + pi->subgraphs_begin_offset;
    pi_b = pi + pi->subgraphs_end_offset;
    if (pi_a < pi_b) {
      
      // Set u.f
      dv_node_flag_set(u->f, DV_NODE_FLAG_UNION);
      dv_node_flag_set(u->f, DV_NODE_FLAG_SHRINKED);
      // Allocate memory for all child nodes
      u_a = p;
      u_b = p + (pi_b - pi_a);
      dv_check(u_b <= plim);
      p = u_b;
      // Set ux.pi
      pi_x = pi_a;
      for (u_x = u_a; u_x < u_b; u_x++) {
        dv_dag_node_init(u_x, u, pi_x - P->T);
        pi_x++;
        if (u_x->d > G->dmax)
          G->dmax = u_x->d;
      }
      // Push child nodes to stack
      for (u_x = u_b-1; u_x >= u_a; u_x--) {
        dv_stack_push(s, (void *) u_x);
      }
      // Set u.head, u.tails
      u->head = u_a;
      dv_llist_add(u->tails, (void *) (u_b - 1));
      // Set ux.links, u.tails
      for (u_x = u_a; u_x < u_b - 1; u_x++) {
        // x -> x+1
        dv_llist_add(u_x->links, (void *) (u_x + 1));
        (u_x + 1)->pre = u_x;
        pi_x = dv_pidag_get_node(u_x->pii);
        if (pi_x->info.kind == dr_dag_node_kind_create_task) {
          pi_t = pi_x + pi_x->child_offset;
          dv_check(p < plim);
          u_t = p++;
          dv_dag_node_init(u_t, u, pi_t - P->T);
          // Push u_t to stack
          dv_stack_push(s, (void *) u_t);
          // c -> T
          dv_llist_add(u_x->links, (void *) u_t);
          u_t->pre = u_x;
          // --> u.tails
          dv_llist_add(u->tails, (void *) u_t);
        }
      }

    }
  }

  return p;
}

void dv_convert_pidag_to_dvdag(dr_pi_dag *P, dv_dag_t *G) {
  G->n = P->n;
  G->nw = P->num_workers;
  G->st = P->S;
  // Initialize rt, T
  G->T = (dv_dag_node_t *) dv_malloc( sizeof(dv_dag_node_t) * G->n );
  dv_dag_node_t * p = G->T;
  dv_dag_node_t * plim = G->T + G->n;
  dv_check(p < plim);
  G->rt = p++;
  dv_dag_node_init(G->rt, 0, 0);
  G->dmax = 0;
  dr_pi_dag_node *pi = dv_pidag_get_node(G->rt->pii);
  G->bt = pi->info.start.t - 1;
  G->et = pi->info.end.t + 1;
  // Traverse pidag's nodes
  dv_stack_t s[1];
  dv_stack_init(s);
  dv_stack_push(s, (void *) G->rt);
  while (s->top) {
    dv_dag_node_t * x = (dv_dag_node_t *) dv_stack_pop(s);
    pi = dv_pidag_get_node(x->pii);
    p = dv_traverse_node(pi, x, p, plim, s, G);
  }
  // Drawing parameters
  G->init = 1;
  G->zoom_ratio = 1.0;
  G->x = G->y = 0.0;
  G->basex = G->basey = 0.0;
  dv_llist_init(G->itl);
  // Layout status
  G->cur_d = 0;
  G->cur_d_ex = 0;
}

/*-----------------end of Read .dag format-----------*/
