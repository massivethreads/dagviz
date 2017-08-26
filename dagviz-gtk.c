#include "interface.c"

/*-----------------Main begins-----------------*/

static void
on_application_activate(GApplication * app, _unused_ gpointer user_data) {
  dv_open_gui(0, NULL, GTK_APPLICATION(app));
}

int
main_usegtkapplication(int argc, char * argv[]) {
  /* Initialization */

  /* GTK */
  gtk_init(&argc, &argv);
  
  /* DVG */
  dv_global_state_init();
  
  /* PIDAG */
  int i;
  if (argc > 1) {
    for (i=1; i<argc; i++)
      dv_create_new_pidag(argv[i]);
  }
  
  /* Viewport */
  dv_viewport_t * VP = dv_viewport_create_new();
  dv_viewport_init(VP);

  /* DAG -> VIEW <- Viewport initialization */
  dm_dag_t * D;
  dv_view_t * V;
  for (i=0; i<DMG->nP; i++) {
    D = dv_create_new_dag(&DMG->P[i]);
    //print_dvdag(D);
    V = dv_create_new_view(D);
    // Expand
    int count = 0;
    while (V->D->n < 10 && count < 2) {
      printf("V %ld: %ld\n", V-DVG->V, V->D->n);
      dv_do_expanding_one(V);
      count++;
    }
  }
  if (DVG->nV == 1) {
    V = DVG->V;
    dv_view_add_viewport(V, VP);
    //dv_view_change_lt(V, 4);
  } else if (DVG->nV >= 2) {
    dv_viewport_change_split(VP, 1);
    dv_view_add_viewport(&DVG->V[0], VP->vp1);
    dv_view_add_viewport(&DVG->V[1], VP->vp2);
  }  
  dv_switch_focused_viewport();

  /* Open Gtk GUI */
  GtkApplication * app = gtk_application_new("com.github.zanton.dagviz", 0);
  g_signal_connect(G_OBJECT(app), "activate", G_CALLBACK(on_application_activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  return status;
}

int
main(int argc, char * argv[]) {
  /* Initialization */
  
  /* GTK */
  gtk_init(&argc, &argv);

  /* DVG, GUI */
  dv_global_state_init();
  dv_gui_init(GUI);

  /* Environment variables */
  //dv_get_env();
  //dv_alarm_init();
  
  /* PIDAG */
  int i;
  for (i = 1; i < argc; i++) {
    glob_t globbuf;
    glob(argv[i], GLOB_TILDE | GLOB_PERIOD | GLOB_BRACE, NULL, &globbuf);
    int j;
    for (j = 0; j < (int) globbuf.gl_pathc; j++) {
      _unused_ dm_pidag_t * P = dv_create_new_pidag(globbuf.gl_pathv[j]);
    }
    if (globbuf.gl_pathc > 0)
      globfree(&globbuf);
  }
  for (i = 0; i < DMG->nP; i++) {
    dm_pidag_t * P = &DMG->P[i];
    dm_dag_t * D = dv_create_new_dag(P);
    dv_view_t * V = dv_create_new_view(D);
    printf("P=%p, D=%p, V=%p\n", P, D, V);
    /* Expand */
    (void) V;
    /*
    int count = 0;
    while (V->D->n < 10 && count < 2) {
      dv_do_expanding_one(V);
      count++;
    }
    */
  }
  
  /* Viewports */
  dv_viewport_t * VP = dv_viewport_create_new();
  dv_viewport_init(VP);
  if (DMG->nD == 1)
    dv_viewport_divide_onedag_1(VP, DMG->D);
  else if (DMG->nD == 2)
    dv_viewport_divide_twodags_1(VP, DMG->D, DMG->D + 1);
  else if (DMG->nD == 3)
    dv_viewport_divide_threedags_1(VP, DMG->D, DMG->D + 1, DMG->D + 2);
  else if (DMG->nD > 3)
    dv_viewport_divide_twodags_1(VP, DMG->D, DMG->D + 1);
  
  /* Open GUI */
  dv_open_gui(argc, argv, NULL);
  //dv_switch_focused_viewport(); // darea can grab focus only when it is visible already
  gtk_main();
  return 1;
}

/*-----------------Main ends-------------------*/
