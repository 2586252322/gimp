/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "frame_manager.h"
#include "gimage.h"
#include "gdisplay.h" 
#include "ops_buttons.h"
#include "general.h"
#include "interface.h"
#include "actionarea.h"
#include "tools.h"
#include "rect_selectP.h"
#include "layer.h"
#include "layer_pvt.h"
#include "clone.h"
#include "fileops.h"

#include "tools/forward.xpm"
#include "tools/forward_is.xpm"
#include "tools/backwards.xpm"
#include "tools/backwards_is.xpm"
#include "tools/raise.xpm"
#include "tools/raise_is.xpm"
#include "tools/lower.xpm"
#include "tools/lower_is.xpm"
#include "tools/delete.xpm"
#include "tools/delete_is.xpm"
#include "tools/area.xpm"
#include "tools/area_is.xpm"
#include "tools/update.xpm"
#include "tools/update_is.xpm"
#include "tools/istep.xbm"
#include "tools/iflip.xbm"
#include "tools/ibg.xbm"

#define STORE_LIST_WIDTH  	200
#define STORE_LIST_HEIGHT	150 
#define	STORE_MAX_NUM		6
#define BUTTON_EVENT_MASK  GDK_EXPOSURE_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | \
                           GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
	
			   
#define NORMAL 0
#define SELECTED 1
#define INSENSITIVE 2
			   


typedef struct
{
  GtkWidget *istep;
  GtkWidget *iflip;
  GtkWidget *ibg;
  GtkWidget *label;
  GImage *gimage;
  GtkWidget *list_item;
  char flip;
  char bg;
  char step; 
  char selected;
  char active;  
  GImage *bg_image; 
}store_t;

/* functions */
static gint frame_manager_step_size (GtkWidget *, gpointer);

static gint frame_manager_jump_to (GtkWidget *, gpointer);
static void frame_manager_jump_to_update (char *, frame_manager_t*);
static gint frame_manager_auto_save (GtkWidget *, gpointer);

static gint frame_manager_flip_area (GtkWidget *, gpointer);
static gint frame_manager_update_whole_area (GtkWidget *, gpointer);
static gint frame_manager_flip_delete (GtkWidget *, gpointer);

static void frame_manager_next_filename (GImage *gimage, char **whole, char **raw, char
dir, frame_manager_t *fm);
static char* frame_manager_get_name (GImage *image);
static char* frame_manager_get_frame (GImage *image);
static char* frame_manager_get_ext (GImage *image);
static void frame_manager_this_filename (GImage *gimage, char **whole, char **raw, 
    char *num, frame_manager_t *fm);

static GtkWidget* frame_manager_create_menu (GImage *image);
static void frame_manager_update_menu ();
static gint frame_manager_link_menu (GtkWidget *, gpointer);

static gint frame_manager_store_list (GtkWidget *, GdkEvent *);
static gint frame_manager_store_size (GtkWidget *, gpointer);
static gint frame_manager_store_select_update (GtkWidget *, gpointer);
static gint frame_manager_store_deselect_update (GtkWidget *, gpointer);
static gint frame_manager_store_step_button (GtkWidget *, GdkEvent *);
static gint frame_manager_store_flip_button (GtkWidget *, GdkEvent *);
static gint frame_manager_store_bg_button (GtkWidget *, GdkEvent *);
static void frame_manager_store_add (GImage *, frame_manager_t *, int);
static store_t* frame_manager_store_new (GImage *gimage, char active);
static void frame_manager_store_delete (store_t*, frame_manager_t*, char);
static void frame_manager_store_unselect (frame_manager_t *fm);
static void frame_manager_store_load (store_t *item, frame_manager_t*);
static void frame_manager_store_clear (frame_manager_t*);
static void frame_manager_store_new_option (GtkWidget *, gpointer);

static void frame_manager_flip_redraw (store_t *item);
static void frame_manager_step_redraw (store_t *item);
static void frame_manager_bg_redraw (store_t *item);
static void frame_manager_save (GImage *gimage, frame_manager_t *fm);

static gint frame_manager_transparency (GtkAdjustment *, gpointer);
static gint frame_manager_close (GtkWidget *, gpointer);

static void frame_manager_link_forward (frame_manager_t*);
static void frame_manager_link_backward (frame_manager_t*);

static int frame_manager_check (frame_manager_t *fm);
static int frame_manager_get_bg_id ();

static ActionAreaItem action_items[] =
{
  { "Close", frame_manager_close, NULL, NULL }  
};

static OpsButton step_button[] =
{
  { backwards_xpm, backwards_is_xpm, frame_manager_step_backwards, "Step Backwards", NULL, NULL, NULL, NULL, NULL, NULL },
  { forward_xpm, forward_is_xpm, frame_manager_step_forward, "Step Forward", NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, 0, 0, NULL, NULL, NULL, NULL }
};

static OpsButton flip_button[] =
{
  { backwards_xpm, backwards_is_xpm, frame_manager_flip_backwards, "Flip Backwards", NULL, NULL, NULL, NULL, NULL, NULL },
  { forward_xpm, forward_is_xpm, frame_manager_flip_forward, "Flip Forward", NULL, NULL, NULL, NULL, NULL, NULL },
  { raise_xpm, raise_is_xpm, frame_manager_flip_raise, "Raise", NULL, NULL, NULL, NULL, NULL, NULL },
  { lower_xpm, lower_is_xpm, frame_manager_flip_lower, "Lower", NULL, NULL, NULL, NULL, NULL, NULL },
  { area_xpm, area_is_xpm, frame_manager_flip_area, "Set Area", NULL, NULL, NULL, NULL, NULL, NULL },
  { update_xpm, update_is_xpm, frame_manager_update_whole_area, "Update Whole Area", NULL, NULL, NULL, NULL, NULL, NULL },
  { delete_xpm, delete_is_xpm, frame_manager_flip_delete, "Delete", NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, 0, 0, NULL, NULL, NULL, NULL }
};


/* variables */
static frame_manager_t *frame_manager=NULL; 
static store_t store[STORE_MAX_NUM];
static GdkPixmap *istep_pixmap[3] = { NULL, NULL, NULL };
static GdkPixmap *iflip_pixmap[3] = { NULL, NULL, NULL };
static GdkPixmap *ibg_pixmap[3] = { NULL, NULL, NULL };
static int s_x, s_y, e_x, e_y;
static char onionskin=0;
static int NEW_OPTION=0;

void
frame_manager_free ()
{
  if (frame_manager)
    {
      g_free (frame_manager);

      frame_manager = NULL;
    }
}

