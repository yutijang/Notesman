#include <cstddef>
#include <cstdio>
#include <cstring>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <glib.h>

static gchar* toFileUri(const char* path) {
    if (path == nullptr || *path == '\0') { return nullptr; }

    // Đã là URI (file://, http://, https:// ...)
    if (g_uri_parse_scheme(path) != nullptr) { return g_strdup(path); }

    // Absolute filesystem path
    if (g_path_is_absolute(path) != 0) { return g_filename_to_uri(path, nullptr, nullptr); }

    // Relative path → absolute → URI
    gchar* cwd = g_get_current_dir();
    gchar* abs = g_build_filename(cwd, path, nullptr);
    g_free(cwd);

    gchar* uri = g_filename_to_uri(abs, nullptr, nullptr);
    g_free(abs);

    return uri;
}

static gchar* readStdinAll() {
    GString* buf = g_string_new(nullptr);
    gchar tmp[4096];

    while (feof(stdin) == 0) {
        std::size_t n = fread(tmp, 1, sizeof(tmp), stdin);
        if (n > 0) { g_string_append_len(buf, tmp, static_cast<gssize>(n)); }
    }

    return g_string_free(buf, FALSE);
}

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);

    if (argc < 2) {
        g_printerr("Usage: webkitgtk_viewer <uri> [title]\n");
        return 1;
    }

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);

    const char* title = (argc >= 3) ? argv[2] : "View detail resource";
    gtk_window_set_title(GTK_WINDOW(window), title);

    GtkWidget* webview = webkit_web_view_new();
    gtk_container_add(GTK_CONTAINER(window), webview);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);

    if (argc >= 2 && strcmp(argv[1], "--stdin") == 0) {
        gchar* html = readStdinAll();
        webkit_web_view_load_html(WEBKIT_WEB_VIEW(webview), html, nullptr);
        g_free(html);
    } else {
        gchar* uri = toFileUri(argv[1]);
        if (uri == nullptr) {
            g_printerr("Invalid path or URI\n");
            return 1;
        }

        webkit_web_view_load_uri(WEBKIT_WEB_VIEW(webview), uri);
        g_free(uri);
    }

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
