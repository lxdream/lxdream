#include <gnome.h>

GdkImage *img;
GtkWindow *video_win;
GtkWidget *video_area;
char *video_data;

void video_open( void )
{
    img = gdk_image_new( GDK_IMAGE_FASTEST, gdk_visual_get_system(),
                         640, 480 );
    video_win = GTK_WINDOW(gtk_window_new( GTK_WINDOW_TOPLEVEL ));
    video_area = gtk_image_new_from_image(img, NULL);
    gtk_widget_show( video_area );
    gtk_container_add( GTK_CONTAINER(video_win), video_area );
    video_data = img->mem;
    
    gtk_window_set_title( video_win, "DreamOn! - Emulation Window" );
    gtk_window_set_policy( video_win, FALSE, FALSE, FALSE );
    gtk_window_set_default_size( video_win, 640, 480 );
    
    gtk_widget_show( GTK_WIDGET(video_win) );
}

void video_update_frame( void )
{
    gtk_widget_queue_draw( video_area );
}

void video_update_size( int hres, int vres, int colmode )
{
    /* do something intelligent */
}
