#define __SP_OBJECT_UI_C__

/*
 * Unser-interface related object extension
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * This code is in public domain
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "object-ui.h"
#include "xml/repr.h"

static void sp_object_type_menu(GType type, SPObject *object, SPDesktop *desktop, GtkMenu *menu);

/* Append object-specific part to context menu */

void
sp_object_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu)
{
    GObjectClass *klass;
    klass = G_OBJECT_GET_CLASS(object);
    while (G_TYPE_CHECK_CLASS_TYPE((klass), SP_TYPE_OBJECT)) {
        GType type;
        type = G_TYPE_FROM_CLASS(klass);
        sp_object_type_menu(type, object, desktop, menu);
        klass = (GObjectClass*)g_type_class_peek_parent(klass);
    }
}

/* Implementation */

#include <gtk/gtkmenuitem.h>
#include <gtk/gtksignal.h>

#include <glibmm/i18n.h>

#include "sp-item-group.h"
#include "sp-anchor.h"
#include "sp-image.h"
#include "sp-rect.h"
#include "sp-star.h"
#include "sp-ellipse.h"
#include "sp-spiral.h"

#include "document.h"
#include "desktop-handles.h"
#include "selection.h"

#include "dialogs/item-properties.h"
#include "dialogs/object-attributes.h"
#include "dialogs/fill-style.h"
#include "dialogs/object-properties.h"

#include "sp-flowtext.h"
#include "sp-flowregion.h"
#include "sp-flowdiv.h"
#include "sp-path.h"


static void sp_item_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu);
static void sp_group_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu);
static void sp_anchor_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu);
static void sp_image_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu);
static void sp_shape_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu);

static void
sp_object_type_menu(GType type, SPObject *object, SPDesktop *desktop, GtkMenu *menu)
{
    static GHashTable *t2m = NULL;
    void (* handler)(SPObject *object, SPDesktop *desktop, GtkMenu *menu);
    if (!t2m) {
        t2m = g_hash_table_new(NULL, NULL);
        g_hash_table_insert(t2m, GUINT_TO_POINTER(SP_TYPE_ITEM), (void*)sp_item_menu);
        g_hash_table_insert(t2m, GUINT_TO_POINTER(SP_TYPE_GROUP), (void*)sp_group_menu);
        g_hash_table_insert(t2m, GUINT_TO_POINTER(SP_TYPE_ANCHOR), (void*)sp_anchor_menu);
        g_hash_table_insert(t2m, GUINT_TO_POINTER(SP_TYPE_IMAGE), (void*)sp_image_menu);
        g_hash_table_insert(t2m, GUINT_TO_POINTER(SP_TYPE_SHAPE), (void*)sp_shape_menu);
    }
    handler = (void (*)(SPObject*, SPDesktop*, GtkMenu*))g_hash_table_lookup(t2m, GUINT_TO_POINTER(type));
    if (handler) handler(object, desktop, menu);
}

/* SPItem */

static void sp_item_properties(GtkMenuItem *menuitem, SPItem *item);
static void sp_item_select_this(GtkMenuItem *menuitem, SPItem *item);
static void sp_item_create_link(GtkMenuItem *menuitem, SPItem *item);

/* Generate context menu item section */

static void
sp_item_menu(SPObject *object, SPDesktop *desktop, GtkMenu *m)
{
    SPItem *item;
    GtkWidget *w;

    item = (SPItem *) object;

    /* Item dialog */
    w = gtk_menu_item_new_with_mnemonic(_("Object _Properties"));
    gtk_object_set_data(GTK_OBJECT(w), "desktop", desktop);
    gtk_signal_connect(GTK_OBJECT(w), "activate", GTK_SIGNAL_FUNC(sp_item_properties), item);
    gtk_widget_show(w);
    gtk_menu_append(GTK_MENU(m), w);
    /* Separator */
    w = gtk_menu_item_new();
    gtk_widget_show(w);
    gtk_menu_append(GTK_MENU(m), w);
    /* Select item */
    w = gtk_menu_item_new_with_mnemonic(_("_Select This"));
    if (SP_DT_SELECTION(desktop)->includes(item)) {
        gtk_widget_set_sensitive(w, FALSE);
    } else {
        gtk_object_set_data(GTK_OBJECT(w), "desktop", desktop);
        gtk_signal_connect(GTK_OBJECT(w), "activate", GTK_SIGNAL_FUNC(sp_item_select_this), item);
    }
    gtk_widget_show(w);
    gtk_menu_append(GTK_MENU(m), w);
    /* Create link */
    w = gtk_menu_item_new_with_mnemonic(_("_Create Link"));
    gtk_object_set_data(GTK_OBJECT(w), "desktop", desktop);
    gtk_signal_connect(GTK_OBJECT(w), "activate", GTK_SIGNAL_FUNC(sp_item_create_link), item);
    gtk_widget_set_sensitive(w, !SP_IS_ANCHOR(item));
    gtk_widget_show(w);
    gtk_menu_append(GTK_MENU(m), w);
}

