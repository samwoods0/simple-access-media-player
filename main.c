#include <stdio.h>
#include <gtk/gtk.h>

#include <dirent.h>
#define NUM_FILES 25

//gui with gtk
//demux, decode with ffmpeg
//display with SDL (+OpenGL ?)


static void apply_styles()
{
	GtkCssProvider *css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_path(css_provider,"styles.css");

	gtk_style_context_add_provider_for_display
		(
		 gdk_display_get_default(),
		 GTK_STYLE_PROVIDER(css_provider),
		 GTK_STYLE_PROVIDER_PRIORITY_USER
		);

	g_object_unref(css_provider);
}
static void activate(GtkApplication *app, gpointer *user_data)
{
	GtkWidget *window;
	GtkWidget *grid;
	GtkWidget *box;
	GtkWidget *label;
	
	DIR *dir;
	struct dirent *entry;
	const char *path = "./media_files";
	
	window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window),"Media Player");
	gtk_window_set_default_size(GTK_WINDOW(window), 800,600);
	
	grid = gtk_grid_new();
	gtk_window_set_child(GTK_WINDOW(window), grid);
	gtk_grid_set_row_spacing(GTK_GRID(grid), 50);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 50);

	dir = opendir(path);
	int row = 0;
	int file_ended = 0;
	while(file_ended == 0)
	{
		for(int i = 0; i < 3; i++)
		{
			if((entry = readdir(dir)) != NULL)
			{
				if(entry->d_name[0] == '.')
				{
					i-=1;
					continue;
				}
				box = gtk_box_new(GTK_ORIENTATION_VERTICAL,50);

				label = gtk_label_new(entry->d_name);
				gtk_box_append(GTK_BOX(box), label);
				gtk_widget_add_css_class(box,"box");
				gtk_grid_attach(GTK_GRID(grid), box, i, row, 1, 1);

			}else file_ended = 1;
		}
		row++;
	}


	apply_styles();
	gtk_window_present(GTK_WINDOW(window));

}

int main(int argc, char **argv)
{
	GtkApplication *app;
	int status;

	app = gtk_application_new("hello.gtk.world", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	status = g_application_run(G_APPLICATION (app), argc, argv);
	g_object_unref(app);

	return status;
}
