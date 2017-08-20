#include "dagrenderer.h"

DAGRenderer::DAGRenderer() {
  if (!dm_global_state_initialized()) {
    dm_global_state_init();
  }
}

char *
DAGRenderer::parse_python_string(PyObject * pyobj) {
  char * s = NULL;
  if (!PyArg_Parse(pyobj, "s", &s)) {
    dm_perror("could not parse PyObject.");
    return s;
  }
  return s;
}

int
DAGRenderer::parse_python_int(PyObject * pyobj) {
  int i = -1;
  if (!PyArg_Parse(pyobj, "i", &i)) {
    dm_perror("could not parse PyObject.");
    return i;
  }
  return i;
}

void
DAGRenderer::setDAG(char * filename) {
  if (mDAG) {
    dm_perror("this DAGRenderer has got a DAG.");
    return;
  }
  dm_dag_t * D = dm_add_dag(filename);
  if (!D) {
    dm_perror("DAGModel could not add DAG from %s.", filename);
    return;
  }
  mDAG = D;
  layout_(D);
}

void
DAGRenderer::setViewport_(QWidget * VP) {
  if (mViewport) {
    dm_perror("this DAGRenderer has got a Viewport.");
    return;
  }
  mViewport = VP;
}

PyObject *
DAGRenderer::compute_dag_statistics(int D_id) {
  /* compute */
  dm_dag_t * D = dm_get_dag(D_id);
  dm_compute_dag(D);

  /* build result into a Python object */
  PyObject * ret = NULL;
  if (D) {
    dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, D->rt);    
    ret = Py_BuildValue("{s:s,s:K,s:K,s:K,s:[L,L,L,L]}",
                        "name", D->name_on_graph,
                        "work", DMG->SBG->work[D_id],
                        "delay", DMG->SBG->delay[D_id],
                        "nowork", DMG->SBG->nowork[D_id],
                        "counters", pi->info.counters_1[0],
                        pi->info.counters_1[1], pi->info.counters_1[2],
                        pi->info.counters_1[3]);
  } else {
    dm_perror("could not calculate DAG %d.", D_id);
  }
  return ret;
}

void
DAGRenderer::layout_dag_(dm_dag_t * D) {
  dm_layout_dag(D);
}

void
DAGRenderer::layout_(dm_dag_t * D) {
  layout_dag_(D);
}

void
DAGRenderer::do_animation_tick() {
  if (!animation_on) {
    dm_perror("do_animation_tick() is called while there is no on-going animation.\n");
    return;
  }
  dm_animation_tick(mDAG->anim);
  if (!mDAG->anim->on) {
    do_animation_stop();
  }  
  layout();
  update();
}

void
DAGRenderer::do_animation_start() {
  if (!animation_on) {
    animation_on = true;
    if (!animation_timer) {
      animation_timer = new QTimer();
      QObject::connect(animation_timer, &QTimer::timeout, this, &DAGRenderer::do_animation_tick);
      //QObject::connect(animation_timer, SIGNAL(timeout()), this, SLOT(do_animation_tick()));
    }
    animation_timer->start(mDAG->anim->step);
  } else {
    dm_pwarning("animation is already started.");
  }
}

void
DAGRenderer::do_animation_stop() {
  if (animation_on) {
    animation_timer->stop();
    animation_on = false;
  } else {
    dm_pwarning("animation is already stopped.");
  }
}

void
DAGRenderer::do_animation_add_node(dm_dag_node_t * node) {
  dm_animation_add_node(mDAG->anim, node);
  /* start animation timer if necessary */
  if (mDAG->anim->on && !animation_on) {
    do_animation_start();
  }
}

void
DAGRenderer::do_animation_remove_node(dm_dag_node_t * node) {
  dm_animation_remove_node(mDAG->anim, node);
}

void
DAGRenderer::do_animation_reverse_node(dm_dag_node_t * node) {
  dm_animation_reverse_node(mDAG->anim, node);
}

void
DAGRenderer::do_expanding_one_1(dm_dag_t * D, dm_dag_node_t * node) {
  if (!dm_is_inner_loaded(node)) {
    if (dm_dag_build_node_inner(D, node) != DM_OK) {
      return;
    }
  }
  /* add to animation */
  if (dm_is_shrinking(node)) {
    dm_node_flag_remove(node->f, DM_NODE_FLAG_SHRINKING);
    dm_node_flag_set(node->f, DM_NODE_FLAG_SHRINKED);
    dm_node_flag_set(node->f, DM_NODE_FLAG_EXPANDING);
    do_animation_reverse_node(node);
  } else {
    dm_node_flag_set(node->f, DM_NODE_FLAG_EXPANDING);
    do_animation_add_node(node);
  }
}

