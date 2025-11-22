#include <gtk/gtk.h>
#include <assert.h>
#include <dlfcn.h>

#include "config_ui.h"

#if GTK_CHECK_VERSION(4,0,0)
#define USE_LIST_AS_TREE	1
#endif

static GtkApplication *app;
typedef int (*InitSelfFunc)(CUCtrl p);
typedef void (*DestroySelfFunc)(CUCtrl p);

typedef struct{
	bool scroll;
}CU_PANEL_PRIV;

#if !GTK_CHECK_VERSION(4,0,0)
static void (*p_gtk_scrolled_window_set_overlay_scrolling) (
  GtkScrolledWindow* scrolled_window,
  gboolean overlay_scrolling
);

#endif

#if GTK_CHECK_VERSION(4,0,0)
// 由于菜单绑定在了主窗口上，那么我们不应该在这儿销毁窗口，否则GTK报错
static gboolean on_window_del(GtkWindow *widget,CUCtrl p)
{
	assert(p->self==widget);
	cu_quit();
	return TRUE;
}
#else
static gboolean on_window_del(GtkWidget *widget,GdkEvent  *event,CUCtrl p)
{
	assert(p->self==widget);
	p->self=NULL;
	cu_quit();
	return FALSE;
}
#endif

#if GTK_CHECK_VERSION(4,0,0)

int cu_ctrl_init_window(CUCtrl p)
{
	GtkWidget *w;
	w=gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(w),p->text);
	gtk_window_set_resizable(GTK_WINDOW(w),FALSE);
	gtk_widget_set_size_request(GTK_WIDGET(w),p->pos.w,p->pos.h);
	g_signal_connect(w,"close-request",G_CALLBACK(on_window_del),p);
	p->self=w;
	
	w=gtk_fixed_new();
	gtk_window_set_child(GTK_WINDOW(p->self),GTK_WIDGET(w));
	gtk_widget_set_size_request(GTK_WIDGET(w),p->pos.w,p->pos.h);
	gtk_widget_show(w);
	
	g_object_set_data(G_OBJECT(p->self),"fixed",w);
	return 0;
}

#else

int cu_ctrl_init_window(CUCtrl p)
{
	GtkWidget *w;
	w=gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(w),p->text);
	gtk_window_set_resizable(GTK_WINDOW(w),FALSE);
	gtk_widget_set_size_request(GTK_WIDGET(w),p->pos.w,p->pos.h);
#if GTK_CHECK_VERSION(3,94,0)
	g_signal_connect(w,"close-request",G_CALLBACK(on_window_del),p);
#else
	gtk_window_set_position(GTK_WINDOW(w),GTK_WIN_POS_CENTER);
	g_signal_connect(w,"delete-event",G_CALLBACK(on_window_del),p);
#endif
	p->self=w;
	
	w=gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(p->self),GTK_WIDGET(w));
	gtk_widget_set_size_request(GTK_WIDGET(w),p->pos.w,p->pos.h);
	gtk_widget_show(w);
	
	g_object_set_data(G_OBJECT(p->self),"fixed",w);
	
	return 0;
}
#endif

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
#if GTK_CHECK_VERSION(3,92,0)
	gtk_label_set_xalign(p->self,0);
	gtk_label_set_yalign(p->self,0.5);
#else
	gtk_misc_set_alignment(GTK_MISC(p->self),0,0.5);
#endif
	cu_ctrl_add_to_parent(p);
	return 0;
}

static void set_tooltip(CUCtrl p)
{
	const char *text=l_xml_get_prop(p->node,"tooltip");
	if(!text || !text[0])
		return;
	char *t=cu_translate(text);
	gtk_widget_set_tooltip_text(p->self,t);
	l_free(t);
}

