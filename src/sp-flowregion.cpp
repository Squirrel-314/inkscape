#define __SP_FLOWREGION_C__

/*
 */

#include <config.h>
#include <string.h>

#include <xml/repr.h>

#include "sp-object.h"
#include "sp-item.h"
#include "sp-shape.h"
#include "sp-text.h"
#include "sp-use.h"
#include "style.h"

#include "sp-flowregion.h"

#include "libnrtype/FlowDest.h"

#include "display/curve.h"

#include "libnr/nr-point.h"
#include "libnr/nr-matrix.h"
#include "libnr/nr-point-ops.h"
#include "libnr/nr-matrix-ops.h"
#include "libnr/nr-path.h"

#include "livarot/LivarotDefs.h"
#include "livarot/Path.h"
#include "livarot/Shape.h"

static void sp_flowregion_class_init (SPFlowregionClass *klass);
static void sp_flowregion_init (SPFlowregion *group);
static void sp_flowregion_dispose (GObject *object);

static void sp_flowregion_child_added (SPObject * object, SPRepr * child, SPRepr * ref);
static void sp_flowregion_remove_child (SPObject * object, SPRepr * child);
static void sp_flowregion_update (SPObject *object, SPCtx *ctx, guint flags);
static void sp_flowregion_modified (SPObject *object, guint flags);
static SPRepr *sp_flowregion_write (SPObject *object, SPRepr *repr, guint flags);

static gchar * sp_flowregion_description (SPItem * item);

static SPItemClass * flowregion_parent_class;

static void sp_flowregionexclude_class_init (SPFlowregionExcludeClass *klass);
static void sp_flowregionexclude_init (SPFlowregionExclude *group);
static void sp_flowregionexclude_dispose (GObject *object);

static void sp_flowregionexclude_child_added (SPObject * object, SPRepr * child, SPRepr * ref);
static void sp_flowregionexclude_remove_child (SPObject * object, SPRepr * child);
static void sp_flowregionexclude_update (SPObject *object, SPCtx *ctx, guint flags);
static void sp_flowregionexclude_modified (SPObject *object, guint flags);
static SPRepr *sp_flowregionexclude_write (SPObject *object, SPRepr *repr, guint flags);

static gchar * sp_flowregionexclude_description (SPItem * item);

static SPItemClass * flowregionexclude_parent_class;


static void         GetDest(SPObject* child,flow_dest* computed,NR::Matrix itr_mat);

GType
sp_flowregion_get_type (void)
{
	static GType group_type = 0;
	if (!group_type) {
		GTypeInfo group_info = {
			sizeof (SPFlowregionClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) sp_flowregion_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (SPFlowregion),
			16,	/* n_preallocs */
			(GInstanceInitFunc) sp_flowregion_init,
			NULL,	/* value_table */
		};
		group_type = g_type_register_static (SP_TYPE_ITEM, "SPFlowregion", &group_info, (GTypeFlags)0);
	}
	return group_type;
}

static void
sp_flowregion_class_init (SPFlowregionClass *klass)
{
	GObjectClass * object_class;
	SPObjectClass * sp_object_class;
	SPItemClass * item_class;
	
	object_class = (GObjectClass *) klass;
	sp_object_class = (SPObjectClass *) klass;
	item_class = (SPItemClass *) klass;
	
	flowregion_parent_class = (SPItemClass *)g_type_class_ref (SP_TYPE_ITEM);
	
	object_class->dispose = sp_flowregion_dispose;
	
	sp_object_class->child_added = sp_flowregion_child_added;
	sp_object_class->remove_child = sp_flowregion_remove_child;
	sp_object_class->update = sp_flowregion_update;
	sp_object_class->modified = sp_flowregion_modified;
	sp_object_class->write = sp_flowregion_write;
	
	item_class->description = sp_flowregion_description;
}

static void
sp_flowregion_init (SPFlowregion *group)
{
	group->nbComp=group->maxComp=0;
	group->computed=NULL;
}

static void
sp_flowregion_dispose(GObject *object)
{
	SPFlowregion *group=(SPFlowregion *)object;
	for (int i=0;i<group->nbComp;i++) delete group->computed[i];
	if ( group->computed ) free(group->computed);
	group->nbComp=group->maxComp=0;
	group->computed=NULL;
}