void
frame_manager_create ()
{

  GDisplay *gdisplay;
  GtkWidget *vbox, *hbox, *lvbox, *rvbox, *label, *separator, *button_box, *listbox,
  *utilbox, *slider, *button, *abox;

  char tmp[256], *temp;

  if (!frame_manager)
    {
      gdisplay = gdisplay_active ();
      if (!gdisplay)
	{
	  frame_manager = NULL;
	  return; 
	}
      
      if (!(gdisplay->gimage->filename))
	{
	  frame_manager = NULL;
	  return; 
	}


      frame_manager = (frame_manager_t*) g_malloc (sizeof (frame_manager_t));
    
      if (frame_manager == NULL)
	return; 
     
      gdisplay->framemanager = 1; 
      
      frame_manager->gdisplay = gdisplay;
      frame_manager->store_option = NULL;

      s_x = 0;
      s_y = 0;
      e_x = gdisplay->disp_width;
      e_y = gdisplay->disp_height;
      
      /* the shell */
      frame_manager->shell = gtk_dialog_new ();
      gtk_window_set_wmclass (GTK_WINDOW (frame_manager->shell), "frame_manager", "Gimp");
      gtk_window_set_policy (GTK_WINDOW (frame_manager->shell), FALSE, FALSE, FALSE);
      sprintf (tmp, "Frame Manager for %s\0", gdisplay->gimage->filename); 
      gtk_window_set_title (GTK_WINDOW (frame_manager->shell), tmp);
      
      /* vbox */
      vbox = gtk_vbox_new (FALSE, 1);
      gtk_container_border_width (GTK_CONTAINER (vbox), 1);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (frame_manager->shell)->vbox), vbox, TRUE, TRUE, 0);
      gtk_widget_show (vbox);
      
      /* hbox */
      hbox = gtk_hbox_new (FALSE, 1);
      gtk_container_border_width (GTK_CONTAINER (hbox), 1);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);
    
      /* vbox */
      lvbox = gtk_vbox_new (FALSE, 1);
      gtk_container_border_width (GTK_CONTAINER (lvbox), 1);
      gtk_box_pack_start (GTK_BOX (hbox), lvbox, TRUE, TRUE, 0);
      gtk_widget_show (lvbox);
      
      rvbox = gtk_vbox_new (FALSE, 1);
      gtk_container_border_width (GTK_CONTAINER (rvbox), 1);
      gtk_box_pack_start (GTK_BOX (hbox), rvbox, TRUE, TRUE, 0);
      gtk_widget_show (rvbox);


      /*** left side ***/
      /* auto save */
      frame_manager->auto_save = gtk_toggle_button_new_with_label ("auto save");
      gtk_box_pack_start (GTK_BOX (lvbox), frame_manager->auto_save, FALSE, FALSE, 2);
      gtk_signal_connect (GTK_OBJECT (frame_manager->auto_save), "toggled",
	  (GtkSignalFunc) frame_manager_auto_save,
	  frame_manager);
      gtk_widget_show (frame_manager->auto_save);  
      frame_manager->auto_save_on = 0; 
      
      /* line */
      separator = gtk_hseparator_new (); 
      gtk_box_pack_start (GTK_BOX (lvbox), separator, FALSE, FALSE, 4);
      gtk_widget_show (separator); 
      
      /* step size */
      hbox = gtk_hbox_new (FALSE, 1);
      gtk_container_border_width (GTK_CONTAINER (hbox), 1);
      gtk_box_pack_start (GTK_BOX (lvbox), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);
      
      label = gtk_label_new ("Step Size"); 
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
      gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
      gtk_widget_show (label);

      /* arrow buttons */
      frame_manager->step_size = gtk_spin_button_new (gtk_adjustment_new (1, 1, 100, 1, 1, 0), 
	  1.0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), frame_manager->step_size, FALSE, FALSE, 2);
      /*gtk_widget_set_events (frame_manager->step_size, GDK_ENTER_NOTIFY_MASK|GDK_LEAVE_NOTIFY_MASK);
      gtk_signal_connect (GTK_OBJECT (frame_manager->step_size), "event",
	  (GtkSignalFunc) frame_manager_step_size,
	  frame_manager);
      */gtk_widget_show (frame_manager->step_size);

      
      /* step buttons */
      button_box = ops_button_box_new2 (frame_manager->shell, tool_tips, step_button, frame_manager);
      gtk_box_pack_start (GTK_BOX (lvbox), button_box, FALSE, FALSE, 2);
      gtk_widget_show (button_box);



      /* jump to */
      hbox = gtk_hbox_new (FALSE, 1);
      gtk_container_border_width (GTK_CONTAINER (hbox), 1);
      gtk_box_pack_start (GTK_BOX (lvbox), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);
     
      sprintf (&(tmp[0]), "%s.\0", frame_manager_get_name (gdisplay->gimage));
      label = gtk_label_new (tmp);     
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
      gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
      gtk_widget_show (label);

      temp = strdup (frame_manager_get_frame (gdisplay->gimage)); 
      frame_manager->jump_to = gtk_entry_new ();
      gtk_widget_set_usize (GTK_WIDGET (frame_manager->jump_to), 50, 0);
      gtk_entry_set_text (GTK_ENTRY (frame_manager->jump_to), temp); 
      gtk_box_pack_start (GTK_BOX (hbox), frame_manager->jump_to, FALSE, FALSE, 0);
      gtk_signal_connect (GTK_OBJECT (frame_manager->jump_to), "activate",
	  (GtkSignalFunc) frame_manager_jump_to,
	  frame_manager);
      gtk_widget_show (frame_manager->jump_to);
     
      sprintf (&(tmp[0]), ".%s\0", frame_manager_get_ext (gdisplay->gimage));
      label = gtk_label_new (tmp);     
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
      gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
      gtk_widget_show (label);
      
      /* line */
      separator = gtk_hseparator_new (); 
      gtk_box_pack_start (GTK_BOX (lvbox), separator, FALSE, FALSE, 4);
      gtk_widget_show (separator); 
      
      /* link */
      hbox = gtk_hbox_new (FALSE, 1);
      gtk_container_border_width (GTK_CONTAINER (hbox), 1);
      gtk_box_pack_start (GTK_BOX (lvbox), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);

      label = gtk_label_new ("Link:");
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 2);
      gtk_widget_show (label);

      frame_manager->link_option_menu = gtk_option_menu_new ();

      frame_manager->link_menu = frame_manager_create_menu (gdisplay->gimage);
      gtk_signal_connect_object (GTK_OBJECT (frame_manager->link_option_menu), 
	  "button_press_event",
	  GTK_SIGNAL_FUNC (frame_manager_link_menu), 
	  frame_manager);

      gtk_box_pack_start (GTK_BOX (hbox), 
	  frame_manager->link_option_menu, TRUE, TRUE, 2);
      gtk_widget_show (frame_manager->link_option_menu);
      gtk_option_menu_set_menu (GTK_OPTION_MENU (frame_manager->link_option_menu), 
	  frame_manager->link_menu);

      frame_manager->linked_display = NULL;

      /*** right side ***/
  
     /* store list */
     frame_manager->stores = NULL; 
     
      /* store area */
      listbox = gtk_scrolled_window_new (NULL, NULL);
      gtk_widget_set_usize (listbox, STORE_LIST_WIDTH, STORE_LIST_HEIGHT);
      gtk_box_pack_start (GTK_BOX (rvbox), listbox, TRUE, TRUE, 2);

      frame_manager->store_list = gtk_list_new ();
      gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (listbox), frame_manager->store_list);
      gtk_list_set_selection_mode (GTK_LIST (frame_manager->store_list), GTK_SELECTION_SINGLE);
      gtk_signal_connect (GTK_OBJECT (frame_manager->store_list), "event",
	  (GtkSignalFunc) frame_manager_store_list,
	  frame_manager);
      gtk_container_set_focus_vadjustment (GTK_CONTAINER (frame_manager->store_list),
	  gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (listbox)));
      GTK_WIDGET_UNSET_FLAGS (GTK_SCROLLED_WINDOW (listbox)->vscrollbar, GTK_CAN_FOCUS);

      gtk_widget_show (frame_manager->store_list);
      gtk_widget_show (listbox);

      /* line */
      separator = gtk_hseparator_new (); 
      gtk_box_pack_start (GTK_BOX (lvbox), separator, FALSE, FALSE, 4);
      gtk_widget_show (separator); 
      
      /* trans slider */
      utilbox = gtk_hbox_new (FALSE, 1);
      gtk_box_pack_start (GTK_BOX (rvbox), utilbox, FALSE, FALSE, 0);
      gtk_widget_show (utilbox);

      label = gtk_label_new ("Opacity:");
      gtk_box_pack_start (GTK_BOX (utilbox), label, FALSE, FALSE, 2);
      gtk_widget_show (label);

      frame_manager->trans_data = GTK_ADJUSTMENT (gtk_adjustment_new (1.0, 0.0, 1.0, .01, .01, 0.0));
      slider = gtk_hscale_new (frame_manager->trans_data);
      gtk_range_set_update_policy (GTK_RANGE (slider), GTK_UPDATE_CONTINUOUS);
      gtk_scale_set_value_pos (GTK_SCALE (slider), GTK_POS_RIGHT);
      gtk_box_pack_start (GTK_BOX (utilbox), slider, TRUE, TRUE, 0);
      gtk_signal_connect (GTK_OBJECT (frame_manager->trans_data), "value_changed",
	  (GtkSignalFunc) frame_manager_transparency,
	  frame_manager);
      gtk_widget_show (slider);
      
      /* line */
      separator = gtk_hseparator_new (); 
      gtk_box_pack_start (GTK_BOX (rvbox), separator, FALSE, FALSE, 4);
      gtk_widget_show (separator); 
     
      /* num of store */
      hbox = gtk_hbox_new (FALSE, 1);
      gtk_container_border_width (GTK_CONTAINER (hbox), 1);
      gtk_box_pack_start (GTK_BOX (rvbox), hbox, TRUE, TRUE, 0);
      gtk_widget_show (hbox);
      
      label = gtk_label_new ("Number of Store"); 
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
      gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
      gtk_widget_show (label);

      /* arrow buttons */
      frame_manager->store_size = gtk_spin_button_new (gtk_adjustment_new (1, 1, 100, 1, 1, 0), 
	  1.0, 0);
      gtk_box_pack_start (GTK_BOX (hbox), frame_manager->store_size, FALSE, FALSE, 2);

      gtk_signal_connect (GTK_OBJECT (frame_manager->store_size), "activate",
	  (GtkSignalFunc) frame_manager_store_size,
	  frame_manager);
      gtk_widget_show (frame_manager->store_size);

      button = gtk_button_new_with_label ("Apply");
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 2);
      gtk_signal_connect (GTK_OBJECT (button), "clicked",
	  (GtkSignalFunc) frame_manager_store_size,
	  frame_manager);
      gtk_widget_show (button);
      
      
      /* buttons */
      button_box = ops_button_box_new2 (frame_manager->shell, tool_tips, flip_button, frame_manager);
      gtk_box_pack_start (GTK_BOX (rvbox), button_box, FALSE, FALSE, 2);
      gtk_widget_show (button_box);
      
      /* line */
      separator = gtk_hseparator_new (); 
      gtk_box_pack_start (GTK_BOX (rvbox), separator, FALSE, FALSE, 4);
      gtk_widget_show (separator); 
      
      /* Close */
      action_items[0].user_data = frame_manager;
      build_action_area (GTK_DIALOG (frame_manager->shell), action_items, 1, 0);
      
      gtk_widget_show (frame_manager->shell);
      frame_manager_store_add (gdisplay->gimage, frame_manager, 0);
      
    }
  else
    {
      gdisplay = gdisplay_active ();
      if (!gdisplay)
	{
	  return; 
	}
      if (!GTK_WIDGET_VISIBLE (frame_manager->shell))
	{
	  if (gdisplay)
	    {
	      gtk_widget_show (frame_manager->shell);
	      frame_manager_store_add (gdisplay->gimage, frame_manager, 0); 
	    }
	}
      else
	{
	  if (gdisplay)
	    {
	      gdk_window_raise(frame_manager->shell->window);
	    }
	}
    }
}

static gint 
frame_manager_auto_save (GtkWidget *w, gpointer client_data)
{

  frame_manager_rm_onionskin ((frame_manager_t*)client_data); 

  return TRUE; 
}

