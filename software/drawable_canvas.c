/*
 * This is a GTK+ program that allows you to draw on a canvas
 * with the mouse. It will also support undoing the previous stroke.
 * 
 * In the future, it will also include support via Skeltrack with an Xbox Kinect.
 *
 * Code sample starting point from https://docs.gtk.org/gtk3/getting_started.html#custom-drawing
 *
 * todo:
 *	  - Controls graphic at the bottom of the screen:
 *		  * A for pen down
 *		  * B to erase
 *		  * Y to confirm
 *	  - Make BUTTON_ERASE (B) work to undo last path, rather than erase entire image
 *			(low priority)
 *	  - also a low priority - video screen below drawing area so u can tell you're in the screen?
 *
 * @author Ella Moss
 */
#include <drawable_canvas.h>

cairo_surface_t  *surface		= NULL;
static GtkWidget *drawing_area;

GList* points_list = NULL;
GList* current_path = NULL;
static gdouble last_x = -1.0;
static gdouble last_y = -1.0;
static volatile int stage_active = 0;
static void draw_brush (GtkWidget *widget, gdouble x, gdouble y);

/***
 * This function clears the drawing off of the surface.
 */
static void clear_surface(){
	cairo_t *cr;
	cr = cairo_create(surface);

	//Sets drawing color to white
	cairo_set_source_rgb(cr, 1, 1, 1);

	cairo_paint(cr);
	cairo_destroy(cr);
}

/* Create a new surface of the appropriate size to store our scribbles */
static gboolean configure_event_cb (GtkWidget         *widget,
		GdkEventConfigure *event,
		gpointer           data)
{
	surface = gdk_window_create_similar_surface(gtk_widget_get_window (widget),
			CAIRO_CONTENT_COLOR,
			gtk_widget_get_allocated_width (widget),
			gtk_widget_get_allocated_height (widget));
	/* Initialize the surface to white */
	clear_surface ();

	/* We've handled the configure event, no need for further processing. */
	return TRUE;
}

static void destroy_widget(GtkWidget* widget, gpointer data){
	gtk_widget_destroy(widget);
}

static gboolean draw_cb (GtkWidget *widget,
		cairo_t   *cr,
		gpointer   data);
static void advance_stage(){
	// save their little doodle
	cairo_surface_write_to_png(surface, "drawing.png");

	// Clear the screen
	clear_surface();

	// Prepare for moving to next stage
	if (surface){
		cairo_surface_destroy (surface);
		surface = NULL;
	}
	if(drawing_area){
		g_signal_handlers_disconnect_by_func(drawing_area, G_CALLBACK(draw_cb), NULL);
		//gtk_widget_destroy(drawing_area);
		gtk_widget_hide(drawing_area);
	}
	if(joints_list != NULL)
		skeltrack_joint_list_free(joints_list); 

	// Change window title to next stage.
	// This also triggers the signal router for you.
	stage_active = 0;
	gtk_window_set_title(window, TITLE_SELECTOR);
}
/* Redraw the screen from the surface. Note that the ::draw
 * signal receives a ready-to-be-used cairo_t that is already
 * clipped to only draw the exposed areas of the widget
 */
SkeltrackJointList joints_list;
volatile int pen_down = 0;
int last_hand_x = -1;
int last_hand_y = -1;
int time_last_updated = 0;
int max_pix_per_sec = 75;
#define NUM_SAMPLES 8
int x_samples[NUM_SAMPLES];
int y_samples[NUM_SAMPLES];
int sample_i = 0;
int sample_ready = 0;
static gboolean draw_cb (GtkWidget *widget,
		cairo_t   *cr,
		gpointer   data)
{
	if(surface == NULL || drawing_area == NULL || cr == NULL){
		printf("something was null\n");
		return FALSE;
	}
	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_paint (cr);

	GAsyncResult *res = NULL;
	GError *error = NULL;

	// Process if pen is up or down
	if(btn_available && stage_active){
		if(event.number == BTN_RIGHT_A){
			pen_down = event.value;
			// button up: finish path
			if(!event.value){
				points_list = g_list_append(points_list, current_path);
				current_path = NULL;
				last_x = -1.0;
				last_y = -1.0;
			}
		}
		if(event.number == BTN_RIGHT_B){
			// b: clear surface
			clear_surface ();
			gtk_widget_queue_draw (widget);
		}
		if(event.number == BTN_RIGHT_Y 
				&& event.value == 1){
			btn_available = 0;
			advance_stage();
			printf("Advancing stage\n");
			return TRUE;
		}
	}
	btn_available = 0;

	if(error == NULL && joints_list != NULL){
		SkeltrackJoint *right_hand = skeltrack_joint_list_get_joint(joints_list, SKELTRACK_JOINT_ID_RIGHT_HAND);
		if(right_hand != NULL){
			// Paint right hand on canvas as a circle
			// Do some noise filtering first:
			int now_x = WINDOW_WIDTH - right_hand->screen_x;
			int now_y = right_hand->screen_y;
			/* THRESHOLD STRATEGY
			int dx = last_hand_x == -1 ? 0 : now_x - last_hand_x;
			int dy = last_hand_y == -1 ? 0 : now_y - last_hand_y;
			int dt = time(NULL) - time_last_updated;
			if(time_last_updated == 0)
				dt = 1;
			int dxdt = dt == 0 ? dx : dx/dt;
			int dydt = dt == 0 ? dy : dy/dt;
			//g_print("dxdt: %d, dydt: %d\n", dxdt, dydt);
			if(abs(dxdt) > max_pix_per_sec || abs(dydt) > max_pix_per_sec){
				//ignore this point
				return;
			}
			*/
			/* AVERAGE STRATEGY */
			x_samples[sample_i] = now_x;
			y_samples[sample_i++] = now_y;
			int avg_x = 0;
			int avg_y = 0;
			if(sample_i >= NUM_SAMPLES){
				for(int i=0; i<sample_i; i++){
					avg_x += x_samples[i];
					avg_y += y_samples[i];
				}
				avg_x /= NUM_SAMPLES;
				avg_y /= NUM_SAMPLES;
				sample_i = 0;
				sample_ready = 1;
			}


			/*
			last_hand_x = now_x;
			last_hand_y = now_y;
			time_last_updated = time(NULL);
			*/

			cairo_set_source_rgb(cr, 1, 0, 0);
			cairo_arc(cr, last_hand_x, last_hand_y, 10, 0, 2 * G_PI);
			cairo_fill(cr);

			if(sample_ready){
				if(pen_down){
					draw_brush(widget, avg_x, avg_y);
				}
				last_hand_x = avg_x;
				last_hand_y = avg_y;
				sample_ready = 0;
			}
		}
		//if(joints_list != NULL)
			//skeltrack_joint_list_free(joints_list); //this segfaults sometimes???? idk.
	}
	return FALSE;
}

/* This list contains all the points so that when they're ready to move on,
 * you can draw them all at once.
 * points_list:
 * { { {0,0}, {0,1}, ...},
 *	 { {5, 6}, {7,8} ...}}
 */
/* Draw a rectangle on the surface at the given position */
static void draw_brush (GtkWidget *widget,
		gdouble    x,
		gdouble    y)
{
	// Save this point to the path
	DoublePoint* p = malloc(sizeof(DoublePoint));
	p->x = x;
	p->y = y;
	current_path = g_list_append(current_path, p);

	cairo_t *cr;
	
	/* Paint to the surface, where we store our state */
	cr = cairo_create (surface);
	cairo_set_line_width(cr, 6);

	// If last_x and last_y are not -1, then draw from last coordinate to this one.
	if(last_x != -1 && last_y != -1){
		gdouble dx = x - last_x;
		gdouble dy = y - last_y;
		//printf("dx: %f, dy: %f, last_x: %f, last_y: %f\n", dx, dy, last_x, last_y);
		cairo_new_path(cr);
		cairo_move_to(cr, last_x, last_y);
		cairo_line_to(cr, x, y);
		cairo_stroke(cr);
		/* Now invalidate the drawing area along the line. */
		gtk_widget_queue_draw_area (widget, MIN(last_x, x) - 12, MIN(last_y, y) - 12, ABS(dx)+24.0, ABS(dy)+24.0);
	}
	else{
		cairo_rectangle (cr, x - 3, y - 3, 6, 6);
		cairo_fill (cr);
		gtk_widget_queue_draw_area (widget, x - 3, y - 3, 6, 6);
	}
	cairo_destroy (cr);

	last_x = x;
	last_y = y;
}

/* Handle button press events by either drawing a rectangle
 * or clearing the surface, depending on which button was pressed.
 * The ::button-press signal handler receives a GdkEventButton
 * struct which contains this information.
 */

