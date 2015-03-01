#include "dagviz.h"

char * dv_get_node_kind_name(dr_dag_node_kind_t kind) {
  switch (kind) {
  case dr_dag_node_kind_create_task:
    return "create";
  case dr_dag_node_kind_wait_tasks:
    return "wait";
  case dr_dag_node_kind_other:
    return "other";
  case dr_dag_node_kind_end_task:
    return "end";
  case dr_dag_node_kind_section:
    return "section";
  case dr_dag_node_kind_task:
    return "task";
  default:
    return "unknown";
  }
}

char * dv_get_edge_kind_name(dr_dag_edge_kind_t kind) {
  switch (kind) {
  case dr_dag_edge_kind_end:
    return "end";
  case dr_dag_edge_kind_create:
    return "create";
  case dr_dag_edge_kind_create_cont:
    return "create_cont";
  case dr_dag_edge_kind_wait_cont:
    return "wait_cont";
  case dr_dag_edge_kind_other_cont:
    return "other_cont";
  case dr_dag_edge_kind_max:
    return "max";
  default:
    return "unknown";
  }
}

void dv_print_pidag_node(dr_pi_dag_node * dn, int i) {
  printf(
    "DAG node %d:\n"
    "  info.start: %llu, %p, %ld, %ld\n"
    "  info.end: %llu, %p, %ld, %ld\n"
    //"  info.est: %llu\n"
    "  info.kind: %d (%s)\n"
    "  info.logical_node_counts: %ld/%ld/%ld\n"
    "  info.logical_edge_counts: %ld/%ld/%ld/%ld\n"
    "  info.n_child_create_tasks: %ld\n"
    "  info.worker/cpu: %d, %d\n"
    "  edges_begin/end: %ld, %ld\n",
    i,
    (unsigned long long) dn->info.start.t,
    dn->info.start.pos.file,
    dn->info.start.pos.file_idx,
    dn->info.start.pos.line,
    (unsigned long long) dn->info.end.t,
    dn->info.end.pos.file,
    dn->info.end.pos.file_idx,
    dn->info.end.pos.line,
    //(unsigned long long) dn->info.est,
    dn->info.kind,
    dv_get_node_kind_name(dn->info.kind),
    dn->info.logical_node_counts[0],
    dn->info.logical_node_counts[1],
    dn->info.logical_node_counts[2],
    dn->info.logical_edge_counts[0],
    dn->info.logical_edge_counts[1],
    dn->info.logical_edge_counts[2],
    dn->info.logical_edge_counts[3],
    dn->info.n_child_create_tasks,
    dn->info.worker,
    dn->info.cpu,
    dn->edges_begin,
    dn->edges_end
  );
  if (dn->info.kind == dr_dag_node_kind_create_task) {
    printf("  child_offset: %ld\n",
           dn->child_offset
           );
  } else if (dn->info.kind == dr_dag_node_kind_section
             || dn->info.kind == dr_dag_node_kind_task) {
    printf("  subgraphs_begin_offset: %ld\n"
           "  subgraphs_end_offset: %ld\n",
           dn->subgraphs_begin_offset,
           dn->subgraphs_end_offset);
  }
}

void dv_print_pidag_edge(dr_pi_dag_edge * de, int i) {
  printf(
         "DAG edge %d:\n"
         "  kind: %d (%s)\n"
         "  u: %ld\n"
         "  v: %ld\n",
         i,
         de->kind,
         dv_get_edge_kind_name(de->kind),
         de->u,
         de->v
         );
}

void dv_print_pi_string_table(dr_pi_string_table * stp, int i) {
  printf(
         "String table %d:\n"
         "  n: %ld\n"
         "  sz: %ld\n",
         i,
         stp->n,
         stp->sz
         );
  printf("  I:");
  int j;
  for (j=0; j<stp->n; j++) {
    printf(" %ld", stp->I[j]);
  }
  printf("\n");
  printf("  C:");
  for (j=0; j<stp->n; j++) {
    printf(" \"%s\"\n    ", stp->C + stp->I[j]);
  }
  printf("\n");
}


static void print_dvdag_node(dv_dag_t *D, dv_dag_node_t *node, int i) {
  dr_pi_dag_node *pi = dv_pidag_get_node(D->P, node);
  int kind = pi->info.kind;
  printf("Node %d (%p): %d(%s)\n",         
         i,
         node,
         kind,
         dv_get_node_kind_name(kind));
}

