#define __SP_TEXT_EDIT_C__

/**
 * \brief Text editing dialog
 *
 * Author:
 *   Lauris Kaplinski <lauris@ximian.com>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <config.h>

#include <string.h>
#include <libnrtype/FontFactory.h>
#include <libnrtype/FontInstance.h>

#include <glib.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkframe.h>
#include <gtk/gtktable.h>
#include <gtk/gtktextview.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkcombo.h>

#ifdef WITH_GTKSPELL
extern "C" {
#include <gtkspell/gtkspell.h>
}
#endif

#include "macros.h"
#include "helper/sp-intl.h"
#include "helper/window.h"
#include "../widgets/font-selector.h"
#include "../forward.h"
#include "../inkscape.h"
#include "../document.h"
#include "../desktop-handles.h"
#include "../selection.h"
#include "../style.h"
#include "../sp-text.h"
#include "../inkscape-stock.h"
#include <libnrtype/font-style-to-pos.h>

#include "dialog-events.h"
#include "../prefs-utils.h"
#include "../verbs.h"
#include "../interface.h"
#include "../svg/stringstream.h"

#include "text-edit.h"


static void sp_text_edit_dialog_modify_selection (Inkscape::Application *inkscape, SPSelection *sel, guint flags, GtkWidget *dlg);
static void sp_text_edit_dialog_change_selection (Inkscape::Application *inkscape, SPSelection *sel, GtkWidget *dlg);

static void sp_text_edit_dialog_set_default (GtkButton *button, GtkWidget *dlg);
static void sp_text_edit_dialog_apply (GtkButton *button, GtkWidget *dlg);
static void sp_text_edit_dialog_close (GtkButton *button, GtkWidget *dlg);

static void sp_text_edit_dialog_read_selection (GtkWidget *dlg, gboolean style, gboolean content);

static void sp_text_edit_dialog_text_changed (GtkTextBuffer *tb, GtkWidget *dlg);
static void sp_text_edit_dialog_font_changed (SPFontSelector *fontsel, font_instance *font, GtkWidget *dlg);
static void sp_text_edit_dialog_any_toggled (GtkToggleButton *tb, GtkWidget *dlg);
static void sp_text_edit_dialog_line_spacing_changed (GtkEditable *editable, GtkWidget *dlg);

static SPText *sp_ted_get_selected_text_item (void);
static unsigned sp_ted_get_selected_text_count (void);


static const gchar *spacings[] = {"90%", "100%", "110%", "120%", "133%", "150%", "200%", NULL};

static GtkWidget *dlg = NULL;
static win_data wd;
// impossible original values to make sure they are read from prefs
static gint x = -1000, y = -1000, w = 0, h = 0; 
static gchar const *prefs_path = "dialogs.textandfont";




static void
sp_text_edit_dialog_destroy (GtkObject *object, gpointer data)
{
    sp_signal_disconnect_by_data (INKSCAPE, dlg);
    wd.win = dlg = NULL;
    wd.stop = 0;
}



static gboolean
sp_text_edit_dialog_delete (GtkObject *object, GdkEvent *event, gpointer data)
{
    gtk_window_get_position ((GtkWindow *) dlg, &x, &y);
    gtk_window_get_size ((GtkWindow *) dlg, &w, &h);

    prefs_set_int_attribute (prefs_path, "x", x);
    prefs_set_int_attribute (prefs_path, "y", y);
    prefs_set_int_attribute (prefs_path, "w", w);
    prefs_set_int_attribute (prefs_path, "h", h);

    return FALSE; // which means, go ahead and destroy it
}


/**
    These callbacks set the eatkeys flag when the text editor is entered and cancel it when it's left.
    This flag is used to prevent passing keys from the dialog to canvas, so that the text editor
    can handle keys like Esc and Ctrl+Z itself.
 */
gboolean
text_view_focus_in (GtkWidget *w, GdkEventKey *event, gpointer data)
{
    GObject *dlg = (GObject *) data;
    g_object_set_data (dlg, "eatkeys", GINT_TO_POINTER (TRUE));
    return FALSE;
}

