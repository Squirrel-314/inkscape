#define __SP_GRADIENT_C__

/** \file
 * SPGradient, SPStop, SPLinearGradient, SPRadialGradient.
 */
/*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jasper van de Gronde <th.v.d.gronde@hccnet.nl>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2009 Jasper van de Gronde
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 *
 */

#define noSP_GRADIENT_VERBOSE

#include <cstring>
#include <string>

#include <libnr/nr-matrix-fns.h>
#include <libnr/nr-matrix-ops.h>
#include <libnr/nr-matrix-scale-ops.h>
#include <2geom/transforms.h>

#include <sigc++/functors/ptr_fun.h>
#include <sigc++/adaptors/bind.h>

#include "display/cairo-utils.h"
#include "libnr/nr-gradient.h"
#include "libnr/nr-pixops.h"
#include "svg/svg.h"
#include "svg/svg-color.h"
#include "svg/css-ostringstream.h"
#include "attributes.h"
#include "document-private.h"
#include "gradient-chemistry.h"
#include "sp-gradient-reference.h"
#include "sp-linear-gradient.h"
#include "sp-radial-gradient.h"
#include "sp-stop.h"
#include "streq.h"
#include "uri.h"
#include "xml/repr.h"

#define SP_MACROS_SILENT
#include "macros.h"

/// Has to be power of 2
#define NCOLORS NR_GRADIENT_VECTOR_LENGTH

static void sp_stop_class_init(SPStopClass *klass);
static void sp_stop_init(SPStop *stop);

static void sp_stop_build(SPObject *object, SPDocument *document, Inkscape::XML::Node *repr);
static void sp_stop_set(SPObject *object, unsigned key, gchar const *value);
static Inkscape::XML::Node *sp_stop_write(SPObject *object, Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, guint flags);

static SPObjectClass *stop_parent_class;

/**
 * Registers SPStop class and returns its type.
 */
GType
sp_stop_get_type()
{
    static GType type = 0;
    if (!type) {
        GTypeInfo info = {
            sizeof(SPStopClass),
            NULL, NULL,
            (GClassInitFunc) sp_stop_class_init,
            NULL, NULL,
            sizeof(SPStop),
            16,
            (GInstanceInitFunc) sp_stop_init,
            NULL,   /* value_table */
        };
        type = g_type_register_static(SP_TYPE_OBJECT, "SPStop", &info, (GTypeFlags)0);
    }
    return type;
}

/**
 * Callback to initialize SPStop vtable.
 */
static void sp_stop_class_init(SPStopClass *klass)
{
    SPObjectClass *sp_object_class = (SPObjectClass *) klass;

    stop_parent_class = (SPObjectClass *) g_type_class_ref(SP_TYPE_OBJECT);

    sp_object_class->build = sp_stop_build;
    sp_object_class->set = sp_stop_set;
    sp_object_class->write = sp_stop_write;
}

/**
 * Callback to initialize SPStop object.
 */
static void
sp_stop_init(SPStop *stop)
{
    stop->offset = 0.0;
    stop->currentColor = false;
    stop->specified_color.set( 0x000000ff );
    stop->opacity = 1.0;
}

/**
 * Virtual build: set stop attributes from its associated XML node.
 */
static void sp_stop_build(SPObject *object, SPDocument *document, Inkscape::XML::Node *repr)
{
    if (((SPObjectClass *) stop_parent_class)->build)
        (* ((SPObjectClass *) stop_parent_class)->build)(object, document, repr);

    sp_object_read_attr(object, "offset");
    sp_object_read_attr(object, "stop-color");
    sp_object_read_attr(object, "stop-opacity");
    sp_object_read_attr(object, "style");
}

/**
 * Virtual set: set attribute to value.
 */
static void
sp_stop_set(SPObject *object, unsigned key, gchar const *value)
{
    SPStop *stop = SP_STOP(object);

    switch (key) {
        case SP_ATTR_STYLE: {
        /** \todo
         * fixme: We are reading simple values 3 times during build (Lauris).
         * \par
         * We need presentation attributes etc.
         * \par
         * remove the hackish "style reading" from here: see comments in
         * sp_object_get_style_property about the bugs in our current
         * approach.  However, note that SPStyle doesn't currently have
         * stop-color and stop-opacity properties.
         */
            {
                gchar const *p = sp_object_get_style_property(object, "stop-color", "black");
                if (streq(p, "currentColor")) {
                    stop->currentColor = true;
                } else {
                    guint32 const color = sp_svg_read_color(p, 0);
                    stop->specified_color.set( color );
                }
            }
            {
                gchar const *p = sp_object_get_style_property(object, "stop-opacity", "1");
                gdouble opacity = sp_svg_read_percentage(p, stop->opacity);
                stop->opacity = opacity;
            }
            object->requestModified(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
            break;
        }
        case SP_PROP_STOP_COLOR: {
            {
                gchar const *p = sp_object_get_style_property(object, "stop-color", "black");
                if (streq(p, "currentColor")) {
                    stop->currentColor = true;
                } else {
                    stop->currentColor = false;
                    guint32 const color = sp_svg_read_color(p, 0);
                    stop->specified_color.set( color );
                }
            }
            object->requestModified(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
            break;
        }
        case SP_PROP_STOP_OPACITY: {
            {
                gchar const *p = sp_object_get_style_property(object, "stop-opacity", "1");
                gdouble opacity = sp_svg_read_percentage(p, stop->opacity);
                stop->opacity = opacity;
            }
            object->requestModified(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
            break;
        }
        case SP_ATTR_OFFSET: {
            stop->offset = sp_svg_read_percentage(value, 0.0);
            object->requestModified(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
            break;
        }
        default: {
            if (((SPObjectClass *) stop_parent_class)->set)
                (* ((SPObjectClass *) stop_parent_class)->set)(object, key, value);
            break;
        }
    }
}

/**
 * Virtual write: write object attributes to repr.
 */
static Inkscape::XML::Node *
sp_stop_write(SPObject *object, Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags)
{
    SPStop *stop = SP_STOP(object);

    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:stop");
    }

    guint32 specifiedcolor = stop->specified_color.toRGBA32( 255 );
    gfloat opacity = stop->opacity;

    if (((SPObjectClass *) stop_parent_class)->write)
        (* ((SPObjectClass *) stop_parent_class)->write)(object, xml_doc, repr, flags);

    // Since we do a hackish style setting here (because SPStyle does not support stop-color and
    // stop-opacity), we must do it AFTER calling the parent write method; otherwise
    // sp_object_write would clear our style= attribute (bug 1695287)

    Inkscape::CSSOStringStream os;
    os << "stop-color:";
    if (stop->currentColor) {
        os << "currentColor";
    } else {
        gchar c[64];
        sp_svg_write_color(c, sizeof(c), specifiedcolor);
        os << c;
    }
    os << ";stop-opacity:" << opacity;
    repr->setAttribute("style", os.str().c_str());
    repr->setAttribute("stop-color", NULL);
    repr->setAttribute("stop-opacity", NULL);
    sp_repr_set_css_double(repr, "offset", stop->offset);
    /* strictly speaking, offset an SVG <number> rather than a CSS one, but exponents make no sense
     * for offset proportions. */

    return repr;
}

/**
 * Return stop's color as 32bit value.
 */
guint32
sp_stop_get_rgba32(SPStop const *const stop)
{
    guint32 rgb0 = 0;
    /* Default value: arbitrarily black.  (SVG1.1 and CSS2 both say that the initial
     * value depends on user agent, and don't give any further restrictions that I can
     * see.) */
    if (stop->currentColor) {
        char const *str = sp_object_get_style_property(stop, "color", NULL);
        if (str) {
            rgb0 = sp_svg_read_color(str, rgb0);
        }
        unsigned const alpha = static_cast<unsigned>(stop->opacity * 0xff + 0.5);
        g_return_val_if_fail((alpha & ~0xff) == 0,
                             rgb0 | 0xff);
        return rgb0 | alpha;
    } else {
        return stop->specified_color.toRGBA32( stop->opacity );
    }
}

/**
 * Return stop's color as SPColor.
 */
static SPColor
sp_stop_get_color(SPStop const *const stop)
{
    if (stop->currentColor) {
        char const *str = sp_object_get_style_property(stop, "color", NULL);
        guint32 const dfl = 0;
        /* Default value: arbitrarily black.  (SVG1.1 and CSS2 both say that the initial
         * value depends on user agent, and don't give any further restrictions that I can
         * see.) */
        guint32 color = dfl;
        if (str) {
            color = sp_svg_read_color(str, dfl);
        }
        SPColor ret( color );
        return ret;
    } else {
        return stop->specified_color;
    }
}

/*
 * Gradient
 */

static void sp_gradient_class_init(SPGradientClass *klass);
static void sp_gradient_init(SPGradient *gr);

static void sp_gradient_build(SPObject *object, SPDocument *document, Inkscape::XML::Node *repr);
static void sp_gradient_release(SPObject *object);
static void sp_gradient_set(SPObject *object, unsigned key, gchar const *value);
static void sp_gradient_child_added(SPObject *object,
                                    Inkscape::XML::Node *child,
                                    Inkscape::XML::Node *ref);
static void sp_gradient_remove_child(SPObject *object, Inkscape::XML::Node *child);
static void sp_gradient_modified(SPObject *object, guint flags);
static Inkscape::XML::Node *sp_gradient_write(SPObject *object, Inkscape::XML::Document *doc, Inkscape::XML::Node *repr,
                                              guint flags);

static void gradient_ref_modified(SPObject *href, guint flags, SPGradient *gradient);

static bool sp_gradient_invalidate_vector(SPGradient *gr);
static void sp_gradient_rebuild_vector(SPGradient *gr);

static void gradient_ref_changed(SPObject *old_ref, SPObject *ref, SPGradient *gradient);

SPGradientSpread sp_gradient_get_spread(SPGradient *gradient);
SPGradientUnits sp_gradient_get_units(SPGradient *gradient);

static SPPaintServerClass *gradient_parent_class;

/**
 * Registers SPGradient class and returns its type.
 */
GType
sp_gradient_get_type()
{
    static GType gradient_type = 0;
    if (!gradient_type) {
        GTypeInfo gradient_info = {
            sizeof(SPGradientClass),
            NULL, NULL,
            (GClassInitFunc) sp_gradient_class_init,
            NULL, NULL,
            sizeof(SPGradient),
            16,
            (GInstanceInitFunc) sp_gradient_init,
            NULL,   /* value_table */
        };
        gradient_type = g_type_register_static(SP_TYPE_PAINT_SERVER, "SPGradient",
                                               &gradient_info, (GTypeFlags)0);
    }
    return gradient_type;
}

/**
 * SPGradient vtable initialization.
 */
static void
sp_gradient_class_init(SPGradientClass *klass)
{
    SPObjectClass *sp_object_class = (SPObjectClass *) klass;

    gradient_parent_class = (SPPaintServerClass *)g_type_class_ref(SP_TYPE_PAINT_SERVER);

    sp_object_class->build = sp_gradient_build;
    sp_object_class->release = sp_gradient_release;
    sp_object_class->set = sp_gradient_set;
    sp_object_class->child_added = sp_gradient_child_added;
    sp_object_class->remove_child = sp_gradient_remove_child;
    sp_object_class->modified = sp_gradient_modified;
    sp_object_class->write = sp_gradient_write;
}

/**
 * Callback for SPGradient object initialization.
 */
static void
sp_gradient_init(SPGradient *gr)
{
    gr->ref = new SPGradientReference(SP_OBJECT(gr));
    gr->ref->changedSignal().connect(sigc::bind(sigc::ptr_fun(gradient_ref_changed), gr));

    /** \todo
     * Fixme: reprs being rearranged (e.g. via the XML editor)
     * may require us to clear the state.
     */
    gr->state = SP_GRADIENT_STATE_UNKNOWN;

    gr->units = SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX;
    gr->units_set = FALSE;

    gr->gradientTransform = Geom::identity();
    gr->gradientTransform_set = FALSE;

    gr->spread = SP_GRADIENT_SPREAD_PAD;
    gr->spread_set = FALSE;

    gr->has_stops = FALSE;

    gr->vector.built = false;
    gr->vector.stops.clear();

    gr->color = NULL;

    new (&gr->modified_connection) sigc::connection();
}

/**
 * Virtual build: set gradient attributes from its associated repr.
 */
static void
sp_gradient_build(SPObject *object, SPDocument *document, Inkscape::XML::Node *repr)
{
    SPGradient *gradient = SP_GRADIENT(object);

    if (((SPObjectClass *) gradient_parent_class)->build)
        (* ((SPObjectClass *) gradient_parent_class)->build)(object, document, repr);

    SPObject *ochild;
    for ( ochild = sp_object_first_child(object) ; ochild ; ochild = SP_OBJECT_NEXT(ochild) ) {
        if (SP_IS_STOP(ochild)) {
            gradient->has_stops = TRUE;
            break;
        }
    }

    sp_object_read_attr(object, "gradientUnits");
    sp_object_read_attr(object, "gradientTransform");
    sp_object_read_attr(object, "spreadMethod");
    sp_object_read_attr(object, "xlink:href");

    /* Register ourselves */
    sp_document_add_resource(document, "gradient", object);
}

/**
 * Virtual release of SPGradient members before destruction.
 */
static void
sp_gradient_release(SPObject *object)
{
    SPGradient *gradient = (SPGradient *) object;

#ifdef SP_GRADIENT_VERBOSE
    g_print("Releasing gradient %s\n", SP_OBJECT_ID(object));
#endif

    if (SP_OBJECT_DOCUMENT(object)) {
        /* Unregister ourselves */
        sp_document_remove_resource(SP_OBJECT_DOCUMENT(object), "gradient", SP_OBJECT(object));
    }

    if (gradient->ref) {
        gradient->modified_connection.disconnect();
        gradient->ref->detach();
        delete gradient->ref;
        gradient->ref = NULL;
    }

    if (gradient->color) {
        g_free(gradient->color);
        gradient->color = NULL;
    }

    gradient->modified_connection.~connection();

    if (((SPObjectClass *) gradient_parent_class)->release)
        ((SPObjectClass *) gradient_parent_class)->release(object);
}

/**
 * Set gradient attribute to value.
 */
static void
sp_gradient_set(SPObject *object, unsigned key, gchar const *value)
{
    SPGradient *gr = SP_GRADIENT(object);

    switch (key) {
        case SP_ATTR_GRADIENTUNITS:
            if (value) {
                if (!strcmp(value, "userSpaceOnUse")) {
                    gr->units = SP_GRADIENT_UNITS_USERSPACEONUSE;
                } else {
                    gr->units = SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX;
                }
                gr->units_set = TRUE;
            } else {
                gr->units = SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX;
                gr->units_set = FALSE;
            }
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_GRADIENTTRANSFORM: {
            Geom::Matrix t;
            if (value && sp_svg_transform_read(value, &t)) {
                gr->gradientTransform = t;
                gr->gradientTransform_set = TRUE;
            } else {
                gr->gradientTransform = Geom::identity();
                gr->gradientTransform_set = FALSE;
            }
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        }
        case SP_ATTR_SPREADMETHOD:
            if (value) {
                if (!strcmp(value, "reflect")) {
                    gr->spread = SP_GRADIENT_SPREAD_REFLECT;
                } else if (!strcmp(value, "repeat")) {
                    gr->spread = SP_GRADIENT_SPREAD_REPEAT;
                } else {
                    gr->spread = SP_GRADIENT_SPREAD_PAD;
                }
                gr->spread_set = TRUE;
            } else {
                gr->spread_set = FALSE;
            }
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_XLINK_HREF:
            if (value) {
                try {
                    gr->ref->attach(Inkscape::URI(value));
                } catch (Inkscape::BadURIException &e) {
                    g_warning("%s", e.what());
                    gr->ref->detach();
                }
            } else {
                gr->ref->detach();
            }
            break;
        default:
            if (((SPObjectClass *) gradient_parent_class)->set)
                ((SPObjectClass *) gradient_parent_class)->set(object, key, value);
            break;
    }
}

/**
 * Gets called when the gradient is (re)attached to another gradient.
 */
static void
gradient_ref_changed(SPObject *old_ref, SPObject *ref, SPGradient *gr)
{
    if (old_ref) {
        gr->modified_connection.disconnect();
    }
    if ( SP_IS_GRADIENT(ref)
         && ref != gr )
    {
        gr->modified_connection = ref->connectModified(sigc::bind<2>(sigc::ptr_fun(&gradient_ref_modified), gr));
    }

    // Per SVG, all unset attributes must be inherited from linked gradient.
    // So, as we're now (re)linked, we assign linkee's values to this gradient if they are not yet set -
    // but without setting the _set flags.
    // FIXME: do the same for gradientTransform too
    if (!gr->units_set)
        gr->units = sp_gradient_get_units (gr);
    if (!gr->spread_set)
        gr->spread = sp_gradient_get_spread (gr);

    /// \todo Fixme: what should the flags (second) argument be? */
    gradient_ref_modified(ref, 0, gr);
}

/**
 * Callback for child_added event.
 */
static void
sp_gradient_child_added(SPObject *object, Inkscape::XML::Node *child, Inkscape::XML::Node *ref)
{
    SPGradient *gr = SP_GRADIENT(object);

    sp_gradient_invalidate_vector(gr);

    if (((SPObjectClass *) gradient_parent_class)->child_added)
        (* ((SPObjectClass *) gradient_parent_class)->child_added)(object, child, ref);

    SPObject *ochild = sp_object_get_child_by_repr(object, child);
    if ( ochild && SP_IS_STOP(ochild) ) {
        gr->has_stops = TRUE;
    }

    /// \todo Fixme: should we schedule "modified" here?
    object->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/**
 * Callback for remove_child event.
 */
static void
sp_gradient_remove_child(SPObject *object, Inkscape::XML::Node *child)
{
    SPGradient *gr = SP_GRADIENT(object);

    sp_gradient_invalidate_vector(gr);

    if (((SPObjectClass *) gradient_parent_class)->remove_child)
        (* ((SPObjectClass *) gradient_parent_class)->remove_child)(object, child);

    gr->has_stops = FALSE;
    SPObject *ochild;
    for ( ochild = sp_object_first_child(object) ; ochild ; ochild = SP_OBJECT_NEXT(ochild) ) {
        if (SP_IS_STOP(ochild)) {
            gr->has_stops = TRUE;
            break;
        }
    }

    /* Fixme: should we schedule "modified" here? */
    object->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/**
 * Callback for modified event.
 */
static void
sp_gradient_modified(SPObject *object, guint flags)
{
    SPGradient *gr = SP_GRADIENT(object);

    if (flags & SP_OBJECT_CHILD_MODIFIED_FLAG) {
        sp_gradient_invalidate_vector(gr);
    }

    if (flags & SP_OBJECT_STYLE_MODIFIED_FLAG) {
        sp_gradient_ensure_colors(gr);
    }

    if (flags & SP_OBJECT_MODIFIED_FLAG) flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    flags &= SP_OBJECT_MODIFIED_CASCADE;

    // FIXME: climb up the ladder of hrefs
    GSList *l = NULL;
    for (SPObject *child = sp_object_first_child(object) ; child != NULL; child = SP_OBJECT_NEXT(child) ) {
        g_object_ref(G_OBJECT(child));
        l = g_slist_prepend(l, child);
    }
    l = g_slist_reverse(l);
    while (l) {
        SPObject *child = SP_OBJECT(l->data);
        l = g_slist_remove(l, child);
        if (flags || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            child->emitModified(flags);
        }
        g_object_unref(G_OBJECT(child));
    }
}

SPStop* SPGradient::getFirstStop()
{
    SPStop* first = 0;
    for (SPObject *ochild = sp_object_first_child(this); ochild && !first; ochild = SP_OBJECT_NEXT(ochild)) {
        if (SP_IS_STOP(ochild)) {
            first = SP_STOP(ochild);
        }
    }
    return first;
}

int SPGradient::getStopCount() const
{
    int count = 0;

    for (SPStop *stop = const_cast<SPGradient*>(this)->getFirstStop(); stop && stop->getNextStop(); stop = stop->getNextStop()) {
        count++;
    }

    return count;
}

/**
 * Write gradient attributes to repr.
 */
static Inkscape::XML::Node *
sp_gradient_write(SPObject *object, Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags)
{
    SPGradient *gr = SP_GRADIENT(object);

    if (((SPObjectClass *) gradient_parent_class)->write)
        (* ((SPObjectClass *) gradient_parent_class)->write)(object, xml_doc, repr, flags);

    if (flags & SP_OBJECT_WRITE_BUILD) {
        GSList *l = NULL;
        for (SPObject *child = sp_object_first_child(object); child; child = SP_OBJECT_NEXT(child)) {
            Inkscape::XML::Node *crepr;
            crepr = child->updateRepr(xml_doc, NULL, flags);
            if (crepr) l = g_slist_prepend(l, crepr);
        }
        while (l) {
            repr->addChild((Inkscape::XML::Node *) l->data, NULL);
            Inkscape::GC::release((Inkscape::XML::Node *) l->data);
            l = g_slist_remove(l, l->data);
        }
    }

    if (gr->ref->getURI()) {
        gchar *uri_string = gr->ref->getURI()->toString();
        repr->setAttribute("xlink:href", uri_string);
        g_free(uri_string);
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || gr->units_set) {
        switch (gr->units) {
            case SP_GRADIENT_UNITS_USERSPACEONUSE:
                repr->setAttribute("gradientUnits", "userSpaceOnUse");
                break;
            default:
                repr->setAttribute("gradientUnits", "objectBoundingBox");
                break;
        }
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || gr->gradientTransform_set) {
        gchar *c=sp_svg_transform_write(gr->gradientTransform);
        repr->setAttribute("gradientTransform", c);
        g_free(c);
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || gr->spread_set) {
        /* FIXME: Ensure that gr->spread is the inherited value
         * if !gr->spread_set.  Not currently happening: see sp_gradient_modified.
         */
        switch (gr->spread) {
            case SP_GRADIENT_SPREAD_REFLECT:
                repr->setAttribute("spreadMethod", "reflect");
                break;
            case SP_GRADIENT_SPREAD_REPEAT:
                repr->setAttribute("spreadMethod", "repeat");
                break;
            default:
                repr->setAttribute("spreadMethod", "pad");
                break;
        }
    }

    return repr;
}

/**
 * Forces the vector to be built, if not present (i.e., changed).
 *
 * \pre SP_IS_GRADIENT(gradient).
 */
void
sp_gradient_ensure_vector(SPGradient *gradient)
{
    g_return_if_fail(gradient != NULL);
    g_return_if_fail(SP_IS_GRADIENT(gradient));

    if (!gradient->vector.built) {
        sp_gradient_rebuild_vector(gradient);
    }
}

/**
 * Set units property of gradient and emit modified.
 */
void
sp_gradient_set_units(SPGradient *gr, SPGradientUnits units)
{
    if (units != gr->units) {
        gr->units = units;
        gr->units_set = TRUE;
        SP_OBJECT(gr)->requestModified(SP_OBJECT_MODIFIED_FLAG);
    }
}

/**
 * Set spread property of gradient and emit modified.
 */
void
sp_gradient_set_spread(SPGradient *gr, SPGradientSpread spread)
{
    if (spread != gr->spread) {
        gr->spread = spread;
        gr->spread_set = TRUE;
        SP_OBJECT(gr)->requestModified(SP_OBJECT_MODIFIED_FLAG);
    }
}

/**
 * Returns the first of {src, src-\>ref-\>getObject(),
 * src-\>ref-\>getObject()-\>ref-\>getObject(),...}
 * for which \a match is true, or NULL if none found.
 *
 * The raison d'être of this routine is that it correctly handles cycles in the href chain (e.g., if
 * a gradient gives itself as its href, or if each of two gradients gives the other as its href).
 *
 * \pre SP_IS_GRADIENT(src).
 */
static SPGradient *
chase_hrefs(SPGradient *const src, bool (*match)(SPGradient const *))
{
    g_return_val_if_fail(SP_IS_GRADIENT(src), NULL);

    /* Use a pair of pointers for detecting loops: p1 advances half as fast as p2.  If there is a
       loop, then once p1 has entered the loop, we'll detect it the next time the distance between
       p1 and p2 is a multiple of the loop size. */
    SPGradient *p1 = src, *p2 = src;
    bool do1 = false;
    for (;;) {
        if (match(p2)) {
            return p2;
        }

        p2 = p2->ref->getObject();
        if (!p2) {
            return p2;
        }
        if (do1) {
            p1 = p1->ref->getObject();
        }
        do1 = !do1;

        if ( p2 == p1 ) {
            /* We've been here before, so return NULL to indicate that no matching gradient found
             * in the chain. */
            return NULL;
        }
    }
}

/**
 * True if gradient has stops.
 */
static bool has_stopsFN(SPGradient const *gr)
{
    return SP_GRADIENT_HAS_STOPS(gr);
}

/**
 * True if gradient has spread set.
 */
static bool
has_spread_set(SPGradient const *gr)
{
    return gr->spread_set;
}

/**
 * True if gradient has units set.
 */
static bool
has_units_set(SPGradient const *gr)
{
    return gr->units_set;
}


SPGradient *SPGradient::getVector(bool force_vector)
{
    SPGradient * src = chase_hrefs(this, has_stopsFN);

    if (force_vector) {
        src = sp_gradient_ensure_vector_normalized(src);
    }
    return src;
}

/**
 * Returns the effective spread of given gradient (climbing up the refs chain if needed).
 *
 * \pre SP_IS_GRADIENT(gradient).
 */
SPGradientSpread
sp_gradient_get_spread(SPGradient *gradient)
{
    g_return_val_if_fail(SP_IS_GRADIENT(gradient), SP_GRADIENT_SPREAD_PAD);

    SPGradient const *src = chase_hrefs(gradient, has_spread_set);
    return ( src
             ? src->spread
             : SP_GRADIENT_SPREAD_PAD ); // pad is the default
}

/**
 * Returns the effective units of given gradient (climbing up the refs chain if needed).
 *
 * \pre SP_IS_GRADIENT(gradient).
 */
SPGradientUnits
sp_gradient_get_units(SPGradient *gradient)
{
    g_return_val_if_fail(SP_IS_GRADIENT(gradient), SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX);

    SPGradient const *src = chase_hrefs(gradient, has_units_set);
    return ( src
             ? src->units
             : SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX ); // bbox is the default
}


/**
 * Clears the gradient's svg:stop children from its repr.
 */
void
sp_gradient_repr_clear_vector(SPGradient *gr)
{
    Inkscape::XML::Node *repr = SP_OBJECT_REPR(gr);

    /* Collect stops from original repr */
    GSList *sl = NULL;
    for (Inkscape::XML::Node *child = repr->firstChild() ; child != NULL; child = child->next() ) {
        if (!strcmp(child->name(), "svg:stop")) {
            sl = g_slist_prepend(sl, child);
        }
    }
    /* Remove all stops */
    while (sl) {
        /** \todo
         * fixme: This should work, unless we make gradient
         * into generic group.
         */
        sp_repr_unparent((Inkscape::XML::Node *)sl->data);
        sl = g_slist_remove(sl, sl->data);
    }
}

/**
 * Writes the gradient's internal vector (whether from its own stops, or
 * inherited from refs) into the gradient repr as svg:stop elements.
 */
void
sp_gradient_repr_write_vector(SPGradient *gr)
{
    g_return_if_fail(gr != NULL);
    g_return_if_fail(SP_IS_GRADIENT(gr));

    Inkscape::XML::Document *xml_doc = sp_document_repr_doc(SP_OBJECT_DOCUMENT(gr));
    Inkscape::XML::Node *repr = SP_OBJECT_REPR(gr);

    /* We have to be careful, as vector may be our own, so construct repr list at first */
    GSList *cl = NULL;

    for (guint i = 0; i < gr->vector.stops.size(); i++) {
        Inkscape::CSSOStringStream os;
        Inkscape::XML::Node *child = xml_doc->createElement("svg:stop");
        sp_repr_set_css_double(child, "offset", gr->vector.stops[i].offset);
        /* strictly speaking, offset an SVG <number> rather than a CSS one, but exponents make no
         * sense for offset proportions. */
        gchar c[64];
        sp_svg_write_color(c, sizeof(c), gr->vector.stops[i].color.toRGBA32( 0x00 ));
        os << "stop-color:" << c << ";stop-opacity:" << gr->vector.stops[i].opacity;
        child->setAttribute("style", os.str().c_str());
        /* Order will be reversed here */
        cl = g_slist_prepend(cl, child);
    }

    sp_gradient_repr_clear_vector(gr);

    /* And insert new children from list */
    while (cl) {
        Inkscape::XML::Node *child = static_cast<Inkscape::XML::Node *>(cl->data);
        repr->addChild(child, NULL);
        Inkscape::GC::release(child);
        cl = g_slist_remove(cl, child);
    }
}


static void
gradient_ref_modified(SPObject */*href*/, guint /*flags*/, SPGradient *gradient)
{
    if (sp_gradient_invalidate_vector(gradient)) {
        SP_OBJECT(gradient)->requestModified(SP_OBJECT_MODIFIED_FLAG);
        /* Conditional to avoid causing infinite loop if there's a cycle in the href chain. */
    }
}

/** Return true iff change made. */
static bool
sp_gradient_invalidate_vector(SPGradient *gr)
{
    bool ret = false;

    if (gr->color != NULL) {
        g_free(gr->color);
        gr->color = NULL;
        ret = true;
    }

    if (gr->vector.built) {
        gr->vector.built = false;
        gr->vector.stops.clear();
        ret = true;
    }

    return ret;
}

/** Creates normalized color vector */
static void
sp_gradient_rebuild_vector(SPGradient *gr)
{
    gint len = 0;
    for ( SPObject *child = sp_object_first_child(SP_OBJECT(gr)) ;
          child != NULL ;
          child = SP_OBJECT_NEXT(child) ) {
        if (SP_IS_STOP(child)) {
            len ++;
        }
    }

    gr->has_stops = (len != 0);

    gr->vector.stops.clear();

    SPGradient *ref = gr->ref->getObject();
    if ( !gr->has_stops && ref ) {
        /* Copy vector from referenced gradient */
        gr->vector.built = true;   // Prevent infinite recursion.
        sp_gradient_ensure_vector(ref);
        if (!ref->vector.stops.empty()) {
            gr->vector.built = ref->vector.built;
            gr->vector.stops.assign(ref->vector.stops.begin(), ref->vector.stops.end());
            return;
        }
    }

    for (SPObject *child = sp_object_first_child(SP_OBJECT(gr)) ;
         child != NULL;
         child = SP_OBJECT_NEXT(child) ) {
        if (SP_IS_STOP(child)) {
            SPStop *stop = SP_STOP(child);

            SPGradientStop gstop;
            if (gr->vector.stops.size() > 0) {
                // "Each gradient offset value is required to be equal to or greater than the
                // previous gradient stop's offset value. If a given gradient stop's offset
                // value is not equal to or greater than all previous offset values, then the
                // offset value is adjusted to be equal to the largest of all previous offset
                // values."
                gstop.offset = MAX(stop->offset, gr->vector.stops.back().offset);
            } else {
                gstop.offset = stop->offset;
            }

            // "Gradient offset values less than 0 (or less than 0%) are rounded up to
            // 0%. Gradient offset values greater than 1 (or greater than 100%) are rounded
            // down to 100%."
            gstop.offset = CLAMP(gstop.offset, 0, 1);

            gstop.color = sp_stop_get_color(stop);
            gstop.opacity = stop->opacity;

            gr->vector.stops.push_back(gstop);
        }
    }

    // Normalize per section 13.2.4 of SVG 1.1.
    if (gr->vector.stops.size() == 0) {
        /* "If no stops are defined, then painting shall occur as if 'none' were specified as the
         * paint style."
         */
        {
            SPGradientStop gstop;
            gstop.offset = 0.0;
            gstop.color.set( 0x00000000 );
            gstop.opacity = 0.0;
            gr->vector.stops.push_back(gstop);
        }
        {
            SPGradientStop gstop;
            gstop.offset = 1.0;
            gstop.color.set( 0x00000000 );
            gstop.opacity = 0.0;
            gr->vector.stops.push_back(gstop);
        }
    } else {
        /* "If one stop is defined, then paint with the solid color fill using the color defined
         * for that gradient stop."
         */
        if (gr->vector.stops.front().offset > 0.0) {
            // If the first one is not at 0, then insert a copy of the first at 0.
            SPGradientStop gstop;
            gstop.offset = 0.0;
            gstop.color = gr->vector.stops.front().color;
            gstop.opacity = gr->vector.stops.front().opacity;
            gr->vector.stops.insert(gr->vector.stops.begin(), gstop);
        }
        if (gr->vector.stops.back().offset < 1.0) {
            // If the last one is not at 1, then insert a copy of the last at 1.
            SPGradientStop gstop;
            gstop.offset = 1.0;
            gstop.color = gr->vector.stops.back().color;
            gstop.opacity = gr->vector.stops.back().opacity;
            gr->vector.stops.push_back(gstop);
        }
    }

    gr->vector.built = true;
}

/**
 * The gradient's color array is newly created and set up from vector.
 */
void
sp_gradient_ensure_colors(SPGradient *gr)
{
    if (!gr->vector.built) {
        sp_gradient_rebuild_vector(gr);
    }
    g_return_if_fail(!gr->vector.stops.empty());

    /// \todo Where is the memory freed?
    if (!gr->color) {
        gr->color = g_new(guchar, 4 * NCOLORS);
    }

    // This assumes that gr->vector is a zero-order B-spline (box function) approximation of the "true" gradient.
    // This means that the "true" gradient must be prefiltered using a zero order B-spline and then sampled.
    // Furthermore, the first element corresponds to offset="0" and the last element to offset="1".

    double remainder[4] = {0,0,0,0};
    double remainder_for_end[4] = {0,0,0,0}; // Used at the end
    switch(gr->spread) {
    case SP_GRADIENT_SPREAD_PAD:
        remainder[0] = 0.5*gr->vector.stops[0].color.v.c[0]; // Half of the first cell uses the color of the first stop
        remainder[1] = 0.5*gr->vector.stops[0].color.v.c[1];
        remainder[2] = 0.5*gr->vector.stops[0].color.v.c[2];
        remainder[3] = 0.5*gr->vector.stops[0].opacity;
        remainder_for_end[0] = 0.5*gr->vector.stops[gr->vector.stops.size() - 1].color.v.c[0]; // Half of the first cell uses the color of the last stop
        remainder_for_end[1] = 0.5*gr->vector.stops[gr->vector.stops.size() - 1].color.v.c[1];
        remainder_for_end[2] = 0.5*gr->vector.stops[gr->vector.stops.size() - 1].color.v.c[2];
        remainder_for_end[3] = 0.5*gr->vector.stops[gr->vector.stops.size() - 1].opacity;
        break;
    case SP_GRADIENT_SPREAD_REFLECT:
    case SP_GRADIENT_SPREAD_REPEAT:
        // These two are handled differently, see below.
        break;
    default:
        g_error("Spread type not supported!");
    };
    for (unsigned int i = 0; i < gr->vector.stops.size() - 1; i++) {
        double r0 = gr->vector.stops[i].color.v.c[0];
        double g0 = gr->vector.stops[i].color.v.c[1];
        double b0 = gr->vector.stops[i].color.v.c[2];
        double a0 = gr->vector.stops[i].opacity;
        double r1 = gr->vector.stops[i+1].color.v.c[0];
        double g1 = gr->vector.stops[i+1].color.v.c[1];
        double b1 = gr->vector.stops[i+1].color.v.c[2];
        double a1 = gr->vector.stops[i+1].opacity;
        double o0 = gr->vector.stops[i].offset * (NCOLORS-1);
        double o1 = gr->vector.stops[i + 1].offset * (NCOLORS-1);
        unsigned int ob = (unsigned int) floor(o0+.5); // These are the first and last element that might be affected by this interval.
        unsigned int oe = (unsigned int) floor(o1+.5); // These need to be computed the same to ensure that ob will be covered by the next interval if oe==ob

        if (oe == ob) {
            // Simple case, this interval starts and stops within one cell
            // The contribution of this interval is:
            //    (o1-o0)*(c(o0)+c(o1))/2
            //  = (o1-o0)*(c0+c1)/2
            double dt = 0.5*(o1-o0);
            remainder[0] += dt*(r0 + r1);
            remainder[1] += dt*(g0 + g1);
            remainder[2] += dt*(b0 + b1);
            remainder[3] += dt*(a0 + a1);
        } else {
            // First compute colors for the cells which are fully covered by the current interval.
            // The prefiltered values are equal to the midpoint of each cell here.
            //  f = (j-o0)/(o1-o0)
            //    = j*(1/(o1-o0)) - o0/(o1-o0)
            double f = (ob-o0) / (o1-o0);
            double df = 1. / (o1-o0);
            for (unsigned int j = ob+1; j < oe; j++) {
                f += df;
                gr->color[4 * j + 0] = (unsigned char) floor(255*(r0 + f*(r1-r0)) + .5);
                gr->color[4 * j + 1] = (unsigned char) floor(255*(g0 + f*(g1-g0)) + .5);
                gr->color[4 * j + 2] = (unsigned char) floor(255*(b0 + f*(b1-b0)) + .5);
                gr->color[4 * j + 3] = (unsigned char) floor(255*(a0 + f*(a1-a0)) + .5);
            }

            // Now handle the beginning
            // The contribution of the last point is already in remainder.
            // The contribution of this point is:
            //    (ob+.5-o0)*(c(o0)+c(ob+.5))/2
            //  = (ob+.5-o0)*c((o0+ob+.5)/2)
            //  = (ob+.5-o0)*(c0+((o0+ob+.5)/2-o0)*df*(c1-c0))
            //  = (ob+.5-o0)*(c0+(ob+.5-o0)*df*(c1-c0)/2)
            double dt = ob+.5-o0;
            f = 0.5*dt*df;
            if (ob==0 && gr->spread==SP_GRADIENT_SPREAD_REFLECT) {
                // The first half of the first cell is just a mirror image of the second half, so simply multiply it by 2.
                gr->color[4 * ob + 0] = (unsigned char) floor(2*255*(remainder[0] + dt*(r0 + f*(r1-r0))) + .5);
                gr->color[4 * ob + 1] = (unsigned char) floor(2*255*(remainder[1] + dt*(g0 + f*(g1-g0))) + .5);
                gr->color[4 * ob + 2] = (unsigned char) floor(2*255*(remainder[2] + dt*(b0 + f*(b1-b0))) + .5);
                gr->color[4 * ob + 3] = (unsigned char) floor(2*255*(remainder[3] + dt*(a0 + f*(a1-a0))) + .5);
            } else if (ob==0 && gr->spread==SP_GRADIENT_SPREAD_REPEAT) {
                // The first cell is the same as the last cell, so save whatever is in the second half here and deal with the rest later.
                remainder_for_end[0] = remainder[0] + dt*(r0 + f*(r1-r0));
                remainder_for_end[1] = remainder[1] + dt*(g0 + f*(g1-g0));
                remainder_for_end[2] = remainder[2] + dt*(b0 + f*(b1-b0));
                remainder_for_end[3] = remainder[3] + dt*(a0 + f*(a1-a0));
            } else {
                // The first half of the cell was already in remainder.
                gr->color[4 * ob + 0] = (unsigned char) floor(255*(remainder[0] + dt*(r0 + f*(r1-r0))) + .5);
                gr->color[4 * ob + 1] = (unsigned char) floor(255*(remainder[1] + dt*(g0 + f*(g1-g0))) + .5);
                gr->color[4 * ob + 2] = (unsigned char) floor(255*(remainder[2] + dt*(b0 + f*(b1-b0))) + .5);
                gr->color[4 * ob + 3] = (unsigned char) floor(255*(remainder[3] + dt*(a0 + f*(a1-a0))) + .5);
            }

            // Now handle the end, which should end up in remainder
            // The contribution of this point is:
            //    (o1-oe+.5)*(c(o1)+c(oe-.5))/2
            //  = (o1-oe+.5)*c((o1+oe-.5)/2)
            //  = (o1-oe+.5)*(c0+((o1+oe-.5)/2-o0)*df*(c1-c0))
            dt = o1-oe+.5;
            f = (0.5*(o1+oe-.5)-o0)*df;
            remainder[0] = dt*(r0 + f*(r1-r0));
            remainder[1] = dt*(g0 + f*(g1-g0));
            remainder[2] = dt*(b0 + f*(b1-b0));
            remainder[3] = dt*(a0 + f*(a1-a0));
        }
    }
    switch(gr->spread) {
    case SP_GRADIENT_SPREAD_PAD:
        gr->color[4 * (NCOLORS-1) + 0] = (unsigned char) floor(255*(remainder[0]+remainder_for_end[0]) + .5);
        gr->color[4 * (NCOLORS-1) + 1] = (unsigned char) floor(255*(remainder[1]+remainder_for_end[1]) + .5);
        gr->color[4 * (NCOLORS-1) + 2] = (unsigned char) floor(255*(remainder[2]+remainder_for_end[2]) + .5);
        gr->color[4 * (NCOLORS-1) + 3] = (unsigned char) floor(255*(remainder[3]+remainder_for_end[3]) + .5);
        break;
    case SP_GRADIENT_SPREAD_REFLECT:
        // The second half is the same as the first half, so multiply by 2.
        gr->color[4 * (NCOLORS-1) + 0] = (unsigned char) floor(2*255*remainder[0] + .5);
        gr->color[4 * (NCOLORS-1) + 1] = (unsigned char) floor(2*255*remainder[1] + .5);
        gr->color[4 * (NCOLORS-1) + 2] = (unsigned char) floor(2*255*remainder[2] + .5);
        gr->color[4 * (NCOLORS-1) + 3] = (unsigned char) floor(2*255*remainder[3] + .5);
        break;
    case SP_GRADIENT_SPREAD_REPEAT:
        // The second half is the same as the second half of the first cell (which was saved in remainder_for_end).
        gr->color[0] = gr->color[4 * (NCOLORS-1) + 0] = (unsigned char) floor(255*(remainder[0]+remainder_for_end[0]) + .5);
        gr->color[1] = gr->color[4 * (NCOLORS-1) + 1] = (unsigned char) floor(255*(remainder[1]+remainder_for_end[1]) + .5);
        gr->color[2] = gr->color[4 * (NCOLORS-1) + 2] = (unsigned char) floor(255*(remainder[2]+remainder_for_end[2]) + .5);
        gr->color[3] = gr->color[4 * (NCOLORS-1) + 3] = (unsigned char) floor(255*(remainder[3]+remainder_for_end[3]) + .5);
        break;
    }
}

/**
 * Renders gradient vector to buffer as line.
 *
 * RGB buffer background should be set up beforehand.
 *
 * @param len,width,height,rowstride Buffer parameters (1 or 2 dimensional).
 * @param span Full integer width of requested gradient.
 * @param pos Buffer starting position in span.
 */
static void
sp_gradient_render_vector_line_rgba(SPGradient *const gradient, guchar *buf,
                                    gint const len, gint const pos, gint const span)
{
    g_return_if_fail(gradient != NULL);
    g_return_if_fail(SP_IS_GRADIENT(gradient));
    g_return_if_fail(buf != NULL);
    g_return_if_fail(len > 0);
    g_return_if_fail(pos >= 0);
    g_return_if_fail(pos + len <= span);
    g_return_if_fail(span > 0);

    if (!gradient->color) {
        sp_gradient_ensure_colors(gradient);
    }

    gint idx = (pos * 1024 << 8) / span;
    gint didx = (1024 << 8) / span;

    for (gint x = 0; x < len; x++) {
        /// \todo Can this be done with 4 byte copies?
        *buf++ = gradient->color[4 * (idx >> 8)];
        *buf++ = gradient->color[4 * (idx >> 8) + 1];
        *buf++ = gradient->color[4 * (idx >> 8) + 2];
        *buf++ = gradient->color[4 * (idx >> 8) + 3];
        idx += didx;
    }
}

/**
 * Render rectangular RGBA area from gradient vector.
 */
void
sp_gradient_render_vector_block_rgba(SPGradient *const gradient, guchar *buf,
                                     gint const width, gint const height, gint const rowstride,
                                     gint const pos, gint const span, bool const horizontal)
{
    g_return_if_fail(gradient != NULL);
    g_return_if_fail(SP_IS_GRADIENT(gradient));
    g_return_if_fail(buf != NULL);
    g_return_if_fail(width > 0);
    g_return_if_fail(height > 0);
    g_return_if_fail(pos >= 0);
    g_return_if_fail((horizontal && (pos + width <= span)) || (!horizontal && (pos + height <= span)));
    g_return_if_fail(span > 0);

    if (horizontal) {
        sp_gradient_render_vector_line_rgba(gradient, buf, width, pos, span);
        for (gint y = 1; y < height; y++) {
            memcpy(buf + y * rowstride, buf, 4 * width);
        }
    } else {
        guchar *tmp = (guchar *)alloca(4 * height);
        sp_gradient_render_vector_line_rgba(gradient, tmp, height, pos, span);
        for (gint y = 0; y < height; y++) {
            guchar *b = buf + y * rowstride;
            for (gint x = 0; x < width; x++) {
                *b++ = tmp[0];
                *b++ = tmp[1];
                *b++ = tmp[2];
                *b++ = tmp[3];
            }
            tmp += 4;
        }
    }
}

/**
 * Render rectangular RGB area from gradient vector.
 */
void
sp_gradient_render_vector_block_rgb(SPGradient *gradient, guchar *buf,
                                    gint const width, gint const height, gint const /*rowstride*/,
                                    gint const pos, gint const span, bool const horizontal)
{
    g_return_if_fail(gradient != NULL);
    g_return_if_fail(SP_IS_GRADIENT(gradient));
    g_return_if_fail(buf != NULL);
    g_return_if_fail(width > 0);
    g_return_if_fail(height > 0);
    g_return_if_fail(pos >= 0);
    g_return_if_fail((horizontal && (pos + width <= span)) || (!horizontal && (pos + height <= span)));
    g_return_if_fail(span > 0);

    if (horizontal) {
        guchar *tmp = (guchar*)alloca(4 * width);
        sp_gradient_render_vector_line_rgba(gradient, tmp, width, pos, span);
        for (gint y = 0; y < height; y++) {
            guchar *t = tmp;
            for (gint x = 0; x < width; x++) {
                gint a = t[3];
                gint fc = (t[0] - buf[0]) * a;
                buf[0] = buf[0] + ((fc + (fc >> 8) + 0x80) >> 8);
                fc = (t[1] - buf[1]) * a;
                buf[1] = buf[1] + ((fc + (fc >> 8) + 0x80) >> 8);
                fc = (t[2] - buf[2]) * a;
                buf[2] = buf[2] + ((fc + (fc >> 8) + 0x80) >> 8);
                buf += 3;
                t += 4;
            }
        }
    } else {
        guchar *tmp = (guchar*)alloca(4 * height);
        sp_gradient_render_vector_line_rgba(gradient, tmp, height, pos, span);
        for (gint y = 0; y < height; y++) {
            guchar *t = tmp + 4 * y;
            for (gint x = 0; x < width; x++) {
                gint a = t[3];
                gint fc = (t[0] - buf[0]) * a;
                buf[0] = buf[0] + ((fc + (fc >> 8) + 0x80) >> 8);
                fc = (t[1] - buf[1]) * a;
                buf[1] = buf[1] + ((fc + (fc >> 8) + 0x80) >> 8);
                fc = (t[2] - buf[2]) * a;
                buf[2] = buf[2] + ((fc + (fc >> 8) + 0x80) >> 8);
            }
        }
    }
}

Geom::Matrix
sp_gradient_get_g2d_matrix(SPGradient const *gr, Geom::Matrix const &ctm, Geom::Rect const &bbox)
{
    if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
        return ( Geom::Scale(bbox.dimensions())
                 * Geom::Translate(bbox.min())
                 * Geom::Matrix(ctm) );
    } else {
        return ctm;
    }
}

Geom::Matrix
sp_gradient_get_gs2d_matrix(SPGradient const *gr, Geom::Matrix const &ctm, Geom::Rect const &bbox)
{
    if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
        return ( gr->gradientTransform
                 * Geom::Scale(bbox.dimensions())
                 * Geom::Translate(bbox.min())
                 * Geom::Matrix(ctm) );
    } else {
        return gr->gradientTransform * ctm;
    }
}

void
sp_gradient_set_gs2d_matrix(SPGradient *gr, Geom::Matrix const &ctm,
                            Geom::Rect const &bbox, Geom::Matrix const &gs2d)
{
    gr->gradientTransform = gs2d * ctm.inverse();
    if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX ) {
        gr->gradientTransform = ( gr->gradientTransform
                                  * Geom::Translate(-bbox.min())
                                  * Geom::Scale(bbox.dimensions()).inverse() );
    }
    gr->gradientTransform_set = TRUE;

    SP_OBJECT(gr)->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/*
 * Linear Gradient
 */

class SPLGPainter;

/// A context with linear gradient, painter, and gradient renderer.
struct SPLGPainter {
    SPPainter painter;
    SPLinearGradient *lg;

    NRLGradientRenderer lgr;
};

static void sp_lineargradient_class_init(SPLinearGradientClass *klass);
static void sp_lineargradient_init(SPLinearGradient *lg);

static void sp_lineargradient_build(SPObject *object,
                                    SPDocument *document,
                                    Inkscape::XML::Node *repr);
static void sp_lineargradient_set(SPObject *object, unsigned key, gchar const *value);
static Inkscape::XML::Node *sp_lineargradient_write(SPObject *object, Inkscape::XML::Document *doc, Inkscape::XML::Node *repr,
                                                    guint flags);

static SPPainter *sp_lineargradient_painter_new(SPPaintServer *ps,
                                                Geom::Matrix const &full_transform,
                                                Geom::Matrix const &parent_transform,
                                                NRRect const *bbox);
static void sp_lineargradient_painter_free(SPPaintServer *ps, SPPainter *painter);
static cairo_pattern_t *sp_lineargradient_create_pattern(SPPaintServer *ps, cairo_t *ct, NRRect const *bbox, double opacity);
static void sp_lg_fill(SPPainter *painter, NRPixBlock *pb);

static SPGradientClass *lg_parent_class;

/**
 * Register SPLinearGradient class and return its type.
 */
GType
sp_lineargradient_get_type()
{
    static GType type = 0;
    if (!type) {
        GTypeInfo info = {
            sizeof(SPLinearGradientClass),
            NULL, NULL,
            (GClassInitFunc) sp_lineargradient_class_init,
            NULL, NULL,
            sizeof(SPLinearGradient),
            16,
            (GInstanceInitFunc) sp_lineargradient_init,
            NULL,   /* value_table */
        };
        type = g_type_register_static(SP_TYPE_GRADIENT, "SPLinearGradient", &info, (GTypeFlags)0);
    }
    return type;
}

/**
 * SPLinearGradient vtable initialization.
 */
static void sp_lineargradient_class_init(SPLinearGradientClass *klass)
{
    SPObjectClass *sp_object_class = (SPObjectClass *) klass;
    SPPaintServerClass *ps_class = (SPPaintServerClass *) klass;

    lg_parent_class = (SPGradientClass*)g_type_class_ref(SP_TYPE_GRADIENT);

    sp_object_class->build = sp_lineargradient_build;
    sp_object_class->set = sp_lineargradient_set;
    sp_object_class->write = sp_lineargradient_write;

    ps_class->painter_new = sp_lineargradient_painter_new;
    ps_class->painter_free = sp_lineargradient_painter_free;
    ps_class->pattern_new = sp_lineargradient_create_pattern;
}

/**
 * Callback for SPLinearGradient object initialization.
 */
static void sp_lineargradient_init(SPLinearGradient *lg)
{
    lg->x1.unset(SVGLength::PERCENT, 0.0, 0.0);
    lg->y1.unset(SVGLength::PERCENT, 0.0, 0.0);
    lg->x2.unset(SVGLength::PERCENT, 1.0, 1.0);
    lg->y2.unset(SVGLength::PERCENT, 0.0, 0.0);
}

/**
 * Callback: set attributes from associated repr.
 */
static void sp_lineargradient_build(SPObject *object,
                                    SPDocument *document,
                                    Inkscape::XML::Node *repr)
{
    if (((SPObjectClass *) lg_parent_class)->build)
        (* ((SPObjectClass *) lg_parent_class)->build)(object, document, repr);

    sp_object_read_attr(object, "x1");
    sp_object_read_attr(object, "y1");
    sp_object_read_attr(object, "x2");
    sp_object_read_attr(object, "y2");
}

/**
 * Callback: set attribute.
 */
static void
sp_lineargradient_set(SPObject *object, unsigned key, gchar const *value)
{
    SPLinearGradient *lg = SP_LINEARGRADIENT(object);

    switch (key) {
        case SP_ATTR_X1:
            lg->x1.readOrUnset(value, SVGLength::PERCENT, 0.0, 0.0);
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_Y1:
            lg->y1.readOrUnset(value, SVGLength::PERCENT, 0.0, 0.0);
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_X2:
            lg->x2.readOrUnset(value, SVGLength::PERCENT, 1.0, 1.0);
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_Y2:
            lg->y2.readOrUnset(value, SVGLength::PERCENT, 0.0, 0.0);
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        default:
            if (((SPObjectClass *) lg_parent_class)->set)
                (* ((SPObjectClass *) lg_parent_class)->set)(object, key, value);
            break;
    }
}

/**
 * Callback: write attributes to associated repr.
 */
static Inkscape::XML::Node *
sp_lineargradient_write(SPObject *object, Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags)
{
    SPLinearGradient *lg = SP_LINEARGRADIENT(object);

    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:linearGradient");
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || lg->x1._set)
        sp_repr_set_svg_double(repr, "x1", lg->x1.computed);
    if ((flags & SP_OBJECT_WRITE_ALL) || lg->y1._set)
        sp_repr_set_svg_double(repr, "y1", lg->y1.computed);
    if ((flags & SP_OBJECT_WRITE_ALL) || lg->x2._set)
        sp_repr_set_svg_double(repr, "x2", lg->x2.computed);
    if ((flags & SP_OBJECT_WRITE_ALL) || lg->y2._set)
        sp_repr_set_svg_double(repr, "y2", lg->y2.computed);

    if (((SPObjectClass *) lg_parent_class)->write)
        (* ((SPObjectClass *) lg_parent_class)->write)(object, xml_doc, repr, flags);

    return repr;
}

/**
 * Create linear gradient context.
 *
 * Basically we have to deal with transformations
 *
 * 1) color2norm - maps point in (0,NCOLORS) vector to (0,1) vector
 * 2) norm2pos - maps (0,1) vector to x1,y1 - x2,y2
 * 2) gradientTransform
 * 3) bbox2user
 * 4) ctm == userspace to pixel grid
 *
 * See also (*) in sp-pattern about why we may need parent_transform.
 *
 * \todo (point 1 above) fixme: I do not know how to deal with start > 0
 * and end < 1.
 */
static SPPainter *
sp_lineargradient_painter_new(SPPaintServer *ps,
                              Geom::Matrix const &full_transform,
                              Geom::Matrix const &/*parent_transform*/,
                              NRRect const *bbox)
{
    SPLinearGradient *lg = SP_LINEARGRADIENT(ps);
    SPGradient *gr = SP_GRADIENT(ps);

    if (!gr->color) sp_gradient_ensure_colors(gr);

    SPLGPainter *lgp = g_new(SPLGPainter, 1);

    lgp->painter.type = SP_PAINTER_IND;
    lgp->painter.fill = sp_lg_fill;

    lgp->lg = lg;

    /** \todo
     * Technically speaking, we map NCOLORS on line [start,end] onto line
     * [0,1].  I almost think we should fill color array start and end in
     * that case. The alternative would be to leave these just empty garbage
     * or something similar. Originally I had 1023.9999 here - not sure
     * whether we have really to cut out ceil int (Lauris).
     */
    Geom::Matrix color2norm(Geom::identity());
    Geom::Matrix color2px;
    if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
        Geom::Matrix norm2pos(Geom::identity());

        /* BBox to user coordinate system */
        Geom::Matrix bbox2user(bbox->x1 - bbox->x0, 0, 0, bbox->y1 - bbox->y0, bbox->x0, bbox->y0);

        Geom::Matrix color2pos = color2norm * norm2pos;
        Geom::Matrix color2tpos = color2pos * gr->gradientTransform;
        Geom::Matrix color2user = color2tpos * bbox2user;
        color2px = color2user * full_transform;

    } else {
        /* Problem: What to do, if we have mixed lengths and percentages? */
        /* Currently we do ignore percentages at all, but that is not good (lauris) */

        Geom::Matrix norm2pos(Geom::identity());
        Geom::Matrix color2pos = color2norm * norm2pos;
        Geom::Matrix color2tpos = color2pos * gr->gradientTransform;
        color2px = color2tpos * full_transform;

    }
    // TODO: remove color2px_nr after converting to 2geom
    NR::Matrix color2px_nr = from_2geom(color2px);
    nr_lgradient_renderer_setup(&lgp->lgr, gr->color, sp_gradient_get_spread(gr), &color2px_nr,
                                lg->x1.computed, lg->y1.computed,
                                lg->x2.computed, lg->y2.computed);

    return (SPPainter *) lgp;
}

static void
sp_lineargradient_painter_free(SPPaintServer */*ps*/, SPPainter *painter)
{
    g_free(painter);
}

/**
 * Directly set properties of linear gradient and request modified.
 */
void
sp_lineargradient_set_position(SPLinearGradient *lg,
                               gdouble x1, gdouble y1,
                               gdouble x2, gdouble y2)
{
    g_return_if_fail(lg != NULL);
    g_return_if_fail(SP_IS_LINEARGRADIENT(lg));

    /* fixme: units? (Lauris)  */
    lg->x1.set(SVGLength::NONE, x1, x1);
    lg->y1.set(SVGLength::NONE, y1, y1);
    lg->x2.set(SVGLength::NONE, x2, x2);
    lg->y2.set(SVGLength::NONE, y2, y2);

    SP_OBJECT(lg)->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/**
 * Callback when linear gradient object is rendered.
 */
static void
sp_lg_fill(SPPainter *painter, NRPixBlock *pb)
{
    SPLGPainter *lgp = (SPLGPainter *) painter;

    if (lgp->lg->color == NULL) {
        sp_gradient_ensure_colors (lgp->lg);
        lgp->lgr.vector = lgp->lg->color;
    }

    nr_render((NRRenderer *) &lgp->lgr, pb, NULL);
}

/*
 * Radial Gradient
 */

class SPRGPainter;

/// A context with radial gradient, painter, and gradient renderer.
struct SPRGPainter {
    SPPainter painter;
    SPRadialGradient *rg;
    NRRGradientRenderer rgr;
};

static void sp_radialgradient_class_init(SPRadialGradientClass *klass);
static void sp_radialgradient_init(SPRadialGradient *rg);

static void sp_radialgradient_build(SPObject *object,
                                    SPDocument *document,
                                    Inkscape::XML::Node *repr);
static void sp_radialgradient_set(SPObject *object, unsigned key, gchar const *value);
static Inkscape::XML::Node *sp_radialgradient_write(SPObject *object, Inkscape::XML::Document *doc, Inkscape::XML::Node *repr,
                                                    guint flags);

static SPPainter *sp_radialgradient_painter_new(SPPaintServer *ps,
                                                Geom::Matrix const &full_transform,
                                                Geom::Matrix const &parent_transform,
                                                NRRect const *bbox);
static void sp_radialgradient_painter_free(SPPaintServer *ps, SPPainter *painter);
static cairo_pattern_t *sp_radialgradient_create_pattern(SPPaintServer *ps, cairo_t *ct, NRRect const *bbox, double opacity);

static void sp_rg_fill(SPPainter *painter, NRPixBlock *pb);

static SPGradientClass *rg_parent_class;

/**
 * Register SPRadialGradient class and return its type.
 */
GType
sp_radialgradient_get_type()
{
    static GType type = 0;
    if (!type) {
        GTypeInfo info = {
            sizeof(SPRadialGradientClass),
            NULL, NULL,
            (GClassInitFunc) sp_radialgradient_class_init,
            NULL, NULL,
            sizeof(SPRadialGradient),
            16,
            (GInstanceInitFunc) sp_radialgradient_init,
            NULL,   /* value_table */
        };
        type = g_type_register_static(SP_TYPE_GRADIENT, "SPRadialGradient", &info, (GTypeFlags)0);
    }
    return type;
}

/**
 * SPRadialGradient vtable initialization.
 */
static void sp_radialgradient_class_init(SPRadialGradientClass *klass)
{
    SPObjectClass *sp_object_class = (SPObjectClass *) klass;
    SPPaintServerClass *ps_class = (SPPaintServerClass *) klass;

    rg_parent_class = (SPGradientClass*)g_type_class_ref(SP_TYPE_GRADIENT);

    sp_object_class->build = sp_radialgradient_build;
    sp_object_class->set = sp_radialgradient_set;
    sp_object_class->write = sp_radialgradient_write;

    ps_class->painter_new = sp_radialgradient_painter_new;
    ps_class->painter_free = sp_radialgradient_painter_free;
    ps_class->pattern_new = sp_radialgradient_create_pattern;
}

/**
 * Callback for SPRadialGradient object initialization.
 */
static void
sp_radialgradient_init(SPRadialGradient *rg)
{
    rg->cx.unset(SVGLength::PERCENT, 0.5, 0.5);
    rg->cy.unset(SVGLength::PERCENT, 0.5, 0.5);
    rg->r.unset(SVGLength::PERCENT, 0.5, 0.5);
    rg->fx.unset(SVGLength::PERCENT, 0.5, 0.5);
    rg->fy.unset(SVGLength::PERCENT, 0.5, 0.5);
}

/**
 * Set radial gradient attributes from associated repr.
 */
static void
sp_radialgradient_build(SPObject *object, SPDocument *document, Inkscape::XML::Node *repr)
{
    if (((SPObjectClass *) rg_parent_class)->build)
        (* ((SPObjectClass *) rg_parent_class)->build)(object, document, repr);

    sp_object_read_attr(object, "cx");
    sp_object_read_attr(object, "cy");
    sp_object_read_attr(object, "r");
    sp_object_read_attr(object, "fx");
    sp_object_read_attr(object, "fy");
}

/**
 * Set radial gradient attribute.
 */
static void
sp_radialgradient_set(SPObject *object, unsigned key, gchar const *value)
{
    SPRadialGradient *rg = SP_RADIALGRADIENT(object);

    switch (key) {
        case SP_ATTR_CX:
            if (!rg->cx.read(value)) {
                rg->cx.unset(SVGLength::PERCENT, 0.5, 0.5);
            }
            if (!rg->fx._set) {
                rg->fx.value = rg->cx.value;
                rg->fx.computed = rg->cx.computed;
            }
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_CY:
            if (!rg->cy.read(value)) {
                rg->cy.unset(SVGLength::PERCENT, 0.5, 0.5);
            }
            if (!rg->fy._set) {
                rg->fy.value = rg->cy.value;
                rg->fy.computed = rg->cy.computed;
            }
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_R:
            if (!rg->r.read(value)) {
                rg->r.unset(SVGLength::PERCENT, 0.5, 0.5);
            }
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_FX:
            if (!rg->fx.read(value)) {
                rg->fx.unset(rg->cx.unit, rg->cx.value, rg->cx.computed);
            }
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SP_ATTR_FY:
            if (!rg->fy.read(value)) {
                rg->fy.unset(rg->cy.unit, rg->cy.value, rg->cy.computed);
            }
            object->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        default:
            if (((SPObjectClass *) rg_parent_class)->set)
                ((SPObjectClass *) rg_parent_class)->set(object, key, value);
            break;
    }
}

/**
 * Write radial gradient attributes to associated repr.
 */
static Inkscape::XML::Node *
sp_radialgradient_write(SPObject *object, Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags)
{
    SPRadialGradient *rg = SP_RADIALGRADIENT(object);

    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:radialGradient");
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || rg->cx._set) sp_repr_set_svg_double(repr, "cx", rg->cx.computed);
    if ((flags & SP_OBJECT_WRITE_ALL) || rg->cy._set) sp_repr_set_svg_double(repr, "cy", rg->cy.computed);
    if ((flags & SP_OBJECT_WRITE_ALL) || rg->r._set) sp_repr_set_svg_double(repr, "r", rg->r.computed);
    if ((flags & SP_OBJECT_WRITE_ALL) || rg->fx._set) sp_repr_set_svg_double(repr, "fx", rg->fx.computed);
    if ((flags & SP_OBJECT_WRITE_ALL) || rg->fy._set) sp_repr_set_svg_double(repr, "fy", rg->fy.computed);

    if (((SPObjectClass *) rg_parent_class)->write)
        (* ((SPObjectClass *) rg_parent_class)->write)(object, xml_doc, repr, flags);

    return repr;
}

/**
 * Create radial gradient context.
 */
static SPPainter *
sp_radialgradient_painter_new(SPPaintServer *ps,
                              Geom::Matrix const &full_transform,
                              Geom::Matrix const &/*parent_transform*/,
                              NRRect const *bbox)
{
    SPRadialGradient *rg = SP_RADIALGRADIENT(ps);
    SPGradient *gr = SP_GRADIENT(ps);

    if (!gr->color) sp_gradient_ensure_colors(gr);

    SPRGPainter *rgp = g_new(SPRGPainter, 1);

    rgp->painter.type = SP_PAINTER_IND;
    rgp->painter.fill = sp_rg_fill;

    rgp->rg = rg;

    Geom::Matrix gs2px;

    if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
        /** \todo
         * fixme: We may try to normalize here too, look at
         * linearGradient (Lauris)
         */

        /* BBox to user coordinate system */
        Geom::Matrix bbox2user(bbox->x1 - bbox->x0, 0, 0, bbox->y1 - bbox->y0, bbox->x0, bbox->y0);

        Geom::Matrix gs2user = gr->gradientTransform * bbox2user;

        gs2px = gs2user * full_transform;
    } else {
        /** \todo
         * Problem: What to do, if we have mixed lengths and percentages?
         * Currently we do ignore percentages at all, but that is not
         * good (lauris)
         */

        gs2px = gr->gradientTransform * full_transform;
    }
    // TODO: remove gs2px_nr after converting to 2geom
    NR::Matrix gs2px_nr = from_2geom(gs2px);
    nr_rgradient_renderer_setup(&rgp->rgr, gr->color, sp_gradient_get_spread(gr),
                                &gs2px_nr,
                                rg->cx.computed, rg->cy.computed,
                                rg->fx.computed, rg->fy.computed,
                                rg->r.computed);

    return (SPPainter *) rgp;
}