void
DAGRenderer::do_expanding_one_r(dm_dag_t * D, dm_dag_node_t * node) {
  if (!dm_is_set(node)) {
    dm_dag_node_set(D, node);
  }
  if (dm_is_union(node)) {
    if ((!dm_is_inner_loaded(node)
         || dm_is_shrinked(node)
         || dm_is_shrinking(node))
        && !dm_is_expanding(node)) {
      do_expanding_one_1(D, node);
    } else {
      /* Call inward */
      do_expanding_one_r(D, node->head);
    }
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    do_expanding_one_r(D, x);
  }
}

void
DAGRenderer::do_expanding_one_(dm_dag_t * D) {
  /*
  dm_do_expanding_one(D);
  */
  do_expanding_one_r(D, D->rt);
}

void
DAGRenderer::do_collapsing_one_1(_unused_ dm_dag_t * D, dm_dag_node_t * node) {
  /* add to animation */
  if (dm_is_expanding(node)) {
    dm_node_flag_remove(node->f, DM_NODE_FLAG_EXPANDING);
    dm_node_flag_remove(node->f, DM_NODE_FLAG_SHRINKED);
    dm_node_flag_set(node->f, DM_NODE_FLAG_SHRINKING);
    do_animation_reverse_node(node);
  } else {
    dm_node_flag_set(node->f, DM_NODE_FLAG_SHRINKING);
    do_animation_add_node(node);
  }
}

void
DAGRenderer::do_collapsing_one_depth_r(dm_dag_t * D, dm_dag_node_t * node, int depth) {
  if (!dm_is_set(node)) {
    return;
  }
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
      do_collapsing_one_1(D, node);
    } else {
      /* Call inward */
      do_collapsing_one_depth_r(D, node->head, depth);
    }
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    do_collapsing_one_depth_r(D, x, depth);
  }
}

void
DAGRenderer::do_collapsing_one_(dm_dag_t * D) {
  /*
  dm_do_collapsing_one(D);
  */
  do_collapsing_one_depth_r(D, D->rt, D->collapsing_d - 1);
}

static void
draw_edge_1(QPainter * qp, dm_dag_t * D, dm_dag_node_t * u, dm_dag_node_t * v) {
  int cid = 0;
  /* Get coordinates */
  double x1, y1, x2, y2;
  dm_node_coordinate_t * uc = &u->c[cid];
  dm_node_coordinate_t * vc = &v->c[cid];
  x1 = uc->x;
  y1 = uc->y + uc->dw;
  x2 = vc->x;
  y2 = vc->y;
  if (y1 > y2)
    return;

  /* Draw edge */
  QPainterPath path;
  path.moveTo(x1, y1);
  dr_pi_dag_node * pi;
  pi = dm_pidag_get_node_by_dag_node(D->P, u);
  if (pi->info.kind == dr_dag_node_kind_create_task)
    path.lineTo(x2, y1);
  else
    path.lineTo(x1, y2);
  path.lineTo(x2, y2);
  qp->strokePath(path, QPen(Qt::black));
}

static void
drend_draw_path_isosceles_triangle(QPainterPath * path, double x, double y, double w, double h) {
  path->moveTo(x + w/2.0, y);
  path->lineTo(x        , y + h);
  path->lineTo(x + w    , y + h);
  path->lineTo(x + w/2.0, y);
}

static void
drend_draw_path_isosceles_triangle_upside_down(QPainterPath * path, double x, double y, double w, double h) {
  path->moveTo(x        , y);
  path->lineTo(x + w/2.0, y + h);
  path->lineTo(x + w    , y);
  path->lineTo(x        , y);
}

static void
drend_draw_path_rectangle(QPainterPath * path, double x, double y, double w, double h) {
  path->addRect(x, y, w, h);
}

static void
drend_draw_path_rounded_rectangle(QPainterPath * path, double x, double y, double w, double h) {
  path->addRoundedRect(QRectF(x, y, w, h), w/8.0, h/8.0);
}

static void
drend_draw_path_circle(QPainterPath * path, double x, double y, double w) {
  path->addEllipse(x, y, w, w);
}

static QColor
lookup_color(dr_pi_dag_node * pi, double alpha) {
  int v = pi->info.worker;
  
  QStringList colors = QColor::colorNames();
  int n_colors = colors.size();
  int i = (v + n_colors) % n_colors;
  QColor color = QColor(colors.at(i));
  color.setAlphaF(alpha * color.alphaF());

  return color;
}

static QLinearGradient
lookup_gradient(_unused_ dr_pi_dag_node * pi, _unused_ double alpha) {
  QVector<QColor> stop_colors = {QColor(255,165,0), Qt::yellow, Qt::cyan};
  int n = stop_colors.size();
  
  //QLinearGradient g = QLinearGradient(0.0, 0.5, 1.0, 0.5);
  QLinearGradient g = QLinearGradient(0.0, 0.0, 1.0, 1.0);
  g.setCoordinateMode(QGradient::ObjectBoundingMode);
  g.setSpread(QGradient::PadSpread);
  
  double step = 1.0 / (n - 1);
  for (int i = 0; i < n; i++) {
    QColor color = stop_colors[i];
    color.setAlphaF(alpha * color.alphaF());
    g.setColorAt(i * step, color);
  }
  
  return g;
}

static void
draw_dag_node_1(QPainter * qp, dm_dag_t * D, dm_dag_node_t * node, _unused_ int * on_global_cp) {
  /* Get inputs */
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
  dr_dag_node_kind_t kind = pi->info.kind;
  int cid = 0;
  dm_node_coordinate_t * nodeco = &node->c[cid];
  double x = nodeco->x;
  double y = nodeco->y;

  /* Count drawn node */
  if (node->d > D->cur_d) {
    D->cur_d = node->d;
  }
  if (dm_is_union(node) && dm_is_inner_loaded(node)
      && dm_is_shrinked(node)
      && node->d < D->cur_d_ex) {
    D->cur_d_ex = node->d;
  }
  
  /* Alpha, Margin */
  double alpha = 1.0;
  double margin = 0.0;

  /* Coordinates */
  double xx, yy, w, h;
  
  /* Draw path */
  QPainterPath path;
  if (kind == dr_dag_node_kind_section || kind == dr_dag_node_kind_task) {
    
    if (dm_is_union(node)) {

      if (dm_is_inner_loaded(node)
          && (dm_is_expanding(node) || dm_is_shrinking(node))) {

        /* Calculate alpha, margin */
        margin = DMG->opts.union_node_puffing_margin;
        if (dm_is_expanding(node)) {
          alpha *= dm_get_alpha_fading_out(D, node);
          margin *= dm_get_alpha_fading_in(D, node);
        } else {
          alpha *= dm_get_alpha_fading_in(D, node);
          margin *= dm_get_alpha_fading_out(D, node);
        }
        /* Calculate coordinates: large-sized box */
        xx = x - nodeco->lw - margin;
        yy = y - margin;
        w = nodeco->lw + nodeco->rw + 2 * margin;
        h = nodeco->dw + 2 * margin;
      
      } else {
      
        /* Calculate coordinates: normal-sized box */
        xx = x - nodeco->lw;
        yy = y;
        w = nodeco->lw + nodeco->rw;
        h = nodeco->dw;
      
      }

      /* Draw path */
      if (kind == dr_dag_node_kind_section) {
        drend_draw_path_rounded_rectangle(&path, xx, yy, w, h);
      } else {
        drend_draw_path_rectangle(&path, xx, yy, w, h);
      }

    } else { // is collective but not union

      /* Calculate coordinates: normal-sized box */
      xx = x - nodeco->lw;
      yy = y;
      w = nodeco->lw + nodeco->rw;
      h = nodeco->dw;

      /* Draw path */
      if (kind == dr_dag_node_kind_section) {
        drend_draw_path_rounded_rectangle(&path, xx, yy, w, h);
      } else {
        drend_draw_path_rectangle(&path, xx, yy, w, h);
      }
      
    }
    
  } else {
    
    /* Calculate coordinates */
    xx = x - nodeco->lw;
    yy = y;
    w = nodeco->lw + nodeco->rw;
    h = nodeco->dw;
    
    /* Draw path */
    if (kind == dr_dag_node_kind_create_task)
      drend_draw_path_isosceles_triangle(&path, xx, yy, w, h);
    else if (kind == dr_dag_node_kind_wait_tasks)
      drend_draw_path_isosceles_triangle_upside_down(&path, xx, yy, w, h);
    else
      drend_draw_path_circle(&path, xx, yy, w);

  }
  
  /* Calculate alpha (based on parent) */
  if (dm_is_shrinking(node->parent)) {
    alpha *= dm_get_alpha_fading_out(D, node->parent);
  } else if (dm_is_expanding(node->parent)) {
    alpha *= dm_get_alpha_fading_in(D, node->parent);
  }
  if (alpha > 1.0) alpha = 1.0;
  if (alpha < 0.0) alpha = 0.0;

  /* Color */
  QColor pen_color = QColor(Qt::black);
  pen_color.setAlphaF(alpha * pen_color.alphaF());
  QPen pen = QPen(pen_color, DMG->opts.nlw_collective_node_factor * DMG->opts.nlw);
  QColor brush_color = lookup_color(pi, alpha);
  QBrush brush = QBrush(brush_color);
  if (pi->info.worker == -1) {
    QLinearGradient brush_gradient = lookup_gradient(pi, alpha);
    brush = QBrush(brush_gradient);
  }

  /* Draw node's filling */
  qp->fillPath(path, brush);
  
  /* Draw node's border */
  qp->strokePath(path, pen);

  /* Highlight */
  if (node->highlight) {
    qp->fillPath(path, QColor(30, 30, 30, 128));
  }
}

