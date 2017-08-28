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
  fileName = (char *) dm_malloc((strlen(filename) + 1) * sizeof(char));
  strcpy(fileName, filename);
  shortFileName = D->P->short_filename;
  shortName = dm_get_distinct_components_name_string(D->P->short_filename);
}

void
DAGRenderer::addViewport_(QWidget * VP, int cid) {
  mViewports[cid].append(VP);
  if (cid == DM_LAYOUT_PARAPROF_COORDINATE && !mDAG->H) {
    mDAG->H = (dm_histogram_t *) dm_malloc( sizeof(dm_histogram_t) );
    dm_histogram_init(mDAG->H);
    mDAG->H->D = mDAG;
    dm_histogram_reset(mDAG->H);
  }
};

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
DAGRenderer::update() {
  for (int cid = 0; cid < DM_NUM_COORDINATES; cid++) {
    QVector<QWidget *>::iterator i;
    for (i = mViewports[cid].begin(); i != mViewports[cid].end(); i++) {
      QWidget * VP = *i;
      VP->update();
    }
  }
}

void
DAGRenderer::update(QWidget * VP) {
  VP->update();
}

void
DAGRenderer::layout1_(dm_dag_t * D, int cid) {
  dm_dag_layout1(D, cid);
}

void
DAGRenderer::layout2_(dm_dag_t * D, int cid) {
  dm_dag_layout2(D, cid);
}

void
DAGRenderer::layout3_(dm_dag_t * D, int cid) {
  dm_dag_layout3(D, cid);
}

void
DAGRenderer::layout__(dm_dag_t * D, int cid) {
  double x_proportion = 0.0;
  double y_proportion = 0.0;
  if (anchor[cid].enabled && cid != DM_LAYOUT_PARAPROF_COORDINATE) {
    if (anchor[cid].node) {
      dm_node_coordinate_t * c = &anchor[cid].node->c[cid];
      double left = c->x - c->lw;
      double right = c->x + c->rw;
      x_proportion = (anchor[cid].x - left) / (right - left);
      y_proportion = (anchor[cid].y - c->y) / c->dw;
    }
  }

  switch (cid) {
  case DM_LAYOUT_DAG_COORDINATE:
    layout1_(D, cid);
    break;
  case DM_LAYOUT_DAG_BOX_LINEAR_COORDINATE:
  case DM_LAYOUT_DAG_BOX_POWER_COORDINATE:
  case DM_LAYOUT_DAG_BOX_LOG_COORDINATE:
    layout2_(D, cid);
    break;
  case DM_LAYOUT_PARAPROF_COORDINATE:
    layout3_(D, cid);
    break;
  default:
    dm_perror("unknown cid=%d", cid);
    return;
  }
  
  if (anchor[cid].enabled && cid != DM_LAYOUT_PARAPROF_COORDINATE) {
    if (anchor[cid].node) {
      dm_node_coordinate_t * c = &anchor[cid].node->c[cid];
      double left = c->x - c->lw;
      double right = c->x + c->rw;
      double new_anchor_x = x_proportion * (right - left) + left;
      mDx[cid] += anchor[cid].x - new_anchor_x;
      double new_anchor_y = y_proportion * c->dw + c->y;
      mDy[cid] += anchor[cid].y - new_anchor_y;
    }
  }
}

