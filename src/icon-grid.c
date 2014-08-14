/**
 * Copyright (c) 2009 LxDE Developers, see the file AUTHORS for details.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <gtk/gtk.h>
#include <string.h>

#include "icon-grid.h"

/* Properties */
enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_SPACING,
  PROP_CONSTRAIN_WIDTH
  //PROP_FILL_WIDTH
};

/* Representative of an icon grid.  This is a manager that packs widgets into a rectangular grid whose size adapts to conditions. */
struct _PanelIconGrid
{
    GtkContainer container;			/* Parent widget */
    GList * children;				/* List of icon grid elements */
    GtkOrientation orientation;			/* Desired orientation */
    gint child_width;				/* Desired child width */
    gint child_height;				/* Desired child height */
    gint spacing;				/* Desired spacing between grid elements */
    gint border;				/* Desired border around grid elements */
    gint target_dimension;			/* Desired dimension perpendicular to orientation */
    gboolean constrain_width : 1;		/* True if width should be constrained by allocated space */
    gboolean fill_width : 1;			/* True if children should fill unused width */
    int rows;					/* Computed layout rows */
    int columns;				/* Computed layout columns */
    int constrained_child_width;		/* Child width constrained by allocation */
};

struct _PanelIconGridClass
{
    GtkContainerClass parent_class;
};

static void panel_icon_grid_size_request(GtkWidget *widget, GtkRequisition *requisition);

/* Establish the widget placement of an icon grid. */
static void panel_icon_grid_size_allocate(GtkWidget *widget,
                                          GtkAllocation *allocation)
{
    PanelIconGrid *ig = PANEL_ICON_GRID(widget);
    GtkRequisition req;
    GtkAllocation child_allocation;
    int child_width;
    int child_height;
    GtkTextDirection direction;
    int limit;
    int x_initial;
    int x_delta;
    int x, y;
    GList *ige;
    GtkWidget *child;

    /* Get and save the desired container geometry. */
    if (ig->orientation == GTK_ORIENTATION_HORIZONTAL && allocation->height > 1)
        ig->target_dimension = allocation->height;
    else if (ig->orientation == GTK_ORIENTATION_VERTICAL && allocation->width > 1)
        ig->target_dimension = allocation->width;
    child_width = ig->child_width;
    child_height = ig->child_height;

    /* Calculate required size without borders */
    panel_icon_grid_size_request(widget, &req);
    req.width -= 2 * ig->border;
    req.height -= 2 * ig->border;

    /* Get the constrained child geometry if the allocated geometry is insufficient.
     * All children are still the same size and share equally in the deficit. */
    ig->constrained_child_width = ig->child_width;
    if ((ig->columns != 0) && (ig->rows != 0) && (allocation->width > 1))
    {
        if (req.width > allocation->width)
            ig->constrained_child_width = child_width = (allocation->width + ig->spacing - 2 * ig->border) / ig->columns - ig->spacing;
        if (ig->orientation == GTK_ORIENTATION_HORIZONTAL && req.height < allocation->height)
            child_height = (allocation->height + ig->spacing - 2 * ig->border) / ig->rows - ig->spacing;
    }

    /* Initialize parameters to control repositioning each visible child. */
    direction = gtk_widget_get_direction(widget);
    limit = ig->border + ((ig->orientation == GTK_ORIENTATION_HORIZONTAL)
        ?  (ig->rows * (child_height + ig->spacing))
        :  (ig->columns * (child_width + ig->spacing)));
    x_initial = ((direction == GTK_TEXT_DIR_RTL)
        ? allocation->width - child_width - ig->border
        : ig->border);
    x_delta = child_width + ig->spacing;
    if (direction == GTK_TEXT_DIR_RTL) x_delta = - x_delta;

    /* Reposition each visible child. */
    x = x_initial;
    y = ig->border;
    for (ige = ig->children; ige != NULL; ige = ige->next)
    {
        child = ige->data;
        if (gtk_widget_get_visible(child))
        {
            /* Do necessary operations on the child. */
            gtk_widget_size_request(child, &req);
            child_allocation.x = x;
            child_allocation.y = y;
            child_allocation.width = child_width;
            child_allocation.height = child_height;
            if (!gtk_widget_get_has_window (widget))
            {
                child_allocation.x += allocation->x;
                child_allocation.y += allocation->y;
            }
            // FIXME: if fill_width and rows > 1 then delay allocation
            gtk_widget_size_allocate(child, &child_allocation);

            /* Advance to the next grid position. */
            if (ig->orientation == GTK_ORIENTATION_HORIZONTAL)
            {
                y += child_height + ig->spacing;
                if (y >= limit)
                {
                    y = ig->border;
                    x += x_delta;
                    // FIXME: if fill_width and rows = 1 then allocate whole column
                }
            }
            else
            {
                // FIXME: if fill_width then use aspect to check delta
                x += x_delta;
                if ((direction == GTK_TEXT_DIR_RTL) ? (x <= 0) : (x >= limit))
                {
                    x = x_initial;
                    y += child_height + ig->spacing;
                }
            }
        }
    }
}