static void
sp_radialgradient_painter_free(SPPaintServer */*ps*/, SPPainter *painter)
{
    g_free(painter);
}

/**
 * Directly set properties of radial gradient and request modified.
 */
void
sp_radialgradient_set_position(SPRadialGradient *rg,
                               gdouble cx, gdouble cy, gdouble fx, gdouble fy, gdouble r)
{
    g_return_if_fail(rg != NULL);
    g_return_if_fail(SP_IS_RADIALGRADIENT(rg));

    /* fixme: units? (Lauris)  */
    rg->cx.set(SVGLength::NONE, cx, cx);
    rg->cy.set(SVGLength::NONE, cy, cy);
    rg->fx.set(SVGLength::NONE, fx, fx);
    rg->fy.set(SVGLength::NONE, fy, fy);
    rg->r.set(SVGLength::NONE, r, r);

    SP_OBJECT(rg)->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/**
 * Callback when radial gradient object is rendered.
 */
static void
sp_rg_fill(SPPainter *painter, NRPixBlock *pb)
{
    SPRGPainter *rgp = (SPRGPainter *) painter;

    if (rgp->rg->color == NULL) {
        sp_gradient_ensure_colors (rgp->rg);
        rgp->rgr.vector = rgp->rg->color;
    }

    nr_render((NRRenderer *) &rgp->rgr, pb, NULL);
}

/* CAIRO RENDERING STUFF */

static void
sp_gradient_pattern_common_setup(cairo_pattern_t *cp,
                                 SPGradient *gr,
                                 NRRect const *bbox,
                                 double opacity)
{
    // set spread type
    switch (sp_gradient_get_spread(gr)) {
    case SP_GRADIENT_SPREAD_REFLECT:
        cairo_pattern_set_extend(cp, CAIRO_EXTEND_REFLECT);
        break;
    case SP_GRADIENT_SPREAD_REPEAT:
        cairo_pattern_set_extend(cp, CAIRO_EXTEND_REPEAT);
        break;
    case SP_GRADIENT_SPREAD_PAD:
    default:
        cairo_pattern_set_extend(cp, CAIRO_EXTEND_PAD);
        break;
    }

    // add stops
    for (std::vector<SPGradientStop>::iterator i = gr->vector.stops.begin();
         i != gr->vector.stops.end(); ++i)
    {
        // multiply stop opacity by paint opacity
        cairo_pattern_add_color_stop_rgba(cp, i->offset,
            i->color.v.c[0], i->color.v.c[1], i->color.v.c[2], i->opacity * opacity);
    }

    // set pattern matrix
    Geom::Matrix gs2user = gr->gradientTransform;
    if (gr->units == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
        Geom::Matrix bbox2user(bbox->x1 - bbox->x0, 0, 0, bbox->y1 - bbox->y0, bbox->x0, bbox->y0);
        gs2user *= bbox2user;
    }
    ink_cairo_pattern_set_matrix(cp, gs2user.inverse());
}

static cairo_pattern_t *
sp_radialgradient_create_pattern(SPPaintServer *ps,
                                 cairo_t */* ct */,
                                 NRRect const *bbox,
                                 double opacity)
{
    SPRadialGradient *rg = SP_RADIALGRADIENT(ps);
    SPGradient *gr = SP_GRADIENT(ps);

    if (!gr->color) sp_gradient_ensure_colors(gr);

    cairo_pattern_t *cp = cairo_pattern_create_radial(
        rg->fx.computed, rg->fy.computed, 0,
        rg->cx.computed, rg->cy.computed, rg->r.computed);

    sp_gradient_pattern_common_setup(cp, gr, bbox, opacity);

    return cp;
}

static cairo_pattern_t *
sp_lineargradient_create_pattern(SPPaintServer *ps,
                                 cairo_t */* ct */,
                                 NRRect const *bbox,
                                 double opacity)
{
    SPLinearGradient *lg = SP_LINEARGRADIENT(ps);
    SPGradient *gr = SP_GRADIENT(ps);

    if (!gr->color) sp_gradient_ensure_colors(gr);

    cairo_pattern_t *cp = cairo_pattern_create_linear(
        lg->x1.computed, lg->y1.computed,
        lg->x2.computed, lg->y2.computed);

    sp_gradient_pattern_common_setup(cp, gr, bbox, opacity);

    return cp;
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