static GImage *
frame_manager_find_gimage (char *whole, frame_manager_t *fm)
{
  GSList *list=NULL;
  store_t *item;

  list = fm->stores;
  while (list)
    {
      item = (store_t*) list->data;
      if (strcmp (item->gimage->filename, whole))
	return item->gimage;
    }
  return NULL; 
}

gint 
frame_manager_step_forward (GtkWidget *w, gpointer client_data)
{
  char *whole, *raw, autosave;
  int num=0, i=0, j=0, gi[7];
  GSList *list=NULL;
  store_t *item;
  GImage *gimages[7], *gimage; 

  frame_manager_t *fm;
  if (client_data)
    fm = (frame_manager_t*) client_data;
  else
    fm = frame_manager;

  if (!frame_manager_check (fm))
    {
      return 1; 
    }
  frame_manager_rm_onionskin (fm); 
  autosave = gtk_toggle_button_get_active((GtkToggleButton*)fm->auto_save);


  whole = (char*) malloc (sizeof(char)*255);
  raw = (char*) malloc (sizeof(char)*255);

  /*
     loop through the stores
     if step is checked
     exchange that store
   */
  list = fm->stores;
  if (gtk_toggle_button_get_active((GtkToggleButton*)fm->auto_save))
  {
    while (list)
    {
      item = (store_t*) list->data;
      gi[i] = 0; 
      gimages[i++] = item->gimage;
      list = g_slist_next (list);
    }
  }
  list = fm->stores;
  while (list)
    {
      item = (store_t*) list->data;
      if (item->step)
	{
	  frame_manager_next_filename (item->gimage, &whole, &raw, 1, fm);
	  gimage = NULL; 
	  for (j=0; j<i; j++)
	    {
	      if (!strcmp (gimages[j]->filename, whole))
		{
		  gimage = gimages[j];
		  gi[j] = 1; 
		  j = i;
		}
	    }
	  if (item->selected)
	    {
	      if (gimage)
		{
		  fm->gdisplay->gimage = gimage;
		  fm->gdisplay->ID = fm->gdisplay->gimage->ID;
		  frame_manager_store_add (fm->gdisplay->gimage, fm, num);
		  frame_manager_jump_to_update (
		      frame_manager_get_frame (fm->gdisplay->gimage), fm);
		  frame_manager_link_forward (fm);
		}
	      else
	      if (file_load (whole, raw, fm->gdisplay))
		{
		  fm->gdisplay->ID = fm->gdisplay->gimage->ID;
		  frame_manager_store_add (fm->gdisplay->gimage, fm, num);
		  frame_manager_jump_to_update (
		      frame_manager_get_frame (fm->gdisplay->gimage), fm);
		  frame_manager_link_forward (fm);
		}
	      else
		{
		  printf ("ERROR\n");
		}
	    }
	  else
	    {
	      if (gimage)
		{
		  frame_manager_store_add (gimage, fm, num);
		}
	      else
		if ((gimage = file_load_without_display (whole, raw, fm->gdisplay)))
		{
		  frame_manager_store_add (gimage, fm, num);
		}
	      else
		{
		  printf ("ERROR\n");
		}
	    }
	}
      num ++; 
      list = g_slist_next (list); 
    }
  for (j=0; j<i; j++)
    {
      if (!gi[j])
	{
	  file_save (gimages[j]->ID, gimages[j]->filename,
	      prune_filename (gimages[j]->filename));
	  gimage_delete (gimages[j]); 	
	}
    }
  free (whole);
  free (raw); 

  return TRUE; 

}

gint 
frame_manager_step_backwards (GtkWidget *w, gpointer client_data)
{
  char *whole, *raw, autosave;
  int num=0, i=0, j=0, gi[7];
  GSList *list=NULL;
  store_t *item;
  GImage *gimages[7], *gimage; 

  frame_manager_t *fm;
  if (client_data)
    fm = (frame_manager_t*) client_data;
  else
    fm = frame_manager;

  if (!frame_manager_check (fm))
    {
      return 1; 
    }
  frame_manager_rm_onionskin (fm); 
  autosave = gtk_toggle_button_get_active((GtkToggleButton*)fm->auto_save);


  whole = (char*) malloc (sizeof(char)*255);
  raw = (char*) malloc (sizeof(char)*255);

  /*
     loop through the stores
     if step is checked
     exchange that store
   */
  list = fm->stores;
  if (gtk_toggle_button_get_active((GtkToggleButton*)fm->auto_save))
  {
    while (list)
    {
      item = (store_t*) list->data;
      gi[i] = 0; 
      gimages[i++] = item->gimage;
      list = g_slist_next (list);
    }
  }
  list = fm->stores;
  while (list)
    {
      item = (store_t*) list->data;
      if (item->step)
	{
	  frame_manager_next_filename (item->gimage, &whole, &raw, 0, fm);
	  gimage = NULL; 
	  for (j=0; j<i; j++)
	    {
	      if (!strcmp (gimages[j]->filename, whole))
		{
		  gimage = gimages[j];
		  gi[j] = 1; 
		  j = i;
		}
	    }
	  if (item->selected)
	    {
	      if (gimage)
		{
		  fm->gdisplay->gimage = gimage;
		  fm->gdisplay->ID = fm->gdisplay->gimage->ID;
		  frame_manager_store_add (fm->gdisplay->gimage, fm, num);
		  frame_manager_jump_to_update (
		      frame_manager_get_frame (fm->gdisplay->gimage), fm);
		  frame_manager_link_forward (fm);
		}
	      else
	      if (file_load (whole, raw, fm->gdisplay))
		{
		  fm->gdisplay->ID = fm->gdisplay->gimage->ID;
		  frame_manager_store_add (fm->gdisplay->gimage, fm, num);
		  frame_manager_jump_to_update (
		      frame_manager_get_frame (fm->gdisplay->gimage), fm);
		  frame_manager_link_forward (fm);
		}
	      else
		{
		  printf ("ERROR\n");
		}
	    }
	  else
	    {
	      if (gimage)
		{
		  frame_manager_store_add (gimage, fm, num);
		}
	      else
		if ((gimage = file_load_without_display (whole, raw, fm->gdisplay)))
		{
		  frame_manager_store_add (gimage, fm, num);
		}
	      else
		{
		  printf ("ERROR\n");
		}
	    }
	}
      num ++; 
      list = g_slist_next (list); 
    }
  for (j=0; j<i; j++)
    {
      if (!gi[j])
	{
	  file_save (gimages[j]->ID, gimages[j]->filename,
	      prune_filename (gimages[j]->filename));
	  gimage_delete (gimages[j]); 	
	}
    }
  free (whole);
  free (raw); 

  return TRUE; 

}

static gint 
frame_manager_step_size (GtkWidget *w, gpointer client_data)
{
  frame_manager_rm_onionskin ((frame_manager_t*)client_data); 
  return TRUE; 
  
}

static gint 
frame_manager_jump_to (GtkWidget *w, gpointer client_data)
{
  char *whole, *raw;
  int num=1;
frame_manager_t *fm = (frame_manager_t*) client_data;

  if (!frame_manager_check (fm))
    return 1; 
  frame_manager_rm_onionskin (fm); 

  

  whole = (char*) malloc (sizeof(char)*255);
  raw = (char*) malloc (sizeof(char)*255);

  frame_manager_this_filename (fm->gdisplay->gimage, &whole, &raw, 
      gtk_entry_get_text (fm->jump_to), fm);
  if (file_load (whole, raw, fm->gdisplay))
    {
      fm->gdisplay->ID = fm->gdisplay->gimage->ID;
      frame_manager_store_add (fm->gdisplay->gimage, fm, 0);  

    }
  else
    {
      printf ("ERROR\n");
    }
  free (whole);
  free (raw); 

  return TRUE; 
}

static void
frame_manager_jump_to_update (char *i, frame_manager_t *fm)
{
  gtk_entry_set_text (fm->jump_to, i); 
}


gint 
frame_manager_flip_forward (GtkWidget *w, gpointer client_data)
{

  GSList *list=NULL;
  int flag=0;
  GList *item_list=NULL; 
  store_t *item, *first=NULL, *cur;
  int num;

  frame_manager_t *fm;
  if (client_data)
    fm = (frame_manager_t*) client_data; 
  else
    fm = frame_manager;
  if (!frame_manager_check (fm))
    return; 

  
  if (onionskin)
    {
      item_list =  (GList*) GTK_LIST(fm->store_list)->selection;
      num = gtk_list_child_position (GTK_LIST (fm->store_list), item_list->data);
      item = (store_t*) g_slist_nth (fm->stores, num)->data;
      item->gimage->active_layer->opacity = item->gimage->active_layer->opacity ? 0 : 1;
      drawable_update (GIMP_DRAWABLE(item->gimage->active_layer),
	  s_x, s_y, e_x, e_y);
      gdisplays_flush ();
      return 1;
    }
  
  list = fm->stores; 

  /* find the next active item */ 

  while (list)
    {
      item = (store_t*) list->data;
      if (!first && item->flip)
	{
	  first = item;
	}
      if (flag && item->flip)
	{
	  item->selected = 1;
	  frame_manager_store_load (item, fm); 
	  frame_manager_jump_to_update (frame_manager_get_frame (item->gimage), fm); 
	  return; 
	}
      if (item->selected)
	{
	  item->selected = 0;
	  cur = item;  
	  flag = 1;
	}
      list = g_slist_next (list);

    }
  if (first)
    {
      first->selected = 1;
      frame_manager_store_load (first, fm);
      frame_manager_jump_to_update (frame_manager_get_frame (first->gimage), fm); 
    }
  else
    {
      cur->selected = 1;
    }

  return TRUE;
}