void
DAGRenderer::layout_(dm_dag_t * D) {
  for (int cid = 0; cid < DM_NUM_COORDINATES; cid++) {
    if (mViewports[cid].count() > 0) {
      layout__(D, cid);
    }
  }
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
DAGRenderer::do_motion_tick() {
  if (!motion.on) {
    dm_perror("do_motion_tick() is called while there is no on-going animation.");
    return;
  }
  double t = dm_get_time();
  if (t - motion.start_t < motion.duration) {
    double ratio = (t - motion.start_t) / motion.duration;
    double dx = ratio * (motion.x1 - motion.x0);
    double dy = ratio * (motion.y1 - motion.y0);
    mDx[motion.cid] -= dx;
    mDy[motion.cid] -= dy;
    motion.x0 += dx;
    motion.y0 += dy;
    motion.duration -= t - motion.start_t;
    motion.start_t = t;
  } else {
    double dx = motion.x1 - motion.x0;
    double dy = motion.y1 - motion.y0;
    mDx[motion.cid] -= dx;
    mDy[motion.cid] -= dy;
    do_motion_stop();
  }
  update(motion.VP);
}

void
DAGRenderer::do_motion_start_(double x0, double y0, dm_dag_node_t * node, int cid, QWidget * VP) {
  motion.on = true;
  motion.cid = cid;
  motion.duration = DM_ANIMATION_DURATION;
  motion.VP = VP;
  motion.x0 = x0;
  motion.y0 = y0;
  dm_node_coordinate_t * node_c = &node->c[cid];
  motion.x1 = node_c->x + (node_c->rw - node_c->lw) / 2.0;
  motion.y1 = node_c->y + node_c->dw / 2.0;
  if (!motion.timer) {
    motion.timer = new QTimer();
    QObject::connect(motion.timer, &QTimer::timeout, this, &DAGRenderer::do_motion_tick);
  }
  motion.start_t = dm_get_time();
  if (!motion.timer->isActive()) {
    motion.timer->start(motion.step);
  }
}

void
DAGRenderer::do_motion_start(double x0, double y0, void * node, int cid, void * VP) {
  do_motion_start_(x0, y0, (dm_dag_node_t *) node, cid, (QWidget *) VP);
}

void
DAGRenderer::do_motion_stop() {
  motion.on = false;
  motion.timer->stop();
}

void
DAGRenderer::do_expanding_one_1_(dm_dag_t * D, dm_dag_node_t * node) {
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
  /* Histogram */
  if (D->H && D->H->head_e) {
    dm_histogram_remove_node(D->H, node, NULL);
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_children(node, x)) ) {
      dm_histogram_add_node(D->H, x, NULL);
    }
  }
}

void
DAGRenderer::do_expanding_one_r_(dm_dag_t * D, dm_dag_node_t * node) {
  if (!dm_is_set(node)) {
    dm_dag_node_set(D, node);
  }
  if (dm_is_union(node)) {
    if ((!dm_is_inner_loaded(node)
         || dm_is_shrinked(node)
         || dm_is_shrinking(node))
        && !dm_is_expanding(node)) {
      do_expanding_one_1_(D, node);
    } else {
      /* Call inward */
      do_expanding_one_r_(D, node->head);
    }
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    do_expanding_one_r_(D, x);
  }
}

void
DAGRenderer::do_expanding_one_(dm_dag_t * D) {
  /*
  dm_do_expanding_one(D);
  */
  do_expanding_one_r_(D, D->rt);
}

void
DAGRenderer::do_collapsing_one_1_(_unused_ dm_dag_t * D, dm_dag_node_t * node) {
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
  /* Histogram */
  if (D->H && D->H->head_e) {
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_children(node, x)) ) {
      dm_histogram_remove_node(D->H, x, NULL);
    }
    dm_histogram_add_node(D->H, node, NULL);
  }
}

void
DAGRenderer::do_collapsing_one_r_(dm_dag_t * D, dm_dag_node_t * node) {
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
    if (!has_expanded_node) {
      // collapsing node
      do_collapsing_one_1_(D, node);
    } else {
      /* Call inward */
      do_collapsing_one_r_(D, node->head);
    }
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    do_collapsing_one_r_(D, x);
  }
}

void
DAGRenderer::do_collapsing_one_depth_r_(dm_dag_t * D, dm_dag_node_t * node, int depth) {
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
      // collapsing node
      do_collapsing_one_1_(D, node);
    } else {
      /* Call inward */
      do_collapsing_one_depth_r_(D, node->head, depth);
    }
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    do_collapsing_one_depth_r_(D, x, depth);
  }
}

void
DAGRenderer::do_collapsing_one_(dm_dag_t * D) {
  /*
  dm_do_collapsing_one(D);
  */
  do_collapsing_one_depth_r_(D, D->rt, D->collapsing_d - 1);
}