int cu_ctrl_init_edit(CUCtrl p)
{
	p->self=gtk_entry_new();
	cu_ctrl_add_to_parent(p);
	const char *text=l_xml_get_prop(p->node,"placeholder");
	if(text && text[0])
	{
		char *t=cu_translate(text);
		gtk_entry_set_placeholder_text(p->self,t);
		l_free(t);
	}
	set_tooltip(p);
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

static void button_click(GtkWidget *w,CUCtrl p)
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

#if USE_LIST_AS_TREE

static GListModel *create_tree_model(GObject *item,CUCtrl tree)
{
	CUCtrl p=item?g_object_get_data(item,"cuctrl"):tree;
	if(p->type==CU_ITEM && p->self)
	{
		return G_LIST_MODEL(p->self);
	}
	if(!p->child)
	{
		return NULL;
	}
	GListStore *result=g_list_store_new (GTK_TYPE_STRING_OBJECT);
	int pos=0;
	GSList *slist=g_slist_alloc();
	for(CUCtrl c=p->child;c!=NULL;c=c->next)
	{
		slist=g_slist_prepend(slist,c);
	}
	for(GSList *p=slist;p!=NULL;p=p->next,pos++)
	{
		CUCtrl c=p->data;
		if(!c)
			break;
		GObject *o=(GObject*)gtk_string_object_new(c->text);
		g_object_set_data(o,"cuctrl",c);
		if(c->init)
			c->self=g_list_store_new(GTK_TYPE_STRING_OBJECT);
		g_list_store_append(result,o);
	}
	g_slist_free(slist);
	return G_LIST_MODEL(result);
}

static void tree_changed(GtkListView* self,guint position)
{
	GListModel *model=G_LIST_MODEL(gtk_list_view_get_model(self));
	GtkTreeListRow *row=(GtkTreeListRow*)g_list_model_get_object(model,position);
	if(!row)
		return;
	GObject *o=gtk_tree_list_row_get_item(row);
	CUCtrl c=g_object_get_data(o,"cuctrl");
	cu_ctrl_action_run(c,c->action);
}

static gboolean tree_right_click(GtkGestureClick *gesture,int n_press, double x, double y, CUCtrl p)
{
	GtkWidget *tree=gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(p->self));
	GtkSingleSelection *model=GTK_SINGLE_SELECTION(gtk_list_view_get_model(GTK_LIST_VIEW(tree)));
	GtkTreeListRow *row=(GtkTreeListRow*)gtk_single_selection_get_selected_item(model);
	if(!row)
		return FALSE;
	GObject *o=gtk_tree_list_row_get_item(row);
	CUCtrl item=g_object_get_data(o,"cuctrl");
	if(item->menu!=NULL)
	{
		item->menu->x=(int)x;
		item->menu->y=(int)y;
		cu_menu_popup(p,item->menu);
	}
	return FALSE;
}

int cu_ctrl_init_tree(CUCtrl p)
{
// https://gitlab.gnome.org/GNOME/gtk/-/blob/main/demos/gtk-demo/listview_settings.ui
	const char *ui_string=
  "<interface>"
    "<template class=\"GtkListItem\">"
      "<property name=\"child\">"
        "<object class=\"GtkTreeExpander\" id=\"expander\">"
          "<binding name=\"list-row\">"
            "<lookup name=\"item\">GtkListItem</lookup>"
          "</binding>"
          "<property name=\"child\">"
            "<object class=\"GtkLabel\">"
              "<property name=\"xalign\">0</property>"
              "<binding name=\"label\">"
                "<lookup name=\"string\" type=\"GtkStringObject\">"
                  "<lookup name=\"item\">expander</lookup>"
                "</lookup>"
              "</binding>"
            "</object>"
          "</property>"
        "</object>"
      "</property>"
    "</template>"
  "</interface>";

	GtkWidget *tree=gtk_list_view_new(
			NULL,
			gtk_builder_list_item_factory_new_from_bytes(NULL,g_bytes_new_static(ui_string,strlen(ui_string)))
		);
	gtk_list_view_set_single_click_activate(GTK_LIST_VIEW(tree),TRUE);
	g_signal_connect(G_OBJECT(tree),"activate",(void*)tree_changed,p);
	GtkWidget *sw=gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw),tree);
	p->self=sw;
	gtk_widget_show(tree);
	g_object_set_data(p->self,"tree",tree);
	cu_ctrl_add_to_parent(p);
	
	GtkGesture *gesture = gtk_gesture_click_new ();
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
    GtkEventController *controller = GTK_EVENT_CONTROLLER (gesture);
    g_signal_connect (controller, "pressed", G_CALLBACK (tree_right_click), p);
    gtk_widget_add_controller (tree, controller);
	return 0;
}

int cu_ctrl_init_tree_done(CUCtrl p)
{
	GtkListView *tree=(GtkListView*)gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(p->self));
	GListModel *root=create_tree_model(NULL,p);
	GtkTreeListModel *model=gtk_tree_list_model_new(
			root,
			FALSE,
			TRUE,
			(void*)create_tree_model,
			p,
			NULL);
	gtk_tree_list_model_set_autoexpand(model,TRUE);
	GtkSingleSelection *selection=gtk_single_selection_new(G_LIST_MODEL(model));
	gtk_list_view_set_model(tree,GTK_SELECTION_MODEL(selection));
	g_object_unref(selection);
	return 0;
}

int cu_ctrl_init_item(CUCtrl p)
{
	if(p->parent->type!=CU_ITEM)
	{
		return 0;
	}
	GListStore *model=p->parent->self;
	GObject *o=(GObject*)gtk_string_object_new(p->text);
	g_object_set_data(o,"cuctrl",p);
	g_list_store_append(model,o);
	return 0;
}

#else
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

#if GTK_CHECK_VERSION(4,0,0)
static gboolean tree_right_click(GtkGestureClick *gesture,int n_press, double x, double y, CUCtrl p)
{
	GtkWidget *tree=gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(p->self));
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint x_bin_window, y_bin_window;
	gtk_tree_view_convert_widget_to_bin_window_coords (GTK_TREE_VIEW (tree), (gint) x, (gint) y, &x_bin_window, &y_bin_window);
	GtkTreePath *path;
	gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree), x_bin_window, y_bin_window, &path, NULL, NULL, NULL);
	if(!path)
		return FALSE;
	model=gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
	gtk_tree_model_get_iter(model,&iter,path);
	gtk_tree_path_free(path);
	CUCtrl item;
	gtk_tree_model_get(model, &iter, 1, &item, -1);
	if(item->menu!=NULL)
	{
		item->menu->x=(int)x;
		item->menu->y=(int)y;
		cu_menu_popup(p,item->menu);
	}
	return FALSE;
}

#else
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
#endif

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
#if GTK_CHECK_VERSION(3,94,0)
	GtkGesture *gesture = gtk_gesture_click_new ();
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
    GtkEventController *controller = GTK_EVENT_CONTROLLER (gesture);
    g_signal_connect (controller, "pressed", G_CALLBACK (tree_right_click), p);
    gtk_widget_add_controller (w, controller);
#else
	g_signal_connect(G_OBJECT(w),"button-release-event",G_CALLBACK(tree_right_click),p);
#endif

#if GTK_CHECK_VERSION(4,0,0)
	GtkWidget *scrolled_window=gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window),w);
#else
	GtkWidget *scrolled_window=gtk_scrolled_window_new(NULL,NULL);
	gtk_container_add(GTK_CONTAINER(scrolled_window),w);
	if(p_gtk_scrolled_window_set_overlay_scrolling)
	{
		p_gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scrolled_window),TRUE);
	}
#endif
	p->self=scrolled_window;
	gtk_widget_show(w);
	g_object_set_data(p->self,"tree",w);
	cu_ctrl_add_to_parent(p);
	return 0;
}

int cu_ctrl_init_tree_done(CUCtrl p)
{
	return 0;
}

int cu_ctrl_init_item(CUCtrl p)
{
	CUCtrl tree;
	GtkTreeStore *model;
	GtkTreeIter iter;
	GtkTreeView *w;
	for(tree=p;tree->type!=CU_TREE;tree=tree->parent);
	w=GTK_TREE_VIEW(g_object_get_data(tree->self,"tree"));

	model=(GtkTreeStore*)gtk_tree_view_get_model(w);
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
		gtk_tree_view_expand_row(GTK_TREE_VIEW(w),p->parent->self,TRUE);
	
	return 0;
}
#endif

int cu_ctrl_init_group(CUCtrl p)
{
	return 0;
}

int cu_ctrl_init_page(CUCtrl p)
{
	GtkWidget *fixed=gtk_fixed_new();
#if GTK_CHECK_VERSION(4,0,0)
	p->self=gtk_scrolled_window_new();
#else
	p->self=gtk_scrolled_window_new(NULL, NULL);
	if(p_gtk_scrolled_window_set_overlay_scrolling)
		p_gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(p->self),TRUE);
#endif
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(p->self),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	g_object_set_data(G_OBJECT(p->self),"fixed",fixed);
	gtk_widget_set_size_request(GTK_WIDGET(fixed),p->pos.w,p->pos.h);
	gtk_widget_show(fixed);
#if GTK_CHECK_VERSION(4,0,0)
	gtk_scrolled_window_set_child(p->self,fixed);
#else
	gtk_container_add(p->self,fixed);
#endif
	p->priv=l_new0(CU_PANEL_PRIV);
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

int cu_ctrl_init_link(CUCtrl p)
{
	const char *href=l_xml_get_prop(p->node,"href");
	if(!href)
		href="";
	p->self=gtk_link_button_new_with_label(href,p->text);
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
	cu_ctrl_init_separator,
	cu_ctrl_init_link,
};

static InitSelfFunc done_funcs[]={
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	cu_ctrl_init_tree_done,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

int cu_ctrl_init_self(CUCtrl p)
{
	return init_funcs[p->type](p);
}

int cu_ctrl_init_done(CUCtrl p)
{
	InitSelfFunc cb=done_funcs[p->type];
	if(!cb)
		return 0;
	return cb(p);
}

static void cu_ctrl_destroy_window(CUCtrl p)
{
#if GTK_CHECK_VERSION(4,0,0)
	gtk_window_destroy(p->self);
#else
	gtk_widget_destroy(p->self);
#endif
}

static void cu_ctrl_destroy_item(CUCtrl p)
{
#if !USE_LIST_AS_TREE
	gtk_tree_path_free(p->self);
#endif
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
#if GTK_CHECK_VERSION(4,0,0)
		gtk_entry_buffer_set_text(gtk_entry_get_buffer(GTK_ENTRY(p->self)),s?s:"",-1);
#else
		gtk_entry_set_text(GTK_ENTRY(p->self),s?s:"");
#endif
		break;
	case CU_CHECK:
#if GTK_CHECK_VERSION(4,0,0)
		if(!s || !s[0] || s[0]!='1')
			gtk_check_button_set_active(GTK_CHECK_BUTTON(p->self),FALSE);
		else
			gtk_check_button_set_active(GTK_CHECK_BUTTON(p->self),TRUE);
#else
		if(!s || !s[0] || s[0]!='1')
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->self),FALSE);
		else
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p->self),TRUE);
#endif
		break;
	case CU_COMBO:
#if GTK_CHECK_VERSION(4,0,0)
		gtk_entry_buffer_set_text(gtk_entry_get_buffer(GTK_ENTRY(gtk_combo_box_get_child(GTK_COMBO_BOX(p->self)))),s?s:"",-1);
#else
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(p->self))),s?s:"");
#endif
		break;
	case CU_FONT:
#if GTK_CHECK_VERSION(3,94,0)
		gtk_font_chooser_set_font(GTK_FONT_CHOOSER(p->self),s?s:"");
#else
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(p->self),s?s:"");
#endif
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
			{
				gtk_image_clear(GTK_IMAGE(p->self));
			}
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
#if GTK_CHECK_VERSION(4,0,0)
		res=l_strdup(gtk_entry_buffer_get_text(gtk_entry_get_buffer(GTK_ENTRY(p->self))));
#else
		res=l_strdup(gtk_entry_get_text(GTK_ENTRY(p->self)));
#endif
		break;
	case CU_CHECK:
#if GTK_CHECK_VERSION(4,0,0)
		if(gtk_check_button_get_active(GTK_CHECK_BUTTON(p->self)))
			res=l_strdup("1");
		else
			res=l_strdup("0");
#else
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p->self)))
			res=l_strdup("1");
		else
			res=l_strdup("0");
#endif
		break;
	case CU_FONT:
#if GTK_CHECK_VERSION(3,94,0)
	{
		char *temp=gtk_font_chooser_get_font(GTK_FONT_CHOOSER(p->self));
		res=l_strdup(temp);
		g_free(temp);
	}
#else
		res=l_strdup(gtk_font_button_get_font_name(GTK_FONT_BUTTON(p->self)));
#endif
		break;
	case CU_COMBO:
#if GTK_CHECK_VERSION(4,0,0)
	{
		GtkWidget *entry=gtk_combo_box_get_child(p->self);
		GtkEntryBuffer *buf=gtk_entry_get_buffer(GTK_ENTRY(entry));
		res=l_strdup(gtk_entry_buffer_get_text(buf));
	}
#else
		res=l_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(p->self)))));
#endif
		break;
	case CU_LIST:
	{
		int active=gtk_combo_box_get_active(GTK_COMBO_BOX(p->self));
		gchar **list=p->data;
		if(active<0) break;
		if(list) res=list[active]?l_strdup(list[active]):NULL;
		break;
	}}
	return res;
}

int cu_ctrl_set_prop(CUCtrl p,const char *prop)
{
	return 0;
}
#if 0

#if GTK_CHECK_VERSION(4,0,0)

int cu_init(void)
{
	int dpi;
	gtk_init();
	main_loop=g_main_loop_new(NULL,FALSE);
	dpi=cu_screen_dpi();
	if(dpi>96)
		CU_SCALE=dpi/96.0;
	return 0;
}
int cu_loop(void)
{
	g_main_loop_run(main_loop);
	return 0;
}
#else

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
#endif

#endif

int cu_init(void)
{
#if !GTK_CHECK_VERSION(4,0,0)
	p_gtk_scrolled_window_set_overlay_scrolling=dlsym(NULL,"gtk_scrolled_window_set_overlay_scrolling");
#endif
	return 0;
}

static void gtk_activate(GtkApplication *app,CULoopArg *arg)
{
	int dpi=cu_screen_dpi();
	if(dpi>96)
		CU_SCALE=dpi/96.0;
	void (*activate)(CULoopArg *)=arg->priv;
	activate(arg);
}

#ifndef G_APPLICATION_DEFAULT_FLAGS
#define G_APPLICATION_DEFAULT_FLAGS G_APPLICATION_FLAGS_NONE
#endif

int cu_loop(void (*activate)(CULoopArg *),CULoopArg*arg)
{
	app=gtk_application_new(arg->app_id,G_APPLICATION_DEFAULT_FLAGS);
	arg->priv=activate;
	g_signal_connect (app, "activate", G_CALLBACK(gtk_activate), arg);
	g_application_run (G_APPLICATION (app), 0,NULL);
	if(arg && arg->win)
	{
		cu_ctrl_free(arg->win);
		arg->win=NULL;
	}
	g_object_unref(app);
	app=NULL;

	return 0;
}

int cu_screen_dpi(void)
{
#if GTK_CHECK_VERSION(4,0,0)
	GdkDisplay *dpy=gdk_display_get_default();
	GtkSettings *p=gtk_settings_get_for_display(dpy);
	int dpi=96;
	int temp=0;
	if(getenv("GDK_DPI_SCALE"))
	{
		double scale=strtod(getenv("GDK_DPI_SCALE"),NULL);
		temp=(int)1024*96*scale;
		g_object_set(p,"gtk-xft-dpi",temp,NULL);
	}
	g_object_get(p,"gtk-xft-dpi",&temp,NULL);
	if(temp>0)
	{
		dpi=temp/1024;
	}
	return dpi;
#else
	return (int)gdk_screen_get_resolution(gdk_screen_get_default());
#endif
}

#if GTK_CHECK_VERSION(4,0,0)
static void _cu_confirm_response(GtkDialog *dlg,int response_id,gpointer userdata)
{
	*(int*)userdata=(response_id==GTK_RESPONSE_OK);
}
int cu_confirm(CUCtrl p,const char *message)
{
	int response=-1;
	GtkWidget *dlg;
	p=cu_ctrl_get_root(p);
	dlg=gtk_message_dialog_new(p->self,
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK_CANCEL,
		"%s",
		message);
	gtk_window_set_title(GTK_WINDOW(dlg),p->text);
	g_signal_connect (dlg, "response",  G_CALLBACK (_cu_confirm_response), &response);
	gtk_widget_show(dlg);
	while(response==-1)
		cu_step();
	gtk_window_destroy(GTK_WINDOW(dlg));
	return response;
}
#else
int cu_confirm(CUCtrl p,const char *message)
{
	GtkWidget *dlg;
	gint ret;
	p=cu_ctrl_get_root(p);
	dlg=gtk_message_dialog_new(p->self,
		GTK_DIALOG_MODAL,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK_CANCEL,
		"%s",
		message);
	gtk_window_set_title(GTK_WINDOW(dlg),p->text);
	gtk_window_set_position(GTK_WINDOW(dlg),GTK_WIN_POS_CENTER);
	ret=gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
	if(ret==GTK_RESPONSE_OK)
		return 1;
	return 0;
}
#endif

#if GTK_CHECK_VERSION(4,0,0)
void cu_menu_init_self(CUMenu m)
{
	if(m->self)
		return;
	GMenu *model=g_menu_new();
	m->self=gtk_popover_menu_new_from_model(G_MENU_MODEL(model));
	g_object_unref(model);
	gtk_popover_set_has_arrow(GTK_POPOVER(m->self),FALSE);
	gtk_widget_set_halign (GTK_WIDGET(m->self), GTK_ALIGN_START);
}
void cu_menu_destroy_self(CUMenu m)
{
	if(!m->self)
		return;
	gtk_widget_unparent(m->self);
	//g_object_unref(m->self);
	m->self=NULL;
}
static void cu_menu_popup_activate(GSimpleAction *action,GVariant *parameter,CUMenuEntry e)
{
	e->cb(NULL,e->arg);
}

void cu_menu_popup(CUCtrl p,CUMenu m)
{
	int i;
	if(!m)
		return;
	cu_menu_init_self(m);
	GMenu *model=(GMenu*)gtk_popover_menu_get_menu_model(GTK_POPOVER_MENU(m->self));
	g_menu_remove_all(model);
	GActionGroup *actions=(GActionGroup*)g_simple_action_group_new();
	for(i=0;i<m->count;i++)
	{
		CUMenuEntry e=m->entries+i;
		char temp[32];
		if(!e->text)
			break;
		sprintf(temp,"y.test%d",i);
		g_menu_append(model,e->text,temp);
		GSimpleAction *action=g_simple_action_new(temp+2,NULL);
		g_signal_connect(action,"activate",G_CALLBACK(cu_menu_popup_activate),e);
		g_action_map_add_action(G_ACTION_MAP(actions),G_ACTION(action));
	}
	if(!gtk_widget_get_parent(m->self))
		gtk_widget_set_parent(m->self,GTK_WIDGET(cu_ctrl_get_root(p)->self));	
	gtk_widget_insert_action_group(GTK_WIDGET(m->self),"y",actions);
	
	GdkRectangle rectangle;
	rectangle.x = m->x;
	rectangle.y = m->y;
	rectangle.width = 1;
	rectangle.height = 1;
	gtk_popover_set_pointing_to (GTK_POPOVER(m->self), &rectangle);
	
	gtk_widget_show(GTK_WIDGET(m->self));
}
#else
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
#endif

int cu_quit(void)
{
	cu_quit_ui=1;
	g_application_quit(G_APPLICATION(app));
	return 0;
}

int cu_step(void)
{
#if GTK_CHECK_VERSION(4,0,0)
	GMainContext *ctx=g_main_context_default();
	while(g_main_context_pending(ctx))
		g_main_context_iteration(ctx,TRUE);
#else
	while(gtk_events_pending())
		gtk_main_iteration();
#endif
	return 0;
}

static gboolean ui_call_wraper(void **p)
{
	void (*cb)(void*)=p[0];
	void *arg=p[1];
	l_free(p);
	cb(arg);
	return FALSE;
}

int cu_call(void (*cb)(void*),void *arg)
{
	void **p=l_cnew(2,void*);
	p[0]=cb;
	p[1]=arg;
	g_idle_add((GSourceFunc)ui_call_wraper,p);
	return 0;
}