gint 
frame_manager_flip_backwards (GtkWidget *w, gpointer client_data)
{
  GSList *list=NULL;
  int flag=0, num;
  store_t *item, *prev=NULL, *next=NULL, *cur;
  frame_manager_t *fm;
  GList *item_list=NULL;
  
  if(client_data)
    fm = (frame_manager_t*) client_data; 
  else
    fm = frame_manager; 

  if (!frame_manager_check (fm))
    return; 

  if (onionskin)
    {
      item_list =  (GList*) GTK_LIST(fm->store_list)->selection;
      num = gtk_list_child_position (GTK_LIST (fm->store_list), item_list->data);
      item = (store_t*) g_slist_nth (fm->stores, num)->data;
      item->gimage->active_layer->opacity = item->gimage->active_layer->opacity ? 0 : 1;
      drawable_update (GIMP_DRAWABLE(item->gimage->active_layer),
	  s_x, s_y, e_x, e_y);
      gdisplays_flush ();
      return 1;
    }
  list = fm->stores; 

  /* find the next active item */ 
  while (list)
    {
      item = (store_t*) list->data;
      if (flag  && item->flip)
	{
	  next = item;
	}
      if (item->selected)
	{
	  item->selected = 0;
	  cur = item; 
	  flag = 1;
	}
      if (!flag && item->flip)
	{
	  prev = item;
	}
      list = g_slist_next (list);
    }
  if (prev)
    {
      prev->selected = 1;
      frame_manager_store_load (prev, fm);
      frame_manager_jump_to_update (frame_manager_get_frame (prev->gimage), fm); 
    }
  else if (next)
    {
      next->selected = 1;
      frame_manager_store_load (next, fm); 
      frame_manager_jump_to_update (frame_manager_get_frame (next->gimage), fm); 
    }
  else
    {
      cur->selected = 1;
    }
  return TRUE;
}

gint 
frame_manager_flip_raise (GtkWidget *w, gpointer client_data)
{
  int i, j;
  store_t *item;
  GList *item_list=NULL, *list=NULL;
  frame_manager_t *fm;
  if(client_data)
    fm = (frame_manager_t*) client_data; 
  else
    fm = frame_manager; 
  item_list =  (GList*) GTK_LIST(fm->store_list)->selection;
  if (!frame_manager_check (fm))
    return FALSE; 
  frame_manager_rm_onionskin (fm); 

  if (item_list && g_slist_length (fm->stores) > 1)
    {
      i = gtk_list_child_position (GTK_LIST (fm->store_list), item_list->data);
      j = i - 1 > 0 ? i - 1 : 0;
      item = (store_t*) g_slist_nth (fm->stores, i)->data;
      list = g_list_append (list, item->list_item);
      gtk_list_remove_items (GTK_LIST (fm->store_list), list);
      fm->stores = g_slist_remove(fm->stores, item);
      gtk_list_insert_items (GTK_LIST (fm->store_list), list, j);
      fm->stores = g_slist_insert(fm->stores, item, j);
      gtk_list_select_item (GTK_LIST (fm->store_list), j); 
    }
  return TRUE; 
}

gint 
frame_manager_flip_lower (GtkWidget *w, gpointer client_data)
{
  int i, j;
  store_t *item;
  GList *item_list=NULL, *list=NULL;
  frame_manager_t *fm;
  if(client_data)
    fm = (frame_manager_t*) client_data; 
  else
    fm = frame_manager; 
  if (!frame_manager_check (fm))
    return FALSE; 
  frame_manager_rm_onionskin (fm); 
  item_list =  (GList*) GTK_LIST(fm->store_list)->selection;

  if (item_list && g_slist_length (fm->stores) > 1)
    {
      i = gtk_list_child_position (GTK_LIST (fm->store_list), item_list->data);
      j = i + 1 < g_slist_length (fm->stores) ? i + 1 : i;  
      item = (store_t*) g_slist_nth (fm->stores, i)->data;
      list = g_list_append (list, item->list_item);
      gtk_list_remove_items (GTK_LIST (fm->store_list), list);
      fm->stores = g_slist_remove(fm->stores, item);
      gtk_list_insert_items (GTK_LIST (fm->store_list), list, j);
      fm->stores = g_slist_insert(fm->stores, item, j);
      gtk_list_select_item (GTK_LIST (fm->store_list), j); 
    }
  return TRUE;

}

static gint 
frame_manager_flip_area (GtkWidget *w, gpointer client_data)
{
  RectSelect * rect_sel;
  frame_manager_t *fm = (frame_manager_t*) client_data; 
  GDisplay *gdisplay = fm->gdisplay;
  if (!frame_manager_check (fm))
    return FALSE; 

  if (active_tool->type == RECT_SELECT)
    {
      rect_sel = (RectSelect *) active_tool->private;
      s_x = MIN (rect_sel->x, rect_sel->x + rect_sel->w);
      s_y = MIN (rect_sel->y, rect_sel->y + rect_sel->h);
      e_x = MAX (rect_sel->x, rect_sel->x + rect_sel->w) - s_x;
      e_y = MAX (rect_sel->y, rect_sel->y + rect_sel->h) - s_y;

      if (!s_x && !s_y && !e_x && !e_y)
	{
	  s_x = 0;
	  s_y = 0;
	  e_x = gdisplay->disp_width;
	  e_y = gdisplay->disp_height;

	}  
    }
  else
    {
      s_x = 0;
      s_y = 0;
      e_x = gdisplay->disp_width;
      e_y = gdisplay->disp_height;  
    }

  return TRUE;
}

static gint 
frame_manager_update_whole_area (GtkWidget *w, gpointer client_data)
{
  RectSelect * rect_sel;
  frame_manager_t *fm = (frame_manager_t*) client_data; 
  GDisplay *gdisplay = fm->gdisplay;
  if (!frame_manager_check (fm))
    return FALSE; 

  gdisplay_add_update_area (fm->gdisplay, 0, 0, 
      fm->gdisplay->disp_width, fm->gdisplay->disp_height);
  gdisplays_flush ();

  return TRUE;
}


static gint 
frame_manager_flip_delete (GtkWidget *w, gpointer client_data)
{
  int index;
  store_t *item;
  GList *item_list=NULL, *list=NULL;
frame_manager_t *fm = (frame_manager_t*) client_data;

  if (!frame_manager_check (fm))
    return FALSE; 
  frame_manager_rm_onionskin (fm); 
  item_list =  (GList*) GTK_LIST(fm->store_list)->selection;

  if (item_list && g_slist_length (fm->stores) > 1)
    {
      index = gtk_list_child_position (GTK_LIST (fm->store_list), item_list->data);
      item = (store_t*) g_slist_nth (fm->stores, index)->data;
      list = g_list_append (list, item->list_item); 
      gtk_list_remove_items (GTK_LIST (fm->store_list), list);
      frame_manager_store_delete (item, fm, 1);
      fm->stores = g_slist_remove(fm->stores, item); 
      g_free (item);
      item = NULL; 
      if (fm->stores)
	{
	  item = (store_t*) g_slist_nth (fm->stores, 0)->data;
	  if (item)
	    frame_manager_store_load (item, fm);
	}
    }
  return TRUE;

}


gint 
frame_manager_close (GtkWidget *w, gpointer client_data)
{
  frame_manager_rm_onionskin ((frame_manager_t*)client_data); 
  frame_manager_store_clear ((frame_manager_t*)client_data); 
  
  if (GTK_WIDGET_VISIBLE (frame_manager->shell))
    gtk_widget_hide (frame_manager->shell);

  return TRUE; 
}

static void
frame_manager_update_menu ()
{

  frame_manager->link_menu = frame_manager_create_menu (frame_manager->gdisplay->gimage);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (frame_manager->link_option_menu), 
      frame_manager->link_menu);
}

static gint 
frame_manager_change_menu (GtkWidget *w, gpointer client_data)
{
  if (!frame_manager_check (frame_manager))
    return; 
  frame_manager_rm_onionskin (frame_manager);

  if (((long)client_data)) 
    frame_manager->linked_display = gdisplay_find_display (((long)client_data)); 
  else
    frame_manager->linked_display = NULL; 

  return 1; 
}

static GtkWidget* 
frame_manager_create_menu (GImage *gimage)
{
  extern GSList *display_list;
  GSList *list = display_list;
  GImage *img; 
  GtkMenuItem *menu_item;
  GtkWidget *menu;
  int num_items=0;
  char *name, label[255]; 

  if (!frame_manager_check (frame_manager))
    return; 
  menu = gtk_menu_new ();

  if (!num_items)
    {
      menu_item = gtk_menu_item_new_with_label ("NONE");
      gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
	  (GtkSignalFunc) frame_manager_change_menu,
	  (gpointer) (0));
      gtk_container_add (GTK_CONTAINER (menu), menu_item);
      gtk_widget_show (menu_item);
	
    }
  
  while (list)
    {
      img = ((GDisplay*) list->data)->gimage;
 
      if (img != gimage)
	{
	  name = prune_filename (gimage_filename (img));
	  sprintf (label, "%s-%d", name, img->ID);
	  menu_item = gtk_menu_item_new_with_label (label); 

	  gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
	      (GtkSignalFunc) frame_manager_change_menu,
	      (gpointer) ((long)img->ID));
	  gtk_container_add (GTK_CONTAINER (menu), menu_item);
	  gtk_widget_show (menu_item);

	  num_items ++;
	}
      
      list = g_slist_next (list); 
    }
 

  return menu; 
}

static int 
frame_manager_link_menu (GtkWidget *w, gpointer client_data)
{
  if (!frame_manager_check (frame_manager))
    return; 
  frame_manager_rm_onionskin (frame_manager); 

  frame_manager_update_menu ();
  
  return 1; 
}


static gint 
frame_manager_store_list (GtkWidget *w, GdkEvent *e)
{
/*
  if (!frame_manager_check (frame_manager))
    return 0; 
*/
  return 0; 
}

static void
frame_manager_store_add (GImage *gimage, frame_manager_t *fm, int num)
{
  store_t *item;
  GList *item_list = NULL; 
  char frame[20];
  char selected, flip, bg;

  if (!gimage || num<0 /*|| num >= gtk_spin_button_get_value_as_int (fm->store_size)*/)
    return;


  if (!g_slist_length (fm->stores))
    {
      item = frame_manager_store_new (gimage, 1);
      item->selected = 1;
      fm->stores = g_slist_insert (fm->stores, item, num);
      gtk_list_insert_items (GTK_LIST (fm->store_list),
	  g_list_append(item_list, item->list_item), num);
      strcpy (frame, frame_manager_get_frame (gimage));
      gtk_entry_set_text (GTK_ENTRY (fm->jump_to), frame);
      frame_manager_store_unselect (fm);
      gtk_list_select_item (GTK_LIST (fm->store_list), num);
    }
  else
    if (g_slist_length (fm->stores) <= num)
      {
	/* just add the store to the end */
	item = frame_manager_store_new (gimage, 0);
	fm->stores = g_slist_append (fm->stores, item); 
	gtk_list_insert_items (GTK_LIST (fm->store_list),
	    g_list_append(item_list, item->list_item), num);

	
      }
    else 
      {
	/* rm the item and then add new item in place */
	item = (store_t*) g_slist_nth (fm->stores, num)->data;
	selected = item->selected; 
	flip = item->flip; 
	bg = item->bg; 
	item_list = g_list_append (item_list, item->list_item);
	gtk_list_remove_items (GTK_LIST (fm->store_list), item_list);
	frame_manager_store_delete (item, fm, 0);
	fm->stores = g_slist_remove(fm->stores, item);
	g_free (item);
	/* if item selected, keep selected */
	item_list = NULL; 	
	item = frame_manager_store_new (gimage, 1);
	item->selected = selected; 
	item->flip = flip; 
	item->bg = bg; 
	fm->stores = g_slist_insert (fm->stores, item, num);
	gtk_list_insert_items (GTK_LIST (fm->store_list),
	    g_list_append(item_list, item->list_item), num);


	if (selected)
	  {
	    strcpy (frame, frame_manager_get_frame (gimage));
	    gtk_entry_set_text (GTK_ENTRY (fm->jump_to), frame);
	    frame_manager_store_unselect (fm);
	    gtk_list_select_item (GTK_LIST (fm->store_list), num);
	  }
      }
}

static store_t* 
frame_manager_store_new (GImage *gimage, char active)
{

  store_t *item; 
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *alignment;
  GtkWidget *list_item; 

  item = (store_t*) malloc (sizeof (store_t));
  item->list_item = list_item = gtk_list_item_new (); 
  item->gimage = gimage;
  if (active)
    {
      item->step = 1;
      item->flip = 1;
      item->bg = 0;
      item->selected = 0;
      item->active = 1;
    }
  else
    {
      item->step = 0;
      item->flip = 0;
      item->bg = 0;
      item->selected = 0;
      item->active = 0;
    }
  
  gtk_object_set_user_data (GTK_OBJECT (list_item), item);

  gtk_signal_connect (GTK_OBJECT (list_item), "select",
      (GtkSignalFunc) frame_manager_store_select_update,
      item);
  gtk_signal_connect (GTK_OBJECT (list_item), "deselect",
      (GtkSignalFunc) frame_manager_store_deselect_update,
      item);

  vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER (list_item), vbox);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 1);
  gtk_widget_show (hbox);

  /* Create the visibility toggle button */
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), alignment, FALSE, TRUE, 2);
  gtk_widget_show (alignment);
  
  item->istep = gtk_drawing_area_new ();
  gtk_drawing_area_size (GTK_DRAWING_AREA (item->istep), istep_width, istep_height);
  gtk_widget_set_events (item->istep, BUTTON_EVENT_MASK);
  gtk_signal_connect (GTK_OBJECT (item->istep), "event",
      (GtkSignalFunc) frame_manager_store_step_button,
      item);
  gtk_object_set_user_data (GTK_OBJECT (item->istep), item);
  gtk_container_add (GTK_CONTAINER (alignment), item->istep);
  gtk_widget_show (item->istep);
  
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), alignment, FALSE, TRUE, 2);
  gtk_widget_show (alignment);
  
  item->iflip = gtk_drawing_area_new ();
  gtk_drawing_area_size (GTK_DRAWING_AREA (item->iflip), iflip_width, iflip_height);
  gtk_widget_set_events (item->iflip, BUTTON_EVENT_MASK);
  gtk_signal_connect (GTK_OBJECT (item->iflip), "event",
      (GtkSignalFunc) frame_manager_store_flip_button,
      item);
  gtk_object_set_user_data (GTK_OBJECT (item->iflip), item);
  gtk_container_add (GTK_CONTAINER (alignment), item->iflip);
  gtk_widget_show (item->iflip);
  
  alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (hbox), alignment, FALSE, TRUE, 2);
  gtk_widget_show (alignment);
  
  item->ibg = gtk_drawing_area_new ();
  gtk_drawing_area_size (GTK_DRAWING_AREA (item->ibg), ibg_width, ibg_height);
  gtk_widget_set_events (item->ibg, BUTTON_EVENT_MASK);
  gtk_signal_connect (GTK_OBJECT (item->ibg), "event",
      (GtkSignalFunc) frame_manager_store_bg_button,
      item);
  gtk_object_set_user_data (GTK_OBJECT (item->ibg), item);
  gtk_container_add (GTK_CONTAINER (alignment), item->ibg);
  gtk_widget_show (item->ibg);
  
  /*  the channel name label */
  if (active)
    item->label = gtk_label_new (prune_filename (item->gimage->filename)); 
  else
    item->label = gtk_label_new ("nothing");

  gtk_box_pack_start (GTK_BOX (hbox), item->label, FALSE, FALSE, 2);
  gtk_widget_show (item->label);

  gtk_widget_show (list_item);
  gtk_widget_ref (item->list_item);

  return item;
  
}

static void
frame_manager_store_delete (store_t *store, frame_manager_t *fm, char flag)
{
  /* get rid of gimage */
  if (gtk_toggle_button_get_active((GtkToggleButton*)fm->auto_save))
    {
      /* save image */
      if (flag)
      file_save (store->gimage->ID, store->gimage->filename, 
	  prune_filename (store->gimage->filename));
      if (active_tool->type == CLONE)
	clone_delete_image (store->gimage);
      if (flag)
	gimage_delete (store->gimage);
    }
  else
    {
      if (active_tool->type == CLONE)
	clone_delete_image (store->gimage);
      gimage_delete (store->gimage);  
    }
}

static void 
frame_manager_save (GImage *gimage, frame_manager_t *fm)
{
  frame_manager_rm_onionskin (fm); 
  if (gtk_toggle_button_get_active((GtkToggleButton*)fm->auto_save))
    {
      /* save image */
      file_save (gimage->ID, gimage->filename, prune_filename (gimage->filename));
    }

}


static void
frame_manager_rm_store (frame_manager_t *fm)
{
  if (!frame_manager_check (fm))
    return; 
  frame_manager_rm_onionskin (fm); 
}

static void 
frame_manager_store_unselect (frame_manager_t *fm)
{

  store_t *item;  
  GSList *list = fm->stores;

  frame_manager_rm_onionskin (fm); 
  while (list)
    {
      item = (store_t*) list->data;
      if (item->selected)
	{
	  item->selected = 0;
	  gtk_list_unselect_item (GTK_LIST (fm->store_list), 
	      g_slist_index (fm->stores, item)); 
	}
      list = g_slist_next (list);
    }
  
}

static void
frame_manager_store_load (store_t *item, frame_manager_t *fm)
{
  if (!frame_manager_check (fm))
    return; 
  
  item->selected = 1;
  gtk_list_select_item (GTK_LIST (fm->store_list), 
      g_slist_index (fm->stores, item));
  fm->gdisplay->gimage = item->gimage;
  fm->gdisplay->ID = item->gimage->ID; 
  gdisplays_update_title (fm->gdisplay->gimage->ID);
  gdisplay_add_update_area (fm->gdisplay, s_x, s_y, e_x, e_y);
  gdisplays_flush ();

  if (active_tool->type == CLONE)
    clone_flip_image (); 
}