static void
draw_dag_node_r(QPainter * qp, dm_dag_t * D, dm_dag_node_t * node, int * parent_on_global_cp) {
  if (!node || !dm_is_set(node))
    return;

  /* Check if node is still on the global critical paths */
  int me_on_global_cp[DM_NUM_CRITICAL_PATHS];
  int cp;
  for (cp = 0; cp < DM_NUM_CRITICAL_PATHS; cp++) {
    if (parent_on_global_cp[cp] && dm_node_flag_check(node->f, DMG->oncp_flags[cp]))
      me_on_global_cp[cp] = 1;
    else
      me_on_global_cp[cp] = 0;
  }
  
  //if (!dm_dag_node_is_invisible(V, node)) {
  if (1) {
    /* Draw node */
    if (!dm_is_union(node) || !dm_is_inner_loaded(node)
        || dm_is_shrinked(node) || dm_is_shrinking(node)) {
      draw_dag_node_1(qp, D, node, me_on_global_cp);
    }
    /* Call inward */
    if (!dm_is_single(node)) {
      draw_dag_node_r(qp, D, node->head, me_on_global_cp);
    }
  }
  
  /* Call link-along */
  //if (!dm_dag_node_link_is_invisible(V, node)) {
  if (1) {
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
      /* Draw edge first */
      QPainterPath path;
      if (dm_is_single(node)) {
        if (dm_is_single(x))
          draw_edge_1(qp, D, node, x);
        else
          draw_edge_1(qp, D, node, dm_dag_node_get_single_head(x->head));      
      } else {
        dm_dag_node_t * tail = NULL;
        while ( (tail = dm_dag_node_traverse_tails(node, tail)) ) {
          dm_dag_node_t * last = dm_dag_node_get_single_last(tail);        
          if (dm_is_single(x))
            draw_edge_1(qp, D, last, x);
          else
            draw_edge_1(qp, D, last, dm_dag_node_get_single_head(x->head));
        }      
      }
      qp->strokePath(path, QPen(Qt::black));
      /* Call recursively then */
      draw_dag_node_r(qp, D, x, parent_on_global_cp);
    }
  }
}

void
DAGRenderer::draw_dag_(QPainter * qp, dm_dag_t * D) {
  // qp->setPen(Qt::blue);
  // qp->setBrush(Qt::blue);
  // qp->drawRect(100, 120, 50, 30);
  // qp->drawText(200, 135, "I'm drawn in C++");

  dm_llist_init(D->itl);
  D->cur_d = 0;
  D->cur_d_ex = D->dmax;
  int on_global_cp[DM_NUM_CRITICAL_PATHS];
  int i;
  for (i = 0; i < DM_NUM_CRITICAL_PATHS; i++) {
    if (D->show_critical_paths[i])
      on_global_cp[i] = 1;
    else
      on_global_cp[i] = 0;
  }
  draw_dag_node_r(qp, D, D->rt, on_global_cp);
}

void
DAGRenderer::draw_(QPainter * qp, dm_dag_t * D) {
  qp->setRenderHint(QPainter::Antialiasing);
  draw_dag_(qp, D);
}

PyObject *
DAGRenderer::get_dag_node_info_(dm_dag_node_t * node) {
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(mDAG->P, node);
  /* build DAG node's info into a Python object */
  PyObject * ret = NULL;
  if (pi) {
    ret = Py_BuildValue("{s:(L,i,i,L,L),s:(L,i,i,L,L),s:L,s:L,s:L,s:L,s:L,s:i,s:i,s:i,s:[L,L,L,L]}",
                        "start", pi->info.start.t, pi->info.start.worker, pi->info.start.cpu, pi->info.start.pos.file_idx, pi->info.start.pos.line,
                        "end", pi->info.end.t, pi->info.end.worker, pi->info.end.cpu, pi->info.end.pos.file_idx, pi->info.end.pos.line,
                        "est", pi->info.est,
                        "t_1", pi->info.t_1,
                        "t_inf", pi->info.t_inf,
                        "first_ready_t", pi->info.first_ready_t,
                        "last_start_t", pi->info.last_start_t,
                        "worker", pi->info.worker,
                        "cpu", pi->info.cpu,
                        "kind", pi->info.kind,
                        "counters", pi->info.counters_1[0], pi->info.counters_1[1], pi->info.counters_1[2], pi->info.counters_1[3]                        
                        );
  }
  return ret;
}

/***** end of Rendering DAG *****/

