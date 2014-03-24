#include "dagviz.h"

GtkWidget *window;
GtkWidget *darea;


/*-----------------GUI functions-------------------*/
static void do_drawing(cairo_t *cr)
{
	cairo_translate(cr, 0.5*G->width + G->x, 0.5*G->height + G->y);
	cairo_scale(cr, G->zoom_ratio, G->zoom_ratio);
	draw_dvgraph(cr, G);
  //draw_hello(cr);
  //draw_rounded_rectangle(cr);
}

static void do_zooming(double zoom_ratio)
{
	G->zoom_ratio = zoom_ratio;
	gtk_widget_queue_draw(darea);
}

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	G->width = gtk_widget_get_allocated_width(widget);
	G->height = gtk_widget_get_allocated_height(widget);
  do_drawing(cr);
  return FALSE;
}

static gboolean on_btn_zoomin_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	do_zooming(G->zoom_ratio * DV_ZOOM_INCREMENT);
	return TRUE;
}

static gboolean on_btn_zoomout_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	do_zooming(G->zoom_ratio / DV_ZOOM_INCREMENT);
	return TRUE;
}

static gboolean on_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
	if (event->direction == GDK_SCROLL_UP) {
		do_zooming(G->zoom_ratio * DV_ZOOM_INCREMENT);
	} else if (event->direction == GDK_SCROLL_DOWN) {
		do_zooming(G->zoom_ratio / DV_ZOOM_INCREMENT);
	}
	return TRUE;
}

static dv_graph_node_t *get_clicked_node(double x, double y) {
	dv_graph_node_t *ret = NULL;
	dv_grid_line_t *vl, *hl;
	int i;
	for (i=0; i<G->n; i++) {
		vl = G->T[i].vl;
		hl = G->T[i].hl;
		if (vl->c - DV_RADIUS < x && x < vl->c + DV_RADIUS
				&& hl->c - DV_RADIUS < y && y < hl->c + DV_RADIUS) {
			ret = &G->T[i];
			break;
		}
	}
	return ret;
}

static void *linked_list_remove(dv_linked_list_t *list, void *item) {
	void * ret = NULL;
	dv_linked_list_t *l = list;
	dv_linked_list_t *pre = NULL;
	while (l) {
		if (l->item == item) {
			break;
		}
		pre = l;
		l = l->next;
	}
	if (l && l->item == item) {
		ret = l->item;
		if (pre) {
			pre->next = l->next;
			free(l);
		} else {
			l->item = NULL;
		}		
	}
	return ret;
}

static void linked_list_add(dv_linked_list_t *list, void *item) {
	if (list->item == NULL) {
		list->item = item;
	} else {
		dv_linked_list_t *newl = (dv_linked_list_t *) malloc(sizeof(dv_linked_list_t));
		newl->item = item;
		newl->next = NULL;
		dv_linked_list_t *l = list;
		while (l->next)
			l = l->next;
		l->next = newl;
	}
}

static gboolean on_button_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->type == GDK_BUTTON_PRESS) {
		// Drag
		S->drag_on = 1;
		S->pressx = event->x;
		S->pressy = event->y;
	}	else if (event->type == GDK_BUTTON_RELEASE) {
		// Drag
		S->drag_on = 0;
		S->pressx = 0;
		S->pressy = 0;
	} else if (event->type == GDK_2BUTTON_PRESS) {
		// Info tag
		double ox = (event->x - 0.5*G->width - G->x) / G->zoom_ratio;
		double oy = (event->y - 0.5*G->height - G->y) / G->zoom_ratio;
		dv_graph_node_t *node_pressed = get_clicked_node(ox, oy);
		if (node_pressed) {
			if (!linked_list_remove(&G->itl, node_pressed)) {
				linked_list_add(&G->itl, node_pressed);
			}
			gtk_widget_queue_draw(darea);
		}
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
	GtkToolItem *btn_zoomin = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
	GtkToolItem *btn_zoomout = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomin, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomout, -1);
	g_signal_connect(G_OBJECT(btn_zoomin), "clicked", G_CALLBACK(on_btn_zoomin_clicked), NULL);
	g_signal_connect(G_OBJECT(btn_zoomout), "clicked", G_CALLBACK(on_btn_zoomout_clicked), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

	// Combo box
	GtkToolItem *btn_combo = gtk_tool_item_new();
	GtkWidget *combobox = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "worker", "Worker");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "cpu", "CPU");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "kind", "Node kind");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 0);
	g_signal_connect(G_OBJECT(combobox), "changed", G_CALLBACK(on_combobox_changed), NULL);
	gtk_container_add(GTK_CONTAINER(btn_combo), combobox);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo, -1);
	
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


/*-----------------Main begins-----------------*/

int main(int argc, char *argv[])
{
	if (argc > 1) {
		dr_pi_dag P[1];
		read_dag_file_to_pidag(argv[1], P);
		convert_pidag_to_dvgraph(P, G);
		//print_dvgraph_to_stdout(G);
		layout_dvgraph(G);
		printf("finished layout.\n");
		//print_layout_to_stdout(G);
		//check_layout(G);
		S->drag_on = 0;
		S->pressx = 0.0;
		S->pressy = 0.0;
	}
	//return 1;
	/*if (argc > 1)
		read_dag_file_to_stdout(argv[1]);
	return 1;
	*/
	return open_gui(argc, argv);
}

/*-----------------Main ends-------------------*/
