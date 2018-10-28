/*
 * Inkview - An SVG file viewer.
 *
 * Copyright (C) 2018 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INKVIEW_WINDOW_H
#define INKVIEW_WINDOW_H

#include <gtkmm.h>

class SPDocument;

class InkviewWindow : public Gtk::ApplicationWindow {

public:
    InkviewWindow(const Gio::Application::type_vec_files files,
                  bool fullscreen, bool recursive, int timer, double scale, bool preload);

private:
    std::vector<Glib::RefPtr<Gio::File> >
    create_file_list(const std::vector<Glib::RefPtr<Gio::File > >& files);
    void update_title();
    bool show_document(SPDocument* document);
    SPDocument* load_document();
    void preload_documents();

    Gio::Application::type_vec_files  _files;
    bool   _fullscreen;
    bool   _recursive;
    int    _timer;
    double _scale;
    bool   _preload;

    int _index;
    std::vector<SPDocument*> _documents;

    GtkWidget* _view;
    Gtk::Window* _controlwindow;

    // Callbacks
    void show_control();
    void show_next();
    void show_prev();
    void show_first();
    void show_last();

    bool key_press(GdkEventKey* event);
    bool on_timer();
};

#endif // INKVIEW_WINDOW_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :