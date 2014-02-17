#include <gtkmm/drawingarea.h>

class DagView : public Gtk::DrawingArea
{
 public:
  DagView();
  virtual ~DagView();

 protected:
  virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
  bool draw_clock(const Cairo::RefPtr<Cairo::Context>& cr);
  bool draw_test(const Cairo::RefPtr<Cairo::Context>& cr);
  bool on_timeout();

  double m_radius;
  double m_line_width;
};
