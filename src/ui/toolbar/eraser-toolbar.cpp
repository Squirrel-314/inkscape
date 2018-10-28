/**
 * @file
 * Erasor aux toolbar
 */
/* Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Frank Felfe <innerspace@iname.com>
 *   John Cliff <simarilius@yahoo.com>
 *   David Turner <novalis@gnu.org>
 *   Josh Andler <scislac@scislac.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2003 MenTaLguY
 * Copyright (C) 1999-2011 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <glibmm/i18n.h>

#include "eraser-toolbar.h"

#include <array>

#include "desktop.h"
#include "document-undo.h"
#include "widgets/ege-adjustment-action.h"
#include "widgets/ink-toggle-action.h"
#include "widgets/toolbox.h"
#include "ui/widget/ink-select-one-action.h"
#include "ui/icon-names.h"
#include "ui/tools/eraser-tool.h"

using Inkscape::DocumentUndo;
using Inkscape::UI::ToolboxFactory;
using Inkscape::UI::PrefPusher;

//########################
//##       Eraser       ##
//########################

// A dummy function for PrefPusher.
// The code was calling the update_presets_list function in the calligraphy tool
// which was immediately returning. TODO: Investigate this further.
void eraser_update_presets_list(GObject *tbl)
{
    return;
}

static void sp_erc_width_value_changed( GtkAdjustment *adj, GObject *tbl )
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setDouble( "/tools/eraser/width", gtk_adjustment_get_value(adj) );
}

static void sp_erc_mass_value_changed( GtkAdjustment *adj, GObject* tbl )
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setDouble( "/tools/eraser/mass", gtk_adjustment_get_value(adj) );
}

static void sp_erc_velthin_value_changed( GtkAdjustment *adj, GObject* tbl )
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setDouble("/tools/eraser/thinning", gtk_adjustment_get_value(adj) );
}

static void sp_erc_cap_rounding_value_changed( GtkAdjustment *adj, GObject* tbl )
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setDouble( "/tools/eraser/cap_rounding", gtk_adjustment_get_value(adj) );
}

static void sp_erc_tremor_value_changed( GtkAdjustment *adj, GObject* tbl )
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setDouble( "/tools/eraser/tremor", gtk_adjustment_get_value(adj) );
}

static void sp_set_tbl_eraser_mode_visibility(GObject *const tbl, const guint eraser_mode)
{
    gtk_action_set_visible( GTK_ACTION( g_object_get_data(tbl, "split") ), (eraser_mode == ERASER_MODE_CUT));

    const gboolean visibility = (eraser_mode != ERASER_MODE_DELETE);
    const std::array<const gchar *, 6> arr = {"cap_rounding", "mass", "thinning", "tremor", "usepressure", "width"};
    for (const gchar * str : arr) {
        gtk_action_set_visible( GTK_ACTION( g_object_get_data(tbl, str) ), visibility );
    }
}

static void sp_erasertb_mode_changed( GObject *tbl, int mode )
{
    SPDesktop *desktop = static_cast<SPDesktop *>(g_object_get_data( tbl, "desktop" ));
    if (DocumentUndo::getUndoSensitive(desktop->getDocument())) {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->setInt( "/tools/eraser/mode", mode );
    }
    sp_set_tbl_eraser_mode_visibility(tbl, mode);

    // only take action if run by the attr_changed listener
    if (!g_object_get_data( tbl, "freeze" )) {
        // in turn, prevent listener from responding
        g_object_set_data( tbl, "freeze", GINT_TO_POINTER(TRUE) );

        /*
        if ( eraser_mode != ERASER_MODE_DELETE ) {
        } else {
        }
        */
        // TODO finish implementation

        g_object_set_data( tbl, "freeze", GINT_TO_POINTER(FALSE) );
    }
}

static void sp_toggle_break_apart( GtkToggleAction* act, gpointer data )
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    gboolean active = gtk_toggle_action_get_active(act);
    prefs->setBool("/tools/eraser/break_apart", active);
}