/* Establish the geometry of an icon grid. */
static void panel_icon_grid_size_request(GtkWidget *widget,
                                         GtkRequisition *requisition)
{
    PanelIconGrid *ig = PANEL_ICON_GRID(widget);
    int visible_children = 0;
    GList *ige;
    int target_dimension = ig->target_dimension;
    gint old_rows = ig->rows;
    gint old_columns = ig->columns;

    /* Count visible children. */
    for (ige = ig->children; ige != NULL; ige = ige->next)
        if (gtk_widget_get_visible(ige->data))
            visible_children += 1;

    if (ig->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        /* In horizontal orientation, fit as many rows into the available height as possible.
         * Then allocate as many columns as necessary.  Guard against zerodivides. */
        ig->rows = 0;
        if ((ig->child_height + ig->spacing) != 0)
            ig->rows = (target_dimension + ig->spacing - ig->border * 2) / (ig->child_height + ig->spacing);
        if (ig->rows == 0)
            ig->rows = 1;
        ig->columns = (visible_children + (ig->rows - 1)) / ig->rows;
        if ((ig->columns == 1) && (ig->rows > visible_children))
            ig->rows = visible_children;
    }
    else
    {
        /* In vertical orientation, fit as many columns into the available width as possible.
         * Then allocate as many rows as necessary.  Guard against zerodivides. */
        ig->columns = 0;
        if ((ig->child_width + ig->spacing) != 0)
            ig->columns = (target_dimension + ig->spacing - ig->border * 2) / (ig->child_width + ig->spacing);
        if (ig->columns == 0)
            ig->columns = 1;
        ig->rows = (visible_children + (ig->columns - 1)) / ig->columns;
        if ((ig->rows == 1) && (ig->columns > visible_children))
            ig->columns = visible_children;
    }

    /* Compute the requisition. */
    if ((ig->columns == 0) || (ig->rows == 0))
    {
        requisition->width = 1;
        requisition->height = 1;
        gtk_widget_hide(widget);	/* Necessary to get the plugin to disappear */
    }
    else
    {
        int column_spaces = ig->columns - 1;
        int row_spaces = ig->rows - 1;
        if (column_spaces < 0) column_spaces = 0;
        if (row_spaces < 0) row_spaces = 0;
        requisition->width = ig->child_width * ig->columns + column_spaces * ig->spacing + 2 * ig->border;
        requisition->height = ig->child_height * ig->rows + row_spaces * ig->spacing + 2 * ig->border;
        gtk_widget_show(widget);
    }
    if (ig->rows != old_rows || ig->columns != old_columns)
        gtk_widget_queue_resize(widget);
}

/* Handler for "size-request" event on the icon grid element. */
static void icon_grid_element_size_request(GtkWidget * widget, GtkRequisition * requisition, PanelIconGrid * ig)
{
    /* This is our opportunity to request space for the element. */
    // FIXME: if fill_width then calculate width from aspect
    requisition->width = ig->child_width;
    if ((ig->constrain_width) && (ig->constrained_child_width > 1))
        requisition->width = ig->constrained_child_width;
    requisition->height = ig->child_height;
}

/* Add an icon grid element and establish its initial visibility. */
static void panel_icon_grid_add(GtkContainer *container, GtkWidget *widget)
{
    PanelIconGrid *ig = PANEL_ICON_GRID(container);

    /* Insert at the tail of the child list.  This keeps the graphics in the order they were added. */
    ig->children = g_list_append(ig->children, widget);

    /* Add the widget to the layout container. */
    g_signal_connect(G_OBJECT(widget), "size-request",
                     G_CALLBACK(icon_grid_element_size_request), container);
    gtk_widget_set_parent(widget, GTK_WIDGET(container));
    gtk_widget_queue_resize(GTK_WIDGET(container));
}