static void
frame_manager_store_add_stores (frame_manager_t *fm)
{
  int num, cur_num, i, f, l;
  GList *item_list=NULL, *list=NULL;
  store_t *item;
  char *whole, *raw, tmp[256], tmp2[256], *frame;
  GImage *gimage;

  cur_num = g_slist_length (fm->stores);
  item_list =  (GList*) GTK_LIST(fm->store_list)->selection;
  if(!item_list)
    return;
  
  i = gtk_list_child_position (GTK_LIST (fm->store_list), item_list->data);
  item = (store_t*) g_slist_nth (fm->stores, i)->data;

 switch (NEW_OPTION)
   {
#if 0
   case 0: /* do nothing */
    num = gtk_spin_button_get_value_as_int (fm->store_size) - g_slist_length (fm->stores); 
    for (i=0; i<num; i++)
      {
	frame_manager_store_add (item->gimage, fm, cur_num+i); 
      }
    break;
#endif
   case 0: /* add prev */
    num = gtk_spin_button_get_value_as_int (fm->store_size) - g_slist_length (fm->stores); 
    frame = strdup (frame_manager_get_frame (item->gimage));
    l = strlen (frame);
    f = atoi (frame);  
    whole = (char*) malloc (sizeof(char)*255);
    raw = (char*) malloc (sizeof(char)*255);

    for (i=0; i<num; i++)
      {
	sprintf (tmp, "%d\0", f-(i+1));
	while (strlen (tmp) != l)
	  {
	    sprintf (tmp2, "0%s\0", tmp);
	    strcpy (tmp, tmp2); 
	  }
	frame_manager_this_filename (item->gimage, &whole, &raw, 
	    tmp, fm);

	/* find the current */
	if ((gimage=file_load_without_display (whole, raw, fm->gdisplay)))
	  {
	    item = frame_manager_store_new (gimage, 1);
	    item->selected = 0;
	    fm->stores = g_slist_append (fm->stores, item);
	    gtk_list_append_items (GTK_LIST (fm->store_list),
		g_list_append(list, item->list_item));

	    list=NULL; 
	  }
      }
    break;
   case 1: /* add next */
    num = gtk_spin_button_get_value_as_int (fm->store_size) - g_slist_length (fm->stores); 
    frame = strdup (frame_manager_get_frame (item->gimage));
    l = strlen (frame);
    f = atoi (frame);  
    whole = (char*) malloc (sizeof(char)*255);
    raw = (char*) malloc (sizeof(char)*255);

    for (i=0; i<num; i++)
      {
	sprintf (tmp, "%d\0", f+i+1);
	while (strlen (tmp) != l)
	  {
	    sprintf (tmp2, "0%s\0", tmp);
	    strcpy (tmp, tmp2);
	   printf ("%s\n", tmp);  
	  }
	frame_manager_this_filename (item->gimage, &whole, &raw, 
	    tmp, fm);

	/* find the current */
	if ((gimage=file_load_without_display (whole, raw, fm->gdisplay)))
	  {
	    item = frame_manager_store_new (gimage, 1);
	    item->selected = 0;
	    fm->stores = g_slist_append (fm->stores, item);
	    gtk_list_append_items (GTK_LIST (fm->store_list),
		g_list_append(list, item->list_item));

	    list=NULL; 
	  }
      }
    break;
   case 2: /* cp */
    num = gtk_spin_button_get_value_as_int (fm->store_size) - g_slist_length (fm->stores);

    gimage = item->gimage;

    for (i=0; i<num; i++)
      {
	item = frame_manager_store_new (gimage_copy (gimage), 1);
	item->selected = 0;
	fm->stores = g_slist_append (fm->stores, item);
	gtk_list_append_items (GTK_LIST (fm->store_list),
	                    g_list_append(list, item->list_item));
	list=NULL;
      }
    break;
   default:
    printf ("ERROR: \n");
    break;
   } 
}

static void
frame_manager_store_option_close (GtkWidget *w, gpointer client_data)
{
  frame_manager_t *fm = (frame_manager_t*) client_data;

  if (fm)
    {
      if (GTK_WIDGET_VISIBLE (fm->store_option))
	{
	  gtk_widget_hide (fm->store_option); 
	
	  frame_manager_store_add_stores (fm); 
	}
    }
}

char *options[3] =
{
 /* "Do nothing",*/
  "Add before",
  "Add after",
  "Copy cur"
};

static ActionAreaItem offset_action_items[] =
{
  { "Close",         frame_manager_store_option_close, NULL, NULL },
};

static void 
frame_manager_store_option_option (GtkWidget *w, gpointer client_data)
{
  NEW_OPTION = (int) client_data; 
}

static void
frame_manager_store_new_options (frame_manager_t *fm)
{

  int i;
  GtkWidget *vbox, *radio_button;

  GSList *group = NULL;

  if (!fm->store_option)
    { 
      fm->store_option = gtk_dialog_new ();
      gtk_window_set_wmclass (GTK_WINDOW (fm->store_option), "Store Option", "Gimp");
      gtk_window_set_policy (GTK_WINDOW (fm->store_option), FALSE, FALSE, FALSE);
      gtk_window_set_title (GTK_WINDOW (fm->store_option), "New Store Option");

      vbox = gtk_vbox_new (FALSE, 1);
      gtk_container_border_width (GTK_CONTAINER (vbox), 1);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (fm->store_option)->vbox),
	  vbox, TRUE, TRUE, 0);

      for (i = 0; i < 3; i++)
	{
	  radio_button = gtk_radio_button_new_with_label (group, options[i]);
	  group = gtk_radio_button_group (GTK_RADIO_BUTTON (radio_button));
	  gtk_signal_connect (GTK_OBJECT (radio_button), "toggled",
	      (GtkSignalFunc) frame_manager_store_option_option,
	      (void *)((long) i));
	  gtk_box_pack_start (GTK_BOX (vbox), radio_button, FALSE, FALSE, 0);
	  gtk_widget_show (radio_button);
	}

      offset_action_items[0].user_data = fm;
      build_action_area (GTK_DIALOG (fm->store_option), offset_action_items, 1, 0);

      gtk_widget_show (vbox);
      gtk_widget_show (fm->store_option); 
    }
  else
    if (!GTK_WIDGET_VISIBLE (fm->store_option))
      {
	gtk_widget_show (fm->store_option);
      }
}

static gint 
frame_manager_store_size (GtkWidget *w, gpointer client_data)
{
  frame_manager_t *fm;
  int num, cur_num;

  fm = (frame_manager_t*)client_data; 

  if (!frame_manager_check (fm))
    return; 
  frame_manager_rm_onionskin (fm); 


  num = gtk_spin_button_get_value_as_int (fm->store_size); 
  cur_num = g_slist_length (fm->stores);
 
  if (num > cur_num)
    {
      frame_manager_store_new_options (fm); 
    }
  else if (num < cur_num)
    {
      printf ("rm store\n"); 
    }
  
  return TRUE; 
}


static gint 
frame_manager_store_select_update (GtkWidget *w, gpointer client_pointer)
{
  store_t *item = (store_t*) client_pointer;
  
  frame_manager_rm_onionskin (frame_manager);

  if (item->active)
    frame_manager_store_load (item, frame_manager); 
  else 
    {
      file_open_callback (NULL, NULL);
    } 
  return 1; 
}

static gint 
frame_manager_store_deselect_update (GtkWidget *w, gpointer client_pointer)
{

  store_t *item = (store_t*) client_pointer;
  frame_manager_rm_onionskin (frame_manager);
  item->selected = 0;
  return 1; 
}

static gint 
frame_manager_store_step_button (GtkWidget *w, GdkEvent *event)
{
  store_t *item;
  
  item = (store_t*) gtk_object_get_user_data (GTK_OBJECT (w));

  switch (event->type)
    {
    case GDK_EXPOSE:
      frame_manager_step_redraw (item);
      return 1;

    case GDK_BUTTON_PRESS:
      item->step = item->active ? item->step ? 0 : 1 : 0;
      frame_manager_step_redraw (item);
      return 1;
    default:
      break;
    }
  return 0;
}

static gint 
frame_manager_store_flip_button (GtkWidget *w, GdkEvent *event)
{
  store_t *item;
  
  item = (store_t*) gtk_object_get_user_data (GTK_OBJECT (w));

  switch (event->type)
    {
    case GDK_EXPOSE:
      frame_manager_flip_redraw (item);
      return 1;

    case GDK_BUTTON_PRESS:
      item->flip = item->active ? item->flip ? 0 : 1 :0;
      frame_manager_flip_redraw (item);
      return 1;
    default:
      break;
    }
  return 0;
}

static gint 
frame_manager_store_bg_button (GtkWidget *w, GdkEvent *event)
{
  store_t *item, *i;
  GList *list=frame_manager->stores;
  
  item = (store_t*) gtk_object_get_user_data (GTK_OBJECT (w));

  switch (event->type)
    {
    case GDK_EXPOSE:
      frame_manager_bg_redraw (item);
      return 1;

    case GDK_BUTTON_PRESS:
      if (!item->bg)
	{
	  while (list)
	    {
	      i = (store_t*) list->data; 
	      if (i->bg)
		{
		  i->bg = 0;
		  frame_manager_bg_redraw (i);
		}	   
	      list = g_slist_next (list);   
	    }
	}
      item->bg = item->active ? item->bg ? 0 : 1 : 0;
      frame_manager_bg_redraw (item);
      return 1;
    default:
      break;
    }
  return 0;
}