static void
draw_edge_1(QPainter * qp, dm_dag_t * D, dm_dag_node_t * u, dm_dag_node_t * v, int cid) {
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
lookup_color(dr_pi_dag_node * pi, double alpha = 1.0, int type = 0) {
  int v = pi->info.worker;
  if (type == 1)
    v = pi->info.kind + 5;
  
  QStringList colors = QColor::colorNames();
  int n_colors = colors.size();
  int i = (v + n_colors) % n_colors;
  QColor color = QColor(colors.at(i));
  color.setAlphaF(alpha * color.alphaF());

  return color;
}

static QLinearGradient
lookup_gradient(_unused_ dr_pi_dag_node * pi, _unused_ double alpha, int cid) {
  QVector<QColor> stop_colors = {QColor(255,165,0), Qt::yellow, Qt::cyan};
  int n = stop_colors.size();

  QLinearGradient g = QLinearGradient(0.0, 0.0, 1.0, 1.0);
  if (cid != DM_LAYOUT_DAG_COORDINATE) {
    g = QLinearGradient(0.0, 0.5, 1.0, 0.5);
  }
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
draw_highlight_node(QPainter * qp, QPainterPath & path, _unused_ dm_dag_node_t * node) {
  QLinearGradient g = QLinearGradient(0.0, 0.5, 1.0, 0.5);
  g.setCoordinateMode(QGradient::ObjectBoundingMode);
  g.setSpread(QGradient::PadSpread);
  g.setColorAt(0.0, QColor(255, 255, 255, 10));
  g.setColorAt(1.0, QColor(255, 255, 255, 120));
  //QBrush brush = QBrush(QColor(255, 255, 255, 100));
  QBrush brush = QBrush(g);
  qp->fillPath(path, brush);
  QPen pen = QPen(Qt::red, DMG->opts.nlw * 3);
  qp->strokePath(path, pen);
}

void
DAGRenderer::draw1_node_1(QPainter * qp, dm_dag_t * D, dm_dag_node_t * node, _unused_ int * on_global_cp, int cid) {
  /* Get inputs */
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
  dr_dag_node_kind_t kind = pi->info.kind;
  dm_node_coordinate_t * node_c = &node->c[cid];
  double x = node_c->x;
  double y = node_c->y;

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
  QPen pen = QPen(Qt::black, DMG->opts.nlw);
  if (kind == dr_dag_node_kind_section || kind == dr_dag_node_kind_task) {
    
    if (dm_is_union(node)) {

      pen.setWidth(DMG->opts.nlw_collective_node_factor * DMG->opts.nlw);

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
        xx = x - node_c->lw - margin;
        yy = y - margin;
        w = node_c->lw + node_c->rw + 2 * margin;
        h = node_c->dw + 2 * margin;
      
      } else {
      
        /* Calculate coordinates: normal-sized box */
        xx = x - node_c->lw;
        yy = y;
        w = node_c->lw + node_c->rw;
        h = node_c->dw;
      
      }

      /* Draw path */
      if (kind == dr_dag_node_kind_section) {
        drend_draw_path_rounded_rectangle(&path, xx, yy, w, h);
      } else {
        drend_draw_path_rectangle(&path, xx, yy, w, h);
      }

    } else { // is collective but not union

      /* Calculate coordinates: normal-sized box */
      xx = x - node_c->lw;
      yy = y;
      w = node_c->lw + node_c->rw;
      h = node_c->dw;

      /* Draw path */
      if (kind == dr_dag_node_kind_section) {
        drend_draw_path_rounded_rectangle(&path, xx, yy, w, h);
      } else {
        drend_draw_path_rectangle(&path, xx, yy, w, h);
      }
      
    }
    
  } else {
    
    /* Calculate coordinates */
    xx = x - node_c->lw;
    yy = y;
    w = node_c->lw + node_c->rw;
    h = node_c->dw;
    
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
  QColor pen_color = pen.color();
  pen_color.setAlphaF(alpha * pen_color.alphaF());
  pen.setColor(pen_color);
  QColor brush_color = lookup_color(pi, alpha);
  QBrush brush = QBrush(brush_color);
  if (pi->info.worker == -1) {
    QLinearGradient brush_gradient = lookup_gradient(pi, alpha, cid);
    brush = QBrush(brush_gradient);
  }

  /* Draw node's filling */
  qp->fillPath(path, brush);
  
  /* Draw node's border */
  qp->strokePath(path, pen);

  /* Highlight */
  if (node->highlight) {
    draw_highlight_node(qp, path, node);
  }
}

int
rectangle_is_out_of_view(QPainter * qp, QRectF r) {
  QTransform t = qp->transform().inverted();
  QRectF bb = t.mapRect( QRectF(0, 0, qp->device()->width(), qp->device()->height()) );
  if (bb.intersects(r))
    return 0;
  int ret = 0;
  /* above */
  if (r.y() + r.height() < bb.y())
    ret |= DM_DAG_NODE_HIDDEN_ABOVE;
  /* right */
  if (r.x() > bb.x() + bb.width())
    ret |= DM_DAG_NODE_HIDDEN_RIGHT;
  /* below */
  if (r.y() > bb.y() + bb.height())
    ret |= DM_DAG_NODE_HIDDEN_BELOW;
  /* left */
  if (r.x() + r.width() < bb.x())
    ret |= DM_DAG_NODE_HIDDEN_LEFT;
  return ret;
}

int
dag_node_is_out_of_view(QPainter * qp, dm_dag_node_t * node, int cid) {
  dm_node_coordinate_t * node_c = &node->c[cid];
  double x = node_c->x;
  double y = node_c->y;
  double margin = 0.0;
  if (dm_is_set(node) && dm_is_union(node) && dm_is_inner_loaded(node)
      && (dm_is_expanding(node) || dm_is_shrinking(node))) {
    margin = DMG->opts.union_node_puffing_margin;
  }
  double xx, yy, w, h;
  xx = x - node_c->lw - margin;
  yy = y - margin;
  w = node_c->lw + node_c->rw + 2 * margin;
  h = node_c->dw + 2 * margin;
  return rectangle_is_out_of_view(qp, QRectF(xx, yy, w, h));
}

int
dag_node_and_successors_out_of_view(QPainter * qp, dm_dag_node_t * node, int cid) {
  dm_node_coordinate_t * node_c = &node->c[cid];
  double x = node_c->x;
  double y = node_c->y;
  double margin = 0.0;
  if (dm_is_set(node) && dm_is_union(node) && dm_is_inner_loaded(node)
      && (dm_is_expanding(node) || dm_is_shrinking(node))) {
    margin = DMG->opts.union_node_puffing_margin;
  }
  double xx, yy, w, h;
  xx = x - node_c->link_lw - margin;
  yy = y - margin;
  w = node_c->link_lw + node_c->link_rw + 2 * margin;
  h = node_c->link_dw + 2 * margin;
  return rectangle_is_out_of_view(qp, QRectF(xx, yy, w, h));
}

void
DAGRenderer::draw1_node_r(QPainter * qp, dm_dag_t * D, dm_dag_node_t * node, int * parent_on_global_cp, int cid) {
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

  /* find innermost node that still covers the anchor point */
  if (anchor[cid].enabled) {
    dm_node_coordinate_t * c = &node->c[cid];
    if (anchor[cid].node == NULL ||
        (c->x - c->lw < anchor[cid].x && anchor[cid].x < c->x + c->rw &&
         c->y         < anchor[cid].y && anchor[cid].y < c->y + c->dw)) {
      anchor[cid].node = node;
    }
  }
  
  if (!dag_node_is_out_of_view(qp, node, cid)) {
    /* Draw node */
    if (!dm_is_union(node) || !dm_is_inner_loaded(node)
        || dm_is_shrinked(node) || dm_is_shrinking(node)) {
      draw1_node_1(qp, D, node, me_on_global_cp, cid);
    }
    /* Call inward */
    if (!dm_is_single(node)) {
      draw1_node_r(qp, D, node->head, me_on_global_cp, cid);
    }
  }
  
  /* Call link-along */
  if (!dag_node_and_successors_out_of_view(qp, node, cid)) {
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
      /* Draw edge first */
      QPainterPath path;
      if (dm_is_single(node)) {
        if (dm_is_single(x))
          draw_edge_1(qp, D, node, x, cid);
        else
          draw_edge_1(qp, D, node, dm_dag_node_get_single_head(x->head), cid);
      } else {
        dm_dag_node_t * tail = NULL;
        while ( (tail = dm_dag_node_traverse_tails(node, tail)) ) {
          dm_dag_node_t * last = dm_dag_node_get_single_last(tail);        
          if (dm_is_single(x))
            draw_edge_1(qp, D, last, x, cid);
          else
            draw_edge_1(qp, D, last, dm_dag_node_get_single_head(x->head), cid);
        }      
      }
      qp->strokePath(path, QPen(Qt::black));
      /* Call recursively then */
      draw1_node_r(qp, D, x, parent_on_global_cp, cid);
    }
  }
}

void
DAGRenderer::draw1_(QPainter * qp, dm_dag_t * D, int cid) {
  int on_global_cp[DM_NUM_CRITICAL_PATHS];
  int i;
  for (i = 0; i < DM_NUM_CRITICAL_PATHS; i++) {
    if (D->show_critical_paths[i])
      on_global_cp[i] = 1;
    else
      on_global_cp[i] = 0;
  }
  draw1_node_r(qp, D, D->rt, on_global_cp, cid);
}

void
DAGRenderer::draw2_node_1(QPainter * qp, dm_dag_t * D, dm_dag_node_t * node, _unused_ int * on_global_cp, int cid) {
  /* Get inputs */
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
  dm_node_coordinate_t * node_c = &node->c[cid];
  double x = node_c->x;
  double y = node_c->y;

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
  QPen pen = QPen(Qt::black, DMG->opts.nlw);
  if (dm_is_union(node)) {

    pen.setWidth(DMG->opts.nlw_collective_node_factor * DMG->opts.nlw);

    if ( dm_is_inner_loaded(node)
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
      xx = x - node_c->lw - margin;
      yy = y - margin;
      w = node_c->lw + node_c->rw + 2 * margin;
      h = node_c->dw + 2 * margin;
      
    } else {
      
      /* Calculate coordinates: normal-sized box */
      xx = x - node_c->lw;
      yy = y;
      w = node_c->lw + node_c->rw;
      h = node_c->dw;
      
    }

  } else {
    
    /* Calculate coordinates: normal-sized box (leaf node) */
    xx = x - node_c->lw;
    yy = y;
    w = node_c->lw + node_c->rw;
    h = node_c->dw;
    
  }
  
  /* Draw path */
  drend_draw_path_rectangle(&path, xx, yy, w, h);

  /* Calculate alpha (based on parent) */
  if (dm_is_shrinking(node->parent)) {
    alpha *= dm_get_alpha_fading_out(D, node->parent);
  } else if (dm_is_expanding(node->parent)) {
    alpha *= dm_get_alpha_fading_in(D, node->parent);
  }
  if (alpha > 1.0) alpha = 1.0;
  if (alpha < 0.0) alpha = 0.0;

  /* Color */
  QColor pen_color = pen.color();
  pen_color.setAlphaF(alpha * pen_color.alphaF());
  pen.setColor(pen_color);
  QColor brush_color = lookup_color(pi, alpha);
  QBrush brush = QBrush(brush_color);
  if (pi->info.worker == -1) {
    QLinearGradient brush_gradient = lookup_gradient(pi, alpha, cid);
    brush = QBrush(brush_gradient);
  }

  /* Draw node's filling */
  qp->fillPath(path, brush);

  /* Draw node's border */
  qp->strokePath(path, pen);
  
  /* Highlight */
  if (node->highlight) {
    draw_highlight_node(qp, path, node);
  }
}

void
DAGRenderer::draw2_node_r(QPainter * qp, dm_dag_t * D, dm_dag_node_t * node, int * parent_on_global_cp, int cid) {
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
  
  /* find innermost node that still covers the anchor point */
  if (anchor[cid].enabled) {
    dm_node_coordinate_t * c = &node->c[cid];
    if (anchor[cid].node == NULL ||
        (c->x - c->lw < anchor[cid].x && anchor[cid].x < c->x + c->rw &&
         c->y         < anchor[cid].y && anchor[cid].y < c->y + c->dw)) {
      anchor[cid].node = node;
    }
  }
  
  if (!dag_node_is_out_of_view(qp, node, cid)) {
    /* Draw node */
    if (!dm_is_union(node) || !dm_is_inner_loaded(node)
        || dm_is_shrinked(node) || dm_is_shrinking(node)) {
      draw2_node_1(qp, D, node, me_on_global_cp, cid);
    }
    /* Call inward */
    if (!dm_is_single(node)) {
      draw2_node_r(qp, D, node->head, me_on_global_cp, cid);
    }
  }
  
  /* Call link-along */
  if (!dag_node_and_successors_out_of_view(qp, node, cid)) {
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
      /* Draw edge first */
      QPainterPath path;
      if (dm_is_single(node)) {      
        if (dm_is_single(x))
          draw_edge_1(qp, D, node, x, cid);
        else
          draw_edge_1(qp, D, node, dm_dag_node_get_single_head(x->head), cid);
      } else {
        dm_dag_node_t * tail = NULL;
        while ( (tail = dm_dag_node_traverse_tails(node, tail)) ) {
          dm_dag_node_t * last = dm_dag_node_get_single_last(tail);
          if (dm_is_single(x))
            draw_edge_1(qp, D, last, x, cid);
          else
            draw_edge_1(qp, D, last, dm_dag_node_get_single_head(x->head), cid);
        }
      }
      qp->strokePath(path, QPen(Qt::black));
      /* Call recursively then */
      draw2_node_r(qp, D, x, parent_on_global_cp, cid);
    }
  }
}