#define BUTTON_PEN_DOWN GDK_BUTTON_PRIMARY
#define BUTTON_ERASE GDK_BUTTON_SECONDARY
#define BUTTON_SAVE GDK_BUTTON_MIDDLE
static gboolean button_press_event_cb (GtkWidget      *widget,
		GdkEventButton *event,
		gpointer        data)
{
	/* paranoia check, in case we haven't gotten a configure event */
	if (surface == NULL){
		printf("Something was null\n");
		return FALSE;
	}

	g_print("Button pressed: %d\n", event->button);
	if (event->button == BUTTON_PEN_DOWN)
	{
		draw_brush (widget, event->x, event->y);
	}
	else if (event->button == BUTTON_ERASE)
	{
		clear_surface ();
		gtk_widget_queue_draw (widget);
	}
	else if(event->button == BUTTON_SAVE){
		// Save the current drawing to a file
		advance_stage();
	}

	/* We've handled the event, stop processing */
	return TRUE;
}

/* Handle button release event by resetting last coordinates to
 * -1.0, -1.0 if it was button 1 (left click) that was released.
 */
static gboolean button_release_event_cb (GtkWidget      *widget,
		GdkEventButton *event,
		gpointer        data)
{
	printf("button release\n");
	if (event->button == BUTTON_PEN_DOWN)
	{
		//printf("Button released\n");
		// Add the current path to the list of paths
		points_list = g_list_append(points_list, current_path);
		current_path = NULL;
		last_x = -1.0;
		last_y = -1.0;
	}

	return TRUE;
}

/* Handle motion events by continuing to draw if button 1 is
 * still held down. The ::motion-notify signal handler receives
 * a GdkEventMotion struct which contains this information.
 */
static gboolean motion_notify_event_cb (GtkWidget      *widget,
		GdkEventMotion *event,
		gpointer        data)
{
	/* paranoia check, in case we haven't gotten a configure event */
	if (surface == NULL){
		printf("somethingg was null\n");
		return FALSE;
	}

	if (event->state & GDK_BUTTON1_MASK)
		draw_brush (widget, event->x, event->y);

	/* We've handled it, stop processing */
	return TRUE;
}

static void close_window (void)
{
	if (surface)
		cairo_surface_destroy (surface);
}

/***
 * This function initializes widgets and other objects that the 
 * canvas uses.
 *
 * After this is called, call activate_canvas to select this stage
 * and show all relevant widgets.
 */
GtkWidget* setup_canvas(){
	drawing_area = gtk_drawing_area_new ();

	/* set a minimum size */
	gtk_widget_set_size_request (drawing_area, 500, 500);

	gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);

	// Signals used to handle the backing surface
	g_signal_connect (drawing_area, "draw",
			G_CALLBACK (draw_cb), NULL);
	g_signal_connect (drawing_area,"configure-event",
			G_CALLBACK (configure_event_cb), NULL);

	// Event signals
	g_signal_connect (drawing_area, "motion-notify-event",
			G_CALLBACK (motion_notify_event_cb), NULL);
	g_signal_connect (drawing_area, "button-press-event",
			G_CALLBACK (button_press_event_cb), NULL);
	g_signal_connect (drawing_area, "button-release-event",
			G_CALLBACK (button_release_event_cb), NULL);

	// Ask to receive events the drawing area doesn't normally
	// subscribe to. In particular, we need to ask for the
	// button press and motion notify events that want to handle.

	// unrealize drawing area first for some reason
	gtk_widget_unrealize(drawing_area);
	gtk_widget_set_events (drawing_area, gtk_widget_get_events (drawing_area)
			| GDK_BUTTON_PRESS_MASK
			| GDK_BUTTON_RELEASE_MASK
			| GDK_POINTER_MOTION_MASK);
	gtk_widget_realize(drawing_area);
	gtk_widget_hide(drawing_area);
	return drawing_area;
}

void activate_canvas (GtkApplication *app,
		void*		pspspscpscpskj,
		gpointer	 user_data)
{
	// Resize window
	gtk_widget_set_size_request(vbox, WINDOW_WIDTH, WINDOW_HEIGHT);
	gtk_window_resize(window, WINDOW_WIDTH, WINDOW_HEIGHT);

	// Show drawing area
	printf("showing drawing_area\n");
	configure_event_cb (drawing_area, NULL, NULL);

	g_signal_connect (drawing_area, "draw",
			G_CALLBACK (draw_cb), NULL);

	gtk_widget_show(drawing_area);
	gtk_widget_show(vbox);

	clear_surface();
	stage_active = 1;
}
