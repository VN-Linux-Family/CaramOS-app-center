#include "main_window.h"
#include "types.h"
#include <gtk/gtk.h>

static void activate(GtkApplication* app, gpointer) {
    MainWindow* win = new MainWindow(app);
    gtk_widget_show(win->widget());
    // Đăng ký destroy để giải phóng
    g_signal_connect_swapped(win->widget(), "destroy",
        G_CALLBACK(+[](gpointer p) { delete static_cast<MainWindow*>(p); }), win);
}

int main(int argc, char* argv[]) {
    GtkApplication* app = gtk_application_new(
        "com.ubuntu.store", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
