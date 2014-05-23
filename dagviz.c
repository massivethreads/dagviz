#include "dagviz.h"

GtkWidget *window;
GtkWidget *darea;


/*-----------------DV Visualizer GUI-------------------*/
static void do_drawing(cairo_t *cr)
{
	// First time only
	if (G->init) {
		double height = gtk_widget_get_allocated_height(darea);
		double h = G->rt->dc * DV_VDIS;
		double hh = height - 2 * (DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS);
		if (h > hh) {
			G->zoom_ratio = hh / h;		
		}
		G->init = 0;
	}
	// Draw graph
	cairo_save(cr);
	cairo_translate(cr, G->basex + G->x, G->basey + G->y);
	cairo_scale(cr, G->zoom_ratio, G->zoom_ratio);
	dv_draw_dvdag(cr, G);
	cairo_restore(cr);

	// Draw status line
	dv_draw_status(cr);
	
  //draw_hello(cr);
  //draw_rounded_rectangle(cr);
}

static void do_zooming(double zoom_ratio, double posx, double posy)
{
	if (posx != 0.0 || posy != 0.0) {
		posx -= G->basex + G->x;
		posy -= G->basey + G->y;
		double deltax = posx / G->zoom_ratio * zoom_ratio - posx;
		double deltay = posy / G->zoom_ratio * zoom_ratio - posy;
		G->x -= deltax;
		G->y -= deltay;
	}
	G->zoom_ratio = zoom_ratio;
	gtk_widget_queue_draw(darea);
}

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	// Set parameters
	S->vpw = gtk_widget_get_allocated_width(widget);
	S->vph = gtk_widget_get_allocated_height(widget);
	G->basex = 0.5 * S->vpw;
	G->basey = DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS;
	// Draw
  do_drawing(cr);
  return FALSE;
}

static gboolean on_btn_zoomfit_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	// Default
	double zoom_ratio = 1.0;
	G->x = 0.0;
	G->y = 0.0;
	// Need scaled
	double h = G->rt->dc * DV_VDIS;
	double hh = S->vph - 2 * (DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS);
	if (h > hh) {
		zoom_ratio = hh / h;		
	}
		
	do_zooming(zoom_ratio, 0.0, 0.0);
	return TRUE;
}

static gboolean on_btn_shrink_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	if (S->sel > 0) {
		if (!S->a->on) {
			S->a->new_sel = S->sel - 1;
			dv_animation_start(S->a);
		}
	}
	return TRUE;
}

static gboolean on_btn_expand_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	if (S->sel < G->lvmax) {
		if (!S->a->on) {
			S->a->new_sel = S->sel + 1;
			dv_animation_start(S->a);
		}
	}
	return TRUE;
}

static gboolean on_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
	if (event->direction == GDK_SCROLL_UP) {
		do_zooming(G->zoom_ratio * DV_ZOOM_INCREMENT, event->x, event->y);
	} else if (event->direction == GDK_SCROLL_DOWN) {
		do_zooming(G->zoom_ratio / DV_ZOOM_INCREMENT, event->x, event->y);
	}
	return TRUE;
}

static dv_dag_node_t *find_clicked_node(double x, double y) {
	dv_dag_node_t * ret = NULL;
	double vc, hc;
	int i;
	dv_dag_node_t *node;
	for (i=0; i<G->n; i++) {
		node = G->T + i;
		if ((node->lv <= S->sel)
				&& (!dv_node_flag_check(node->f, DV_NODE_FLAG_UNION)
						|| dv_node_flag_check(node->f, DV_NODE_FLAG_SHRINKED))) {
			vc = node->vl->c;
			hc = node->c;
			if (vc - DV_RADIUS < x && x < vc + DV_RADIUS
					&& hc - DV_RADIUS < y && y < hc + DV_RADIUS) {
				ret = node;
				break;
			}
		}
	}
	return ret;
}

static gboolean on_button_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->type == GDK_BUTTON_PRESS) {
		// Drag
		S->drag_on = 1;
		S->pressx = event->x;
		S->pressy = event->y;
		S->accdisx = 0.0;
		S->accdisy = 0.0;
	}	else if (event->type == GDK_BUTTON_RELEASE) {
		// Drag
		S->drag_on = 0;
		// Info tag
		if (S->accdisx < DV_SAFE_CLICK_RANGE
				&& S->accdisy < DV_SAFE_CLICK_RANGE) {
			double ox = (event->x - G->basex - G->x) / G->zoom_ratio;
			double oy = (event->y - G->basey - G->y) / G->zoom_ratio;
			dv_dag_node_t *node_pressed = find_clicked_node(ox, oy);
			if (node_pressed) {
				if (!dv_llist_remove(G->itl, node_pressed)) {
					dv_llist_add(G->itl, node_pressed);
				}
				gtk_widget_queue_draw(darea);
			}
		}
	} else if (event->type == GDK_2BUTTON_PRESS) {
		// Shrink/Expand
	}
	return TRUE;
}