static void
sp_flowregion_child_added (SPObject *object, SPRepr *child, SPRepr *ref)
{
	SPItem *item;
	
	item = SP_ITEM (object);
	
	if (((SPObjectClass *) (flowregion_parent_class))->child_added)
		(* ((SPObjectClass *) (flowregion_parent_class))->child_added) (object, child, ref);
	
	object->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/* fixme: hide (Lauris) */

static void
sp_flowregion_remove_child (SPObject * object, SPRepr * child)
{
	if (((SPObjectClass *) (flowregion_parent_class))->remove_child)
		(* ((SPObjectClass *) (flowregion_parent_class))->remove_child) (object, child);
	
	object->requestModified(SP_OBJECT_MODIFIED_FLAG);
}


static void
sp_flowregion_update (SPObject *object, SPCtx *ctx, unsigned int flags)
{
	SPFlowregion *group;
	SPObject *child;
	SPItemCtx *ictx, cctx;
	GSList *l;
	
	group = SP_FLOWREGION (object);
	ictx = (SPItemCtx *) ctx;
	cctx = *ictx;
	
	if (((SPObjectClass *) (flowregion_parent_class))->update)
		((SPObjectClass *) (flowregion_parent_class))->update (object, ctx, flags);
	
	if (flags & SP_OBJECT_MODIFIED_FLAG) flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
	flags &= SP_OBJECT_MODIFIED_CASCADE;
	
	l = NULL;
	for (child = sp_object_first_child(object) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
		g_object_ref (G_OBJECT (child));
		l = g_slist_prepend (l, child);
	}
	l = g_slist_reverse (l);
	while (l) {
		child = SP_OBJECT (l->data);
		l = g_slist_remove (l, child);
		if (flags || (child->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
			if (SP_IS_ITEM (child)) {
				SPItem *chi;
				chi = SP_ITEM (child);

				NRMatrix chitransform; // FIXME!!! we need to make everything NR::Matrix
				(chi->transform).copyto (&chitransform);

				nr_matrix_multiply (&cctx.i2doc, &chitransform, &ictx->i2doc);
				nr_matrix_multiply (&cctx.i2vp, &chitransform, &ictx->i2vp);
				child->updateDisplay((SPCtx *)&cctx, flags);
			} else {
				child->updateDisplay(ctx, flags);
			}
		}
		g_object_unref (G_OBJECT (child));
	}

	group->UpdateComputed();
}

void             SPFlowregion::UpdateComputed(void)
{
	SPObject* object=SP_OBJECT(this);
	
	NR::Matrix itr_mat=sp_item_i2root_affine (SP_ITEM(object));
	itr_mat=itr_mat.inverse();
	
	for (int i=0;i<nbComp;i++) delete computed[i];
	nbComp=0;
	
	for (SPObject* child = sp_object_first_child(object) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
		if ( nbComp >= maxComp ) {
			maxComp=2*nbComp+1;
			computed=(flow_dest**)realloc(computed,maxComp*sizeof(flow_dest*));
		}
		computed[nbComp++]=new flow_dest,
		GetDest(child,computed[nbComp-1],itr_mat);
	}
}

static void
sp_flowregion_modified (SPObject *object, guint flags)
{
	SPFlowregion *group;
	SPObject *child;
	GSList *l;
	
	group = SP_FLOWREGION (object);	
	
	if (flags & SP_OBJECT_MODIFIED_FLAG) flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
	flags &= SP_OBJECT_MODIFIED_CASCADE;
	
	l = NULL;
	for (child = sp_object_first_child(object) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
		g_object_ref (G_OBJECT (child));
		l = g_slist_prepend (l, child);
	}
	l = g_slist_reverse (l);
	while (l) {
		child = SP_OBJECT (l->data);
		l = g_slist_remove (l, child);
		if (flags || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
			child->emitModified(flags);
		}
		g_object_unref (G_OBJECT (child));
	}
}

static SPRepr *
sp_flowregion_write (SPObject *object, SPRepr *repr, guint flags)
{
	SPFlowregion *group;
	
	group = SP_FLOWREGION (object);
	
	if ( flags&SP_OBJECT_WRITE_BUILD ) {
		if ( repr == NULL ) repr = sp_repr_new ("flowRegion");
	}

	if (((SPObjectClass *) (flowregion_parent_class))->write)
		((SPObjectClass *) (flowregion_parent_class))->write (object, repr, flags);
	
	return repr;
}


static gchar * sp_flowregion_description (SPItem * item)
{
	SPFlowregion * group;
	
	group = SP_FLOWREGION (item);
	
	//	return g_strdup_printf(_("Flow region"));
	return g_strdup_printf("Flow region");
}

/*
 *
 */

GType
sp_flowregionexclude_get_type (void)
{
	static GType group_type = 0;
	if (!group_type) {
		GTypeInfo group_info = {
			sizeof (SPFlowregionExcludeClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) sp_flowregionexclude_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (SPFlowregionExclude),
			16,	/* n_preallocs */
			(GInstanceInitFunc) sp_flowregionexclude_init,
			NULL,	/* value_table */
		};
		group_type = g_type_register_static (SP_TYPE_ITEM, "SPFlowregionExclude", &group_info, (GTypeFlags)0);
	}
	return group_type;
}

static void
sp_flowregionexclude_class_init (SPFlowregionExcludeClass *klass)
{
	GObjectClass * object_class;
	SPObjectClass * sp_object_class;
	SPItemClass * item_class;
	
	object_class = (GObjectClass *) klass;
	sp_object_class = (SPObjectClass *) klass;
	item_class = (SPItemClass *) klass;
	
	flowregionexclude_parent_class = (SPItemClass *)g_type_class_ref (SP_TYPE_ITEM);
	
	object_class->dispose = sp_flowregionexclude_dispose;
	
	sp_object_class->child_added = sp_flowregionexclude_child_added;
	sp_object_class->remove_child = sp_flowregionexclude_remove_child;
	sp_object_class->update = sp_flowregionexclude_update;
	sp_object_class->modified = sp_flowregionexclude_modified;
	sp_object_class->write = sp_flowregionexclude_write;
	
	item_class->description = sp_flowregionexclude_description;
}

static void
sp_flowregionexclude_init (SPFlowregionExclude *group)
{
	group->computed=new flow_dest;
}

static void
sp_flowregionexclude_dispose(GObject *object)
{
	SPFlowregionExclude *group=(SPFlowregionExclude *)object;
	delete group->computed;
}

static void
sp_flowregionexclude_child_added (SPObject *object, SPRepr *child, SPRepr *ref)
{
	SPItem *item;
	
	item = SP_ITEM (object);
	
	if (((SPObjectClass *) (flowregionexclude_parent_class))->child_added)
		(* ((SPObjectClass *) (flowregionexclude_parent_class))->child_added) (object, child, ref);
	
	object->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/* fixme: hide (Lauris) */

static void
sp_flowregionexclude_remove_child (SPObject * object, SPRepr * child)
{
	if (((SPObjectClass *) (flowregionexclude_parent_class))->remove_child)
		(* ((SPObjectClass *) (flowregionexclude_parent_class))->remove_child) (object, child);
	
	object->requestModified(SP_OBJECT_MODIFIED_FLAG);
}


static void
sp_flowregionexclude_update (SPObject *object, SPCtx *ctx, unsigned int flags)
{
	SPFlowregionExclude *group;
	SPObject *child;
	SPItemCtx *ictx, cctx;
	GSList *l;
	
	group = SP_FLOWREGIONEXCLUDE (object);
	ictx = (SPItemCtx *) ctx;
	cctx = *ictx;
		
	if (((SPObjectClass *) (flowregionexclude_parent_class))->update)
		((SPObjectClass *) (flowregionexclude_parent_class))->update (object, ctx, flags);
	
	if (flags & SP_OBJECT_MODIFIED_FLAG) flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
	flags &= SP_OBJECT_MODIFIED_CASCADE;
	
	l = NULL;
	for (child = sp_object_first_child(object) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
		g_object_ref (G_OBJECT (child));
		l = g_slist_prepend (l, child);
	}
	l = g_slist_reverse (l);
	while (l) {
		child = SP_OBJECT (l->data);
		l = g_slist_remove (l, child);
		if (flags || (child->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
			if (SP_IS_ITEM (child)) {
				SPItem *chi;
				chi = SP_ITEM (child);

				NRMatrix chitransform; // FIXME!!! we need to make everything NR::Matrix
				(chi->transform).copyto (&chitransform);

				nr_matrix_multiply (&cctx.i2doc, &chitransform, &ictx->i2doc);
				nr_matrix_multiply (&cctx.i2vp, &chitransform, &ictx->i2vp);
				child->updateDisplay((SPCtx *)&cctx, flags);
			} else {
				child->updateDisplay(ctx, flags);
			}
		}
		g_object_unref (G_OBJECT (child));
	}

	group->UpdateComputed();
}
void             SPFlowregionExclude::UpdateComputed(void)
{
	SPObject* object=SP_OBJECT(this);
	
	computed->Reset();
	NR::Matrix itr_mat=sp_item_i2root_affine (SP_ITEM(object));
	itr_mat=itr_mat.inverse();
	
	for (SPObject* child = sp_object_first_child(object) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
		GetDest(child,computed,itr_mat);
	}
}

static void
sp_flowregionexclude_modified (SPObject *object, guint flags)
{
	SPFlowregionExclude *group;
	SPObject *child;
	GSList *l;
	
	group = SP_FLOWREGIONEXCLUDE (object);
	
	if (flags & SP_OBJECT_MODIFIED_FLAG) flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
	flags &= SP_OBJECT_MODIFIED_CASCADE;
	
	l = NULL;
	for (child = sp_object_first_child(object) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
		g_object_ref (G_OBJECT (child));
		l = g_slist_prepend (l, child);
	}
	l = g_slist_reverse (l);
	while (l) {
		child = SP_OBJECT (l->data);
		l = g_slist_remove (l, child);
		if (flags || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
			child->emitModified(flags);
		}
		g_object_unref (G_OBJECT (child));
	}
}

static SPRepr *
sp_flowregionexclude_write (SPObject *object, SPRepr *repr, guint flags)
{
	SPFlowregionExclude *group;
	
	group = SP_FLOWREGIONEXCLUDE (object);
	
	if ( flags&SP_OBJECT_WRITE_BUILD ) {
		if ( repr == NULL ) repr = sp_repr_new ("flowRegionExclude");
	}
	
	if (((SPObjectClass *) (flowregionexclude_parent_class))->write)
		((SPObjectClass *) (flowregionexclude_parent_class))->write (object, repr, flags);
	
	return repr;
}


static gchar * sp_flowregionexclude_description (SPItem * item)
{
	SPFlowregionExclude * group;
	
	group = SP_FLOWREGIONEXCLUDE (item);
	
	//	return g_strdup_printf(_("Flow region"));
	return g_strdup_printf("Flow excluded region");
}

/*
 *
 */

static void         GetDest(SPObject* child,flow_dest* computed,NR::Matrix itr_mat)
{	
	if ( child == NULL ) return;
	
	SPCurve *curve=NULL;
		
	SPObject* u_child=child;
	if ( SP_IS_USE(u_child) ) {
		u_child=SP_USE(u_child)->child;
	}
	if ( SP_IS_SHAPE (u_child) ) {
		curve = sp_shape_get_curve (SP_SHAPE (u_child));
	} else if ( SP_IS_TEXT (u_child) ) {
	curve = sp_text_normalized_bpath (SP_TEXT (u_child));
	}
	
	if ( curve ) {
		Path*   temp=new Path;
		NR::Matrix tr_mat=sp_item_i2root_affine (SP_ITEM(u_child));
		tr_mat=itr_mat*tr_mat;
		temp->LoadArtBPath(curve->bpath,tr_mat,true);
		Shape*  n_shp=new Shape;
		temp->Convert(0.25);
		temp->Fill(n_shp,0);
		Shape*  uncross=new Shape;
		SPStyle* style=SP_OBJECT_STYLE(child);
		if ( style && style->fill_rule.computed == 1 ) {
			uncross->ConvertToShape(n_shp,fill_oddEven);
		} else {
			uncross->ConvertToShape(n_shp,fill_nonZero);
		}
		computed->AddShape(uncross);
		delete uncross;
		delete n_shp;
		delete temp;
		sp_curve_unref(curve);
	} else {
		printf("no curve\n");
	}
}

