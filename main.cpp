#include <xcb/xcb.h>
#include <xcb/randr.h>

#include <gio/gio.h>
#include <glib-unix.h>

#include <algorithm>
#include <string>

using namespace std;

static xcb_connection_t *g_conn = nullptr;
static uint16_t g_width = 0;
static uint16_t g_height = 0;

static GSettings *g_setsDesktopBg = nullptr;
static GSettings *g_setsMateIface = nullptr;

static guint g_timeoutOnScalingFactorChange = 0;
static guint g_timeoutOnScreenSizeChange = 0;

static void stopTimeoutXfce4Panel()
{
    if (g_timeoutOnScalingFactorChange)
    {
        g_source_remove(g_timeoutOnScalingFactorChange);
        g_timeoutOnScalingFactorChange = 0;
    }
}
static void stopTimeoutMateDesktop()
{
    if (g_timeoutOnScreenSizeChange)
    {
        g_source_remove(g_timeoutOnScreenSizeChange);
        g_timeoutOnScreenSizeChange = 0;
    }
}

static void showDesktopIcons(bool show)
{
    g_settings_set_boolean(g_setsDesktopBg, "show-desktop-icons", show);
}

static gboolean onScalingFactorChange(gpointer)
{
    g_timeoutOnScalingFactorChange = 0;

    g_spawn_command_line_async("xfce4-panel -r", nullptr);

    gchar *styleRaw = nullptr;
    if (g_spawn_command_line_sync("xfconf-query -c xfwm4 -p /general/theme", &styleRaw, nullptr, nullptr, nullptr) && styleRaw)
    {
        const string_view styleView(styleRaw);
        const auto pos1 = styleView.rfind("\n");
        const auto pos2 = styleView.rfind("-hdpi");
        const auto pos3 = styleView.rfind("-xhdpi");
        const auto pos = min({pos1, pos2, pos3});
        if (pos != string::npos)
        {
            const auto scalingFactor = g_settings_get_int(g_setsMateIface, "window-scaling-factor");
            auto style = string(styleView.substr(0, pos));
            if (scalingFactor == 2)
                style += "-hdpi";
            else if (scalingFactor > 2)
                style += "-xhdpi";
            g_spawn_command_line_async(("xfconf-query -c xfwm4 -p /general/theme -s " + style).c_str(), nullptr);
        }
    }
    g_free(styleRaw);

    showDesktopIcons(false);
    showDesktopIcons(true);

    return false;
}

static void onMateIfaceOrFontChanged(GSettings *, gchar *key, gpointer)
{
    if (g_strcmp0(key, "window-scaling-factor") != 0 && g_strcmp0(key, "dpi") != 0)
        return;

    stopTimeoutXfce4Panel();
    g_timeoutOnScalingFactorChange = g_timeout_add(250, onScalingFactorChange, nullptr);
}

static gboolean onScreenSizeChange(gpointer)
{
    g_timeoutOnScreenSizeChange = 0;

    showDesktopIcons(false);
    showDesktopIcons(true);

    return false;
}

static gboolean processXcbEvents(gint fd, GIOCondition condition, gpointer)
{
    auto e = xcb_poll_for_event(g_conn);

    if (!e)
    {
        if (xcb_connection_has_error(g_conn))
            return false;
        return true;
    }

    if (e->response_type & XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE)
    {
        auto re = reinterpret_cast<xcb_randr_screen_change_notify_event_t *>(e);
        if (re->width != g_width || re->height != g_height)
        {
            g_width = re->width;
            g_height = re->height;

            stopTimeoutMateDesktop();
            g_timeoutOnScreenSizeChange = g_timeout_add(2000, onScreenSizeChange, nullptr);
        }
    }

    free(e);

    return true;
}

int main()
{
    g_conn = xcb_connect(nullptr, nullptr);
    if (!g_conn)
        return -1;

    auto xcbSource = g_unix_fd_add(xcb_get_file_descriptor(g_conn), G_IO_IN, processXcbEvents, nullptr);

    auto screen = xcb_setup_roots_iterator(xcb_get_setup(g_conn)).data;
    xcb_randr_query_version_unchecked(g_conn, XCB_RANDR_MAJOR_VERSION, XCB_RANDR_MINOR_VERSION);
    xcb_randr_select_input(g_conn, screen->root, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
    if (auto reply = xcb_randr_get_screen_info_reply(g_conn, xcb_randr_get_screen_info(g_conn, screen->root), nullptr))
    {
        auto sizes = xcb_randr_get_screen_info_sizes(reply);
        g_width = sizes[0].width;
        g_height = sizes[0].height;
        free(reply);
    }
    xcb_flush(g_conn);

    g_setsDesktopBg = g_settings_new_with_path("org.mate.background", "/org/mate/desktop/background/");
    showDesktopIcons(true);

    g_setsMateIface = g_settings_new_with_path("org.mate.interface", "/org/mate/desktop/interface/");
    g_signal_connect(g_setsMateIface, "changed", G_CALLBACK(onMateIfaceOrFontChanged), nullptr);

    auto setsMateFont = g_settings_new_with_path("org.mate.font-rendering", "/org/mate/desktop/font-rendering/");
    g_signal_connect(setsMateFont, "changed", G_CALLBACK(onMateIfaceOrFontChanged), nullptr);

    auto mainLoop = g_main_loop_new(nullptr, false);
    g_main_loop_run(mainLoop);

    stopTimeoutXfce4Panel();
    stopTimeoutMateDesktop();

    g_object_unref(setsMateFont);
    g_object_unref(g_setsMateIface);
    g_object_unref(g_setsDesktopBg);
    g_object_unref(mainLoop);

    g_source_remove(xcbSource);
    xcb_disconnect(g_conn);

    return 0;
}