static void 
frame_manager_step_redraw (store_t *item)
{
  GdkPixmap *pixmap;
  GdkColor *color;
  GtkStateType state;
  int visible;

  state = item->list_item->state;

  if (GTK_WIDGET_IS_SENSITIVE (item->list_item))
    {
      if (state == GTK_STATE_SELECTED)
	color = &item->istep->style->bg[GTK_STATE_SELECTED];
      else
	color = &item->istep->style->white;
    }
  else
    color = &item->istep->style->bg[GTK_STATE_INSENSITIVE];

  gdk_window_set_background (item->istep->window, color);


  if (item->step)
    {
      if (!istep_pixmap[NORMAL])
	{
	  istep_pixmap[NORMAL] =
	    gdk_pixmap_create_from_data (item->istep->window,
		(gchar*) istep_bits, istep_width, istep_height, -1,
		&item->istep->style->fg[GTK_STATE_NORMAL],
		&item->istep->style->white);
	  istep_pixmap[SELECTED] =
	    gdk_pixmap_create_from_data (item->istep->window,
		(gchar*) istep_bits, istep_width, istep_height, -1,
		&item->istep->style->fg[GTK_STATE_SELECTED],
		&item->istep->style->bg[GTK_STATE_SELECTED]);
	  istep_pixmap[INSENSITIVE] =
	    gdk_pixmap_create_from_data (item->istep->window,
		(gchar*) istep_bits, istep_width, istep_height, -1,
		&item->istep->style->fg[GTK_STATE_INSENSITIVE],
		&item->istep->style->bg[GTK_STATE_INSENSITIVE]);
	}

      if (GTK_WIDGET_IS_SENSITIVE (item->list_item))
	{
	  if (state == GTK_STATE_SELECTED)
	    pixmap = istep_pixmap[SELECTED];
	  else
	    pixmap = istep_pixmap[NORMAL];
	}
      else
	pixmap = istep_pixmap[INSENSITIVE];


      gdk_draw_pixmap (item->istep->window,
	  item->istep->style->black_gc,
	  pixmap, 0, 0, 0, 0, istep_width, istep_height);
    }
  else
    {
      gdk_window_clear (item->istep->window);
    }
}

static void 
frame_manager_flip_redraw (store_t *item)
{
  GdkPixmap *pixmap;
  GdkColor *color;
  GtkStateType state;
  int visible;

  state = item->list_item->state;

  if (GTK_WIDGET_IS_SENSITIVE (item->list_item))
    {
      if (state == GTK_STATE_SELECTED)
	color = &item->iflip->style->bg[GTK_STATE_SELECTED];
      else
	color = &item->iflip->style->white;
    }
  else
    color = &item->iflip->style->bg[GTK_STATE_INSENSITIVE];

  gdk_window_set_background (item->iflip->window, color);


  if (item->flip)
    {
      if (!iflip_pixmap[NORMAL])
	{
	  iflip_pixmap[NORMAL] =
	    gdk_pixmap_create_from_data (item->iflip->window,
		(gchar*) iflip_bits, iflip_width, iflip_height, -1,
		&item->iflip->style->fg[GTK_STATE_NORMAL], 
		&item->iflip->style->white);
	  iflip_pixmap[SELECTED] =
	    gdk_pixmap_create_from_data (item->iflip->window,
		(gchar*) iflip_bits, iflip_width, iflip_height, -1,
		&item->iflip->style->fg[GTK_STATE_SELECTED],
		&item->iflip->style->bg[GTK_STATE_SELECTED]);
	  iflip_pixmap[INSENSITIVE] =
	    gdk_pixmap_create_from_data (item->iflip->window,
		(gchar*) iflip_bits, iflip_width, iflip_height, -1,
		&item->iflip->style->fg[GTK_STATE_INSENSITIVE], 
		&item->iflip->style->bg[GTK_STATE_INSENSITIVE]);
	}

      if (GTK_WIDGET_IS_SENSITIVE (item->list_item))
	{
	  if (state == GTK_STATE_SELECTED)
	    pixmap = iflip_pixmap[SELECTED];
	  else
	    pixmap = iflip_pixmap[NORMAL];
	}
      else
	pixmap = iflip_pixmap[INSENSITIVE];


      gdk_draw_pixmap (item->iflip->window,
	  item->iflip->style->black_gc,
	  pixmap, 0, 0, 0, 0, iflip_width, iflip_height);
    }
  else
    {
      gdk_window_clear (item->iflip->window);
    }
}
static void 
frame_manager_bg_redraw (store_t *item)
{
  GdkPixmap *pixmap;
  GdkColor *color;
  GtkStateType state;
  int visible;

  state = item->list_item->state;

  if (GTK_WIDGET_IS_SENSITIVE (item->list_item))
    {
      if (state == GTK_STATE_SELECTED)
	color = &item->ibg->style->bg[GTK_STATE_SELECTED];
      else
	color = &item->ibg->style->white;
    }
  else
    color = &item->ibg->style->bg[GTK_STATE_INSENSITIVE];

  gdk_window_set_background (item->ibg->window, color);


  if (item->bg)
    {
      if (!ibg_pixmap[NORMAL])
	{
	  ibg_pixmap[NORMAL] =
	    gdk_pixmap_create_from_data (item->ibg->window,
		(gchar*) ibg_bits, ibg_width, ibg_height, -1,
		&item->ibg->style->fg[GTK_STATE_NORMAL],
		&item->ibg->style->white);
	  ibg_pixmap[SELECTED] =
	    gdk_pixmap_create_from_data (item->ibg->window,
		(gchar*) ibg_bits, ibg_width, ibg_height, -1,
		&item->ibg->style->fg[GTK_STATE_SELECTED],
		&item->ibg->style->bg[GTK_STATE_SELECTED]);
	  ibg_pixmap[INSENSITIVE] =
	    gdk_pixmap_create_from_data (item->ibg->window,
		(gchar*) ibg_bits, ibg_width, ibg_height, -1,

		&item->ibg->style->fg[GTK_STATE_INSENSITIVE],
		&item->ibg->style->bg[GTK_STATE_INSENSITIVE]);
	}

      if (GTK_WIDGET_IS_SENSITIVE (item->list_item))
	{
	  if (state == GTK_STATE_SELECTED)
	    pixmap = ibg_pixmap[SELECTED];
	  else
	    pixmap = ibg_pixmap[NORMAL];
	}
      else
	pixmap = ibg_pixmap[INSENSITIVE];


      gdk_draw_pixmap (item->ibg->window,
	  item->ibg->style->black_gc,
	  pixmap, 0, 0, 0, 0, ibg_width, ibg_height);
    }
  else
    {
      gdk_window_clear (item->ibg->window);
    }
}

static gint 
frame_manager_transparency (GtkAdjustment *a, gpointer client_data)
{
  float opacity;
  store_t *fg, *bg;
  int i, j;
  GList *item_list;
  frame_manager_t *fm = (frame_manager_t*) client_data; 

  if (!frame_manager_check ((frame_manager_t*)client_data))
    {
      return TRUE; 
    }
  if (onionskin == 2)
    {
     onionskin = 0;
    return TRUE;  
    }
  /* display */
  opacity = a->value;

  if (g_slist_length (fm->stores) > 1)
    {
      item_list =  (GList*) GTK_LIST(fm->store_list)->selection;

      if (item_list)
	{
	  i = gtk_list_child_position (GTK_LIST (fm->store_list), item_list->data);
	  j = frame_manager_get_bg_id ();
	  if (i != j && j < g_slist_length (fm->stores))
	    {
	      fg = (store_t*) g_slist_nth (fm->stores, i)->data;

	      fg->gimage->active_layer->opacity = opacity; 


	      if (!fg->gimage->onionskin)
		{
		  Layer *layer = fg->gimage->active_layer;

		  bg = (store_t*) g_slist_nth (fm->stores, j)->data; 
		  fg->gimage->onionskin = 1;
		  onionskin=1;
		  fg->bg_image = bg->gimage; 
		  gimage_add_layer2 (fg->gimage, bg->gimage->active_layer, 
		      1, s_x, s_y, e_x, e_y);
		  gimage_set_active_layer (fg->gimage, layer); 
		}
	     drawable_update (GIMP_DRAWABLE(fg->gimage->active_layer),
		  s_x, s_y, e_x, e_y);
  	      /*gdisplay_add_update_area (fm->gdisplay, s_x, s_y, e_x, e_y);
	      */gdisplays_flush ();
	    }	      
	} 

    }
  return TRUE; 
}

static void
frame_manager_next_filename (GImage *gimage, char **whole, char **raw, char dir,
frame_manager_t *fm)
{
 
  char rawname[256], tmp[30], frame_num[30], *name, *frame, *ext;
  int i, num, org_len, cur_len, len1, len2;  

  if (!gimage)
    return;

  strcpy (*whole, gimage->filename);
  strcpy (rawname, prune_filename (*whole));

  len1 = strlen (*whole);
  len2 = strlen (rawname);

  name  = strtok (rawname, ".");
  frame = strtok (NULL, ".");
  ext = strtok (NULL, ".");


  org_len = strlen (frame); 
  num = atoi (frame);
  num = dir ? num + gtk_spin_button_get_value_as_int (fm->step_size) :
    num - gtk_spin_button_get_value_as_int (fm->step_size); 


  sprintf (tmp, "%d\0", num); 
  cur_len = strlen (tmp);
  sprintf (&(frame_num[org_len-cur_len]), "%s", tmp); 

  for (i=0; i<org_len-cur_len; i++)
    {
      frame_num[i] = '0'; 
    } 


  sprintf ((*raw), "%s.%s.%s\0", name, frame_num, ext);  

  sprintf (&((*whole)[len1-len2]), "%s\0", *raw);

}