gboolean
text_view_focus_out (GtkWidget *w, GdkEventKey *event, gpointer data)
{
    GObject *dlg = (GObject *) data;
    g_object_set_data (dlg, "eatkeys", GINT_TO_POINTER (FALSE));
    return FALSE;
}


void
sp_text_edit_dialog (void)
{

    if (!dlg) {
    
        GtkWidget *mainvb, *nb, *vb, *hb, *txt, *fontsel, *preview, 
                  *f, *tbl, *l, *px, *c, *b, *hs;
        GtkTextBuffer *tb;
        GList *sl;
        int i;

        gchar title[500];
        sp_ui_dialog_title_string (SP_VERB_DIALOG_TEXT, title);

        dlg = sp_window_new (title, TRUE);
        if (x == -1000 || y == -1000) {
            x = prefs_get_int_attribute (prefs_path, "x", 0);
            y = prefs_get_int_attribute (prefs_path, "y", 0);
        }
        
        if (w ==0 || h == 0) {
            w = prefs_get_int_attribute (prefs_path, "w", 0);
            h = prefs_get_int_attribute (prefs_path, "h", 0);
        }
        
        if (x != 0 || y != 0) {
            gtk_window_move ((GtkWindow *) dlg, x, y);
        } else {
            gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
        }
        
        if (w && h) 
            gtk_window_resize ((GtkWindow *) dlg, w, h);
        
        sp_transientize (dlg);
        wd.win = dlg;
        wd.stop = 0;
        g_signal_connect ( G_OBJECT (INKSCAPE), "activate_desktop", 
                           G_CALLBACK (sp_transientize_callback), &wd );
                           
        gtk_signal_connect ( GTK_OBJECT (dlg), "event", GTK_SIGNAL_FUNC (sp_dialog_event_handler), dlg );
                             
        gtk_signal_connect ( GTK_OBJECT (dlg), "destroy", G_CALLBACK (sp_text_edit_dialog_destroy), dlg );
        gtk_signal_connect ( GTK_OBJECT (dlg), "delete_event", G_CALLBACK (sp_text_edit_dialog_delete), dlg );
        g_signal_connect ( G_OBJECT (INKSCAPE), "shut_down", G_CALLBACK (sp_text_edit_dialog_delete), dlg );
                           
        g_signal_connect ( G_OBJECT (INKSCAPE), "dialogs_hide", G_CALLBACK (sp_dialog_hide), dlg );
        g_signal_connect ( G_OBJECT (INKSCAPE), "dialogs_unhide", G_CALLBACK (sp_dialog_unhide), dlg );

        gtk_window_set_policy (GTK_WINDOW (dlg), TRUE, TRUE, FALSE);

        mainvb = gtk_vbox_new (FALSE, 0);
        gtk_container_add (GTK_CONTAINER (dlg), mainvb);

        nb = gtk_notebook_new ();
        gtk_container_set_border_width (GTK_CONTAINER (nb), 4);
        gtk_box_pack_start (GTK_BOX (mainvb), nb, TRUE, TRUE, 0);
        g_object_set_data (G_OBJECT (dlg), "notebook", nb);

        /* Vbox inside notebook */
        vb = gtk_vbox_new (FALSE, 0);

        /* Textview */
        f = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (f), GTK_SHADOW_IN);
        tb = gtk_text_buffer_new (NULL);
        txt = gtk_text_view_new_with_buffer (tb);
#ifdef WITH_GTKSPELL
        GError *error = NULL;
        char *errortext = NULL;
        if (gtkspell_new_attach(GTK_TEXT_VIEW(txt), NULL, &error) == NULL) {
            g_print("gtkspell error: %s\n", error->message);
            errortext = g_strdup_printf("GtkSpell was unable to initialize.\n"
                                        "%s", error->message);
            g_error_free(error);
        }
#endif

        gtk_widget_set_size_request (txt, -1, 64);
        gtk_text_view_set_editable (GTK_TEXT_VIEW (txt), TRUE);
        gtk_container_add (GTK_CONTAINER (f), txt);
        gtk_box_pack_start (GTK_BOX (vb), f, TRUE, TRUE, 0);
        g_signal_connect ( G_OBJECT (tb), "changed", 
                           G_CALLBACK (sp_text_edit_dialog_text_changed), dlg );
        g_signal_connect (G_OBJECT (txt), "focus-in-event", G_CALLBACK (text_view_focus_in), dlg);
        g_signal_connect (G_OBJECT (txt), "focus-out-event", G_CALLBACK (text_view_focus_out), dlg);
        g_object_set_data (G_OBJECT (dlg), "text", tb);
        g_object_set_data (G_OBJECT (dlg), "textw", txt);

        /* HBox containing font selection and layout */
        hb = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vb), hb, TRUE, TRUE, 0);

        fontsel = sp_font_selector_new ();
        g_signal_connect ( G_OBJECT (fontsel), "font_set", 
                           G_CALLBACK (sp_text_edit_dialog_font_changed), dlg );
        gtk_box_pack_start (GTK_BOX (hb), fontsel, TRUE, TRUE, 0);
        g_object_set_data (G_OBJECT (dlg), "fontsel", fontsel);

        /* Layout */
        f = gtk_frame_new (_("Layout"));
        gtk_box_pack_start (GTK_BOX (hb), f, FALSE, FALSE, 4);

        tbl = gtk_table_new (3, 4, FALSE);
        gtk_table_set_row_spacings (GTK_TABLE (tbl), 4);
        gtk_table_set_col_spacings (GTK_TABLE (tbl), 4);
        gtk_container_add (GTK_CONTAINER (f), tbl);

        
        l = gtk_label_new (_("Alignment:"));
        gtk_misc_set_alignment (GTK_MISC (l), 1.0, 0.5);
        gtk_table_attach ( GTK_TABLE (tbl), l, 0, 1, 0, 1, 
                           (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 0 );
        px = gtk_image_new_from_stock ( GTK_STOCK_JUSTIFY_LEFT, 
                                        GTK_ICON_SIZE_LARGE_TOOLBAR );
        b = gtk_radio_button_new (NULL);
        g_signal_connect ( G_OBJECT (b), "toggled", 
                           G_CALLBACK (sp_text_edit_dialog_any_toggled), dlg);
        gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (b), FALSE );
        gtk_container_add (GTK_CONTAINER (b), px);
        gtk_table_attach ( GTK_TABLE (tbl), b, 1, 2, 0, 1, 
                           (GtkAttachOptions)0, (GtkAttachOptions)0, 0, 0 );
        g_object_set_data (G_OBJECT (dlg), "text_anchor_start", b);
        px = gtk_image_new_from_stock ( GTK_STOCK_JUSTIFY_CENTER, 
                                        GTK_ICON_SIZE_LARGE_TOOLBAR );
        b = gtk_radio_button_new (gtk_radio_button_group (GTK_RADIO_BUTTON (b)));
        g_signal_connect ( G_OBJECT (b), "toggled", 
                           G_CALLBACK (sp_text_edit_dialog_any_toggled), dlg );
        gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (b), FALSE);
        gtk_container_add (GTK_CONTAINER (b), px);
        gtk_table_attach ( GTK_TABLE (tbl), b, 2, 3, 0, 1, 
                           (GtkAttachOptions)0, (GtkAttachOptions)0, 0, 0 );
        g_object_set_data (G_OBJECT (dlg), "text_anchor_middle", b);
        px = gtk_image_new_from_stock ( GTK_STOCK_JUSTIFY_RIGHT, 
                                        GTK_ICON_SIZE_LARGE_TOOLBAR );
        b = gtk_radio_button_new (gtk_radio_button_group (GTK_RADIO_BUTTON (b)));
        g_signal_connect ( G_OBJECT (b), "toggled", 
                           G_CALLBACK (sp_text_edit_dialog_any_toggled), dlg );
        gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (b), FALSE);
        gtk_container_add (GTK_CONTAINER (b), px);
        gtk_table_attach ( GTK_TABLE (tbl), b, 3, 4, 0, 1, 
                           (GtkAttachOptions)0, (GtkAttachOptions)0, 0, 0 );
        g_object_set_data (G_OBJECT (dlg), "text_anchor_end", b);

        l = gtk_label_new (_("Orientation:"));
        gtk_misc_set_alignment (GTK_MISC (l), 1.0, 0.5);
        gtk_table_attach ( GTK_TABLE (tbl), l, 0, 1, 1, 2, (GtkAttachOptions)0, 
                           (GtkAttachOptions)0, 4, 0);
        px = gtk_image_new_from_stock ( INKSCAPE_STOCK_WRITING_MODE_LR, 
                                        GTK_ICON_SIZE_LARGE_TOOLBAR );
        b = gtk_radio_button_new (NULL);
        g_signal_connect ( G_OBJECT (b), "toggled", 
                           G_CALLBACK (sp_text_edit_dialog_any_toggled), dlg );
        gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (b), FALSE);
        gtk_container_add (GTK_CONTAINER (b), px);
        gtk_table_attach ( GTK_TABLE (tbl), b, 1, 2, 1, 2, 
                           (GtkAttachOptions)0, (GtkAttachOptions)0, 0, 0 );
        g_object_set_data (G_OBJECT (dlg), "writing_mode_lr", b);
        px = gtk_image_new_from_stock ( INKSCAPE_STOCK_WRITING_MODE_TB, 
                                        GTK_ICON_SIZE_LARGE_TOOLBAR );
        b = gtk_radio_button_new (gtk_radio_button_group (GTK_RADIO_BUTTON (b)));
        g_signal_connect ( G_OBJECT (b), "toggled", 
                           G_CALLBACK (sp_text_edit_dialog_any_toggled), dlg );
        gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (b), FALSE);
        gtk_container_add (GTK_CONTAINER (b), px);
        gtk_table_attach ( GTK_TABLE (tbl), b, 2, 3, 1, 2, 
                           (GtkAttachOptions)0, (GtkAttachOptions)0, 0, 0 );
        g_object_set_data (G_OBJECT (dlg), "writing_mode_tb", b);

        l = gtk_label_new (_("Line spacing:"));
        gtk_misc_set_alignment (GTK_MISC (l), 1.0, 0.5);
        gtk_table_attach ( GTK_TABLE (tbl), l, 0, 1, 2, 3, 
                           (GtkAttachOptions)(GTK_EXPAND | GTK_FILL), 
                           (GtkAttachOptions)0, 4, 0 );
        c = gtk_combo_new ();
        gtk_combo_set_value_in_list ((GtkCombo *) c, FALSE, FALSE);
        gtk_combo_set_use_arrows ((GtkCombo *) c, TRUE);
        gtk_combo_set_use_arrows_always ((GtkCombo *) c, TRUE);
        gtk_widget_set_size_request (c, 64, -1);
        /* Setup strings */
        sl = NULL;
        for (i = 0; spacings[i]; i++) {
            sl = g_list_prepend (sl, (void *) spacings[i]);
        }
        sl = g_list_reverse (sl);
        gtk_combo_set_popdown_strings ((GtkCombo *) c, sl);
        g_list_free (sl);
        g_signal_connect ( (GObject *) ((GtkCombo *) c)->entry, 
                           "changed", 
                           (GCallback) sp_text_edit_dialog_line_spacing_changed,
                           dlg );
        gtk_table_attach ( GTK_TABLE (tbl), c, 1, 4, 2, 3, 
                           (GtkAttachOptions)0, (GtkAttachOptions)0, 4, 0 );
        g_object_set_data (G_OBJECT (dlg), "line_spacing", c);

        /* Font preview */
        preview = sp_font_preview_new ();
        gtk_box_pack_start (GTK_BOX (vb), preview, TRUE, TRUE, 4);
        g_object_set_data (G_OBJECT (dlg), "preview", preview);

        l = gtk_label_new (_("Text and font"));
        gtk_notebook_append_page (GTK_NOTEBOOK (nb), vb, l);

        /* Buttons */
        hs = gtk_hseparator_new ();
        gtk_box_pack_start (GTK_BOX (mainvb), hs, FALSE, FALSE, 0);

        hb = gtk_hbox_new (FALSE, 4);
        gtk_container_set_border_width (GTK_CONTAINER (hb), 4);
        gtk_box_pack_start (GTK_BOX (mainvb), hb, FALSE, FALSE, 0);

        b = gtk_button_new_with_label (_("Set as default"));
        g_signal_connect ( G_OBJECT (b), "clicked", 
                           G_CALLBACK (sp_text_edit_dialog_set_default), 
                           dlg );
        gtk_box_pack_start (GTK_BOX (hb), b, FALSE, FALSE, 0);
        g_object_set_data (G_OBJECT (dlg), "default", b);

        b = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
        g_signal_connect ( G_OBJECT (b), "clicked", 
                           G_CALLBACK (sp_text_edit_dialog_close), dlg );
        gtk_box_pack_end (GTK_BOX (hb), b, FALSE, FALSE, 0);

        b = gtk_button_new_from_stock (GTK_STOCK_APPLY);
        g_signal_connect ( G_OBJECT (b), "clicked", 
                           G_CALLBACK (sp_text_edit_dialog_apply), dlg );
        gtk_box_pack_end ( GTK_BOX (hb), b, FALSE, FALSE, 0 );
        g_object_set_data (G_OBJECT (dlg), "apply", b);

        g_signal_connect ( G_OBJECT (INKSCAPE), "modify_selection", 
                           G_CALLBACK (sp_text_edit_dialog_modify_selection), dlg);
        g_signal_connect ( G_OBJECT (INKSCAPE), "change_selection", 
                           G_CALLBACK (sp_text_edit_dialog_change_selection), dlg);

        gtk_widget_show_all (dlg);

        sp_text_edit_dialog_read_selection (dlg, TRUE, TRUE);
    }

    gtk_window_present ((GtkWindow *) dlg);

} // end of sp_text_edit_dialog()



