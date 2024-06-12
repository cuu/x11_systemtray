#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TRAY_WIDTH 400
#define TRAY_HEIGHT 40
#define TRAY_X 100
#define TRAY_Y 100
#define ICON_SIZE 32
#define ICON_PADDING 4

Atom _NET_SYSTEM_TRAY_S, _NET_SYSTEM_TRAY_OPCODE, MANAGER;
Display *display;
Window tray_window;
Window tray_selection_window;

typedef struct IconNode {
  Window icon_window;
  struct IconNode *next;
} IconNode;

IconNode *icon_list = NULL;

void add_icon(Window icon_window) {
  IconNode *new_node = (IconNode *)malloc(sizeof(IconNode));
  new_node->icon_window = icon_window;
  new_node->next = icon_list;
  icon_list = new_node;
}

void remove_icon(Window icon_window) {
  IconNode **current = &icon_list;
  while (*current) {
    if ((*current)->icon_window == icon_window) {
      IconNode *to_delete = *current;
      *current = (*current)->next;
      if (to_delete != NULL) {
        free(to_delete);
      }
      return;
    }
    current = &(*current)->next;
  }
}

void rearrange_icons() {
  int x = 10;
  XWindowAttributes attributes;
  int x_step = ICON_SIZE;
  int y_step = ICON_SIZE;

  for (IconNode *node = icon_list; node != NULL; node = node->next) {

    x_step = ICON_SIZE;
    y_step = ICON_SIZE;

    if (XGetWindowAttributes(display, node->icon_window, &attributes) == 0) {
      fprintf(stderr, "Get icon window size failed\n");
    } else {

      x_step = attributes.width;
      y_step = attributes.height;
    }
    XMoveWindow(display, node->icon_window, x, (TRAY_HEIGHT - y_step) / 2);
    x += x_step + ICON_PADDING;
  }
}

void handle_systray_message(XEvent *event) {
  if (event->type == ClientMessage &&
      event->xclient.message_type == _NET_SYSTEM_TRAY_OPCODE) {
    // Process the system tray message
    if (event->xclient.data.l[1] == 0) { // SYSTEM_TRAY_REQUEST_DOCK
      Window icon_window = (Window)event->xclient.data.l[2];
      XReparentWindow(display, icon_window, tray_window, 0, 0);
      XMapWindow(display, icon_window);

      // Select input events on the icon window to detect its destruction
      XSelectInput(display, icon_window, StructureNotifyMask);

      // Add the icon to the list and rearrange icons
      add_icon(icon_window);
      rearrange_icons();
    }
  }
}

void handle_destroy_notify(XEvent *event) {
  if (event->type == DestroyNotify) {
    Window icon_window = event->xdestroywindow.window;
    // Remove the icon from the list and rearrange icons
    remove_icon(icon_window);
    rearrange_icons();
    printf("Icon window 0x%lx removed from system tray\n", icon_window);
  }
}

int main(void) {
  display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "Cannot open display\n");
    exit(1);
  }

  int screen = DefaultScreen(display);
  Window root = RootWindow(display, screen);

  // Create a simple window with black background and title
  tray_window = XCreateSimpleWindow(display, root, TRAY_X, TRAY_Y, TRAY_WIDTH,
                                    TRAY_HEIGHT, 1, WhitePixel(display, screen),
                                    BlackPixel(display, screen));

  // Set window title
  XStoreName(display, tray_window, "X11 System Tray");

  // Select input events
  XSelectInput(display, tray_window,
               ExposureMask | ButtonPressMask | StructureNotifyMask);

  // Create a graphics context
  GC gc = XCreateGC(display, tray_window, 0, NULL);
  XSetForeground(display, gc, WhitePixel(display, screen));

  // Map (show) the window
  XMapWindow(display, tray_window);

  // Get atoms
  _NET_SYSTEM_TRAY_S = XInternAtom(display, "_NET_SYSTEM_TRAY_S0", False);
  _NET_SYSTEM_TRAY_OPCODE =
      XInternAtom(display, "_NET_SYSTEM_TRAY_OPCODE", False);
  MANAGER = XInternAtom(display, "MANAGER", False);

  // Create selection window for system tray
  tray_selection_window =
      XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);

  // Claim the system tray selection
  XSetSelectionOwner(display, _NET_SYSTEM_TRAY_S, tray_selection_window,
                     CurrentTime);
  if (XGetSelectionOwner(display, _NET_SYSTEM_TRAY_S) !=
      tray_selection_window) {
    fprintf(stderr, "Unable to get system tray selection\n");
    exit(1);
  }

  // Send MANAGER client message
  XClientMessageEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ClientMessage;
  ev.window = root;
  ev.message_type = MANAGER;
  ev.format = 32;
  ev.data.l[0] = CurrentTime;
  ev.data.l[1] = _NET_SYSTEM_TRAY_S;
  ev.data.l[2] = tray_selection_window;
  ev.data.l[3] = 0;
  ev.data.l[4] = 0;
  XSendEvent(display, root, False, StructureNotifyMask, (XEvent *)&ev);

  // Main event loop
  while (1) {
    XEvent event;
    XNextEvent(display, &event);

    if (event.type == Expose) {
      // Clear the window
      XClearWindow(display, tray_window);
    } else if (event.type == ClientMessage) {
      handle_systray_message(&event);
    } else if (event.type == DestroyNotify) {
      handle_destroy_notify(&event);
    } else if (event.type == ButtonPress) {
      printf("Button pressed\n");
    }
  }

  // Close connection to X server
  XCloseDisplay(display);
  return 0;
}