void
DAGRenderer::draw2_(QPainter * qp, dm_dag_t * D, int cid) {
  int on_global_cp[DM_NUM_CRITICAL_PATHS];
  int i;
  for (i = 0; i < DM_NUM_CRITICAL_PATHS; i++) {
    if (D->show_critical_paths[i])
      on_global_cp[i] = 1;
    else
      on_global_cp[i] = 0;
  }
  draw2_node_r(qp, D, D->rt, on_global_cp, cid);
}

void
DAGRenderer::draw3_node_1(QPainter * qp, dm_dag_t * D, dm_dag_node_t * node, _unused_ int * on_global_cp, int cid) {
  /* Get inputs */
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
  dm_node_coordinate_t * node_c = &node->c[cid];
  double x = node_c->x;
  double y = node_c->y;

  /* Count drawn node */
  if (node->d > D->cur_d) {
    D->cur_d = node->d;
  }
  if (dm_is_union(node) && dm_is_inner_loaded(node)
      && dm_is_shrinked(node)
      && node->d < D->cur_d_ex) {
    D->cur_d_ex = node->d;
  }
  
  /* Coordinates */
  double xx, yy, w, h;
  xx = x;
  yy = y;
  w = node_c->rw;
  h = node_c->dw;
  
  /* Pen with adaptive line width */
  double zx = qp->transform().m11();
  double zy = qp->transform().m22();
  double line_width = dm_min(DMG->opts.nlw, DMG->opts.nlw / dm_min(zx, zy));
  QPen pen = QPen(Qt::black, line_width);
  
  /* Draw path */
  QPainterPath path;
  if (!dag_node_is_out_of_view(qp, node, cid)) {
    
    drend_draw_path_rectangle(&path, xx, yy, w, h);
    
    /* Draw node */
    if (dm_is_union(node)) {

      QBrush brush = QBrush(QColor(200, 200, 200, 40));
      qp->fillPath(path, brush);
      
    } else {
      
      QColor brush_color = lookup_color(pi, 1.0, 1);
      QBrush brush = QBrush(brush_color);
      qp->fillPath(path, brush);
      /* Highlight */
      if (node->highlight) {
        draw_highlight_node(qp, path, node);
      }
    }
  }   
}

