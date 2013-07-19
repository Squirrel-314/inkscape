
    
/** @file
 * @brief New From Template - templates widget - implementation
 */
/* Authors:
 *   Jan Darowski <jan.darowski@gmail.com>, supervised by Krzysztof Kosiński   
 *
 * Copyright (C) 2013 Authors
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "template-widget.h"
#include "template-load-tab.h"

#include <gtkmm/alignment.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/messagedialog.h>
#include <iostream>

#include "file.h"

#include <glibmm/convert.h>
#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>


namespace Inkscape {
namespace UI {
    

TemplateWidget::TemplateWidget()
    : _more_info_button("More info")
    , _short_description_label("Short description - I like trains. ad asda asd asdweqe gdfg")
    , _template_author_label("by template_author")
    , _template_name_label("Template_name")
    , _preview_image("preview.png")
{
    pack_start(_template_name_label, Gtk::PACK_SHRINK, 4);
    pack_start(_template_author_label, Gtk::PACK_SHRINK, 0);
    pack_start(_preview_image, Gtk::PACK_SHRINK, 15);
    pack_start(_short_description_label, Gtk::PACK_SHRINK, 4);
    
    _short_description_label.set_line_wrap(true);
    _short_description_label.set_size_request(200);

    Gtk::Alignment *align;
    align = manage(new Gtk::Alignment(Gtk::ALIGN_END, Gtk::ALIGN_CENTER, 0.0, 0.0));
    pack_start(*align, Gtk::PACK_SHRINK, 5);
    align->add(_more_info_button);
    
    _more_info_button.signal_pressed().connect(
    sigc::mem_fun(*this, &TemplateWidget::_displayTemplateDetails));
}


void TemplateWidget::create()
{
    if (_current_template.path == "")
        return;
    if (_current_template.is_procedural){
        
    }
    else {
        sp_file_new(_current_template.path);
    }
}


void TemplateWidget::display(TemplateLoadTab::TemplateData data)
{
    _current_template = data;
    if (data.is_procedural){}
    else{
        _template_name_label.set_text(_current_template.display_name);
        _template_author_label.set_text(_current_template.author);
        _short_description_label.set_text(_current_template.short_description);
        
        Glib::ustring imagePath = Glib::build_filename(Glib::path_get_dirname(_current_template.path),  _current_template.preview_name);
        _preview_image.set(imagePath);
    }
}

void TemplateWidget::_displayTemplateDetails()
{    
    if (_current_template.path == "")
        return;
    
    Glib::ustring message = _current_template.display_name + "\n\n" +
                            _("Path: ") + _current_template.path + "\n\n";
    
    if (_current_template.long_description != "")
        message += _("Description: ") + _current_template.long_description + "\n\n";
    if (_current_template.keywords.size() > 0){
        message += _("Keywords: ");
        for (std::set<Glib::ustring>::iterator it = _current_template.keywords.begin(); it != _current_template.keywords.end(); ++it)
            message += *it + " ";
        message += "\n\n";
    }
    
    if (_current_template.author != "")
        message += _("By: ") + _current_template.author + " " + _current_template.creation_date + "\n\n";
    
    Gtk::MessageDialog dl(message, false, Gtk::MESSAGE_OTHER);
    dl.run();
}

}
}