void sp_eraser_toolbox_prep(SPDesktop *desktop, GtkActionGroup* mainActions, GObject* holder)
{
    GtkIconSize secondarySize = ToolboxFactory::prefToSize("/toolbox/secondary", 1);
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    gint eraser_mode = ERASER_MODE_DELETE;

    {
        InkSelectOneActionColumns columns;

        Glib::RefPtr<Gtk::ListStore> store = Gtk::ListStore::create(columns);

        Gtk::TreeModel::Row row;

        row = *(store->append());
        row[columns.col_label    ] = _("Delete");
        row[columns.col_tooltip  ] = _("Delete objects touched by eraser");
        row[columns.col_icon     ] = INKSCAPE_ICON("draw-eraser-delete-objects");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("Cut");
        row[columns.col_tooltip  ] = _("Cut out from paths and shapes");
        row[columns.col_icon     ] = INKSCAPE_ICON("path-difference");
        row[columns.col_sensitive] = true;

        row = *(store->append());
        row[columns.col_label    ] = _("Clip");
        row[columns.col_tooltip  ] = _("Clip from objects");
        row[columns.col_icon     ] = INKSCAPE_ICON("path-intersection");
        row[columns.col_sensitive] = true;

        InkSelectOneAction* act =
            InkSelectOneAction::create( "EraserModeAction",  // Name
                                        _("Mode"),           // Label
                                        "",                  // Tooltip
                                        "Not Used",          // Icon
                                        store );             // Tree store
        act->use_radio( true );
        act->use_group_label( true );
        eraser_mode = prefs->getInt("/tools/eraser/mode", ERASER_MODE_CLIP); // Used at end
        act->set_active( eraser_mode );

        gtk_action_group_add_action( mainActions, GTK_ACTION( act->gobj() ));
        g_object_set_data( holder, "EraserModeAction", act );

        act->signal_changed().connect(sigc::bind<0>(sigc::ptr_fun(&sp_erasertb_mode_changed), holder));
    }


    /* Width */
    {
        gchar const* labels[] = {_("(no width)"),_("(hairline)"), nullptr, nullptr, nullptr, _("(default)"), nullptr, nullptr, nullptr, nullptr, _("(broad stroke)")};
        gdouble values[] = {0, 1, 3, 5, 10, 15, 20, 30, 50, 75, 100};
        EgeAdjustmentAction *eact = create_adjustment_action( "EraserWidthAction",
                                                              _("Pen Width"), _("Width:"),
                                                              _("The width of the eraser pen (relative to the visible canvas area)"),
                                                              "/tools/eraser/width", 15,
                                                              GTK_WIDGET(desktop->canvas), holder, TRUE, "altx-eraser",
                                                              0, 100, 1.0, 10.0,
                                                              labels, values, G_N_ELEMENTS(labels),
                                                              sp_erc_width_value_changed, nullptr /*unit tracker*/, 1, 0);
        ege_adjustment_action_set_appearance( eact, TOOLBAR_SLIDER_HINT );
        gtk_action_group_add_action( mainActions, GTK_ACTION(eact) );
        g_object_set_data( holder, "width", eact );
        gtk_action_set_sensitive( GTK_ACTION(eact), TRUE );
    }


    /* Use Pressure button */
    {
        InkToggleAction* act = ink_toggle_action_new( "EraserPressureAction",
                                                      _("Eraser Pressure"),
                                                      _("Use the pressure of the input device to alter the width of the pen"),
                                                      INKSCAPE_ICON("draw-use-pressure"),
                                                      GTK_ICON_SIZE_MENU );
        gtk_action_group_add_action( mainActions, GTK_ACTION( act ) );
        PrefPusher *pusher = new PrefPusher(GTK_TOGGLE_ACTION(act), "/tools/eraser/usepressure", eraser_update_presets_list, holder);
        g_signal_connect( holder, "destroy", G_CALLBACK(delete_prefspusher), pusher);
        g_object_set_data( holder, "usepressure", act );
    }


    /* Thinning */
    {
        gchar const* labels[] = {_("(speed blows up stroke)"), nullptr, nullptr, _("(slight widening)"), _("(constant width)"), _("(slight thinning, default)"), nullptr, nullptr, _("(speed deflates stroke)")};
        gdouble values[] = {-100, -40, -20, -10, 0, 10, 20, 40, 100};
        EgeAdjustmentAction* eact = create_adjustment_action( "EraserThinningAction",
                                                              _("Eraser Stroke Thinning"), _("Thinning:"),
                                                              _("How much velocity thins the stroke (> 0 makes fast strokes thinner, < 0 makes them broader, 0 makes width independent of velocity)"),
                                                              "/tools/eraser/thinning", 10,
                                                              GTK_WIDGET(desktop->canvas), holder, FALSE, nullptr,
                                                              -100, 100, 1, 10.0,
                                                              labels, values, G_N_ELEMENTS(labels),
                                                              sp_erc_velthin_value_changed, nullptr /*unit tracker*/, 1, 0);
        gtk_action_group_add_action( mainActions, GTK_ACTION(eact) );
        g_object_set_data( holder, "thinning", eact );
        gtk_action_set_sensitive( GTK_ACTION(eact), TRUE );
    }


    /* Cap Rounding */
    {
            gchar const* labels[] = {_("(blunt caps, default)"), _("(slightly bulging)"), nullptr, nullptr, _("(approximately round)"), _("(long protruding caps)")};
        gdouble values[] = {0, 0.3, 0.5, 1.0, 1.4, 5.0};
        // TRANSLATORS: "cap" means "end" (both start and finish) here
        EgeAdjustmentAction* eact = create_adjustment_action( "EraserCapRoundingAction",
                                                              _("Eraser Cap rounding"), _("Caps:"),
                                                              _("Increase to make caps at the ends of strokes protrude more (0 = no caps, 1 = round caps)"),
                                                              "/tools/eraser/cap_rounding", 0.0,
                                                              GTK_WIDGET(desktop->canvas), holder, FALSE, nullptr,
                                                              0.0, 5.0, 0.01, 0.1,
                                                              labels, values, G_N_ELEMENTS(labels),
                                                              sp_erc_cap_rounding_value_changed, nullptr /*unit tracker*/, 0.01, 2 );
        gtk_action_group_add_action( mainActions, GTK_ACTION(eact) );
        g_object_set_data( holder, "cap_rounding", eact );
        gtk_action_set_sensitive( GTK_ACTION(eact), TRUE );
    }


    /* Tremor */
    {
        gchar const* labels[] = {_("(smooth line)"), _("(slight tremor)"), _("(noticeable tremor)"), nullptr, nullptr, _("(maximum tremor)")};
        gdouble values[] = {0, 10, 20, 40, 60, 100};
        EgeAdjustmentAction* eact = create_adjustment_action( "EraserTremorAction",
                                                              _("EraserStroke Tremor"), _("Tremor:"),
                                                              _("Increase to make strokes rugged and trembling"),
                                                              "/tools/eraser/tremor", 0.0,
                                                              GTK_WIDGET(desktop->canvas), holder, FALSE, nullptr,
                                                              0.0, 100, 1, 10.0,
                                                              labels, values, G_N_ELEMENTS(labels),
                                                              sp_erc_tremor_value_changed, nullptr /*unit tracker*/, 1, 0);

        ege_adjustment_action_set_appearance( eact, TOOLBAR_SLIDER_HINT );
        g_object_set_data( holder, "tremor", eact );
        gtk_action_group_add_action( mainActions, GTK_ACTION(eact) );
        gtk_action_set_sensitive( GTK_ACTION(eact), TRUE );
    }


    /* Mass */
    {
            gchar const* labels[] = {_("(no inertia)"), _("(slight smoothing, default)"), _("(noticeable lagging)"), nullptr, nullptr, _("(maximum inertia)")};
        gdouble values[] = {0.0, 2, 10, 20, 50, 100};
        EgeAdjustmentAction* eact = create_adjustment_action( "EraserMassAction",
                                                              _("Eraser Mass"), _("Mass:"),
                                                              _("Increase to make the eraser drag behind, as if slowed by inertia"),
                                                              "/tools/eraser/mass", 10.0,
                                                              GTK_WIDGET(desktop->canvas), holder, FALSE, nullptr,
                                                              0.0, 100, 1, 10.0,
                                                              labels, values, G_N_ELEMENTS(labels),
                                                              sp_erc_mass_value_changed, nullptr /*unit tracker*/, 1, 0);
        ege_adjustment_action_set_appearance( eact, TOOLBAR_SLIDER_HINT );
        g_object_set_data( holder, "mass", eact );
        gtk_action_group_add_action( mainActions, GTK_ACTION(eact) );
        gtk_action_set_sensitive( GTK_ACTION(eact), TRUE );
    }


    /* Overlap */
    {
        InkToggleAction* act = ink_toggle_action_new( "EraserBreakAppart",
                                                      _("Break apart cut items"),
                                                      _("Break apart cut items"),
                                                      INKSCAPE_ICON("distribute-randomize"),
                                                      secondarySize );
        gtk_toggle_action_set_active( GTK_TOGGLE_ACTION(act), prefs->getBool("/tools/eraser/break_apart", false) );
        g_object_set_data( holder, "split", act );
        g_signal_connect_after( G_OBJECT(act), "toggled", G_CALLBACK(sp_toggle_break_apart), holder) ;
        gtk_action_group_add_action( mainActions, GTK_ACTION(act) );
    }

    sp_set_tbl_eraser_mode_visibility(holder, eraser_mode);
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :