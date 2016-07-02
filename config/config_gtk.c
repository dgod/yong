#include <gtk/gtk.h>
#include <assert.h>

#include "config_ui.h"

typedef int (*InitSelfFunc)(CUCtrl p);
typedef void (*DestroySelfFunc)(CUCtrl p);

static gboolean on_window_del(GtkWidget *widget,GdkEvent  *event,CUCtrl p)
{
	assert(p->self==widget);
	p->self=NULL;
	gtk_main_quit();
	cu_quit_ui=1;
	return FALSE;
}

int cu_ctrl_init_window(CUCtrl p)
{
	GtkWidget *w;
	w=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(w),p->text);
	gtk_window_set_resizable(GTK_WINDOW(w),FALSE);
	gtk_widget_set_size_request(GTK_WIDGET(w),p->pos.w,p->pos.h);
	gtk_window_set_position(GTK_WINDOW(w),GTK_WIN_POS_CENTER);
	g_signal_connect(w,"delete-event",G_CALLBACK(on_window_del),p);
	p->self=w;
	
	w=gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(p->self),GTK_WIDGET(w));
	gtk_widget_set_size_request(GTK_WIDGET(w),p->pos.w,p->pos.h);
	gtk_widget_show(w);
	
	g_object_set_data(G_OBJECT(p->self),"fixed",w);
	
	return 0;
}

static void cu_ctrl_add_to_parent(CUCtrl p)
{
	CUCtrl pp=p->parent;
	GtkWidget *w;
	GType type;
	type=G_OBJECT_TYPE(G_OBJECT(pp->self));
	if(type!=GTK_TYPE_FIXED)
	{
		w=g_object_get_data(G_OBJECT(pp->self),"fixed");
	}
	else
	{
		w=pp->self;
	}
	gtk_fixed_put(GTK_FIXED(w),p->self,p->pos.x,p->pos.y);
	gtk_widget_set_size_request(GTK_WIDGET(p->self),p->pos.w,p->pos.h);
}

int cu_ctrl_init_label(CUCtrl p)
{
	p->self=gtk_label_new(p->text);
	gtk_misc_set_alignment(GTK_MISC(p->self),0,0.5);
	cu_ctrl_add_to_parent(p);
	return 0;
}

int cu_ctrl_init_edit(CUCtrl p)
{
	p->self=gtk_entry_new();
	cu_ctrl_add_to_parent(p);
	return 0;
}

static void list_changed(GtkComboBox *w,CUCtrl p)
{
	cu_ctrl_action_run(p,p->action);
}

int cu_ctrl_init_list(CUCtrl p)
{
	char **list;
	
	if(!p->self)
	{
#if GTK_CHECK_VERSION(3,0,0)
	p->self=gtk_combo_box_text_new();
#else
	p->self=gtk_combo_box_new_text();
#endif
	cu_ctrl_add_to_parent(p);
	}
	
	list=p->view?p->view:p->data;

	//FIXME: we should clear the list here first
	if(list)
	{
		int i;
		for(i=0;list[i]!=NULL;i++)
		{
#if GTK_CHECK_VERSION(3,0,0)
			gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(p->self),NULL,list[i]);
#else
			gtk_combo_box_append_text(GTK_COMBO_BOX(p->self),list[i]);
#endif
		}
	}
	g_signal_connect(p->self,"changed",G_CALLBACK(list_changed),p);
	return 0;
}

int cu_ctrl_init_combo(CUCtrl p)
{
	char **list;
#if GTK_CHECK_VERSION(3,0,0)
	p->self=gtk_combo_box_text_new_with_entry();
#else
	p->self=gtk_combo_box_entry_new_text();
#endif
	cu_ctrl_add_to_parent(p);
	
	list=p->view?p->view:p->data;

	//FIXME: we should clear the list here first
	if(list)
	{
		int i;
		for(i=0;list[i]!=NULL;i++)
		{
#if GTK_CHECK_VERSION(3,0,0)
			gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(p->self),NULL,list[i]);
#else
			gtk_combo_box_append_text(GTK_COMBO_BOX(p->self),list[i]);
#endif
		}
	}
	return 0;
}

int cu_ctrl_init_check(CUCtrl p)
{
	GtkWidget *w;
	w=gtk_check_button_new_with_label(p->text);
	p->self=w;
	cu_ctrl_add_to_parent(p);
	return 0;
}

void button_click(GtkWidget *w,CUCtrl p)
{
	cu_ctrl_action_run(p,p->action);
}

int cu_ctrl_init_button(CUCtrl p)
{
	p->self=gtk_button_new_with_label(p->text);
	cu_ctrl_add_to_parent(p);
	
	g_signal_connect(p->self,"clicked",G_CALLBACK(button_click),p);
	
	return 0;
}

static void tree_changed(GtkTreeSelection *w,CUCtrl p)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	CUCtrl item;
	if(!gtk_tree_selection_get_selected (w, &model, &iter))
		return;
	gtk_tree_model_get (model, &iter, 1, &item, -1);
	cu_ctrl_action_run(item,item->action);
}

static gboolean tree_right_click(GtkWidget *treeview, GdkEventButton *event, CUCtrl p)
{
	if (event->type == GDK_BUTTON_RELEASE  &&  event->button == 3)
	{
		GtkTreeSelection *sel;
		GtkTreeModel *model;
		GtkTreeIter iter;
		sel=gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
		if(gtk_tree_selection_get_selected(sel,&model,&iter))
		{
			CUCtrl item;
			gtk_tree_model_get (model, &iter, 1, &item, -1);
			if(item->menu!=NULL)
				cu_menu_popup(p,item->menu);
		}
	}
	return FALSE;
}

int cu_ctrl_init_tree(CUCtrl p)
{
	GtkWidget *w;
	GtkTreeStore *model;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	
	model=gtk_tree_store_new(2,G_TYPE_STRING,G_TYPE_POINTER);
	w=gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	g_object_unref(G_OBJECT(model));
	gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(w),TRUE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(w),FALSE);
	gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(w),TRUE);
	
	renderer = gtk_cell_renderer_text_new ();
	column=gtk_tree_view_column_new_with_attributes("name",renderer,"text",0,NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(w),column);
	
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (w));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select),"changed",G_CALLBACK(tree_changed),p);
	g_signal_connect(G_OBJECT(w),"button-release-event",G_CALLBACK(tree_right_click),p);
	
	p->self=w;
	cu_ctrl_add_to_parent(p);
	return 0;
}

int cu_ctrl_init_item(CUCtrl p)
{
	CUCtrl tree;
	GtkTreeStore *model;
	GtkTreeIter iter;
	for(tree=p;tree->type!=CU_TREE;tree=tree->parent);
	model=(GtkTreeStore*)gtk_tree_view_get_model(GTK_TREE_VIEW(tree->self));
	if(p->parent->type==CU_ITEM)
	{
		GtkTreeIter parent;
		if(!gtk_tree_model_get_iter(GTK_TREE_MODEL(model),&parent,(GtkTreePath*)p->parent->self))
		{
		}
		gtk_tree_store_append(GTK_TREE_STORE(model),&iter,&parent);
	}
	else
	{
		gtk_tree_store_append(GTK_TREE_STORE(model),&iter,NULL);
	}
	gtk_tree_store_set(GTK_TREE_STORE(model),&iter,0,p->text,1,p,-1);
	p->self=gtk_tree_model_get_path(GTK_TREE_MODEL(model),&iter);
	
	if(p->parent->type==CU_ITEM)
		gtk_tree_view_expand_row(GTK_TREE_VIEW(tree->self),p->parent->self,TRUE);
	
	return 0;
}

int cu_ctrl_init_group(CUCtrl p)
{
	return 0;
}

int cu_ctrl_init_page(CUCtrl p)
{
	p->self=gtk_fixed_new();
	cu_ctrl_add_to_parent(p);
	return 0;
}

int cu_ctrl_init_font(CUCtrl p)
{
	p->self=gtk_font_button_new();
	cu_ctrl_add_to_parent(p);
	return 0;
}

int cu_ctrl_init_image(CUCtrl p)
{
	p->self=gtk_image_new();
	cu_ctrl_add_to_parent(p);
	return 0;
}

int cu_ctrl_init_separator(CUCtrl p)
{
#if GTK_CHECK_VERSION(3,0,0)
	p->self=gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
#else
	p->self=gtk_hseparator_new();
#endif
	cu_ctrl_add_to_parent(p);
	return 0;
}

static InitSelfFunc init_funcs[]={
	cu_ctrl_init_window,
	cu_ctrl_init_label,
	cu_ctrl_init_edit,
	cu_ctrl_init_list,
	cu_ctrl_init_combo,
	cu_ctrl_init_check,
	cu_ctrl_init_button,
	cu_ctrl_init_tree,
	cu_ctrl_init_item,
	cu_ctrl_init_group,
	cu_ctrl_init_page,
	cu_ctrl_init_font,
	cu_ctrl_init_image,
	cu_ctrl_init_separator
};

int cu_ctrl_init_self(CUCtrl p)
{
	return init_funcs[p->type](p);
}

static void cu_ctrl_destroy_window(CUCtrl p)
{
	gtk_widget_destroy(p->self);
}

static void cu_ctrl_destroy_item(CUCtrl p)
{
	gtk_tree_path_free(p->self);
}

static DestroySelfFunc destroy_funcs[]={
	cu_ctrl_destroy_window,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	cu_ctrl_destroy_item,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

void cu_ctrl_destroy_self(CUCtrl p)
{
	if(!p || !p->self)
		return;
	if(destroy_funcs[p->type]!=NULL)
		destroy_funcs[p->type](p);
	p->self=NULL;
}

int cu_ctrl_show_self(CUCtrl p,int b)
{
	if(!p)
		return -1;
	if(p->type==CU_ITEM)
		return -1;
	if(!p->self)
		return -1;
	if(b) gtk_widget_show(p->self);
	else gtk_widget_hide(p->self);
	return 0;
}

static void image_load_cb(GdkPixbufLoader *loader,GdkPixbuf **pixbuf)
{
	if(*pixbuf)
		return;
	*pixbuf=gdk_pixbuf_loader_get_pixbuf(loader);
	g_object_ref(*pixbuf);
}
static GdkPixbuf *ui_image_load(const char *file)
{
	char path[256];
	GdkPixbuf *pixbuf;
	if(!file)
	{
		return 0;
	}
	if(file[0]=='~' && file[1]=='/')
	{
		sprintf(path,"%s/%s",getenv("HOME"),file+2);
	}
	else
	{
		strcpy(path,file);
	}
	if(file[0]=='/')
	{
        	pixbuf = gdk_pixbuf_new_from_file(path, NULL);
	}
	else
	{
		char *contents;
		size_t length;
		GdkPixbufLoader *load;
		contents=l_file_get_contents(file,&length,
				y_im_get_path("HOME"),
				y_im_get_path("DATA"),
				NULL);
		if(!contents)
		{
			//fprintf(stderr,"load %s contents fail\n",file);
			return NULL;
		}
		load=gdk_pixbuf_loader_new();
		pixbuf=NULL;
		g_signal_connect(load,"area-prepared",G_CALLBACK(image_load_cb),&pixbuf);
		if(!gdk_pixbuf_loader_write(load,(const guchar*)contents,length,NULL))
		{
			l_free(contents);
			//fprintf(stderr,"load image %s fail\n",file);
			return NULL;
		}
		l_free(contents);
		pixbuf=gdk_pixbuf_loader_get_pixbuf(load);
		if(pixbuf) g_object_ref(pixbuf);
		gdk_pixbuf_loader_close(load,NULL);
	}
	return pixbuf;
}

int cu_ctrl_set_self(CUCtrl p,const char *s)
{
	switch(p->type){
	case CU_LABEL:
		gtk_label_set_text(GTK_LABEL(p->self),s?s:"");
		break;
	case CU_EDIT:
		gtk_entry_set_text(GTK_ENTRY(p->self),s?s:"");
		break;
	case CU_CHECK:
		if(!s || !s[0] || s[0]!='1')
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->self),FALSE);
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->self),TRUE);
		break;
	case CU_COMBO:
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(p->self))),s?s:"");
		break;
	case CU_FONT:
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(p->self),s?s:"");
		break;
	case CU_LIST:
	{
		char **list=p->data;
		int i;
		int found=0;
		if(list && s) for(i=0;list[i]!=NULL;i++)
		{
			if(!strcmp(list[i],s))
			{
				gtk_combo_box_set_active(GTK_COMBO_BOX(p->self),i);
				found=1;
				break;
			}
		}
		if(!found)
		{
			gtk_combo_box_set_active(GTK_COMBO_BOX(p->self),0);
		}
		break;
	}
	case CU_IMAGE:
		if(s && s[0])
		{
			GdkPixbuf *pixbuf=ui_image_load(s);
			if(pixbuf)
			{
				gtk_image_set_from_pixbuf(GTK_IMAGE(p->self),pixbuf);
				g_object_unref(G_OBJECT(pixbuf));
			}
			else
				gtk_image_clear(GTK_IMAGE(p->self));
		}
		else
		{
			gtk_image_clear(GTK_IMAGE(p->self));
		}
		break;
	}
	return 0;
}

char *cu_ctrl_get_self(CUCtrl p)
{
	char *res=NULL;
	switch(p->type){
	case CU_EDIT:
		res=l_strdup(gtk_entry_get_text(GTK_ENTRY(p->self)));
		break;
	case CU_CHECK:
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p->self)))
			res=l_strdup("1");
		else
			res=l_strdup("0");
		break;
	case CU_FONT:
		res=l_strdup(gtk_font_button_get_font_name(GTK_FONT_BUTTON(p->self)));
		break;
	case CU_COMBO:
		res=l_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(p->self)))));
		break;
	case CU_LIST:
	{
		int active=gtk_combo_box_get_active(GTK_COMBO_BOX(p->self));
		gchar **list=p->data;
		if(active<0) break;
		if(list) res=l_strdup(list[active]);
		break;
	}}
	return res;
}

int cu_ctrl_set_prop(CUCtrl p,const char *prop)
{
	return 0;
}

int cu_init(void)
{
	int dpi;
	GtkWidget *w;
	gtk_init(NULL,NULL);
	
	/* just let gtk get dpi from system */
	w=gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_destroy(w);
	
	dpi=cu_screen_dpi();
	if(dpi>96)
		CU_SCALE=dpi/96.0;
	return 0;
}

int cu_loop(void)
{
	gtk_main();
	return 0;
}

int cu_screen_dpi(void)
{
	return (int)gdk_screen_get_resolution(gdk_screen_get_default());
}

int cu_confirm(CUCtrl p,const char *message)
{
	GtkWidget *dlg;
	gint ret;
	p=cu_ctrl_get_root(p);
	dlg=gtk_message_dialog_new(p->self,
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK_CANCEL,
		message);
	gtk_window_set_title(GTK_WINDOW(dlg),p->text);
	gtk_window_set_position(GTK_WINDOW(dlg),GTK_WIN_POS_CENTER);
	ret=gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
	if(ret==GTK_RESPONSE_OK)
		return 1;
	return 0;
}

void cu_menu_init_self(CUMenu m)
{
	cu_menu_destroy_self(m);
	m->self=gtk_menu_new();
}

void cu_menu_destroy_self(CUMenu m)
{
	if(!m->self)
		return;
	gtk_widget_destroy(m->self);
	m->self=NULL;
}

void cu_menu_popup(CUCtrl p,CUMenu m)
{
	int i;
	if(!m)
		return;
	cu_menu_init_self(m);
	for(i=0;i<m->count;i++)
	{
		GtkWidget *item;
		CUMenuEntry e=m->entries+i;
		if(!e->text) break;
		item=gtk_menu_item_new_with_label(e->text);
		gtk_menu_shell_append(GTK_MENU_SHELL(m->self),item);
		gtk_widget_show(item);
		g_signal_connect(G_OBJECT(item),"activate",
						G_CALLBACK(e->cb),e->arg);
	}
	gtk_menu_popup(GTK_MENU(m->self),NULL,NULL,NULL,NULL,0,gtk_get_current_event_time());
}

int cu_quit(void)
{
	cu_quit_ui=1;
	gtk_main_quit();
	return 0;
}

int cu_step(void)
{
	while(gtk_events_pending())
		gtk_main_iteration();
	return 0;
}