static gboolean on_motion_event(GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
	if (S->drag_on) {
		// Drag
		double deltax = event->x - S->pressx;
		double deltay = event->y - S->pressy;
		G->x += deltax;
		G->y += deltay;
		S->accdisx += deltax;
		S->accdisy += deltay;
		S->pressx = event->x;
		S->pressy = event->y;
		gtk_widget_queue_draw(darea);
	}
	return TRUE;
}

static gboolean on_combobox_changed(GtkComboBox *widget, gpointer user_data) {
	S->nc = gtk_combo_box_get_active(widget);
	gtk_widget_queue_draw(darea);
	return TRUE;
}

int open_gui(int argc, char *argv[])
{
  gtk_init(&argc, &argv);

	// Main window
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
	//gtk_window_fullscreen(GTK_WINDOW(window));
	gtk_window_maximize(GTK_WINDOW(window));
  gtk_window_set_title(GTK_WINDOW(window), "DAG Visualizer");
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);	

	// Toolbar
	GtkWidget *toolbar = gtk_toolbar_new();
	GtkToolItem *btn_zoomfit = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_FIT);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit, -1);
	g_signal_connect(G_OBJECT(btn_zoomfit), "clicked", G_CALLBACK(on_btn_zoomfit_clicked), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

	// Combo box
	GtkToolItem *btn_combo = gtk_tool_item_new();
	GtkWidget *combobox = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "worker", "Worker");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "cpu", "CPU");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "kind", "Node kind");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 2);
	g_signal_connect(G_OBJECT(combobox), "changed", G_CALLBACK(on_combobox_changed), NULL);
	gtk_container_add(GTK_CONTAINER(btn_combo), combobox);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo, -1);

	// Shrink/Expand buttons
	GtkToolItem *btn_shrink = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
	GtkToolItem *btn_expand = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_shrink, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_expand, -1);
	g_signal_connect(G_OBJECT(btn_shrink), "clicked", G_CALLBACK(on_btn_shrink_clicked), NULL);
	g_signal_connect(G_OBJECT(btn_expand), "clicked", G_CALLBACK(on_btn_expand_clicked), NULL);

	// Drawing Area
  darea = gtk_drawing_area_new();
  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL);
	gtk_widget_add_events(GTK_WIDGET(darea), GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
	g_signal_connect(G_OBJECT(darea), "scroll-event", G_CALLBACK(on_scroll_event), NULL);
	g_signal_connect(G_OBJECT(darea), "button-press-event", G_CALLBACK(on_button_event), NULL);
	g_signal_connect(G_OBJECT(darea), "button-release-event", G_CALLBACK(on_button_event), NULL);
	g_signal_connect(G_OBJECT(darea), "motion-notify-event", G_CALLBACK(on_motion_event), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), darea, TRUE, TRUE, 0);

	// Run main loop
  gtk_widget_show_all(window);
  gtk_main();

  return 1;
}
/*-----------------end of DV Visualizer GUI-------------------*/


/*---------------Initialization Functions------*/

static void dv_status_init() {
	S->drag_on = 0;
	S->pressx = 0.0;
	S->pressy = 0.0;
	S->accdisx = 0.0;
	S->accdisy = 0.0;
	S->nc = 2;
	S->sel = 1;
	dv_animation_init(S->a);
}

/*---------------end of Initialization Functions------*/


/*---------------Environment Variables-----*/

static int dv_get_env_int(char * s, int * t) {
	char * v = getenv(s);
	if (v) {
		*t = atoi(v);
		return 1;
	}
	return 0;
}

static int dv_get_env_long(char * s, long * t) {
	char * v = getenv(s);
	if (v) {
		*t = atol(v);
		return 1;
	}
	return 0;
}

static int dv_get_env_string(char * s, char ** t) {
	char * v = getenv(s);
	if (v) {
		*t = strdup(v);
		return 1;
	}
	return 0;
}

static void dv_get_env() {
	dv_get_env_int("DV_DEPTH", &S->sel);
}

/*---------------end of Environment Variables-----*/


/*-----------------Main begins-----------------*/

int main(int argc, char *argv[])
{
	dv_status_init();
	dv_get_env();
	if (argc > 1) {
		dv_read_dag_file_to_pidag(argv[1], P);
		dv_convert_pidag_to_dvdag(P, G);
		//print_dvdag(G);
		dv_layout_dvdag(G);
		printf("finished layout.\n");
		//print_layout(G);
		//check_layout(G);		
	}
	//if (argc > 1)	print_dag_file(argv[1]);
	return open_gui(argc, argv);
	//return 1;
}

/*-----------------Main ends-------------------*/