void
DAGRenderer::draw3_node_r(QPainter * qp, dm_dag_t * D, dm_dag_node_t * node, int * parent_on_global_cp, int cid) {
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
  
  /* Call inward */
  int hidden = dag_node_is_out_of_view(qp, node, cid);
  if (!hidden) {
    if (dm_is_union(node) && dm_is_inner_loaded(node) && dm_is_expanded(node)) {
      draw3_node_r(qp, D, node->head, me_on_global_cp, cid);
    } else {
      /* Draw node */
      draw3_node_1(qp, D, node, me_on_global_cp, cid);
    }
  }
  
  /* Call link-along */
  if (!(hidden & DM_DAG_NODE_HIDDEN_RIGHT)) {
    dm_dag_node_t * next = NULL;
    while ( (next = dm_dag_node_traverse_nexts(node, next)) ) {
      draw3_node_r(qp, D, next, parent_on_global_cp, cid);
    }
  }
}

void
DAGRenderer::draw3_(QPainter * qp, dm_dag_t * D, int cid) {
  int on_global_cp[DM_NUM_CRITICAL_PATHS];
  int i;
  for (i = 0; i < DM_NUM_CRITICAL_PATHS; i++) {
    if (D->show_critical_paths[i])
      on_global_cp[i] = 1;
    else
      on_global_cp[i] = 0;
  }
  draw3_node_r(qp, D, D->rt, on_global_cp, cid);
}

_unused_ static int
paraprof_entry_is_out_of_view(QPainter * qp, dm_dag_t * D, dm_histogram_entry_t * e, int cid) {
  double x, w;
  x = dm_dag_layout_scale_down(D, e->t - D->bt, cid);
  w = 0.0;
  if (e->next)
    w = dm_dag_layout_scale_down(D, e->next->t - D->bt, cid) - x;
  QTransform t = qp->transform().inverted();
  QRectF bb = t.mapRect( QRectF(0, 0, qp->device()->width(), qp->device()->height()) );
  double bound_left = bb.x();
  double bound_right = bb.x() + bb.width();
  int ret = 0;
  if (x > bound_right)
    ret |= DM_DAG_NODE_HIDDEN_RIGHT;
  if (x + w < bound_left)
    ret |= DM_DAG_NODE_HIDDEN_LEFT;
  return ret;  
}

static double
dm_histogram_draw_piece(QPainter * qp, _unused_ dm_dag_t * D, double x, double width, double y, double height, int layer) {
  double xx, yy, w, h;
  xx = x;
  yy = y - height;
  w = width;
  h = height;
  if (!rectangle_is_out_of_view(qp, QRectF(xx, yy, w, h))) {
    //dv_timeline_trim_rectangle(V, &xx, &yy, &w, &h);
    QVector<QColor> colors = {Qt::red, Qt::green, Qt::blue, Qt::magenta, Qt::cyan, Qt::yellow};
    QColor color = colors[(layer + 6) % 6];
    QBrush brush = QBrush(color);
    qp->fillRect(QRectF(xx, yy, w, h), brush);
  }
  return yy;
}