static void
sp_text_edit_dialog_modify_selection ( Inkscape::Application *inkscape, 
                                       SPSelection *sel, 
                                       guint flags, 
                                       GtkWidget *dlg )
{
    gboolean style, content;

    style = 
        ((flags & ( SP_OBJECT_CHILD_MODIFIED_FLAG | 
                    SP_OBJECT_STYLE_MODIFIED_FLAG  )) != 0 );
    
    content = 
        ((flags & ( SP_OBJECT_CHILD_MODIFIED_FLAG | 
                    SP_TEXT_CONTENT_MODIFIED_FLAG  )) != 0 );
    
    sp_text_edit_dialog_read_selection (dlg, style, content);

} // end of sp_text_edit_dialog_modify_selection()



static void
sp_text_edit_dialog_change_selection ( Inkscape::Application *inkscape, 
                                       SPSelection *sel, 
                                       GtkWidget *dlg )
{
    sp_text_edit_dialog_read_selection (dlg, TRUE, TRUE);
}



static void
sp_text_edit_dialog_update_object ( SPText *text, SPRepr *repr )
{
    g_object_set_data (G_OBJECT (dlg), "blocked", GINT_TO_POINTER (TRUE));

    if (text)
    {
        GtkTextBuffer *tb;
        GtkTextIter start, end;
        gchar *str;

        tb = (GtkTextBuffer*)g_object_get_data (G_OBJECT (dlg), "text");

        /* Content */
        if (gtk_text_buffer_get_modified (tb)) {
            gtk_text_buffer_get_bounds (tb, &start, &end);
            str = gtk_text_buffer_get_text (tb, &start, &end, TRUE);
            sp_text_set_repr_text_multiline (text, str);
            g_free (str);    
            gtk_text_buffer_set_modified (tb, FALSE);
        }
    } // end of if (text)

    
    if (repr)
    {
        GtkWidget *fontsel = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "fontsel");
        GtkWidget *preview = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "preview");

        SPCSSAttr *css = sp_repr_css_attr_new ();

        /* font */
        font_instance *font = sp_font_selector_get_font (SP_FONT_SELECTOR (fontsel));

        gchar c[256];
        font->Family(c, 256);
        sp_repr_css_set_property (css, "font-family", c);

        font->Attribute( "weight", c, 256);
        sp_repr_css_set_property (css, "font-weight", c);

        font->Attribute("style", c, 256);
        sp_repr_css_set_property (css, "font-style", c);

        font->Attribute("stretch", c, 256);
        sp_repr_css_set_property (css, "font-stretch", c);

        font->Attribute("variant", c, 256);
        sp_repr_css_set_property (css, "font-variant", c);

        Inkscape::SVGOStringStream os;
        os << sp_font_selector_get_size (SP_FONT_SELECTOR (fontsel));
        sp_repr_css_set_property (css, "font-size", os.str().c_str());

        font->Unref();
        font=NULL;
				
        /* Layout */
        GtkWidget *b = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "text_anchor_start");
        
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (b))) {
            sp_repr_css_set_property (css, "text-anchor", "start");
        } else {
            b = (GtkWidget*)g_object_get_data ( G_OBJECT (dlg), 
                                                "text_anchor_middle");
            if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (b))) {
                sp_repr_css_set_property (css, "text-anchor", "middle");
            } else {
                sp_repr_css_set_property (css, "text-anchor", "end");
            }
        }
        
        b = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "writing_mode_lr");
        
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (b))) {
            sp_repr_css_set_property (css, "writing-mode", "lr");
        } else {
            sp_repr_css_set_property (css, "writing-mode", "tb");
        }
        GtkWidget *combo = (GtkWidget*)g_object_get_data ((GObject *) dlg, "line_spacing");
        const char *sstr = gtk_entry_get_text ((GtkEntry *) ((GtkCombo *) (combo))->entry);
        sp_repr_set_attr (repr, "sodipodi:linespacing", sstr);

        sp_repr_css_change (repr, css, "style");
        sp_repr_css_attr_unref (css);
    
    } // end of if (repr)

    
    if (text) {
        sp_document_done (SP_DT_DOCUMENT (SP_ACTIVE_DESKTOP));
    }

    g_object_set_data (G_OBJECT (dlg), "blocked", NULL);
    
} // end of sp_text_edit_dialog_update_object()



static void
sp_text_edit_dialog_set_default (GtkButton *button, GtkWidget *dlg)
{
    GtkWidget *def;
    SPRepr *repr;

    def = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "default");

    repr = inkscape_get_repr (INKSCAPE, "tools.text");

    sp_text_edit_dialog_update_object (NULL, repr);

    gtk_widget_set_sensitive (def, FALSE);
}



static void
sp_text_edit_dialog_apply (GtkButton *button, GtkWidget *dlg)
{
    GtkWidget *apply, *def;
    SPText *text;
    SPRepr *repr;
    const GSList *item;
    unsigned items;

    g_object_set_data (G_OBJECT (dlg), "blocked", GINT_TO_POINTER (TRUE));

    apply = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "apply");
    def = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "default");

    text = NULL;
    items = 0;
    item = SP_DT_SELECTION(SP_ACTIVE_DESKTOP)->itemList();

    for (; item != NULL; item = item->next) { 
        // apply style to the reprs of all text objects in the selection
        if (!SP_IS_TEXT (item->data)) continue;
        text = SP_TEXT(item->data);
        repr = SP_OBJECT_REPR (text);
        sp_text_edit_dialog_update_object (NULL, repr);
        ++items;
    }
    
    if (items == 0) { 
        // no text objects; apply style to new objects
        repr = inkscape_get_repr (INKSCAPE, "tools.text");
        sp_text_edit_dialog_update_object (NULL, repr);
        gtk_widget_set_sensitive (def, FALSE);
    } else if (items == 1) {
        /* exactly one text object; now set its text, too (this will also 
         *complete the transaction)
         */
        sp_text_edit_dialog_update_object (text, NULL);
    } else {
        // many text objects; do not change text, simply complete the 
        // transaction
        sp_document_done (SP_DT_DOCUMENT (SP_ACTIVE_DESKTOP));
    }

    gtk_widget_set_sensitive (apply, FALSE);

    g_object_set_data (G_OBJECT (dlg), "blocked", NULL);
}



static void
sp_text_edit_dialog_close (GtkButton *button, GtkWidget *dlg)
{
    gtk_widget_destroy (GTK_WIDGET (dlg));
}



static void
sp_text_edit_dialog_read_selection ( GtkWidget *dlg, 
                                     gboolean dostyle, 
                                     gboolean docontent )
{
    if (g_object_get_data (G_OBJECT (dlg), "blocked")) 
        return;
    
    g_object_set_data (G_OBJECT (dlg), "blocked", GINT_TO_POINTER (TRUE));

    GtkWidget *notebook = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "notebook");
    GtkWidget *textw = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "textw");
    GtkWidget *fontsel = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "fontsel");
    GtkWidget *preview = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "preview");
    GtkWidget *apply = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "apply");
    GtkWidget *def = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "default");

    GtkTextBuffer *tb = (GtkTextBuffer*)g_object_get_data (G_OBJECT (dlg), "text");

    SPText *text = sp_ted_get_selected_text_item ();

    SPRepr *repr;
    SPStyle *style;
    if (text)
    {
        guint items = sp_ted_get_selected_text_count ();
        if (items == 1) {
            gtk_widget_set_sensitive (textw, TRUE);
        } else {
            gtk_widget_set_sensitive (textw, FALSE);
        }
        gtk_widget_set_sensitive (apply, FALSE);
        gtk_widget_set_sensitive (def, TRUE);
        style = SP_OBJECT_STYLE (text);

        if (docontent) {
            gchar *str;
            str = sp_text_get_string_multiline (text);

            if (str) {
                int pos;
                pos = 0;

                if (items == 1) {
                    gtk_text_buffer_set_text (tb, str, strlen (str));
                    gtk_text_buffer_set_modified (tb, FALSE);
                }
                sp_font_preview_set_phrase (SP_FONT_PREVIEW (preview), str);
                g_free (str);

            } else {
                gtk_text_buffer_set_text (tb, "", 0);
                sp_font_preview_set_phrase (SP_FONT_PREVIEW (preview), NULL);
            }
        } // end of if (docontent)
        repr = SP_OBJECT_REPR (text);
        
    } else {
        gtk_widget_set_sensitive (textw, FALSE);
        gtk_widget_set_sensitive (apply, FALSE);
        gtk_widget_set_sensitive (def, FALSE);
        repr = inkscape_get_repr (INKSCAPE, "tools.text");
        if (repr) {
            gtk_widget_set_sensitive (notebook, TRUE);
            style = sp_style_new ();
            sp_style_read_from_repr (style, repr);
        } else {
            gtk_widget_set_sensitive (notebook, FALSE);
            style = sp_style_new ();
        }
    }

    if (dostyle) {
        GtkWidget *b, *combo;
        const gchar *sstr;

        font_instance *font = (font_factory::Default())->Face ( style->text->font_family.value, font_style_to_pos(*style) );

        if (font) {
            // the font is oversized, so we need to pass the true size separately
            sp_font_selector_set_font (SP_FONT_SELECTOR (fontsel), font, style->font_size.computed);
            sp_font_preview_set_font (SP_FONT_PREVIEW (preview), font, SP_FONT_SELECTOR(fontsel));
// crashed preview!
//						font->Unref();
//						font=NULL;
        }

        if (style->text_anchor.computed == SP_CSS_TEXT_ANCHOR_START) {
            b = (GtkWidget*)g_object_get_data ( G_OBJECT (dlg), 
                                                "text_anchor_start" );
        } else if (style->text_anchor.computed == SP_CSS_TEXT_ANCHOR_MIDDLE) {
            b = (GtkWidget*)g_object_get_data ( G_OBJECT (dlg), 
                                                "text_anchor_middle" );
        } else {
            b = (GtkWidget*)g_object_get_data ( G_OBJECT (dlg), 
                                                "text_anchor_end" );
        }

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b), TRUE);
        
        if (style->writing_mode.computed == SP_CSS_WRITING_MODE_LR) {
            b = (GtkWidget*)g_object_get_data ( G_OBJECT (dlg), 
                                                "writing_mode_lr" );
        } else {
            b = (GtkWidget*)g_object_get_data ( G_OBJECT (dlg), 
                                                "writing_mode_tb" );
        }
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b), TRUE);
        combo = (GtkWidget*)g_object_get_data ( G_OBJECT (dlg), 
                                                "line_spacing" );
        sstr = (repr) ? sp_repr_attr (repr, "sodipodi:linespacing") : NULL;
        gtk_entry_set_text ((GtkEntry *) ((GtkCombo *) (combo))->entry, 
                    (sstr) ? sstr : (const gchar *) "100%");
    }

    g_object_set_data (G_OBJECT (dlg), "blocked", NULL);
   
} // end of sp_text_edit_dialog_read_selection()



static void
sp_text_edit_dialog_text_changed (GtkTextBuffer *tb, GtkWidget *dlg)
{
    GtkWidget *textw, *preview, *apply, *def;
    SPText *text;
    GtkTextIter start, end;
    gchar *str;

    if (g_object_get_data (G_OBJECT (dlg), "blocked")) 
        return;

    text = sp_ted_get_selected_text_item ();

    textw = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "textw");
    preview = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "preview");
    apply = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "apply");
    def = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "default");

    gtk_text_buffer_get_bounds (tb, &start, &end);
    str = gtk_text_buffer_get_text (tb, &start, &end, TRUE);

    if (str && *str) {
        sp_font_preview_set_phrase (SP_FONT_PREVIEW (preview), str);
    } else {
        sp_font_preview_set_phrase (SP_FONT_PREVIEW (preview), NULL);
    }
    g_free (str);

    if (text) {
        gtk_widget_set_sensitive (apply, TRUE);
    }
    gtk_widget_set_sensitive (def, TRUE);

} // end of sp_text_edit_dialog_text_changed()



static void
sp_text_edit_dialog_font_changed ( SPFontSelector *fsel, 
                                   font_instance *font, 
                                   GtkWidget *dlg )
{
    GtkWidget *preview, *apply, *def;
    SPText *text;

    if (g_object_get_data (G_OBJECT (dlg), "blocked")) 
        return;

    text = sp_ted_get_selected_text_item ();

    preview = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "preview");
    apply = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "apply");
    def = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "default");

    sp_font_preview_set_font (SP_FONT_PREVIEW (preview), font, SP_FONT_SELECTOR(fsel));

    if (text) {
        gtk_widget_set_sensitive (apply, TRUE);
    }
    gtk_widget_set_sensitive (def, TRUE);

} // end of sp_text_edit_dialog_font_changed()



