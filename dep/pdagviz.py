#!/usr/bin/env python

import gtk
import cairo

class MyWidget(gtk.DrawingArea):
    def __init__(self):
        gtk.DrawingArea.__init__(self)
        self.connect('expose-event', self.do_expose_event)

    def do_expose_event(self, widget, event):
        cr = widget.window.cairo_create()

        width = event.area.width
        height = event.area.height

        cr.scale(width, height)

        radial = cairo.RadialGradient(0.25, 0.25, 0.1,  0.5, 0.5, 0.5)
        radial.add_color_stop_rgb(0,  1.0, 0.8, 0.8)
        radial.add_color_stop_rgb(1,  0.9, 0.0, 0.0)

        #for i in range(1, 10):
         #   for j in range(1, 10):
          #      cr.rectangle(i/10.0 - 0.04, j/10.0 - 0.04, 0.08, 0.08)
        cr.rectangle(0.0, 0.0, 1.0, 1.0);
        cr.set_source(radial)
        #cr.set_source_rgb(0.0, 1.0, 0.0);
        cr.fill()


class MyWindow(gtk.Window):
    def __init__(self):
        gtk.Window.__init__(self)
        self.connect('destroy', gtk.main_quit)
        self.set_title('Hello cairo')
        self.set_default_size(512, 512)
        vbox = gtk.VBox()
        self.add(vbox)
        widget = MyWidget()
        vbox.pack_start(widget)
        self.show_all()
        
def main():
    win = MyWindow()
    gtk.main()

if __name__ == '__main__':
    main()