static void
draw_paraprof_entry(QPainter * qp, dm_dag_t * D, dm_histogram_entry_t * e, int cid) {
  if (!e->next) {
    dm_pwarning("not draw entry at t=%lf due to no next", e->t);
    return;
  }
  double x = dm_dag_layout_scale_down(D, e->t - D->bt, cid);
  double w = dm_dag_layout_scale_down(D, e->next->t - D->bt, cid) - x;
  //double y = 0.0;
  double y = - D->H->unit_thick * D->radius;
  double h;
  int i;
  for (i=0; i<dm_histogram_layer_max; i++) {
    h = e->h[i] * D->H->unit_thick * D->radius;
    y = dm_histogram_draw_piece(qp, D, x, w, y, h, i);
  }
}

void
DAGRenderer::draw_paraprof(QPainter * qp, dm_dag_t * D, int cid) {
  if (!D->H) {
    dm_perror("there is no H structure.");
    return;
  }
  dm_histogram_t * H = D->H;
  dm_histogram_entry_t * e = H->head_e;
  while (e != NULL && e->next) {
    int hidden = 0;//paraprof_entry_is_out_of_view(qp, D, e, cid);
    if (!hidden) {
      draw_paraprof_entry(qp, D, e, cid);
    } else if (hidden & DM_DAG_NODE_HIDDEN_RIGHT) {
      break;
    }
    e = e->next;
  }
  
  /* draw full-parallelism line */
  double x = 0;
  double w = dm_dag_layout_scale_down(D, D->et, cid);
  double y = - H->unit_thick * D->radius - D->P->num_workers * (H->unit_thick * D->radius);
  //dv_rectangle_trim(V, &x, &y, &w, NULL);
  double zx = qp->transform().m11();
  QPen pen = QPen(QColor(255, 0, 0, 200), DMG->opts.nlw / zx);
  qp->setPen(pen);
  qp->drawLine(x, y, x + w, y);
}