static void
sp_text_edit_dialog_any_toggled (GtkToggleButton *tb, GtkWidget *dlg)
{
    GtkWidget *apply, *def;
    SPText *text;

    if (g_object_get_data (G_OBJECT (dlg), "blocked"))
        return;

    text = sp_ted_get_selected_text_item ();

    apply = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "apply");
    def = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "default");

    if (text) {
        gtk_widget_set_sensitive (apply, TRUE);
    }
    gtk_widget_set_sensitive (def, TRUE);
}



static void
sp_text_edit_dialog_line_spacing_changed (GtkEditable *editable, GtkWidget *dlg)
{
    GtkWidget *apply, *def;
    SPText *text;

    if (g_object_get_data ((GObject *) dlg, "blocked")) 
        return;

    text = sp_ted_get_selected_text_item ();

    apply = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "apply");
    def = (GtkWidget*)g_object_get_data (G_OBJECT (dlg), "default");

    if (text) {
        gtk_widget_set_sensitive (apply, TRUE);
    }
    gtk_widget_set_sensitive (def, TRUE);
}



static SPText *
sp_ted_get_selected_text_item (void)
{
    if (!SP_ACTIVE_DESKTOP)
        return NULL;

    for (const GSList *item = SP_DT_SELECTION(SP_ACTIVE_DESKTOP)->itemList();
         item != NULL;
         item = item->next)
    {
        if (SP_IS_TEXT(item->data))
            return SP_TEXT (item->data);
    }

    return NULL;
}



static unsigned
sp_ted_get_selected_text_count (void)
{
    if (!SP_ACTIVE_DESKTOP) 
        return 0;

    unsigned int items = 0;

    for (const GSList *item = SP_DT_SELECTION(SP_ACTIVE_DESKTOP)->itemList();
         item != NULL;
         item = item->next)
    {
        if (SP_IS_TEXT(item->data))
            ++items;
    }

    return items;
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=c++:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