void panel_icon_grid_set_constrain_width(PanelIconGrid * ig, gboolean constrain_width)
{
    g_return_if_fail(PANEL_IS_ICON_GRID(ig));

    if ((!ig->constrain_width && !constrain_width) ||
        (ig->constrain_width && constrain_width))
        return;

    ig->constrain_width = !!constrain_width;
    gtk_widget_queue_resize(GTK_WIDGET(ig));
}

/* void panel_icon_grid_set_fill_width(PanelIconGrid * ig, gboolean fill_width)
{
    g_return_if_fail(PANEL_IS_ICON_GRID(ig));

    if ((!ig->fill_width && !fill_width) || (ig->fill_width && fill_width))
        return;

    ig->fill_width = !!fill_width;
    gtk_widget_queue_resize(GTK_WIDGET(ig));
} */

/* Remove an icon grid element. */
static void panel_icon_grid_remove(GtkContainer *container, GtkWidget *widget)
{
    PanelIconGrid *ig = PANEL_ICON_GRID(container);
    GList *children = ig->children;
    GtkWidget *child;

    while (children)
    {
        child = children->data;
        if (widget == child)
        {
            gboolean was_visible = gtk_widget_get_visible(widget);

            /* The child is found.  Remove from child list and layout container. */
            g_signal_handlers_disconnect_by_func(widget,
                                                 icon_grid_element_size_request,
                                                 container);
            gtk_widget_unparent (widget);
            ig->children = g_list_remove_link(ig->children, children);
            g_list_free(children);

            /* Do a relayout if needed. */
            if (was_visible)
                gtk_widget_queue_resize(GTK_WIDGET(ig));
            break;
        }
        children = children->next;
    }
}

/* Get the index of an icon grid element. */
gint panel_icon_grid_get_child_position(PanelIconGrid * ig, GtkWidget * child)
{
    g_return_val_if_fail(PANEL_IS_ICON_GRID(ig), -1);

    return g_list_index(ig->children, child);
}

/* Reorder an icon grid element. */
void panel_icon_grid_reorder_child(PanelIconGrid * ig, GtkWidget * child, gint position)
{
    GList *old_link;
    GList *new_link;
    gint old_position;

    g_return_if_fail(PANEL_IS_ICON_GRID(ig));
    g_return_if_fail(GTK_IS_WIDGET(child));

    old_link = ig->children;
    old_position = 0;
    while (old_link)
    {
        if (old_link->data == child)
            break;
        old_link = old_link->next;
        old_position++;
    }

    g_return_if_fail(old_link != NULL);

    if (position == old_position)
        return;

    /* Remove the child from its current position. */
    ig->children = g_list_delete_link(ig->children, old_link);
    if (position < 0)
        new_link = NULL;
    else
        new_link = g_list_nth(ig->children, position);

    /* If the child was found, insert it at the new position. */
    ig->children = g_list_insert_before(ig->children, new_link, child);

    /* Do a relayout. */
    if (gtk_widget_get_visible(child) && gtk_widget_get_visible(GTK_WIDGET(ig)))
        gtk_widget_queue_resize(child);
}

/* Change the geometry of an icon grid. */
void panel_icon_grid_set_geometry(PanelIconGrid * ig,
    GtkOrientation orientation, gint child_width, gint child_height, gint spacing, gint border, gint target_dimension)
{
    g_return_if_fail(PANEL_IS_ICON_GRID(ig));

    if (ig->orientation == orientation && ig->child_width == child_width &&
            ig->child_height == child_height && ig->spacing == spacing &&
            ig->border == border && ig->target_dimension == target_dimension)
        return;

    ig->orientation = orientation;
    ig->child_width = child_width;
    ig->constrained_child_width = child_width;
    ig->child_height = child_height;
    ig->spacing = spacing;
    ig->border = border;
    ig->target_dimension = target_dimension;
    gtk_widget_queue_resize(GTK_WIDGET(ig));
}

G_DEFINE_TYPE_WITH_CODE(PanelIconGrid, panel_icon_grid, GTK_TYPE_CONTAINER,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_ORIENTABLE, NULL));