static void
sp_item_properties(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)gtk_object_get_data(GTK_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    SP_DT_SELECTION(desktop)->set(item);

    sp_item_dialog();
}

static void
sp_item_select_this(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)gtk_object_get_data(GTK_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    SP_DT_SELECTION(desktop)->set(item);
}

static void
sp_item_create_link(GtkMenuItem *menuitem, SPItem *item)
{
    g_assert(SP_IS_ITEM(item));
    g_assert(!SP_IS_ANCHOR(item));

    SPDesktop *desktop = (SPDesktop*)gtk_object_get_data(GTK_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    Inkscape::XML::Node *repr = sp_repr_new("svg:a");
    sp_repr_add_child(SP_OBJECT_REPR(SP_OBJECT_PARENT(item)), repr, SP_OBJECT_REPR(item));
    SPObject *object = SP_OBJECT_DOCUMENT(item)->getObjectByRepr(repr);
    g_return_if_fail(SP_IS_ANCHOR(object));

    const char *id = SP_OBJECT_REPR(item)->attribute("id");
    Inkscape::XML::Node *child = SP_OBJECT_REPR(item)->duplicate();
    SP_OBJECT(item)->deleteObject(false);
    sp_repr_add_child(repr, child, NULL);
    sp_repr_set_attr(child, "id", id);
    sp_document_done(SP_OBJECT_DOCUMENT(object));

    sp_object_attributes_dialog(object, "SPAnchor");

    SP_DT_SELECTION(desktop)->set(SP_ITEM(object));
}

/* SPGroup */

static void sp_item_group_ungroup_activate(GtkMenuItem *menuitem, SPGroup *group);

static void
sp_group_menu(SPObject *object, SPDesktop *desktop, GtkMenu *menu)
{
    SPItem *item=SP_ITEM(object);
    GtkWidget *w;

    /* "Ungroup" */
    w = gtk_menu_item_new_with_mnemonic(_("_Ungroup"));
    gtk_object_set_data(GTK_OBJECT(w), "desktop", desktop);
    gtk_signal_connect(GTK_OBJECT(w), "activate", GTK_SIGNAL_FUNC(sp_item_group_ungroup_activate), item);
    gtk_widget_show(w);
    gtk_menu_append(GTK_MENU(menu), w);
}

static void
sp_item_group_ungroup_activate(GtkMenuItem *menuitem, SPGroup *group)
{
    SPDesktop *desktop;
    GSList *children;

    g_assert(SP_IS_GROUP(group));

    desktop = (SPDesktop*)gtk_object_get_data(GTK_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    children = NULL;
    sp_item_group_ungroup(group, &children);

    SP_DT_SELECTION(desktop)->setList(children);
    g_slist_free(children);
}

/* SPAnchor */

static void sp_anchor_link_properties(GtkMenuItem *menuitem, SPAnchor *anchor);
static void sp_anchor_link_follow(GtkMenuItem *menuitem, SPAnchor *anchor);
static void sp_anchor_link_remove(GtkMenuItem *menuitem, SPAnchor *anchor);

static void
sp_anchor_menu(SPObject *object, SPDesktop *desktop, GtkMenu *m)
{
    SPItem *item;
    GtkWidget *w;

    item = (SPItem *) object;

    /* Link dialog */
    w = gtk_menu_item_new_with_mnemonic(_("Link _Properties"));
    gtk_object_set_data(GTK_OBJECT(w), "desktop", desktop);
    gtk_signal_connect(GTK_OBJECT(w), "activate", GTK_SIGNAL_FUNC(sp_anchor_link_properties), item);
    gtk_widget_show(w);
    gtk_menu_append(GTK_MENU(m), w);
    /* Separator */
    w = gtk_menu_item_new();
    gtk_widget_show(w);
    gtk_menu_append(GTK_MENU(m), w);
    /* Select item */
    w = gtk_menu_item_new_with_mnemonic(_("_Follow Link"));
    gtk_signal_connect(GTK_OBJECT(w), "activate", GTK_SIGNAL_FUNC(sp_anchor_link_follow), item);
    gtk_widget_show(w);
    gtk_menu_append(GTK_MENU(m), w);
    /* Reset transformations */
    w = gtk_menu_item_new_with_mnemonic(_("_Remove Link"));
    gtk_object_set_data(GTK_OBJECT(w), "desktop", desktop);
    gtk_signal_connect(GTK_OBJECT(w), "activate", GTK_SIGNAL_FUNC(sp_anchor_link_remove), item);
    gtk_widget_show(w);
    gtk_menu_append(GTK_MENU(m), w);
}

static void
sp_anchor_link_properties(GtkMenuItem *menuitem, SPAnchor *anchor)
{
    sp_object_attributes_dialog(SP_OBJECT(anchor), "Link");
}

static void
sp_anchor_link_follow(GtkMenuItem *menuitem, SPAnchor *anchor)
{
    g_return_if_fail(anchor != NULL);
    g_return_if_fail(SP_IS_ANCHOR(anchor));

    /* shell out to an external browser here */
}

static void
sp_anchor_link_remove(GtkMenuItem *menuitem, SPAnchor *anchor)
{
    GSList *children;

    g_return_if_fail(anchor != NULL);
    g_return_if_fail(SP_IS_ANCHOR(anchor));

    children = NULL;
    sp_item_group_ungroup(SP_GROUP(anchor), &children);

    g_slist_free(children);
}

/* Image */

static void sp_image_image_properties(GtkMenuItem *menuitem, SPAnchor *anchor);

static void
sp_image_menu(SPObject *object, SPDesktop *desktop, GtkMenu *m)
{
    SPItem *item;
    GtkWidget *w;

    item = (SPItem *) object;

    /* Link dialog */
    w = gtk_menu_item_new_with_mnemonic(_("Image _Properties"));
    gtk_object_set_data(GTK_OBJECT(w), "desktop", desktop);
    gtk_signal_connect(GTK_OBJECT(w), "activate", GTK_SIGNAL_FUNC(sp_image_image_properties), item);
    gtk_widget_show(w);
    gtk_menu_append(GTK_MENU(m), w);
}

static void
sp_image_image_properties(GtkMenuItem *menuitem, SPAnchor *anchor)
{
    sp_object_attributes_dialog(SP_OBJECT(anchor), "Image");
}

/* SPShape */

static void
sp_shape_fill_settings(GtkMenuItem *menuitem, SPItem *item)
{
    SPDesktop *desktop;

    g_assert(SP_IS_ITEM(item));

    desktop = (SPDesktop*)gtk_object_get_data(GTK_OBJECT(menuitem), "desktop");
    g_return_if_fail(desktop != NULL);

    if (SP_DT_SELECTION(desktop)->isEmpty()) {
        SP_DT_SELECTION(desktop)->set(item);
    }

    sp_object_properties_dialog();
}

static void
sp_shape_menu(SPObject *object, SPDesktop *desktop, GtkMenu *m)
{
    SPItem *item;
    GtkWidget *w;

    item = (SPItem *) object;

    /* Item dialog */
    w = gtk_menu_item_new_with_mnemonic(_("_Fill and Stroke"));
    gtk_object_set_data(GTK_OBJECT(w), "desktop", desktop);
    gtk_signal_connect(GTK_OBJECT(w), "activate", GTK_SIGNAL_FUNC(sp_shape_fill_settings), item);
    gtk_widget_show(w);
    gtk_menu_append(GTK_MENU(m), w);
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