// int
// DAGRenderer::get_cid_from_qpainter(QPainter * qp) {
//   for (int cid = 0; cid < DM_NUM_COORDINATES; cid++) {
//     QVector<QWidget *>::iterator i;
//     for (i = mViewports[cid].begin(); i != mViewports[cid].end(); i++) {
//       QWidget * VP = *i;
//       if (VP == qp->device()) return cid;
//     }
//   }
//   return -1;
// }

void
DAGRenderer::draw_(QPainter * qp, dm_dag_t * D, int cid) {
  if (cid < 0) {
    dm_perror("cannot get CID from the QPainter=%p", qp);
    return;
  }

  dm_llist_init(D->itl);
  D->cur_d = 0;
  D->cur_d_ex = D->dmax;

  qp->setRenderHint(QPainter::Antialiasing);
  qp->translate(mDx[cid], mDy[cid]);
  
  switch (cid) {
  case DM_LAYOUT_DAG_COORDINATE:
    draw1_(qp, D, cid);
    break;
  case DM_LAYOUT_DAG_BOX_LINEAR_COORDINATE:
  case DM_LAYOUT_DAG_BOX_POWER_COORDINATE:
  case DM_LAYOUT_DAG_BOX_LOG_COORDINATE:
    draw2_(qp, D, cid);
    break;
  case DM_LAYOUT_PARAPROF_COORDINATE:
    draw3_(qp, D, cid);
    draw_paraprof(qp, D, cid);
    break;
  default:
    dm_perror("unknown cid=%d", cid);
    return;
  }
}

PyObject *
DAGRenderer::get_dag_node_info_(dm_dag_node_t * node) {
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(mDAG->P, node);
  /* build DAG node's info into a Python object */
  PyObject * ret = NULL;
  if (pi) {
    ret = Py_BuildValue("{s:L,s:(L,i,i,s,L),s:(L,i,i,s,L),s:L,s:L,s:L,s:L,s:L,s:i,s:i,s:i,s:[L,L,L,L]}",
                        "node_id", pi - mDAG->P->T,
                        "start", pi->info.start.t, pi->info.start.worker, pi->info.start.cpu,
                        mDAG->P->S->C + mDAG->P->S->I[pi->info.start.pos.file_idx], pi->info.start.pos.line,
                        "end", pi->info.end.t, pi->info.end.worker, pi->info.end.cpu,
                        mDAG->P->S->C + mDAG->P->S->I[pi->info.end.pos.file_idx], pi->info.end.pos.line,
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

double
DAGRenderer::up_height(int cid) {
  switch (cid) {
  case DM_LAYOUT_DAG_COORDINATE:
  case DM_LAYOUT_DAG_BOX_LINEAR_COORDINATE:
  case DM_LAYOUT_DAG_BOX_POWER_COORDINATE:
  case DM_LAYOUT_DAG_BOX_LOG_COORDINATE:
    return 0.0;
  case DM_LAYOUT_PARAPROF_COORDINATE:
    return dm_histogram_get_max_height(mDAG->H);// + mDAG->H->unit_thick * mDAG->radius;
  default:
    dm_perror("unknown cid=%d", cid);
    return 0.0;
  }
}

double
DAGRenderer::down_height(int cid) {
  switch (cid) {
  case DM_LAYOUT_DAG_COORDINATE:
  case DM_LAYOUT_DAG_BOX_LINEAR_COORDINATE:
  case DM_LAYOUT_DAG_BOX_POWER_COORDINATE:
  case DM_LAYOUT_DAG_BOX_LOG_COORDINATE:
    return mDAG->rt->c[cid].dw;
  case DM_LAYOUT_PARAPROF_COORDINATE:
    return mDAG->P->num_workers * (2 * mDAG->radius);
  default:
    dm_perror("unknown cid=%d", cid);
    return 0.0;
  }
}

/***** end of Rendering DAG *****/