void dv_print_dvdag(dv_dag_t *D) {
  printf(
         "DV DAG: \n"
         "  n: %ld\n"
         "  nw: %ld\n",
         D->P->n,
         D->P->num_workers);
  int i;
  for (i=0; i<D->Tsz; i++)
    if (D->To[i])
      print_dvdag_node(D, D->T + i, i);
}

static void print_layout_node(dv_dag_t *D, dv_dag_node_t *node, int i) {
  /*
  dr_pi_dag_node *pi = dv_pidag_get_node(D->P, node);
  int kind = pi->info.kind;
  printf(
         "  Node %d: (%s)\n"
         "    f(U/Sked/Epding/Sking)=%d%d%d%d\n"
         "    (xpre,xp): (%0.1f,%0.1f)\n"
         "    (x,y): (%0.1f,%0.1f)\n"
         "    (lw,rw,dw): (%0.1f,%0.1f,%0.1f)\n"
         "    (link_lw/rw/dw): (%0.1f,%0.1f,%0.1f)\n",
         i,
         dv_get_node_kind_name(kind),
         dv_is_union(node),
         dv_is_shrinked(node),
         dv_is_expanding(node),
         dv_is_shrinking(node),
         node->xpre, node->xp,
         node->x, node->y,
         node->lw, node->rw, node->dw,
         node->link_lw, node->link_rw, node->link_dw);
  */
}

void dv_print_layout(dv_dag_t *D) {
  printf(
         "Layout of DV DAG: (n=%ld)\n",
         D->P->n);
  int i;
  for (i=0; i<D->Tsz; i++)
    if (D->To[i])
      print_layout_node(D, D->T + i, i);
}


static void * read_pi_dag_node(void * dp, dr_pi_dag_node * des) {
  dr_pi_dag_node * np = (dr_pi_dag_node *) dp;
  *des = *np;
  return ++np;
}

static void * read_pi_dag_edge(void * dp, dr_pi_dag_edge * des) {
  dr_pi_dag_edge * ep = (dr_pi_dag_edge *) dp;
  *des = *ep;
  return ++ep;
}

static void * read_pi_string_table(void * dp, dr_pi_string_table * des) {
  dr_pi_string_table * stp = (dr_pi_string_table *) dp;
  *des = *stp;
  des->I = (long *) ++stp;
  des->C = (const char *) (des->I + des->n);
  return dp + des->sz;
}


void dv_print_dag_file(char * filename) {
  int err;
  int fd;
  struct stat statbuf;
  void *dp, *dp_head;
  
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
  printf("File state:\n"
         "  st_size = %d\n",
         (int) statbuf.st_size);
  
  dp = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (!dp) {
    fprintf(stderr, "mmap: error\n");
    exit(1);
  }

  dp_head = dp;
  long n, m, nw;
  long * ldp = (long *) dp;
  n = *ldp;
  ldp++;
  m = *ldp;
  ldp++;
  nw = *ldp;
  ldp++;
  printf("n = %ld\nm = %ld\nnw = %ld\n", n, m, nw);

  dp = ldp;
  dr_pi_dag_node dn;
  int i;
  for (i=0; i<n; i++) {
    dp = read_pi_dag_node(dp, &dn);
    dv_print_pidag_node(&dn, i);
  }
  dr_pi_dag_edge de;
  for (i=0; i<m; i++) {
    dp = read_pi_dag_edge(dp, &de);
    dv_print_pidag_edge(&de, i);
  }
  dr_pi_string_table S;
  dp = read_pi_string_table(dp, &S);
  dv_print_pi_string_table(&S, 0);
  printf("Have read %ld bytes\n", dp - dp_head);
  
  close(fd);
}

void dv_check_layout(dv_dag_t *G) {
  /*int i;
  for (i=0; i<G->n; i++) {
    dv_dag_node_t *node = G->T + i;
    int kind = node->info->kind;
    if (kind == dr_dag_node_kind_create_task) {
      printf(
             "%ld(%0.0f,%0.0f) -> "
             "%ld(%0.0f,%0.0f), "
             "%ld(%0.0f,%0.0f)\n",
             node->idx, node->vl->c, node->hl->c,
             node->el->idx, node->el->vl->c, node->el->hl->c,
             node->er->idx, node->er->vl->c, node->er->hl->c);
    } else if (kind == dr_dag_node_kind_wait_tasks
               || kind == dr_dag_node_kind_end_task) {
      printf(
             "%ld(%0.0f,%0.0f) -> ",
             node->idx, node->vl->c, node->hl->c);
      if (node->ej)
        printf(
               "%ld(%0.0f,%0.0f)\n",
               node->ej->idx, node->ej->vl->c, node->ej->hl->c);
      else
        printf("null\n");
    }
  }*/
}