static void
frame_manager_this_filename (GImage *gimage, char **whole, char **raw, char *num,
frame_manager_t *fm)
{
 
  char rawname[256], *name, *frame, *ext;
  int len, org_len;

  if (!gimage)
    return;

  strcpy (*whole, gimage->filename);
  strcpy (rawname, prune_filename (*whole));

  len = strlen (*whole);
  org_len = strlen (rawname);
  

  name  = strtok (rawname, ".");
  frame = strtok (NULL, ".");
  ext = strtok (NULL, ".");


  sprintf (*raw, "%s.%s.%s\0", name, num, ext);

  sprintf (&((*whole)[len-org_len]), "%s\0", *raw);

}
  
static char* 
frame_manager_get_name (GImage *gimage)
{
 
  char raw[256], whole[256];
  char *tmp;

  if (!gimage)
    return "ERROR";

  strcpy (whole, gimage->filename);
  strcpy (raw, prune_filename (whole));

  tmp = strdup (strtok (raw, "."));
  
  return tmp; 
  
}

static char* 
frame_manager_get_frame (GImage *gimage)
{
 
  char raw[256], whole[256], frame[20];
  char *tmp;

  if (!gimage)
    return "ERROR";

  strcpy (whole, gimage->filename);
  strcpy (raw, prune_filename (whole));

  strtok (raw, ".");
  tmp =  strtok (NULL, ".");

  strcpy (frame, tmp);  
  return frame; 

}

static char* 
frame_manager_get_ext (GImage *gimage)
{
 
  char raw[256], whole[256];

  if (!gimage)
    return "ERROR";

  strcpy (whole, gimage->filename);
  strcpy (raw, prune_filename (whole));

  strtok (raw, ".");
  strtok (NULL, ".");
  return strdup (strtok (NULL, "."));
  
}
  
void
frame_manager_rm_onionskin (frame_manager_t *fm)
{
  GSList *list=NULL;
  store_t *item;
  
  fm = fm ? fm : frame_manager; 
  list = fm->stores;
  if (onionskin)
    {
      onionskin = 2;
      while (list)
	{

	  item = (store_t*) list->data;
	  if (item->gimage->onionskin)
	    {
	      item->gimage->active_layer->opacity = 1; 
	      item->gimage->onionskin = 0;
	      gimage_remove_layer (item->gimage, item->bg_image->active_layer);
	      GIMP_DRAWABLE (item->bg_image->active_layer)->offset_x = 0; 
	      GIMP_DRAWABLE (item->bg_image->active_layer)->offset_y = 0; 
	      gtk_adjustment_set_value (fm->trans_data, 1);
	    }
	  list = g_slist_next (list);
	}   

    }

}

static void
frame_manager_store_clear (frame_manager_t *fm)
{

  GSList *l=NULL; 
  store_t *item;

  while (fm->stores)
    {
      item = (store_t*) fm->stores->data;
      l = g_list_append (l, item->list_item);
      if (gtk_toggle_button_get_active((GtkToggleButton*)fm->auto_save))
	{
	  /* save image */
	  file_save (item->gimage->ID, item->gimage->filename, prune_filename (item->gimage->
		filename));
	}
      if (!item->selected)
	{
if (active_tool->type == CLONE)
  clone_delete_image (item->gimage);
	  gimage_delete (item->gimage);
	}
      fm->stores = g_slist_remove(fm->stores, item);
      g_free (item);


    }

  fm->stores = NULL; 
  gtk_list_remove_items (GTK_LIST (fm->store_list), l);
}

GimpDrawable*
frame_manager_onionskin_drawable ()
{
  GSList *list=NULL;
  store_t *item;
 
  if (!frame_manager || !onionskin)
    return NULL;

  list = frame_manager->stores;
  while (list)
    {
      item = (store_t*) list->data;
      if (item->gimage->onionskin)
	{
	  return GIMP_DRAWABLE (item->bg_image->active_layer);
	}
      list = g_slist_next (list);
    }   
  printf ("ERROR : something is wrong with onionskin 1\n"); 
    return NULL;
  
}

int
frame_manager_onionskin_display ()
{
 
  GSList *list=NULL;
  store_t *item;

  if (!frame_manager || !onionskin)
    return NULL;

  list = frame_manager->stores;
  while (list)
    {
      item = (store_t*) list->data;
      if (item->gimage->onionskin)
	{
	  return item->bg_image->ID;
	}
      list = g_slist_next (list);
    }
  printf ("ERROR : something is wrong with onionskin 2\n"); 
 
  return NULL; 
  
}

GimpDrawable*
frame_manager_bg_drawable ()
{
  GSList *list=NULL;
  store_t *item;
 
  if (!frame_manager)
    return NULL;

  list = frame_manager->stores;
  while (list)
    {
      item = (store_t*) list->data;
      if (item->bg)
	{
	  return GIMP_DRAWABLE (item->gimage->active_layer);
	}
      list = g_slist_next (list);
    }   
    return NULL;
  
}

int
frame_manager_bg_display ()
{
 
  GSList *list=NULL;
  store_t *item;

  if (!frame_manager)
    return NULL;

  list = frame_manager->stores;
  while (list)
    {
      item = (store_t*) list->data;
      if (item->bg)
	{
	  return item->gimage->ID;
	}
      list = g_slist_next (list);
    }
  return NULL; 
  
}

static int
frame_manager_get_bg_id ()
{
 
  GSList *list=NULL;
  store_t *item;
  int i=0;
  
  if (!frame_manager)
    return NULL;

  list = frame_manager->stores;
  while (list)
    {
      item = (store_t*) list->data;
      if (item->bg)
	{
	  return i;
	}
      list = g_slist_next (list);
      i ++; 
    }
  return NULL; 
  
}

void
frame_manager_set_bg (GDisplay *display)
{
 
  GSList *list=NULL;
  store_t *item, *i;
  
  if (!frame_manager)
    return;

  list = frame_manager->stores;
  while (list)
    {
      i = (store_t*) list->data; 
      if (i->bg)
	{
	  i->bg = 0;
	  frame_manager_bg_redraw (i);
	}          
      list = g_slist_next (list);   
    }
  list = frame_manager->stores;
  while (list)
    {
      item = (store_t*) list->data;
      if (item->gimage->ID == display->ID)
	{
	  item->bg = 1;
	  frame_manager_bg_redraw (item); 
	}
      list = g_slist_next (list);
    }
  
}

void
frame_manager_onionskin_offset (int x, int y)
{
  GSList *list=NULL;
  store_t *item;

  if (!frame_manager || !onionskin)
    return;

  list = frame_manager->stores;
  while (list)
    {
      item = (store_t*) list->data;
      if (item->gimage->onionskin)
	{
	  layer_translate2 (item->bg_image->active_layer, -x, -y, s_x, s_y, e_x, e_y);
	  return; 
	}
      list = g_slist_next (list);
    }
  printf ("ERROR : something is wrong with onionskin 3\n");
	    

}
static void
frame_manager_link_forward (frame_manager_t *fm)
{
  GImage *gimage; 
  char *whole, *raw;
  int num=1;

  if (fm->linked_display)
    {
      whole = (char*) malloc (sizeof(char)*255);
      raw = (char*) malloc (sizeof(char)*255);

      /* ** save image ** */

      gimage = fm->linked_display->gimage; 
      frame_manager_next_filename (fm->linked_display->gimage, &whole, &raw, 1, fm);
      if (file_load (whole, raw, fm->linked_display))
	{
	  if (gtk_toggle_button_get_active((GtkToggleButton*)fm->auto_save))
	    file_save (gimage->ID, gimage->filename,
		prune_filename (gimage->filename));
if (active_tool->type == CLONE)
  clone_delete_image (gimage);
	  gimage_delete (gimage); 	  
	}
      else
	{
	  printf ("ERROR\n");
	}
      free (whole);
      free (raw);
    } 
}

static void
frame_manager_link_backward (frame_manager_t *fm)
{
  GImage *gimage; 
  char *whole, *raw;
  int num=1;

  if (fm->linked_display)
    {
      whole = (char*) malloc (sizeof(char)*255);
      raw = (char*) malloc (sizeof(char)*255);

      /* ** save image ** */

      gimage = fm->linked_display->gimage; 
      frame_manager_next_filename (fm->linked_display->gimage, &whole, &raw, 0, fm);
      if (file_load (whole, raw, fm->linked_display))
	{
	  if (gtk_toggle_button_get_active((GtkToggleButton*)fm->auto_save))
	    file_save (gimage->ID, gimage->filename,
		prune_filename (gimage->filename));
if (active_tool->type == CLONE)
  clone_delete_image (gimage);
	  gimage_delete (gimage); 	  
	}
      else
	{
	  printf ("ERROR\n");
	}
      free (whole);
      free (raw);
    } 
}

static int
frame_manager_check (frame_manager_t *fm)
{
  if (fm==NULL)
    return 0;
  if (!fm->gdisplay)
    return 0;
  if (fm->gdisplay == gdisplay_active ())
    {
      return 1;
    }
  return 0; 
}

void
frame_manager_image_reload (GImage *old, GImage *new)
{
  GSList *list=NULL;
  store_t *item;

  if (!frame_manager)
    return;

  list = frame_manager->stores;
  while (list)
    {
      item = (store_t*) list->data;
      if (item->gimage == old)
	{
	  item->gimage = new;
	}
      list = g_slist_next (list);
    }
  printf ("ERROR : something is wrong with onionskin 2\n"); 
 
 
}