static void panel_icon_grid_set_property(GObject *object, guint prop_id,
                                         const GValue *value, GParamSpec *pspec)
{
    PanelIconGrid *ig = PANEL_ICON_GRID(object);
    gint spacing;
    GtkOrientation orientation;

    switch (prop_id)
    {
    case PROP_ORIENTATION:
        orientation = g_value_get_enum(value);
        if (orientation != ig->orientation)
        {
            ig->orientation = orientation;
            gtk_widget_queue_resize(GTK_WIDGET(ig));
        }
        break;
    case PROP_SPACING:
        spacing = g_value_get_int(value);
        if (spacing != ig->spacing)
        {
            ig->spacing = spacing;
            g_object_notify(object, "spacing");
            gtk_widget_queue_resize(GTK_WIDGET(ig));
        }
        break;
    case PROP_CONSTRAIN_WIDTH:
        panel_icon_grid_set_constrain_width(ig, g_value_get_boolean(value));
        break;
    /* case PROP_FILL_WIDTH:
        panel_icon_grid_set_fill_width(ig, g_value_get_boolean(value));
        break; */
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void panel_icon_grid_get_property(GObject *object, guint prop_id,
                                         GValue *value, GParamSpec *pspec)
{
    PanelIconGrid *ig = PANEL_ICON_GRID(object);

    switch (prop_id)
    {
    case PROP_ORIENTATION:
        g_value_set_enum(value, ig->orientation);
        break;
    case PROP_SPACING:
        g_value_set_int(value, ig->spacing);
        break;
    case PROP_CONSTRAIN_WIDTH:
        g_value_set_boolean(value, ig->constrain_width);
        break;
    /* case PROP_FILL_WIDTH:
        g_value_set_boolean(value, ig->fill_width);
        break; */
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void panel_icon_grid_forall(GtkContainer *container,
                                   gboolean      include_internals,
                                   GtkCallback   callback,
                                   gpointer      callback_data)
{
    PanelIconGrid *ig = PANEL_ICON_GRID(container);
    GList *children = ig->children;
    GtkWidget *child;

    while (children)
    {
        child = children->data;
        children = children->next;
        (* callback)(child, callback_data);
    }
}

static GType panel_icon_grid_child_type(GtkContainer *container)
{
    return GTK_TYPE_WIDGET;
}

static void panel_icon_grid_class_init(PanelIconGridClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS(class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);
    GtkContainerClass *container_class = GTK_CONTAINER_CLASS(class);

    object_class->set_property = panel_icon_grid_set_property;
    object_class->get_property = panel_icon_grid_get_property;

    widget_class->size_request = panel_icon_grid_size_request;
    widget_class->size_allocate = panel_icon_grid_size_allocate;

    container_class->add = panel_icon_grid_add;
    container_class->remove = panel_icon_grid_remove;
    container_class->forall = panel_icon_grid_forall;
    container_class->child_type = panel_icon_grid_child_type;

    g_object_class_override_property(object_class,
                                     PROP_ORIENTATION,
                                     "orientation");
    g_object_class_install_property(object_class,
                                    PROP_SPACING,
                                    g_param_spec_int("spacing",
                                                     "Spacing",
                                                     "The amount of space between children",
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READWRITE));
    g_object_class_install_property(object_class,
                                    PROP_CONSTRAIN_WIDTH,
                                    g_param_spec_boolean("constrain-width",
                                                         "Constrain width",
                                                         "Whether to constrain width by allocated space",
                                                         FALSE, G_PARAM_READWRITE));
}

static void panel_icon_grid_init(PanelIconGrid *ig)
{
    gtk_widget_set_has_window(GTK_WIDGET(ig), FALSE);
    gtk_widget_set_redraw_on_allocate(GTK_WIDGET(ig), FALSE);

    ig->orientation = GTK_ORIENTATION_HORIZONTAL;
}

/* Establish an icon grid in a specified container widget.
 * The icon grid manages the contents of the container.
 * The orientation, geometry of the elements, and spacing can be varied.  All elements are the same size. */
GtkWidget * panel_icon_grid_new(
    GtkOrientation orientation, gint child_width, gint child_height, gint spacing, gint border, gint target_dimension)
{
    /* Create a structure representing the icon grid and collect the parameters. */
    PanelIconGrid * ig = g_object_new(PANEL_TYPE_ICON_GRID,
                                      "orientation", orientation,
                                      "spacing", spacing, NULL);

    ig->child_width = child_width;
    ig->constrained_child_width = child_width;
    ig->child_height = child_height;
    ig->border = border;
    ig->target_dimension = target_dimension;

    return (GtkWidget *)ig;
}
