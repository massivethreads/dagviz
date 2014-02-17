#include <gtkmm/application.h>
#include "DagvizWindow.h"
#include "DagView.h"

int main(int argc, char** argv)
{
  Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(argc, argv, "com.zanton");

  DagvizWindow win;
  win.set_title("DAG Visualizer");

  DagView view;
  win.add(view);
  view.show();

  return app->run(win);
}
		
